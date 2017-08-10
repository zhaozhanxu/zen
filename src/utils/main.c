#include "cmd.h"

#define BUF_LEN 2048
static bool quit = false;

void
zen_get_all_cmd(void)
{
    zen_create_connection("zen_server");
    zen_send(GET_ALL_CMD, strlen(GET_ALL_CMD) + 1, 0);

    char buf[BUF_LEN] = {0};
    ssize_t len;
    while (1) {
        len = zen_recv(buf, BUF_LEN, 0);
    }
}

void
zen_execute_line(char *string)
{
    HIST_ENTRY **list = NULL;

    printf("%s\n", string);
    if (strcmp(string, "quit") == 0 || strcmp(string, "exit") == 0) {
        quit = true;
    } else if (strcmp(string, "list") == 0) {
        list = history_list();
        if (!list) {
            return;
        }
        for (uint8_t i = 0; list[i]; i++) {
            printf("%d: %s\n", i, list[i]->line);
        }
    }
}

int32_t
main(void)
{
    zen_get_all_cmd();
    zen_init_readline();

    while (quit == false) {
        char *line = readline("zen_ctl# ");
        if (line == NULL) {
            break;
        }
        char *s = zen_strip_white(line);
        if (*s) {
            zen_del_old_cmd(s);
            add_history(s);
            zen_execute_line(s);
        }
        free(line);
    }
    return 0;
}
