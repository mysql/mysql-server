/*
 * the key value pair contains a key and a value in a contiguous space.  the
 * key is right after the length fields and the value is right after the key.
 */
struct kv_pair {
    int keylen;
    int vallen;
    char key[];
};

static inline void kv_pair_init(struct kv_pair *pair, void *key, int keylen, void *val, int vallen) {

    pair->keylen = keylen;
    memcpy(pair->key, key, keylen);
    pair->vallen = vallen;
    memcpy(pair->key + keylen, val, vallen);
}

static inline struct kv_pair *kv_pair_malloc(void *key, int keylen, void *val, int vallen) {
    struct kv_pair *pair = toku_malloc(sizeof (struct kv_pair) + keylen + vallen);
    if (pair)
        kv_pair_init(pair, key, keylen, val, vallen);
    return pair;
}

static inline void kv_pair_free(struct kv_pair *pair) {
    toku_free_n(pair, sizeof(struct kv_pair)+pair->keylen+pair->vallen);
}

static inline void *kv_pair_key(struct kv_pair *pair) {
    return pair->key;
}

static inline int kv_pair_keylen(struct kv_pair *pair) {
    return pair->keylen;
}

static inline void *kv_pair_val(struct kv_pair *pair) {
    return pair->key + pair->keylen;
}

static inline int kv_pair_vallen(struct kv_pair *pair) {
    return pair->vallen;
}

struct kv_pair_tag {
    struct kv_pair *pair;
    int oldtag, newtag;
};
