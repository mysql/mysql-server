/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include "includes.h"

// Calculate the fingerprint for a kvpair
static void toku_calc_more_murmur_kvpair (struct x1764 *mm, const void *key, int keylen, const void *val, int vallen) {
    int i;
    i = toku_htod32(keylen);
    x1764_add(mm,  (void*)&i, 4);
    x1764_add(mm,  key, keylen);
    i = toku_htod32(vallen);
    x1764_add(mm, (void*)&i, 4);
    x1764_add(mm, val, vallen);
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
    unsigned char type_c = (unsigned char)type;
    unsigned int a = toku_htod32(xid>>32);
    unsigned int b = toku_htod32(xid&0xffffffff);
    struct x1764 mm;
    x1764_init(&mm);
    x1764_add(&mm, &type_c, 1);
    x1764_add(&mm, &a, 4);
    x1764_add(&mm, &b, 4);
    toku_calc_more_murmur_kvpair(&mm, key, keylen, val, vallen);
    return x1764_finish(&mm);
}
