#include "ethernet.h"

#define NEXT_FORWARD	0
#define NEXT_ARP		1
#define NEXT_IPv4		2

static uint64_t cached_value = 0;
static uint8_t cached_next_node = UINT8_MAX;

static uint64_t
ethernet_input(node_info_t *node)
{
    struct rte_mbuf *mbuf, *mbufs[MAX_RING_NUM];
    uint32_t nb_pkts = get_pkts_from_curr_node(node, mbufs, MAX_RING_NUM);
    uint32_t index = 0;
    while (nb_pkts > index) {
        mbuf = mbufs[index];
        if (nb_pkts > index + 1) {
            rte_prefetch0(mbufs[index + 1]);
        }
        if (ether_inerface[mbuf->port].link.link_status == ETH_LINK_DOWN) {
            rte_pktmbuf_free(mbuf);
            index ++;
            continue;
        }
        struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
        if (is_zero_ether_addr(&eth_hdr->d_addr)) {
            rte_pktmbuf_free(mbuf);
            index ++;
            continue;
        }
        struct ether_addr *port_addr = &ether_inerface[mbuf->port].mac_addr;
        uint64_t value = mac_to_u64(&eth_hdr->s_addr) | mbuf->port;
        //only judge src mac ???? Warnning
        if ((cached_value == value) && (cached_next_node != UINT8_MAX)) {
            goto next_node;
        }
        ethernet_fib_add(value);
        if (is_unicast_ether_addr(&eth_hdr->d_addr) &&
                    !is_same_ether_addr(&eth_hdr->d_addr, port_addr)) {
            cached_value = value;
            cached_next_node = NEXT_FORWARD;
        } else {
            switch (rte_be_to_cpu_16(eth_hdr->ether_type)) {
                case ETHER_TYPE_ARP:
                    cached_value = value;
                    cached_next_node = NEXT_ARP;
                    rte_pktmbuf_adj(mbuf, sizeof(eth_hdr));
                    break;
                case ETHER_TYPE_IPv4:
                    cached_value = value;
                    cached_next_node = NEXT_IPv4;
                    rte_pktmbuf_adj(mbuf, sizeof(eth_hdr));
                    break;
                default:
                    rte_pktmbuf_free(mbuf);
                    goto statistics;
            }
        }

next_node:
        put_pkt_to_next_node(node, mbuf, cached_next_node);

statistics:
        index ++;
    }
    return 0;
}

REGISTER_NODE(ethernet_input_node) = {
    .node_info = {
        .function	= ethernet_input,
        .name		= "ethernet-input",
    }
};

