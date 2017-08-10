#ifndef __NODE_H__
#define __NODE_H__

#include "common.h"
#include "dpdk_env.h"

#define GRAPH_NODE_CONF "/etc/zen/node.conf"

#define MAX_NEXT_NODE 32
#define MAX_RING_NUM 256

struct _node_info_t;
typedef uint64_t (node_function_t)(struct _node_info_t *node);

typedef struct _node_info_t {
    node_function_t *function;
    char *name;
    //uint32_t index;
    void *topology; //to find the node topology
    void *runtime[RTE_MAX_LCORE]; //to find the node topology
} node_info_t;

/*********************************************************************
 * registed nodes
 ********************************************************************/
typedef struct _node_regist {
    node_info_t node_info;
    struct _node_regist *next_regist;
} node_regist_t;

extern node_regist_t *node_regist;

#define REGISTER_NODE(x,...)				\
    __VA_ARGS__ node_regist_t x;			\
static void __add_node_regist_##x(void)		\
__attribute__((__constructor__));			\
static void __add_node_regist_##x(void)		\
{											\
    x.next_regist = node_regist;			\
    node_regist = &x;						\
}											\
__VA_ARGS__ node_regist_t x

/*********************************************************************
 * nodes map registrations to topology
 ********************************************************************/
typedef struct _nodes_t {
    node_info_t *node;
    struct _nodes_t *next;
} nodes_t;
#define HASH_SIZE 256
typedef struct {
    nodes_t *hash_table[HASH_SIZE];
} node_map_t;

extern node_map_t node_map_head;

static inline void
hash_set_node(node_map_t *head, node_info_t *node)
{
    uint64_t pos = rte_jhash(node->name, strlen(node->name), 0) % HASH_SIZE;
    nodes_t *new_nodes = (nodes_t *)calloc(sizeof(nodes_t), 1);
    if (unlikely(!new_nodes)) {
        zen_panic("calloc nodes err!\n");
    }
    new_nodes->node = node;
    new_nodes->next = head->hash_table[pos];
    head->hash_table[pos] = new_nodes;
}

static inline node_info_t *
hash_get_node(node_map_t *head, char *name)
{
    uint64_t pos = rte_jhash(name, strlen(name), 0) % HASH_SIZE;

    nodes_t *nodes = head->hash_table[pos];
    while (nodes) {
        if (!strcmp(nodes->node->name, name)) {
            return nodes->node;
        }
        nodes = nodes->next;
    }
    return NULL;
}

/*********************************************************************
 * nodes topology
 ********************************************************************/
typedef struct _nodes_topology_t {
    node_info_t *node_info;
    struct _nodes_topology_t *next[MAX_NEXT_NODE];
} nodes_topology_t;

extern nodes_topology_t *nodes_topo_head;
static inline nodes_topology_t *
create_node_topology(node_info_t *n)
{
    nodes_topology_t *new_node = calloc(sizeof(nodes_topology_t), 1);
    if (unlikely(!new_node)) {
        zen_log(RTE_LOG_ERR, "can't alloc, so do not create node topology\n");
        return NULL;
    }

    new_node->node_info = n;
    new_node->node_info->topology = new_node;
    return new_node;
}

/*********************************************************************
 * nodes runtime
 ********************************************************************/
typedef struct _nodes_runtime_t {
    bool in_use;
    node_info_t *node_info;
    struct rte_ring *ring;
    struct _nodes_runtime_t *next;
} nodes_runtime_t;

static inline nodes_runtime_t *
create_node_runtime(node_info_t *n, uint8_t lcore_id)
{
    nodes_runtime_t *new_node = calloc(sizeof(nodes_runtime_t), 1);
    if (unlikely(!new_node)) {
        zen_log(RTE_LOG_ERR, "can't alloc, so do not create node runtime\n");
        return NULL;
    }

    char buf[16];
    sprintf(buf, "ring_%d_%p", lcore_id, n);
    new_node->ring = rte_ring_create(buf, MAX_RING_NUM, rte_socket_id(), 0);
    if (unlikely(!new_node->ring)) {
        zen_log(RTE_LOG_ERR, "can't alloc, so do not create ring\n");
        free(new_node);
        return NULL;
    }

    new_node->node_info = n;
    return new_node;
}

void node_init(void);
void put_pkt_to_curr_node(node_info_t *node, struct rte_mbuf *mbuf);
void put_pkts_to_curr_node(node_info_t *node,
                           struct rte_mbuf **mbufs, uint32_t n);
void put_pkt_to_next_node(node_info_t *prev_node,
                          struct rte_mbuf *mbuf, uint8_t slot);
void put_pkts_to_next_node(node_info_t *prev_node,
                           struct rte_mbuf **mbufs, uint32_t n, uint8_t slot);
int32_t get_pkts_from_curr_node(node_info_t *node,
                                struct rte_mbuf **mbufs, uint32_t n);

#endif
