#include "bi_hash.h"

void
bi_hash_init(bi_hash_t *h, uint32_t nb_buckets)
{
    h->log2_nb_buckets = max_log2(nb_buckets);
    h->nb_buckets = 1 << h->log2_nb_buckets;

    h->buckets = calloc(sizeof(bi_hash_bucket_t), h->nb_buckets);
    if (unlikely(!h->buckets)) {
        zen_panic("calloc buckets err!\n");
    }
}

static bi_hash_value_t *
split_and_rehash(bi_hash_t *h, bi_hash_value_t *old_values,
                 uint32_t new_log2_pages)
{
    bi_hash_value_t *new_values, *v, *new_v;

    new_values = alloc_aaa(h, new_log2_pages);
    v = old_values;
}

int32_t
bi_hash_add_del(bi_hash_t *h, BVT(bi_hash_kv) *add_v, int32_t is_add)
{
    uint64_t hash = BV(bi_hash_hash)(add_v);
    uint32_t bucket_index = hash & (h->nb_bucktes - 1);

    bi_hash_bucket_t *b, tmp_b;
    b = h->buckets + bucket_index;

    hash >>= h->log2_nb_buckets;
    while (__sync_lock_test_and_set(&h->writer_lock, 1)) {
        ;
    }

    int32_t rv = 0;
    bi_hash_value_t *v, *new_v, *save_new_v, *working_copy;
    if (b->off_set == 0) {
        if (is_add == 0) {
            rv = -1;
            goto unlock;
        }
        v = alloc_aaa(h, 0);
        *v->kvp = *add_v;
        tmp_b.as_u64 = 0;
        tmp_b.off_set = bi_hash_get_offset(h, v);
        b->as_u64 = tmp_b.as_u64;
        goto unlock;
    }

    make_working_copy(h, b);

    v = bi_hash_get_value(h, h->saved_bucket.offset);
    value_index = hash & ((1 << h->saved_bucket.log2_pages) - 1);
    v += value_index;

    if (is_add) {
        for (uint32_t i = 0; i < BI_HASH_KVP_PER_PAGE; i++) {
            if (!BV(bi_hash_key_cmp(v->kvp[i].key, add_v->key))) {
                memcpy(&(v->kvp[i]), add_v, sizeof(*add_v));
                __sync_synchronize();
                b->as_u64 = h->saved_bucket.as_u64;
                goto unlock;
            }
        }
        for (uint32_t i = 0; i < BI_HASH_KVP_PER_PAGE; i++) {
            if (!BV(bi_hash_is_free(v->kvp[i].key, add_v->key))) {
                memcpy(&(v->kvp[i]), add_v, sizeof(*add_v));
                __sync_synchronize();
                b->as_u64 = h->saved_bucket.as_u64;
                goto unlock;
            }
        }
    } else {
        for (uint32_t i = 0; i < BI_HASH_KVP_PER_PAGE; i++) {
            if (!BV(bi_hash_key_cmp(v->kvp[i].key, add_v->key))) {
                memset(&(v->kvp[i]), 0xff, sizeof(*add_v));
                __sync_synchronize();
                b->as_u64 = h->saved_bucket.as_u64;
                goto unlock;
            }
        }
        rv = -3;
        b->as_u64 = h->saved_bucket.as_u64;
        goto unlock;
    }
    new_log2_pages = h->saved_bucket.log2_pages + 1;

expand_again:
    working_copy = h->working_copies[cpu_number];
    new_v = BV(split_and_rehash)(h, working_copy, new_log2_pages);
    if (new_v == 0) {
        new_log2_pages++;
        goto expand_again;
    }
    save_new_v = new_v;
    new_hash = BV(bi_hash_hash)(add_v);
    new_hash >>= h->log2_nb_buckets;
    new_hash &= (1 << min_log2(vec_len(new_v))) - 1;
    new_v += new_hash;

    for (uint32_t i = 0; i < BI_HASH_KVP_PER_PAGE; i++) {
        if (!BV(bi_hash_is_free)(&(new_v->kvp[i]))) {
            memcpy(&(new_v->kvp[i]), add_v, sizeof(*add_v));
            goto expand_ok;
        }
    }
    new_log2_pages ++;
    free_aaa();
    goto expand_again;

expand_ok:
    tmp_b.log2_pages = min_log2(vec_len(save_new_v));
    tmp_b.offset = bi_hash_get_offset(h, save_new_v);
    __sync_synchronize();
    b->as_u64 = tmp_b.as_u64;
    v = bi_hash_get_value(h, h->saved_bucket.offset);
    BV(value_free) (h, v);

unlock:
    __sync_synchronize();
    h->writer_lock = 0;
    return rv;
}
