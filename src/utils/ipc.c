#include "cmd.h"

static int32_t fd;

void
zen_create_connection(const char *name)
{
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("create socket error.");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path, "%s", SOCK_PATH);

    unlink(addr.sun_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(fd);
        perror("bind error.");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, name);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(fd);
        perror("connect error.");
    }
}

void
zen_delte_connection(void)
{
    close(fd);
}

ssize_t
zen_recv(void *buf, size_t len, int flags)
{
    return recv(fd, buf, len, flags);
}

ssize_t
zen_send(void *buf, size_t len, int flags)
{
    return send(fd, buf, len, flags);
}
