#ifndef __BI_HASH_24_8_H__
#define __BI_HASH_24_8_H__

#undef BI_HASH_TYPE

#define BI_HASH_TYPE _24_8
#define BI_HASH_KVP_PER_PAGE 4

typedef struct {
    uint64_t key[3];
    uint64_t value;
} bi_hash_kv_24_8_t;

#if __SSE4_2__
static inline uint32_t
crc_u32(uint32_t data, uint32_t value)
{
    __asm__ volatile ("crc32l %[data], %[value];":[value] "+r" (value):[data]
                "rm" (data));
    return value;
}

static inline uint64_t
bi_hash_hash_24_8(bi_hash_kv_24_8_t *v)
{
    uint32_t *dp = (uint32_t *) & v->key[0], value;
    value = crc_u32 (dp[0], value);
    value = crc_u32 (dp[1], value);
    value = crc_u32 (dp[2], value);
    value = crc_u32 (dp[3], value);
    value = crc_u32 (dp[4], value);
    value = crc_u32 (dp[5], value);
    return value;
}
#else
static inline uint64_t
bi_hash_hash_24_8(bi_hash_kv_24_8_t *v)
{
    uint64_t tmp = v->key[0] ^ v->key[1] ^ v->key[2];
    return tmp;
}
#endif

static inline int32_t
bi_hash_key_cmp_24_8(uint64_t *a, uint64_t *b)
{
    return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2]));
}

#endif
