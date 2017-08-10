#include "ethernet.h"

static uint64_t cached_value = 0;
static uint8_t cached_port = UINT8_MAX;

static uint64_t
ethernet_output(node_info_t *node)
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
        uint64_t addr = mac_to_u64(&eth_hdr->d_addr);
        if (cached_value == addr && cached_port != UINT8_MAX) {
            rte_eth_tx_burst(cached_port, 0, &mbuf, 1);
            index ++;
            continue;
        }
        //todo: support broadcast multicast flood
        //is_broadcast_ether_addr
        uint64_t value = ethernet_fib_lookup(addr);
        if (!value) {
            //flood
        } else {
            uint8_t port_id = value & 0xff;
            if (ether_inerface[port_id].link.link_status == ETH_LINK_DOWN) {
                rte_pktmbuf_free(mbuf);
                index ++;
                continue;
            }
            cached_value = addr;
            cached_port = port_id;
            //queue id to balance
            rte_eth_tx_burst(cached_port, 0, &mbuf, 1);
        }
        index ++;
    }
    return 0;

}

REGISTER_NODE(ethernet_output_node) = {
    .node_info = {
        .function	= ethernet_output,
        .name		= "ethernet-output",
    }
};

