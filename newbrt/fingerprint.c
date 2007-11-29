/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <arpa/inet.h>
#include <assert.h>
#include "brt-internal.h"

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

u_int32_t toku_calccrc32_kvpair (const void *key, int keylen, const void *val, int vallen) {
    return toku_calc_more_crc32_kvpair(toku_null_crc, key, keylen, val, vallen);
}

u_int32_t toku_calccrc32_cmd (int type, const void *key, int keylen, const void *val, int vallen) {
    unsigned char type_c = type;
    return toku_calc_more_crc32_kvpair(toku_crc32(toku_null_crc,
						  &type_c, 1),
				       key, keylen, val, vallen);
}

u_int32_t toku_calccrc32_cmdstruct (BRT_CMD *cmd) {
    switch (cmd->type) {
    case BRT_NONE:
    case BRT_INSERT:
    case BRT_DELETE:
	return toku_calccrc32_cmd (cmd->type, cmd->u.id.key->data, cmd->u.id.key->size, cmd->u.id.val->data, cmd->u.id.val->size);
    }
    assert(0); /* Should not have come here. */
}
