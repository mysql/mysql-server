/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* Purpose of this file is to provide the world with everything necessary
 * to use the nested transaction logic and nothing else.  No internal
 * requirements of the nested transaction logic belongs here.
 */

#ifndef TOKU_ULE_H
#define TOKU_ULE_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "leafentry.h"
#include "txn_manager.h"
#include <util/mempool.h>

// opaque handles used by outside world (i.e. indexer)
typedef struct ule *ULEHANDLE;	
typedef struct uxr *UXRHANDLE;

// create a ULE by copying the contents of the given leafentry
ULEHANDLE toku_ule_create(LEAFENTRY le);

void toku_ule_free(ULEHANDLE ule_p);

uint64_t ule_num_uxrs(ULEHANDLE ule);
uint32_t ule_get_num_committed(ULEHANDLE ule);
uint32_t ule_get_num_provisional(ULEHANDLE ule);
UXRHANDLE ule_get_uxr(ULEHANDLE ule, uint64_t ith);
int ule_is_committed(ULEHANDLE ule, uint64_t ith);
int ule_is_provisional(ULEHANDLE ule, uint64_t ith);
void *ule_get_key(ULEHANDLE ule);
uint32_t ule_get_keylen(ULEHANDLE ule);

bool uxr_is_insert(UXRHANDLE uxr);
bool uxr_is_delete(UXRHANDLE uxr);
bool uxr_is_placeholder(UXRHANDLE uxr);
void *uxr_get_val(UXRHANDLE uxr);
uint32_t uxr_get_vallen(UXRHANDLE uxr);
TXNID uxr_get_txnid(UXRHANDLE uxr);

//1 does much slower debugging
#define GARBAGE_COLLECTION_DEBUG 0


void fast_msg_to_leafentry(
    FT_MSG   msg, // message to apply to leafentry
    size_t *new_leafentry_memorysize, 
    size_t *new_leafentry_disksize, 
    LEAFENTRY *new_leafentry_p) ;

int apply_msg_to_leafentry(FT_MSG   msg,
                           LEAFENTRY old_leafentry, // NULL if there was no stored data.
                           size_t *new_leafentry_memorysize,
                           LEAFENTRY *new_leafentry_p,
                           OMT *omtp,
                           struct mempool *mp,
                           void **maybe_free,
                           int64_t * numbytes_delta_p);

int garbage_collect_leafentry(LEAFENTRY old_leaf_entry,
                              LEAFENTRY *new_leaf_entry,
                              size_t *new_leaf_entry_memory_size,
                              OMT *omtp,
                              struct mempool *mp,
                              void **maybe_free,
                              const xid_omt_t &snapshot_xids,
                              const rx_omt_t &referenced_xids,
                              const xid_omt_t &live_root_txns);

#endif  // TOKU_ULE_H
