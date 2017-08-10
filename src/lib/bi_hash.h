#ifndef __BI_HASH_H__
#define __BI_HASH_H__

#include "common.h"

#ifndef BI_HASH_TYPE
#error BI_HASH_TYPE not defined
#endif

#define #define BV(a) a##BIHASH_TYPE
#define #define BVT(a) a##BIHASH_TYPE_t

typedef struct bi_hash_value {
    BVT(bi_hash_kv) kvp[BI_HASH_KVP_PER_PAGE];
    struct bi_hash_value *next;
} bi_hash_value_t;

typedef struct {
    union {
        struct {
            uint32_t    off_set;
            uint8_t     pad[3];
            uint8_t     log2_pages;
        };
        uint64_t		as_u64;
    };
} bi_hash_bucket_t;

typedef struct {
    bi_hash_value_t 	*value;
    bi_hash_bucket_t 	*buckets;
    volatile uint32_t 	writer_lock;
    bi_hash_value_t 	**working_copies;
    bi_hash_value_t 	**freelists;
    bi_hash_bucket_t 	saved_bucket;
    uint32_t 			nb_buckets;
    uint32_t 			log2_nb_buckets;
} bi_hash_t;

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

static inline int32_t
bi_hash_search(bi_hash_t *h, BVT(bi_hash_kv) *kvp)
{
    uint64_t hash = BV(bi_hash_hash)(kvp);
    uint32_t bucket_index = hash & (h->nb_bucktes - 1);
    bi_hash_bucket_t *b = h->buckets + bucket_index;

    if (b->offset == 0) {
        return -1;
    }

    hash >>= h->log2_nb_buckets;

    bi_hash_value_t *v = bi_hash_get_value(h, b->offset);
    uint64_t value_index = hash & ((1 << b->log2_pages) - 1);
    v += value_index;

    for (uint32_t i = 0; i < BI_HASH_KVP_PER_PAGE; i++) {
        if (!BV(bi_hash_key_cmp)(v->kvp[i].key, kvp->key)) {
            *kvp = v->kvp[i];
            return 0;
        }
    }
    return -1;
}

#endif
