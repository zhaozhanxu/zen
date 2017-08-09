#include "plugins.h"

static plugins_t *plugins = NULL;

static bool
load_one_plugin(char *path, char *name)
{
    char file[256];
    sprintf(file, "%s/%s", path, name);
    void *handle = dlopen(file, RTLD_LAZY);
    if (handle == NULL) {
        zen_log(RTE_LOG_WARNING, "%s\n", dlerror());
        return false;
    }
    zen_log(RTE_LOG_NOTICE, "open %s successfully!\n", file);

    void *reg_func = dlsym(handle, "plugin_register");
    if (reg_func == NULL) {
        dlclose(handle);
        zen_log(RTE_LOG_ERR, "%s has no function plugin_register\n", file);
        return false;
    }
    zen_log(RTE_LOG_NOTICE, "plugin_register function %p\n", reg_func);

    bool (*fp)(void);
    fp = reg_func;

    if (!(*fp)()) {
        dlclose (handle);
        zen_log(RTE_LOG_ERR,"%s plugin_register call err\n", file);
        return false;
    }

    //todo: there is the same name as exist plugins?
    if (!plugins) {
        plugins = malloc(sizeof(plugins_t) + sizeof(plugin_info_t));
        if (unlikely(!plugins)) {
            zen_panic("alloc plugin main err!\n");
        }
        memset(plugins, 0, sizeof(plugins_t));
        plugins->nb_plugins = 1;
    } else {
        plugins_t *tmp_plugins = plugins;
        plugins = realloc(tmp_plugins, sizeof(plugin_info_t));
        if (unlikely(!plugins)) {
            free(tmp_plugins);
            zen_panic("realloc plugin main err!\n");
        }
        plugins->nb_plugins ++;
    }

    strcpy(plugins->plugin_info[plugins->nb_plugins - 1].name, name);
    plugins->plugin_info[plugins->nb_plugins - 1].handle = handle;
    zen_log(RTE_LOG_ERR, "%s register successfully\n", file);
    return true;
}

void
load_plugins(void)
{
    const char *plugins_path = dpdk_get_global_value("node", "plugins");
    if (unlikely(!plugins_path)) {
        zen_log(RTE_LOG_ERR, "not set plugins_path, set default\n");
        plugins_path = PLUGINS_PATH;
    }

    char tmp[256];
    strcpy(tmp, plugins_path);
    char *plugin_path, *plugins_path_tmp = tmp;
    while ((plugin_path = strsep(&plugins_path_tmp, ",")) != NULL) {
        DIR *dfp = opendir(plugin_path);
        if (dfp == NULL) {
            zen_log(RTE_LOG_ERR, "open dir %s err\n", plugin_path);
            continue;
        }
        zen_log(RTE_LOG_NOTICE, "open dir %s sucessfully\n", plugins_path);

        struct dirent *dp;
        while ((dp = readdir(dfp))) {
            struct stat status;
            if (stat(dp->d_name, &status) < 0) {
                continue;
            }

            if (!S_ISREG(status.st_mode)) {
                continue;
            }

            if (!load_one_plugin(plugin_path, dp->d_name)) {
                continue;
            }
        }
        closedir(dfp);
    }
}
