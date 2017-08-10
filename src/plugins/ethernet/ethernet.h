#ifndef __ETHERNET_H__
#define __ETHERNET_H__

#include "common.h"
#include "plugins.h"
#include "node.h"
#include "init.h"
#include "cli.h"
#include "mem_pool.h"

typedef struct {
    char name[RTE_ETH_NAME_MAX_LEN];
    struct ether_addr mac_addr;
    struct rte_eth_link link;
    //	uint64_t rx_pkts;
    //	uint64_t rx_bytes;
    //	uint64_t tx_pkts;
    //	uint64_t tx_bytes;
    //	uint64_t drop_pkts;
    //	uint64_t drop_bytes;
} ether_interface_t;

extern ether_interface_t ether_inerface[RTE_MAX_ETHPORTS];

/****************************************************************
 * fib
 ***************************************************************/
#define ETHER_FIB_NUM_BUCKETS (64<<10) //64k
#define ETHER_FIB_AGEING_TIME 1440 // 4 hour

typedef struct mac_key_value {
    uint64_t value; //high 48 is mac addr, and low 16 bit is port id
    time_t out_time;
    struct hlist_node node;
} mac_key_value_t;

typedef struct {
    uint32_t global_learn_limit;
    uint32_t nb_buckets;
    rte_spinlock_t lock[ETHER_FIB_NUM_BUCKETS];
    struct hlist_head buckets[ETHER_FIB_NUM_BUCKETS];
} l2_fib_t;

static inline uint64_t mac_to_u64(struct ether_addr *addr)
{
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
    return *((uint64_t *) (addr->addr_bytes - 2)) & ~0xffff;
#else
    return *((uint64_t *) (addr->addr_bytes)) & ~0xffff;
#endif
}

uint64_t ethernet_fib_lookup(uint64_t value);
void ethernet_fib_add(uint64_t value);
void ethernet_fib_del(uint64_t value);

#endif
