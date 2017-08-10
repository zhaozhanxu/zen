#include "arp.h"

/****************************************************************
 * arp table
 ***************************************************************/
static arp_table_t arp_table = {
    .global_learn_limit = ARP_NUM_BUCKETS << 4,
    .nb_buckets = ARP_NUM_BUCKETS,
};

static mem_pool_t *pool;

void
arp_table_init(void)
{
    pool = mem_pool_create(sizeof(arp_key_value_t),
                           arp_table.global_learn_limit);
    if (unlikely(!pool)) {
        zen_panic("alloc arp_table err!\n");
    }
    for (uint64_t i = 0; i < ARP_NUM_BUCKETS; i++) {
        rte_spinlock_init(&arp_table.lock[i]);
        INIT_HLIST_HEAD(&arp_table.buckets[i]);
    }
}

uint64_t
arp_table_lookup(uint32_t addr)
{
    uint32_t index = addr & (ARP_NUM_BUCKETS - 1);
    struct hlist_head *head = &arp_table.buckets[index];

    arp_key_value_t *target;
    hlist_for_each_entry_rcu (target, head, node) {
        if (target->addr == addr) {
            return target->value;
        }
    }
    return 0;
}

void
ar_table_add(uint32_t addr, uint64_t value)
{
    uint32_t index = addr & (ARP_NUM_BUCKETS - 1);
    struct hlist_head *head = &arp_table.buckets[index];
    rte_spinlock_t *lock = &arp_table.lock[index];

    arp_key_value_t *target;
    hlist_for_each_entry_rcu (target, head, node) {
        if (target->addr  == addr) {
            if (value != target->value) {
                target->value = value;
            }
            return;
        }
    }
    arp_key_value_t *key_value = mem_pool_alloc(pool);
    if (!key_value) {
        zen_log(RTE_LOG_NOTICE,
                "can't add arp, because mem_pool_alloc err!\n");
        return;
    }
    time(&key_value->out_time);
    key_value->addr = addr;
    key_value->value = value;
    rte_spinlock_lock(lock);
    hlist_add_head_rcu(&key_value->node, head);
    rte_spinlock_unlock(lock);
}

void
arp_table_del(uint32_t addr)
{
    uint32_t index = addr & (ARP_NUM_BUCKETS - 1);
    struct hlist_head *head = &arp_table.buckets[index];
    rte_spinlock_t *lock = &arp_table.lock[index];

    arp_key_value_t *target;
    hlist_for_each_entry_rcu (target, head, node) {
        if (target->addr == addr) {
            break;
        }
    }
    rte_spinlock_lock(lock);
    if (target && !is_hlist_node_free(&target->node)) {
        hlist_del_rcu(&target->node);
    }
    rte_spinlock_unlock(lock);
    mem_pool_free(pool, (void *)target);
}

void
arp_table_ageing(time_t now)
{
    for (uint32_t index = 0; index < ARP_NUM_BUCKETS; index++) {
        struct hlist_head *head = &arp_table.buckets[index];
        rte_spinlock_t *lock = &arp_table.lock[index];

        arp_key_value_t *target;
        hlist_for_each_entry_rcu (target, head, node) {
            if ((now - target->out_time) >= ARP_AGEING_TIME) {
                rte_spinlock_lock(lock);
                if (target && !is_hlist_node_free(&target->node)) {
                    hlist_del_rcu(&target->node);
                }
                rte_spinlock_unlock(lock);
                mem_pool_free(pool, (void *)target);
            }
        }
    }
}

static uint64_t
arp_input(node_info_t *node)
{
    struct rte_mbuf *mbuf, *mbufs[MAX_RING_NUM];
    uint32_t nb_pkts = get_pkts_from_curr_node(node, mbufs, MAX_RING_NUM);
    uint32_t index = 0;
    while (nb_pkts > index) {
        mbuf = mbufs[index];
        if (nb_pkts > index + 1) {
            rte_prefetch0(mbufs[index + 1]);
        }
        struct arp_hdr *arp_hdr = rte_pktmbuf_mtod(mbuf, struct arp_hdr *);
        uint64_t value = mac_to_u64(&arp_hdr->arp_data.arp_sha) | mbuf->port;
        uint32_t addr = arp_hdr->arp_data.arp_sip;
        arp_table_add(addr, value);

        if (arp_hdr->arp_op == ARP_OP_REPLY) {
            rte_pktmbuf_free(mbuf);
            goto statistics;
        } else if (arp_hdr->arp_op == ARP_OP_REQUEST) {
            //check the target ip match the ip of mbuf->port
            //modify arp from request to reply
            //send packet to l2_output
            //put_pkt_to_next_node(node, mbuf, cached_next_node);
        }

statistics:
        index ++;
    }
    return 0;
}

REGISTER_NODE(arp_input_node) = {
    .node_info = {
        .function	= arp_input,
        .name		= "arp-input",
    }
};

static bool
show_test(void)
{
    return true;
}

CLI_COMMAND(show_test_command) = {
    .path		= "show test info",
    .f			= show_test,
    .help		= "show test",
};

static bool
arp_init(void)
{
    arp_table_init();
    return true;
}

INIT_FUNCTION(arp_init);

bool
plugin_register(void)
{
    //get the heart;
    return true;
}
