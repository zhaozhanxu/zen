#include "common.h"
#include "plugins.h"
#include "node.h"
#include "init.h"

#define NEXT_FORWARD	0
#define NEXT_VLAN		1
#define NEXT_ARP		2
#define NEXT_IPv4		3

static struct ether_addr mac_addr[RTE_MAX_ETHPORTS];

static uint64_t
sample(node_info_t *node)
{
    struct rte_mbuf *mbuf, *mbufs[MAX_RING_NUM];
    uint32_t nb_pkts = get_pkts_from_curr_node(node, mbufs, MAX_RING_NUM);
    uint32_t index = 0;
    while (nb_pkts > index) {
        mbuf = mbufs[index];
        if (nb_pkts > index + 1) {
            rte_prefetch0(mbufs[index + 1]);
        }
        struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
        if (is_zero_ether_addr(&eth_hdr->d_addr)) {
            rte_pktmbuf_free(mbuf);
            index ++;
            continue;
        }
        if (is_unicast_ether_addr(&eth_hdr->d_addr) &&
                                  !is_same_ether_addr(&eth_hdr->d_addr,
                                                      &mac_addr[mbuf->port])) {
            put_pkt_to_next_node(node, mbuf, NEXT_FORWARD);
        } else {
            switch (rte_be_to_cpu_16(eth_hdr->ether_type)) {
                case ETHER_TYPE_VLAN:
                    put_pkt_to_next_node(node, mbuf, NEXT_VLAN);
                    break;
                case ETHER_TYPE_ARP:
                    put_pkt_to_next_node(node, mbuf, NEXT_ARP);
                    break;
                case ETHER_TYPE_IPv4:
                    put_pkt_to_next_node(node, mbuf, NEXT_IPv4);
                    break;
                default:
                    rte_pktmbuf_free(mbuf);
                    break;
            }
        }
        index ++;
    }
    return 0;

}

REGISTER_NODE(sample_node) = {
    .node_info = {
        .function	= sample,
        .name		= "sample",
    }
};

static bool ethernet_input_init(void)
{
    uint8_t count = rte_eth_dev_count();
    for (uint8_t i = 0; i < count; i++) {
        rte_eth_macaddr_get(i, &mac_addr[i]);
    }
    return true;
}

INIT_FUNCTION(ethernet_input_init);

bool plugin_register(void)
{
    //get the heart
    return true;
}
