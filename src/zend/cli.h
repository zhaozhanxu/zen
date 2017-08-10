#ifndef __CLI_H__
#define __CLI_H__

#include "common.h"

#define SERVER_NAME		"zen_server"
#define SOCK_PATH		"/var/run/zen.sock"
#define GET_ALL_CMD		"get_all_cmd"

typedef bool (cli_cmd_func_t) (void);

typedef struct cli_cmd {
    char *path;
    char *help;
    cli_cmd_func_t *f;
    uint64_t arg;
    struct cli_cmd *next;
} cli_cmd_t;

extern cli_cmd_t *cli_cmd_head;

#define CLI_COMMAND(x)					\
    static cli_cmd_t x;					\
static void __cli_cmd_regist_##x(void)	\
__attribute__((__constructor__));		\
static void __cli_cmd_regist_##x(void)	\
{										\
    x.next = cli_cmd_head;				\
    cli_cmd_head = &x;					\
}										\
static cli_cmd_t x

#endif
