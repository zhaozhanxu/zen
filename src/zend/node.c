#include "node.h"

node_regist_t *node_regist = NULL;
// node map registrat and topology
node_map_t node_map_head;
nodes_topology_t *nodes_topo_head = NULL;

static void
register_all_static_nodes(void)
{
    node_regist_t *r = node_regist;
    while (r) {
        //judge the name is unique
        if (unlikely(hash_get_node(&node_map_head,
                                   r->node_info.name) != NULL)) {
            zen_panic("node %s is exist\n", r->node_info.name);
        }
        hash_set_node(&node_map_head, &r->node_info);
        r = r->next_regist;
    }
}

static void
parse_node_topology(void)
{
    if (unlikely(nodes_topo_head != NULL)) {
        zen_log(RTE_LOG_ERR, "node topology not NULL, already parsed\n");
        return;
    }

    const char *graph_node_conf = dpdk_get_global_value("node",
                                                        "graph_node_conf");
    if (!graph_node_conf) {
        zen_log(RTE_LOG_ERR, "graph node config file not set, use default\n");
        graph_node_conf = GRAPH_NODE_CONF;
    }
    zen_log(RTE_LOG_NOTICE, "graph node config %s\n", graph_node_conf);

    struct rte_cfgfile *cfg_file = rte_cfgfile_load(graph_node_conf, 0);
    if (!cfg_file) {
        zen_panic("load file %s err!\n", graph_node_conf);
    }
    zen_log(RTE_LOG_NOTICE, "load file %s success!\n", graph_node_conf);

    int32_t sect_count = rte_cfgfile_num_sections(cfg_file, NULL, 0);
    if (!sect_count) {
        zen_log(RTE_LOG_ERR, "graph node num is 0\n");
        goto quit;
    }
    zen_log(RTE_LOG_NOTICE, "graph node num is %d\n", sect_count);

    char **sect_names = (char **)malloc(sect_count * sizeof(char *));
    if (!sect_names) {
        zen_panic("alloc sect_names err!\n");
    }
    memset(sect_names, 0, sect_count * sizeof(char *));
    for (int32_t i = 0; i < sect_count; i++) {
        sect_names[i] = malloc(64);
        if (!sect_names[i]) {
            zen_panic("alloc sect_names[%d] err!\n", i);
        }
        memset(sect_names[i], 0, 64);
    }
    rte_cfgfile_sections(cfg_file, sect_names, sect_count);

    //for the first node
    node_info_t *n = hash_get_node(&node_map_head, sect_names[0]);
    if (unlikely(!n)) {
        zen_panic("node %s or topology is not exist!\n", sect_names[0]);
    }
    nodes_topo_head = create_node_topology(n);
    if (unlikely(!nodes_topo_head)) {
        zen_panic("can't create node topology head!\n");
    }

    for (int32_t i = 0; i < sect_count; i++) {
        char *sect_name = sect_names[i];
        n = hash_get_node(&node_map_head, sect_name);
        if (unlikely(!n || !n->topology)) {
            zen_panic("node %s or topology is not exist!\n", sect_name);
        }

        int32_t sect_entry_count = rte_cfgfile_section_num_entries(cfg_file,
                                                                   sect_name);
        struct rte_cfgfile_entry * entries = malloc(
                    sect_entry_count * sizeof(struct rte_cfgfile_entry));
        if (!entries) {
            zen_panic("alloc entries err!\n");
        }
        memset(entries, 0, sect_entry_count *
                           sizeof(struct rte_cfgfile_entry));
        rte_cfgfile_section_entries(cfg_file, sect_name, entries,
                                    sect_entry_count);
        zen_log(RTE_LOG_NOTICE, "current node is %s\n", sect_name);
        for (int32_t j = 0; j < sect_entry_count; j++) {
            struct rte_cfgfile_entry *entry = &entries[j];
            uint32_t slot;
            sscanf(entry->name, "%d", &slot);
            if (slot >= MAX_NEXT_NODE) {
                zen_panic("next node num(%d) > max(%d)\n",
                          slot, MAX_NEXT_NODE);
            }
            node_info_t *next_node = hash_get_node(&node_map_head,
                                                   entry->value);
            if (unlikely(!next_node)) {
                zen_panic("next node %s is not exist!\n", entry->value);
            }
            if (!next_node->topology) {
                next_node->topology = create_node_topology(next_node);
            }
            if (unlikely(!next_node->topology)) {
                zen_panic("can't create node topology!\n");
            }
            ((nodes_topology_t *)n->topology)->next[slot] =
                                                    next_node->topology;
            zen_log(RTE_LOG_NOTICE, "next node %d = %s\n", slot, entry->value);
        }
    }

quit:
    rte_cfgfile_close(cfg_file);
}

static void
create_nodes_runtime(void)
{
    if (unlikely(!nodes_topo_head || !node_regist)) {
        zen_panic("node topology is %p and node register is %p\n",
                  nodes_topo_head, node_regist);
        return;
    }

    node_regist_t *r = node_regist;
    while (r) {
        if (r->node_info.topology) {
            for (uint8_t i = 0; i < RTE_MAX_LCORE; i++) {
                if (!rte_lcore_is_enabled(i)) {
                    continue;
                }
                nodes_runtime_t *new_node = create_node_runtime(
                                                    &r->node_info, i);
                if (unlikely(!new_node)) {
                    zen_panic("node runtime create err!\n");
                }
                new_node->node_info->runtime[i] = new_node;
            }
        }
        r = r->next_regist;
    }
}

void
node_init(void)
{
    register_all_static_nodes();
    parse_node_topology();
    create_nodes_runtime();
}

static nodes_runtime_t *nodes_runtime_head[RTE_MAX_LCORE];
static nodes_runtime_t *nodes_runtime_end[RTE_MAX_LCORE];
//because one user use one runtime node
void
put_pkt_to_curr_node(node_info_t *node, struct rte_mbuf *mbuf)
{
    uint32_t lcore_id = rte_lcore_id();
    nodes_runtime_t *r = node->runtime[lcore_id];
    rte_ring_enqueue(r->ring, mbuf);

    if (r->in_use) {
        return;
    }

    r->in_use = true;
    //not need lock
    if (!nodes_runtime_head[lcore_id]) {
        nodes_runtime_head[lcore_id] = r;
        nodes_runtime_end[lcore_id] = r;
    } else {
        nodes_runtime_end[lcore_id]->next = r;
        nodes_runtime_end[lcore_id] = r;
    }
}

void
put_pkts_to_curr_node(node_info_t *node, struct rte_mbuf **mbufs, uint32_t n)
{
    uint32_t lcore_id = rte_lcore_id();
    nodes_runtime_t *r = node->runtime[lcore_id];
    rte_ring_enqueue_bulk(r->ring, (void **)mbufs, n);

    if (r->in_use) {
        return;
    }

    r->in_use = true;
    //not need lock
    if (!nodes_runtime_head[lcore_id]) {
        nodes_runtime_head[lcore_id] = r;
        nodes_runtime_end[lcore_id] = r;
    } else {
        nodes_runtime_end[lcore_id]->next = r;
        nodes_runtime_end[lcore_id] = r;
    }
}

void
put_pkt_to_next_node(node_info_t *prev_node,
                     struct rte_mbuf *mbuf, uint8_t slot)
{
    nodes_topology_t *topology =
            ((nodes_topology_t *)prev_node->topology)->next[slot];
    if (unlikely(!topology)) {
        zen_log(RTE_LOG_DEBUG, "prev_node %p have no %d next node\n",
                prev_node, slot);
        rte_pktmbuf_free(mbuf);
        return;
    }
    node_info_t *node = topology->node_info;
    put_pkt_to_curr_node(node, mbuf);
}

void
put_pkts_to_next_node(node_info_t *prev_node, struct rte_mbuf **mbufs,
                      uint32_t n, uint8_t slot)
{
    nodes_topology_t *topology =
            ((nodes_topology_t *)prev_node->topology)->next[slot];
    if (unlikely(!topology)) {
        zen_log(RTE_LOG_DEBUG, "prev_node %p have no %d next node\n",
                prev_node, slot);
        for (uint32_t i = 0; i < n; i++) {
            rte_pktmbuf_free(mbufs[i]);
        }
        return;
    }
    node_info_t *node = topology->node_info;
    put_pkts_to_curr_node(node, mbufs, n);
}

int32_t
get_pkts_from_curr_node(node_info_t *node, struct rte_mbuf **mbufs, uint32_t n)
{
    uint32_t lcore_id = rte_lcore_id();
    nodes_runtime_t *r = node->runtime[lcore_id];
    int32_t nb_pkts = rte_ring_dequeue_bulk(r->ring, (void **)mbufs, n);

    r->in_use = false;
    if (nodes_runtime_end[lcore_id] == r) {
        nodes_runtime_head[lcore_id] = nodes_runtime_end[lcore_id] = NULL;
    } else {
        nodes_runtime_head[lcore_id] = r->next;
    }

    return nb_pkts;
}
