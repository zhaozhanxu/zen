#include "ethernet.h"

/****************************************************************
 * fib
 ***************************************************************/
static l2_fib_t l2_fib_table = {
    .global_learn_limit = ETHER_FIB_NUM_BUCKETS << 4,
    .nb_buckets = ETHER_FIB_NUM_BUCKETS,
};

static mem_pool_t *pool;

void
ethernet_fib_init(void)
{
    pool = mem_pool_create(sizeof(mac_key_value_t),
                           l2_fib_table.global_learn_limit);
    if (unlikely(!pool)) {
        zen_panic("alloc l2_fib_table err!\n");
    }
    for (uint64_t i = 0; i < ETHER_FIB_NUM_BUCKETS; i++) {
        rte_spinlock_init(&l2_fib_table.lock[i]);
        INIT_HLIST_HEAD(&l2_fib_table.buckets[i]);
    }
}

uint64_t
ethernet_fib_lookup(uint64_t value)
{
    uint32_t index = (value & ~0xffff) & (ETHER_FIB_NUM_BUCKETS - 1);
    struct hlist_head *head = &l2_fib_table.buckets[index];

    mac_key_value_t *target;
    hlist_for_each_entry_rcu (target, head, node) {
        if ((target->value & ~0xffff) == (value & ~0xffff)) {
            return target->value;
        }
    }
    return 0;
}

void
ethernet_fib_add(uint64_t value)
{
    uint32_t index = (value & ~0xffff) & (ETHER_FIB_NUM_BUCKETS - 1);
    struct hlist_head *head = &l2_fib_table.buckets[index];
    rte_spinlock_t *lock = &l2_fib_table.lock[index];

    mac_key_value_t *target;
    hlist_for_each_entry_rcu (target, head, node) {
        if ((target->value & ~0xffff) == (value & ~0xffff)) {
            if (value != target->value) {
                target->value = value;
            }
            return;
        }
    }
    mac_key_value_t *key_value = mem_pool_alloc(pool);
    if (!key_value) {
        zen_log(RTE_LOG_NOTICE,
                "can't add fib, because mem_pool_alloc err!\n");
        return;
    }
    key_value->value = value;
    rte_spinlock_lock(lock);
    hlist_add_head_rcu(&key_value->node, head);
    rte_spinlock_unlock(lock);
}

void
ethernet_fib_del(uint64_t value)
{
    uint32_t index = (value & ~0xffff) & (ETHER_FIB_NUM_BUCKETS - 1);
    struct hlist_head *head = &l2_fib_table.buckets[index];
    rte_spinlock_t *lock = &l2_fib_table.lock[index];

    mac_key_value_t *target;
    hlist_for_each_entry_rcu (target, head, node) {
        if ((target->value & ~0xffff) == (value & ~0xffff)) {
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
ethernet_fib_ageing(time_t now)
{
    for (uint32_t index = 0; index < ETHER_FIB_NUM_BUCKETS; index++) {
        struct hlist_head *head = &l2_fib_table.buckets[index];
        rte_spinlock_t *lock = &l2_fib_table.lock[index];

        mac_key_value_t *target;
        hlist_for_each_entry_rcu (target, head, node) {
            if ((now - target->out_time) >= ETHER_FIB_AGEING_TIME) {
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

ether_interface_t ether_inerface[RTE_MAX_ETHPORTS];

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
ethernet_init(void)
{
    uint8_t count = rte_eth_dev_count();
    for (uint8_t i = 0; i < count; i++) {
        int32_t ret = 0;
        ret = rte_eth_dev_get_name_by_port(i, ether_inerface[i].name);
        if (ret < 0) {
            zen_log(RTE_LOG_ERR, "get port %d name err!\n", i);
            return false;
        }
        rte_eth_macaddr_get(i, &ether_inerface[i].mac_addr);
        rte_eth_link_get_nowait(i, &ether_inerface[i].link);
    }
    return true;
}

INIT_FUNCTION(ethernet_init);

bool
plugin_register(void)
{
    //get the heart
    return true;
}
