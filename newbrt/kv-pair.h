#ifndef KV_PAIR_H
#define KV_PAIR_H

#include "memory.h"
#include <string.h>

/*
 * the key value pair contains a key and a value in a contiguous space.  the
 * key is right after the length fields and the value is right after the key.
 */
struct kv_pair {
    unsigned int keylen;
    unsigned int vallen;
    char key[];
};

/* return the size of a kv pair */
static inline int kv_pair_size(struct kv_pair *pair) {
    return sizeof (struct kv_pair) + pair->keylen + pair->vallen;
}

static inline void kv_pair_init(struct kv_pair *pair, const void *key, unsigned int keylen, const void *val, unsigned int vallen) {
    pair->keylen = keylen;
    memcpy(pair->key, key, keylen);
    pair->vallen = vallen;
    memcpy(pair->key + keylen, val, vallen);
}

static inline struct kv_pair *kv_pair_malloc(const void *key, unsigned int keylen, const void *val, unsigned int vallen) {
    struct kv_pair *pair = toku_malloc(sizeof (struct kv_pair) + keylen + vallen);
    if (pair)
        kv_pair_init(pair, key, keylen, val, vallen);
    return pair;
}

/* replace the val, keep the same key */
static inline struct kv_pair *kv_pair_realloc_same_key(struct kv_pair *p, void *newval, unsigned int newvallen) {
    struct kv_pair *pair = toku_realloc(p, sizeof (struct kv_pair) + p->keylen + newvallen);
    if (pair) {
	pair->vallen = newvallen;
	memcpy(pair->key + pair->keylen, newval, newvallen);
    }
    return pair;
}

static inline void kv_pair_free(struct kv_pair *pair) {
    toku_free_n(pair, sizeof(struct kv_pair)+pair->keylen+pair->vallen);
}

static inline void *kv_pair_key(struct kv_pair *pair) {
    return pair->key;
}

static inline const void *kv_pair_key_const(const struct kv_pair *pair) {
    return pair->key;
}

static inline unsigned int kv_pair_keylen(struct kv_pair *pair) {
    return pair->keylen;
}

static inline void *kv_pair_val(struct kv_pair *pair) {
    return pair->key + pair->keylen;
}

static inline const void *kv_pair_val_const(const struct kv_pair *pair) {
    return pair->key + pair->keylen;
}

static inline unsigned int kv_pair_vallen(struct kv_pair *pair) {
    return pair->vallen;
}

#endif
