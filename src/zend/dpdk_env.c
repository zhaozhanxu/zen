#include "dpdk_env.h"
#include "node.h"

//NOTICE: nb_lcores must be equal or bigger than nb_ports

/*********************************************************************
 * global config
 ********************************************************************/
static struct rte_cfgfile *global_cfg = NULL;

void
dpdk_load_global_config(const char *file)
{
    global_cfg = rte_cfgfile_load(file, 0);
    if (unlikely(!global_cfg)) {
        zen_panic("load file %s err!\n", file);
    } else {
        zen_log(RTE_LOG_NOTICE, "load file %s success!\n", file);
    }
}

void
dpdk_close_global_config(void)
{
    if (unlikely(!global_cfg)) {
        zen_log(RTE_LOG_WARNING, "global_cfg is NULL\n");
        return;
    }
    rte_cfgfile_close(global_cfg);
    global_cfg = NULL;
    zen_log(RTE_LOG_NOTICE, "global_cfg closed\n");
}

const char *
dpdk_get_global_value(const char *sect_name, const char *entry_name)
{
    if (unlikely(!sect_name || !entry_name)) {
        zen_log(RTE_LOG_ERR, "sect_name: %p, entry_name: %p, can't be NULL\n",
                sect_name, entry_name);
        return NULL;
    }
    if (unlikely(!rte_cfgfile_has_section(global_cfg, sect_name))) {
        zen_log(RTE_LOG_ERR, "sect_name %s isn't exist\n", sect_name);
        return NULL;
    }
    return rte_cfgfile_get_entry(global_cfg, sect_name, entry_name);
}

/*********************************************************************
 * dpdk main
 ********************************************************************/
static dpdk_main_t dpdk_main;

dpdk_main_t *
dpdk_get_main(void)
{
    return &dpdk_main;
}

/*********************************************************************
 * memory pool
 ********************************************************************/
static struct rte_mempool *pktmbuf_pool[NB_SOCKETS];

static bool
dpdk_set_pktmbuf_pool(uint32_t socket_id, struct rte_mempool *pool)
{
    if (socket_id >= NB_SOCKETS) {
        zen_log(RTE_LOG_ERR, "socket_id(%d) >= max(%d), so err!\n",
                socket_id, NB_SOCKETS);
        return false;
    }
    pktmbuf_pool[socket_id] = pool;
    return true;
}

static struct rte_mempool *
dpdk_get_pktmbuf_pool(uint32_t socket_id)
{
    if (socket_id >= NB_SOCKETS) {
        zen_log(RTE_LOG_ERR, "socket_id(%d) >= max(%d), so err!\n",
                socket_id, NB_SOCKETS);
        return NULL;
    }
    return pktmbuf_pool[socket_id];
}

static void
dpdk_init_pktmbuf_pool(void)
{
    uint32_t nb_mbuf = 0;

    const char *number = dpdk_get_global_value("dpdk", "mbuf_num");
    if (number) {
        zen_log(RTE_LOG_NOTICE, "nb_mbuf = %s\n", number);
        sscanf(number, "%d", &nb_mbuf);
    } else {
        zen_log(RTE_LOG_NOTICE, "there isn't set nb_mbuf, so set default %d\n",
                NB_MBUF);
        nb_mbuf = NB_MBUF;
    }

    char s[64];
    for (uint32_t lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
        if (!rte_lcore_is_enabled(lcore_id)) {
            continue;
        }
        uint32_t socket_id = rte_lcore_to_socket_id(lcore_id);
        if (socket_id > NB_SOCKETS) {
            zen_panic("socket_id(%d) > max(%d)\n", socket_id, NB_SOCKETS);
        }
        if (!dpdk_get_pktmbuf_pool(socket_id)) {
            sprintf(s, "mbuf_pool_%d", socket_id);
            struct rte_mempool *pool = rte_pktmbuf_pool_create(s, nb_mbuf,
                        0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, socket_id);
            if (pool == NULL) {
                zen_panic("can't init mbuf pool on socket %d\n", socket_id);
            }
            dpdk_set_pktmbuf_pool(socket_id, pool);
            zen_log(RTE_LOG_NOTICE, "allocated mbuf pool on socket %d\n",
                    socket_id);
        }
    }
}

/*********************************************************************
 * lcore
 ********************************************************************/
static void
dpdk_parse_lcore(void)
{
    const char *cmd = dpdk_get_global_value("dpdk", "cmd");
    if (unlikely(!cmd)) {
        zen_panic("dpdk cmd not set\n");
    }

    bool bingo = false;
    char tmp[256];
    strcpy(tmp, cmd);
    char *value, *cmd_tmp = tmp;
    while ((value = strsep(&cmd_tmp, " "))) {
        if (!strlen(value)) {
            continue;
        }
        if (bingo) {
            break;
        }
        if (!strcmp(value, "-c")) {
            bingo = true;
        }
    }
    if (bingo) {
        dpdk_main_t *dm = dpdk_get_main();
        while (isblank(*value)) {
            value ++;
        }
        if (value[0] == '0' && ((value[1] == 'x') || (value[1] == 'X'))) {
            value += 2;
        }
        int32_t len = strlen(value);
        while ((len > 0) && isblank(value[len - 1])) {
            len --;
        }
        memcpy(dm->lcore_mask, value, len);
        zen_log(RTE_LOG_NOTICE, "lcore mask is 0x%s\n", dm->lcore_mask);
    }
}

/*********************************************************************
 * NICS
 ********************************************************************/
static void
dpdk_check_modules(void)
{
    DIR *dfp = opendir("/sys/module");
    if (unlikely(!dfp)) {
        zen_panic("can't readdir /sys/module");
    }

    bool is_uio_exist = false, is_igb_uio_exist = false;
    struct dirent *dp;
    while ((dp = readdir(dfp))) {
        if (dp->d_name[0] == '.') {
            continue;
        }
        if (!memcmp(dp->d_name, "uio", 4)) {
            is_uio_exist = true;
        } else if (!memcmp(dp->d_name, "igb_uio", 8)) {
            is_igb_uio_exist = true;
        }
    }
    if (!is_uio_exist || !is_igb_uio_exist) {
        zen_panic("module not load, uio %d igb_uio %d\n",
                  is_uio_exist, is_igb_uio_exist);
    }
    zen_log(RTE_LOG_NOTICE, "module uio and igb_uio loaded\n");
}

static bool
is_nic_device(const char *slot)
{
    char path[128];
    sprintf(path, "%s/%s/class", NIC_PATH, slot);

    FILE *fp = fopen(path, "r");
    if (unlikely(!fp)) {
        zen_panic("open %s err!\n", path);
    }

    char data[12];
    fread(data, 12, 1, fp);
    fclose(fp);

    int32_t info;
    sscanf(data, "0x%x", &info);
    if (info == ETHERNET_CLASS) {
        zen_log(RTE_LOG_NOTICE, "%s is nic device\n", slot);
        return true;
    } else {
        zen_log(RTE_LOG_NOTICE, "%s is not nic device\n", slot);
        return false;
    }
}

static void
get_info_from_nic(const char *slot, const char *file, uint32_t *info)
{
    char path[128];
    sprintf(path, "%s/%s/%s", NIC_PATH, slot, file);

    FILE *fp = fopen(path, "r");
    if (unlikely(!fp)) {
        zen_panic("open %s err!\n", path);
    }

    char data[12];
    fread(data, 12, 1, fp);
    fclose(fp);

    sscanf(data, "0x%x", info);
}

static void
get_name_and_type_from_nic(const char *slot, dpdk_drv_type_e *type, char *name)
{
    char path[128];
    sprintf(path, "%s/%s/net", NIC_PATH, slot);

    DIR *dfp = opendir(path);
    if (!dfp) {
        *type = DPDK_ETH;
        return;
    }

    *type = DPDK_NORMAL;

    struct dirent *dp;
    while ((dp = readdir(dfp))) {
        if (dp->d_name[0] == '.') {
            continue;
        }

        strcpy(name, dp->d_name);
        break;
    }

    closedir(dfp);
}

static void
get_driver_from_nic(const char *slot, char *info)
{
    char path[128];
    sprintf(path, "%s/%s/driver/module/drivers", NIC_PATH, slot);

    DIR *dfp = opendir(path);
    if (unlikely(!dfp)) {
        zen_panic("open %s err!\n", path);
    }

    struct dirent *dp;
    while ((dp = readdir(dfp))) {
        if (memcmp(dp->d_name, "pci:", 4)) {
            continue;
        }

        strcpy(info, dp->d_name + 4);
        break;
    }

    closedir(dfp);
}

static dpdk_drv_info_t driver_info[RTE_MAX_ETHPORTS];

static void
dpdk_get_all_nics_details(void)
{
    static uint32_t port_index = 0;
    DIR *dfp = opendir(NIC_PATH);
    if (unlikely(!dfp)) {
        zen_panic("open %s error!\n", NIC_PATH);
    }

    struct dirent *dp = NULL;
    while ((dp = readdir(dfp))) {
        if ((dp->d_name[0] == '.') || !is_nic_device(dp->d_name)) {
            continue;
        }

        strcpy(driver_info[port_index].slot, dp->d_name);
        get_driver_from_nic(dp->d_name,
                            driver_info[port_index].driver);
        get_info_from_nic(dp->d_name, "vendor",
                          &driver_info[port_index].vendor_id);
        get_info_from_nic(dp->d_name, "device",
                          &driver_info[port_index].device_id);
        get_name_and_type_from_nic(dp->d_name,
                                   &driver_info[port_index].type,
                                   driver_info[port_index].name);
        port_index ++;
    }

    closedir(dfp);
}

static bool
dpdk_bind_one_nic(const char *dev_id, const char *driver)
{
    bool found = false;
    uint8_t i;
    for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
        if (driver_info[i].type == DPDK_NONE) {
            break;
        }
        if (!strcmp(driver_info[i].name, dev_id) ||
                    !strcmp(driver_info[i].slot, dev_id)) {
            if (driver_info[i].type == DPDK_NORMAL) {
                zen_log(RTE_LOG_NOTICE, "%s found and will bind\n", dev_id);
                found = true;
                break;
            } else {
                zen_log(RTE_LOG_NOTICE, "%s found but was bound\n", dev_id);
                return true;
            }
        }
    }
    if (!found) {
        return false;
    }

    char file[128];
    char data[32];
    sprintf(file, "%s/%s/unbind", DRIVER_PATH, driver_info[i].driver);
    FILE *fp = fopen(file, "a");
    if (fp == NULL) {
        zen_panic("%s: open %s error!\n", __FUNCTION__, file);
    }

    fwrite(driver_info[i].slot, strlen(driver_info[i].slot) + 1, 1, fp);
    fclose(fp);

    sprintf(file, "%s/%s/new_id", DRIVER_PATH, driver);
    fp = fopen(file, "w");
    if (fp == NULL) {
        zen_panic("%s: open %s error!\n", __FUNCTION__, file);
    }

    sprintf(data, "%04x %04x", driver_info[i].vendor_id,
            driver_info[i].device_id);
    fwrite(data, 10, 1, fp);
    fclose(fp);

    sprintf(file, "%s/%s/bind", DRIVER_PATH, driver);
    fp = fopen(file, "a");
    if (fp == NULL) {
        zen_panic("%s: open %s error!\n", __FUNCTION__, file);
    }

    fwrite(driver_info[i].slot, strlen(driver_info[i].slot) + 1, 1, fp);
    strcpy(driver_info[i].driver, driver);
    driver_info[i].type = DPDK_ETH;
    fclose(fp);
    return true;
}

static void
dpdk_bind_nics(void)
{
    dpdk_check_modules();
    dpdk_get_all_nics_details();

    dpdk_main_t *dm = dpdk_get_main();

    const char *nics = dpdk_get_global_value("dpdk", "bind_nics");
    if (unlikely(!nics)) {
        zen_panic("there is no nics to bind\n");
    }
    char tmp[256];
    strcpy(tmp, nics);
    char *nic, *nics_tmp = tmp;
    while ((nic = strsep(&nics_tmp, " "))) {
        if (!strlen(nic)) {
            continue;
        }
        if (dpdk_bind_one_nic(nic, "igb_uio")) {
            dm->nb_ports ++;
            zen_log(RTE_LOG_NOTICE, "bind nic %s successfully!\n", nic);
        }
    }
}

/*********************************************************************
 * ports
 ********************************************************************/
static struct rte_eth_conf port_conf_template = {
    .rxmode = {
        .mq_mode = ETH_MQ_RX_RSS,
        .max_rx_pkt_len = ETHER_MAX_LEN,
        .split_hdr_size = 0,
        .header_split   = 0, /**< Header Split disabled */
        .hw_ip_checksum = 1, /**< IP checksum offload enabled */
        .hw_vlan_filter = 0, /**< VLAN filtering disabled */
        .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
        .hw_strip_crc   = 0, /**< CRC stripped by hardware */
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

static void
dpdk_parse_port_info(uint8_t port_id)
{
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port_id, &dev_info);

    dpdk_main_t *dm = dpdk_get_main();
    dpdk_port_t *port_info = &dm->port_info[port_id];

    port_info->port_conf	= port_conf_template;
    port_info->tx_conf		= dev_info.default_txconf;
    port_info->nb_rx_desc	= DPDK_NB_RX_DESC_DEFAULT;
    port_info->nb_tx_desc	= DPDK_NB_TX_DESC_DEFAULT;

    if (!dev_info.driver_name) {
        dev_info.driver_name = dev_info.pci_dev->driver->driver.name;
    }

    if (port_info->pmd) {
        return;
    }

#define _(s, f) else if (!strcmp(dev_info.driver_name, s)) {    \
    port_info->pmd = DPDK_PMD_##f;                              \
}
    if (0) {
        ;
    }
    foreach_dpdk_pmd
#undef _
    else {
        port_info->pmd = DPDK_PMD_UNKNOWN;
    }

    switch (port_info->pmd) {
        case DPDK_PMD_E1000EM:
        case DPDK_PMD_IGB:
        case DPDK_PMD_IGBVF:
            port_info->port_type = DPDK_PORT_TYPE_ETH_1G;
            break;

        case DPDK_PMD_IXGBE:
        case DPDK_PMD_IXGBEVF:
        case DPDK_PMD_THUNDERX:
            port_info->port_type = DPDK_PORT_TYPE_ETH_10G;
            port_info->nb_rx_desc = DPDK_NB_RX_DESC_10GE;
            port_info->nb_tx_desc = DPDK_NB_TX_DESC_10GE;
            break;

        case DPDK_PMD_I40E:
        case DPDK_PMD_I40EVF:
        case DPDK_PMD_CXGBE:
        case DPDK_PMD_FM10K:
            port_info->port_type = DPDK_PORT_TYPE_ETH_40G;
            port_info->nb_rx_desc = DPDK_NB_RX_DESC_40GE;
            port_info->nb_tx_desc = DPDK_NB_TX_DESC_40GE;
            break;

        default:
            port_info->port_type = DPDK_PORT_TYPE_UNKNOWN;
            break;
    }
    if (ETHER_MAX_LEN > dev_info.max_rx_pktlen) {
        port_info->port_conf.rxmode.max_rx_pkt_len = dev_info.max_rx_pktlen;
    }
}

static inline uint8_t
xdigit2val(uint8_t c)
{
    if (isdigit(c)) {
        return (c - '0');
    } else if (isupper(c)) {
        return (c - 'A' + 10);
    } else {
        return (c - 'a' + 10);
    }
}

static void
dpdk_mapping_lcores_ports(uint8_t nb_ports, const char *lcore_mask)
{
    dpdk_main_t *dm = dpdk_get_main();
    uint8_t port_id = 0, queue_id = 0;

    int32_t len = strlen(lcore_mask);
    uint32_t lcore_id = 0;

    for (int32_t i = 0; i <len && lcore_id < RTE_MAX_LCORE; i++) {
        if (isxdigit(lcore_mask[i]) == 0) {
            zen_panic("lcore_mask %s not xdigit\n", lcore_mask);
        }
        uint8_t tmp = xdigit2val(lcore_mask[i]);
        for (uint8_t j = 0; j < 4 && lcore_id < RTE_MAX_LCORE;
            j++, lcore_id++) {
            if ((1 << j) & tmp) {
                dm->lcore_info[lcore_id].port_num = port_id;
                dm->lcore_info[lcore_id].queue_num = queue_id;
                dm->port_info[port_id].nb_queues = queue_id + 1;
                if (++port_id >= nb_ports) {
                    port_id = 0;
                    queue_id ++;
                }
            }
        }
    }
}

static void
dpdk_init_ports(void)
{
    dpdk_main_t *dm = dpdk_get_main();
    uint8_t nb_ports = dm->nb_ports;
    if (unlikely(nb_ports != rte_eth_dev_count())) {
        zen_panic("I count the port num is %d, but the eal recognize %d\n",
                  nb_ports, rte_eth_dev_count());
    }

    dpdk_mapping_lcores_ports(nb_ports, dm->lcore_mask);

    for (uint8_t port_id = 0; port_id < nb_ports; port_id++) {
        dpdk_parse_port_info(port_id);
    }

    for (uint8_t port_id = 0; port_id < nb_ports; port_id++) {
        dpdk_port_t *port_info = &dm->port_info[port_id];
        uint8_t nb_queues = port_info->nb_queues;

        int32_t ret = rte_eth_dev_configure(port_id, nb_queues,
                                            (uint16_t)nb_queues,
                                            &port_info->port_conf);
        if (unlikely(ret < 0)) {
            zen_panic("Can't configure device: err=%d, port=%d\n",
                      ret, port_id);
        }
        zen_log(RTE_LOG_NOTICE, "Initialized port %d queue_num %d\n",
                port_id, nb_queues);
    }

    for (uint8_t lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
        if (!rte_lcore_is_enabled(lcore_id)) {
            continue;
        }
        uint32_t socket_id = rte_lcore_to_socket_id(lcore_id);
        uint8_t port_id = dm->lcore_info[lcore_id].port_num;
        uint8_t queue_id = dm->lcore_info[lcore_id].queue_num;

        dpdk_port_t *port_info = &dm->port_info[port_id];
        int32_t ret = rte_eth_tx_queue_setup(port_id, queue_id,
                                             port_info->nb_tx_desc,
                                             socket_id,
                                             &dm->port_info[port_id].tx_conf);
        ret |= rte_eth_rx_queue_setup(port_id, queue_id,
                                      port_info->nb_rx_desc, socket_id, NULL,
                                      dpdk_get_pktmbuf_pool(socket_id));
        if (unlikely(ret < 0)) {
            zen_panic("Can't configure queue: err=%d, port=%d queue=%d\n",
                      ret, port_id, queue_id);
        }
        zen_log(RTE_LOG_NOTICE, "Initialized port %d and queue %d\n",
                port_id, queue_id);
    }

    for (uint8_t port_id = 0; port_id < nb_ports; port_id++) {
        int32_t ret = rte_eth_dev_start(port_id);
        if (unlikely(ret < 0)) {
            zen_panic("Can't start device: err=%d, port=%d\n", ret, port_id);
        }
        zen_log(RTE_LOG_NOTICE, "Start port %d\n", port_id);
    }
}

/*********************************************************************
 * common configuration
 ********************************************************************/
static void
dpdk_log_init(void)
{
    const char *log_path = dpdk_get_global_value("linux", "log");
    if (unlikely(!log_path)) {
        zen_log(RTE_LOG_ERR, "there is no log path, set default %s\n",
                LOG_PATH);
        log_path = LOG_PATH;
    }
    FILE *fp = fopen(log_path, "a+b");
    if (unlikely(!fp)) {
        zen_log(RTE_LOG_ERR, "fopen %s err, so don't set\n", log_path);
        return;
    }
    if (rte_openlog_stream(fp) != 0) {
        zen_log(RTE_LOG_ERR, "set log %s err, set default stderr\n", log_path);
        rte_openlog_stream(stderr);
    }
}

static void
dpdk_eal_init(const char *argv0)
{
    const char *cmd = dpdk_get_global_value("dpdk", "cmd");
    if (unlikely(!cmd)) {
        zen_panic("there is no cmd\n");
    }

    char tmp_argv[16][32], *argv[16];
    strcpy(tmp_argv[0], argv0);
    int32_t argc = 1;

    char tmp[256];
    strcpy(tmp, cmd);
    char *one_cmd, *cmd_tmp = tmp;
    while ((one_cmd = strsep(&cmd_tmp, " "))) {
        if (!strlen(one_cmd)) {
            continue;
        }
        strcpy(tmp_argv[argc], one_cmd);
        argc ++;
    }
    for (uint8_t i = 0; i < argc; i++) {
        argv[i] = tmp_argv[i];
    }
    if (rte_eal_init(argc, argv) < 0) {
        zen_panic("can't inital eal\n");
    }
}

void
dpdk_init(const char *argv0)
{
    dpdk_log_init();
    dpdk_bind_nics();
    dpdk_parse_lcore();
    dpdk_eal_init(argv0);
    dpdk_init_pktmbuf_pool();
    dpdk_init_ports();
}

int32_t
main_loop(__attribute__((unused)) void *arg)
{
    uint32_t lcore_id = rte_lcore_id();
    dpdk_main_t *dm = dpdk_get_main();
    uint8_t port_id = dm->lcore_info[lcore_id].port_num;
    uint8_t queue_id = dm->lcore_info[lcore_id].queue_num;

    struct rte_mbuf *mbufs[MAX_RING_NUM];
    while (1) {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, queue_id, mbufs,
                                          MAX_RING_NUM);
        zen_log(RTE_LOG_NOTICE, "rcv pkts number %d\n", nb_rx);
    }
}
