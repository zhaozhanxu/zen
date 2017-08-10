#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include "common.h"
#include "dpdk_env.h"

#define PLUGINS_PATH "/usr/lib/zen_plugins"

typedef struct {
    void *handle;
    char name[32];
} plugin_info_t;

typedef struct {
    uint8_t nb_plugins;
    plugin_info_t plugin_info[0];
} plugins_t;

void load_plugins(void);

#endif
