#ifndef __CMD_H__
#define __CMD_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <readline/readline.h>
#include <readline/history.h>

void zen_create_connection(const char *name);
void zen_delte_connection(void);
ssize_t zen_recv(void *buf, size_t len, int flags);
ssize_t zen_send(void *buf, size_t len, int flags);

void zen_init_readline();
char *zen_strip_white(char *string);
void zen_del_old_cmd(const char *line);

#endif
