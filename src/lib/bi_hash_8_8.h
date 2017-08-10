#ifndef __BI_HASH_8_8_H__
#define __BI_HASH_8_8_H__

#undef BI_HASH_TYPE

#define BI_HASH_TYPE _8_8
#define BI_HASH_KVP_PER_PAGE 4

typedef struct {
    uint64_t key;
    uint64_t value;
} bi_hash_kv_8_8_t;

static inline uint64_t
bi_hash_hash_8_8(bi_hash_kv_8_8_t *v)
{
    return v->key;
}

static inline int32_t
bi_hash_key_cmp_8_8(uint64_t a, uint64_t b)
{
    return a ^ b;
}

#endif
