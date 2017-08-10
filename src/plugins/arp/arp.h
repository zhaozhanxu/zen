#ifndef __arp_h__
#define __arp_h__

#include "common.h"
#include "plugins.h"
#include "node.h"
#include "init.h"
#include "cli.h"
#include "mem_pool.h"

/****************************************************************
 * arp
 ***************************************************************/
#define ARP_NUM_BUCKETS (64<<10) //64k
#define ARP_AGEING_TIME 1440 // 4 hour

typedef struct arp_key_value {
    uint64_t value; //high 48 is mac addr, and low 16 bit is port id
    uint32_t addr; // ip address
    time_t out_time;
    struct hlist_node node;
} arp_key_value_t;

typedef struct {
    uint32_t global_learn_limit;
    uint32_t nb_buckets;
    rte_spinlock_t lock[ARP_NUM_BUCKETS];
    struct hlist_head buckets[ARP_NUM_BUCKETS];
} arp_table_t;

static inline uint64_t mac_to_u64(struct ether_addr *addr)
{
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
    return *((uint64_t *) (addr->addr_bytes - 2)) & ~0xffff;
#else
    return *((uint64_t *) (addr->addr_bytes)) & ~0xffff;
#endif
}

uint64_t arp_table_lookup(uint32_t addr);
void arp_table_add(uint32_t addr, uint64_t value);
void arp_table_del(uint32_t addr);

#endif
