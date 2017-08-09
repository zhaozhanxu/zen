#include "common.h"
#include "dpdk_env.h"

#define DPDK_CONF_FILE "/etc/zen/startup.conf"

int32_t
main(int32_t argc, char **argv)
{
    rte_openlog_stream(stderr);
    rte_set_log_level(RTE_LOG_DEBUG);
    char *config_file = DPDK_CONF_FILE;
    if (argc == 3) {
        if ((argv[1][0] != '-') || (argv[1][1] != 'c')) {
            printf("[usage] %s [-c /your/config/path], now yours %s %s %s\n",
                   argv[0], argv[0], argv[1], argv[2]);
            return -1;
        }
        config_file = argv[2];
    } else if (argc != 1) {
        printf("[usage] %s [-c /your/config/path]\n", argv[0]);
        return -1;
    }
    dpdk_load_global_config(config_file);
    dpdk_init(argv[0]);

    uint32_t lcore_id;
    rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
    RTE_LCORE_FOREACH_SLAVE (lcore_id) {
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            break;
        }
    }

    return 0;
}
