#ifndef __MEM_POOL_H__
#define __MEM_POOL_H__

#include "common.h"

typedef struct obj_on_page {
    void *obj;
    struct obj_on_page *prev;
    struct obj_on_page *next;
} obj_on_page_t;

typedef struct mem_pool {
    volatile int32_t locked;
    size_t size;
    obj_on_page_t *used;
    obj_on_page_t *free;
} mem_pool_t;

static inline uint64_t
min_log2(uint64_t x)
{
    uint64_t a = x, b = 32, c = 0, r = 0;

    for (uint8_t i = 0; i < 4; i++) {
        c = a >> b;
        if (c) {
            a = c;
            r += b;
        }
        b /= 2;
    }
    const uint64_t table = 0x3333333322221104LL;
    uint64_t t = (table >> (4 * a)) & 0xf;
    r = t < 4 ? r + t : ~0;
    return r;
}

static inline uint64_t
max_log2(uint64_t x)
{
    uint64_t l = min_log2(x);
    if (x > (uint64_t)(1 << l)) {
        l ++;
    }
    return l;
}

mem_pool_t *mem_pool_create(uint32_t obj_size, uint32_t nb_objs);
void *mem_pool_alloc(mem_pool_t *mem_pool);
void mem_pool_free(mem_pool_t *mem_pool, void *obj);
void mem_pool_destroy(mem_pool_t *mem_pool);

#endif
