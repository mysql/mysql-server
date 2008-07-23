/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <arpa/inet.h>
#include "brt-internal.h"
#include "toku_assert.h"

// Calculate the fingerprint for a kvpair
static inline u_int32_t toku_calc_more_crc32_kvpair (u_int32_t crc, const void *key, int keylen, const void *val, int vallen) {
    int i;
    i = htonl(keylen);
    crc = toku_crc32(crc, (void*)&i, 4);
    crc = toku_crc32(crc, key, keylen);
    i = htonl(vallen);
    crc = toku_crc32(crc, (void*)&i, 4);
    crc = toku_crc32(crc, val, vallen);
    return crc;
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

u_int32_t toku_calccrc32_cmd (u_int32_t type, TXNID xid, const void *key, u_int32_t keylen, const void *val, u_int32_t vallen) {
    unsigned char type_c = type;
    unsigned int a = htonl(xid>>32);
    unsigned int b = htonl(xid&0xffffffff);
    return toku_calc_more_crc32_kvpair(toku_crc32(toku_crc32(toku_crc32(toku_null_crc,
									&type_c, 1),
							     &a, 4),
						  &b, 4),
				       key, keylen, val, vallen);
}

u_int32_t toku_calccrc32_cmdstruct (BRT_CMD cmd) {
    switch (cmd->type) {
    case BRT_INSERT:
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
    case BRT_COMMIT_ANY:
    case BRT_COMMIT_BOTH:
    case BRT_ABORT_ANY:
    case BRT_ABORT_BOTH:
	return toku_calccrc32_cmd (cmd->type, cmd->xid, cmd->u.id.key->data, cmd->u.id.key->size, cmd->u.id.val->data, cmd->u.id.val->size);
    case BRT_NONE:
	return 0;
    }
    assert(0); /* Should not have come here. */
    return 0;
}
