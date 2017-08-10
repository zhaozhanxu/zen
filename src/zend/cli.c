#include "cli.h"

cli_cmd_t *cli_cmd_head = NULL;

void
process_cmd(const char *cmdline, int32_t fd)
{
    if (!strcmp(cmdline, GET_ALL_CMD)) {
    } else {
    }
}

int32_t
cmd_server_init(void)
{
    unlink("server_socket");
    int32_t server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        zen_panic("create socket error.\n");
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    sprintf(server_addr.sun_path, "%s", SOCK_PATH);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1) {
        close(server_fd);
        zen_panic("bind error.\n");
    }

    if (listen(server_fd, 5) == -1) {
        close(server_fd);
        zen_panic("listen error.\n");
    }

    int32_t client_fd;
    struct sockaddr_un client_addr;
    socklen_t len = sizeof(client_addr);
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        if (client_fd < 0) {
            close(server_fd);
            zen_panic("accept err: %d\n", client_fd);
        }

#define BUFFSIZE 256
        char cmdline[BUFFSIZE];
        int32_t rcv_bytes;
        while ((rcv_bytes = recv(client_fd, cmdline, BUFFSIZE, 0))) {
            process_cmd(cmdline, client_fd);
        }
        close(client_fd);
    }
    close(server_fd);
}
