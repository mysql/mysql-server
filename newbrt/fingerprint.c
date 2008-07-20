/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <arpa/inet.h>
#include "brt-internal.h"
#include "toku_assert.h"
#include "murmur.h"

// Calculate the fingerprint for a kvpair
static void toku_calc_more_murmur_kvpair (struct murmur *mm, const void *key, int keylen, const void *val, int vallen) {
    int i;
    i = htonl(keylen);
    murmur_add(mm,  (void*)&i, 4);
    murmur_add(mm,  key, keylen);
    i = htonl(vallen);
    murmur_add(mm, (void*)&i, 4);
    murmur_add(mm, val, vallen);
}

#if 0
 u_int32_t toku_calccrc32_kvpair (const void *key, int keylen, const void *val, int vallen) {
    return toku_calc_more_crc32_kvpair(toku_null_crc, key, keylen, val, vallen);
}

u_int32_t toku_calccrc32_kvpair_struct (const struct kv_pair *kvp) {
    return toku_calccrc32_kvpair(kv_pair_key_const(kvp), kv_pair_keylen(kvp),
				 kv_pair_val_const(kvp), kv_pair_vallen(kvp));
}
#endif

u_int32_t toku_calc_fingerprint_cmd (u_int32_t type, TXNID xid, const void *key, u_int32_t keylen, const void *val, u_int32_t vallen) {
    unsigned char type_c = type;
    unsigned int a = htonl(xid>>32);
    unsigned int b = htonl(xid&0xffffffff);
    struct murmur mm;
    murmur_init(&mm);
    murmur_add(&mm, &type_c, 1);
    murmur_add(&mm, &a, 4);
    murmur_add(&mm, &b, 4);
    toku_calc_more_murmur_kvpair(&mm, key, keylen, val, vallen);
    return murmur_finish(&mm);
}

u_int32_t toku_calc_fingerprint_cmdstruct (BRT_CMD cmd) {
    switch (cmd->type) {
    case BRT_INSERT:
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
    case BRT_COMMIT_ANY:
    case BRT_COMMIT_BOTH:
    case BRT_ABORT_ANY:
    case BRT_ABORT_BOTH:
	return toku_calc_fingerprint_cmd (cmd->type, cmd->xid, cmd->u.id.key->data, cmd->u.id.key->size, cmd->u.id.val->data, cmd->u.id.val->size);
    case BRT_NONE:
	return 0;
    }
    assert(0); /* Should not have come here. */
    return 0;
}
