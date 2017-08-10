#include "mem_pool.h"

mem_pool_t *
mem_pool_create(uint32_t obj_size, uint32_t nb_objs)
{
    //look dpdk and vpp clib_mem_init
    uint32_t header_size_shift = max_log2(sizeof(mem_pool_t));
    uint8_t obj_header_size = sizeof(obj_on_page_t);
    uint32_t obj_size_shift = max_log2(obj_size + obj_header_size);
    int32_t page_size = getpagesize();
    uint32_t page_size_shift = min_log2(page_size);
    assert(obj_size_shift > page_size_shift);

    //first page must include header
    uint32_t nb_objs_first_page = 1 <<
                                  (min_log2(page_size -
                                           (1 << header_size_shift)) -
                                           obj_size_shift);
    //other page only include obj

    uint32_t nb_objs_one_page = 1 << (page_size_shift - obj_size_shift);
    uint32_t need_pages = (nb_objs - nb_objs_first_page) /
                          nb_objs_one_page + 1;
    need_pages += ((nb_objs % nb_objs_one_page) ? 1 : 0);

    size_t size = need_pages * page_size;
    mem_pool_t *mem_pool = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
                                -1, 0);
    if (mem_pool == (void *)-1) {
      return NULL;
    }

    memset(mem_pool, 0, size);
    mem_pool->size = size;

    uint32_t header_size_aligned = 1 << header_size_shift;
    uint8_t *obj_start = (uint8_t *)mem_pool + header_size_aligned;
    uint32_t obj_size_aligned = 1 << obj_size_shift;
    mem_pool->free = (obj_on_page_t *)obj_start;
    mem_pool->free->obj = obj_start + obj_header_size;
    obj_on_page_t *last_obj = (obj_on_page_t *)obj_start;
    for (uint32_t i = 1; i < nb_objs; i ++) {
        obj_on_page_t *curr_obj = (obj_on_page_t *)obj_start +
                                  (i * obj_size_aligned);
        curr_obj->obj = (uint8_t *)curr_obj + obj_header_size;
        curr_obj->prev = last_obj;
        last_obj->next = curr_obj;
        last_obj = curr_obj;
    }

    return mem_pool;
}

void *
mem_pool_alloc(mem_pool_t *mem_pool)
{
    if (!mem_pool->free) {
        return NULL;
    }

    while (__sync_lock_test_and_set(&mem_pool->locked, 1)) {
        ;
    }

    //get from free list
    obj_on_page_t *tmp = mem_pool->free;
    mem_pool->free = mem_pool->free->next;
    mem_pool->free->prev = NULL;

    //add to used list;
    tmp->next = mem_pool->used;
    if (mem_pool->used) {
        mem_pool->used->prev = tmp;
    }
    mem_pool->used = tmp;

    __sync_lock_release(&mem_pool->locked);

    return tmp->obj;
}

void
mem_pool_free(mem_pool_t *mem_pool, void *obj)
{
    obj_on_page_t *tmp = (obj_on_page_t *)((uint8_t *)obj -
                                            sizeof(obj_on_page_t));

    while (__sync_lock_test_and_set(&mem_pool->locked, 1)) {
        ;
    }

    if (tmp == mem_pool->used) {
        mem_pool->used = mem_pool->used->next;
        mem_pool->used->prev = NULL;
    } else {
        tmp->prev->next = tmp->next;
        if (tmp->next) {
            tmp->next->prev = tmp->prev;
        }
    }

    tmp->next = mem_pool->free;
    if (mem_pool->free) {
        mem_pool->free->prev = tmp;
    }
    mem_pool->free = tmp;
    __sync_lock_release(&mem_pool->locked);
}

void
mem_pool_destroy(mem_pool_t *mem_pool)
{
    munmap(mem_pool, mem_pool->size);
}
