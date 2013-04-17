/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef BACKWARD_10_H
#define BACKWARD_10_H

int le10_committed (u_int32_t klen, void* kval, u_int32_t dlen, void* dval, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result);
int le10_both (TXNID xid, u_int32_t cklen, void* ckval, u_int32_t cdlen, void* cdval, u_int32_t pdlen, void* pdval,
	     u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result);
int le10_provdel (TXNID xid, u_int32_t klen, void* kval, u_int32_t dlen, void* dval,
		u_int32_t *resultsize, u_int32_t *memsize, LEAFENTRY *result);
int le10_provpair (TXNID xid, u_int32_t klen, void* kval, u_int32_t plen, void* pval, u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result);

enum le_state { LE_COMMITTED=1, // A committed pair.
		LE_BOTH,        // A committed pair and a provisional pair.
		LE_PROVDEL,     // A committed pair that has been provisionally deleted
		LE_PROVPAIR };  // No committed value, but a provisional pair.

static inline enum le_state get_le_state(LEAFENTRY le) {
    return (enum le_state)*(unsigned char *)le;
}
#include "ule.h"
//Exposed ule functions for the purpose of upgrading
void toku_upgrade_ule_init_empty_ule(ULE ule, u_int32_t keylen, void * keyp);
void toku_upgrade_ule_remove_innermost_uxr(ULE ule);
void toku_upgrade_ule_push_insert_uxr(ULE ule, TXNID xid, u_int32_t vallen, void * valp);
void toku_upgrade_ule_push_delete_uxr(ULE ule, TXNID xid);
//Exposed brt functions for the purpose of upgrading
void toku_calculate_leaf_stats(BRTNODE node);

#endif
