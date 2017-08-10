#include "init.h"

init_func_list_t *func_head = NULL;

bool
init_func(void)
{
    init_func_list_t *loop = func_head;
    while (loop) {
        if (loop->f()) {
            return false;
        }
        loop = loop->next;
    }
    return true;
}
