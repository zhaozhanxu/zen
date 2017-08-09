#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mount.h>
#include <sys/mman.h>

#include <rte_common.h>
#include <rte_config.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_jhash.h>
#include <rte_cfgfile.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_arp.h>

#define zen_panic(...) rte_panic(__VA_ARGS__)
#define zen_log(level, format, ...) rte_log(level, \
                RTE_LOGTYPE_USER1, "%s: "format, \
                __FUNCTION__, ##__VA_ARGS__)

#endif
