#ifndef __DPDK_ENV_H__
#define __DPDK_ENV_H__

#include "common.h"

#define ETHERNET_CLASS	0x020000
#define NIC_PATH		"/sys/bus/pci/devices"
#define DRIVER_PATH		"/sys/bus/pci/drivers"

#define NB_SOCKETS	4
#define NB_MBUF		32768
#define LOG_PATH	"/tmp/zen.log"

#define DPDK_NB_RX_DESC_DEFAULT		512
#define DPDK_NB_TX_DESC_DEFAULT		512
#define DPDK_NB_RX_DESC_10GE		2048
#define DPDK_NB_TX_DESC_10GE		2048
#define DPDK_NB_RX_DESC_40GE		(4096-128)
#define DPDK_NB_TX_DESC_40GE		2048

typedef enum {
    DPDK_NONE = 0,
    DPDK_NORMAL,
    DPDK_ETH,
} dpdk_drv_type_e;

typedef struct {
    dpdk_drv_type_e	type;
    char slot[15];
    char name[16];
    char driver[16];
    uint32_t vendor_id;
    uint32_t device_id;
} dpdk_drv_info_t;

#define foreach_dpdk_pmd			\
    _("rte_nicvf_pmd", THUNDERX)	\
    _("rte_em_pmd", E1000EM)		\
    _("rte_igb_pmd", IGB)			\
    _("rte_igbvf_pmd", IGBVF)		\
    _("rte_ixgbe_pmd", IXGBE)		\
    _("rte_ixgbevf_pmd", IXGBEVF)	\
    _("rte_i40e_pmd", I40E)			\
    _("rte_i40evf_pmd", I40EVF)		\
    _("rte_enic_pmd", ENIC)			\
    _("rte_vmxnet3_pmd", VMXNET3)	\
    _("rte_bond_pmd", BOND)			\
    _("rte_pmd_fm10k", FM10K)		\
    _("rte_cxgbe_pmd", CXGBE)

typedef enum {
    DPDK_PMD_NONE,
#define _(s, f) DPDK_PMD_##f,
    foreach_dpdk_pmd
#undef _
        DPDK_PMD_UNKNOWN,
} dpdk_pmd_e;

typedef enum {
    DPDK_PORT_TYPE_ETH_1G,
    DPDK_PORT_TYPE_ETH_10G,
    DPDK_PORT_TYPE_ETH_40G,
    DPDK_PORT_TYPE_ETH_BOND,
    DPDK_PORT_TYPE_ETH_SWITCH,
    DPDK_PORT_TYPE_UNKNOWN,
} dpdk_port_type_e;

typedef struct {
    uint8_t	nb_queues;
    dpdk_pmd_e pmd;
    dpdk_port_type_e port_type;
    struct rte_eth_link link;
    struct rte_eth_stats stats;
    uint16_t nb_rx_desc;
    uint16_t nb_tx_desc;
    struct rte_eth_conf port_conf;
    struct rte_eth_txconf tx_conf;
} dpdk_port_t;

typedef struct {
    uint8_t port_num;
    uint8_t queue_num;
} dpdk_lcore_t;

typedef struct {
    uint8_t nb_ports;
    char lcore_mask[31];
    dpdk_port_t port_info[RTE_MAX_ETHPORTS];
    dpdk_lcore_t lcore_info[RTE_MAX_LCORE];
} dpdk_main_t;

void dpdk_load_global_config(const char *file);
void dpdk_close_global_config(void);
const char *dpdk_get_global_value(const char *sect_name,
            const char *entry_name);
dpdk_main_t *dpdk_get_main(void);
void dpdk_init(const char *argv0);
int32_t main_loop(__attribute__((unused)) void *arg);

#endif
