#ifndef __INIT_H__
#define __INIT_H__

#include "common.h"

typedef bool (init_func_t)(void);

typedef struct init_func_list {
    struct init_func_list *next;
    init_func_t *f;
} init_func_list_t;

extern init_func_list_t *func_head;

#define INIT_FUNCTION(x)				\
    init_func_t *_init_func_##x = x;	\
static void __add_init_func_##x(void)	\
__attribute__((__constructor__));		\
static void __add_init_func_##x (void)	\
{										\
    static init_func_list_t init_func;	\
    init_func.next = func_head;			\
    func_head = &init_func;				\
    init_func.f = _init_func_##x;		\
}

bool init_func(void);

#endif
