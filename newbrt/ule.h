/* -*- mode: C; c-basic-offset: 4 -*- */

/* Purpose of this file is to provide the world with everything necessary
 * to use the nested transaction logic and nothing else.  No internal
 * requirements of the nested transaction logic belongs here.
 */

#ifndef TOKU_ULE_H
#define TOKU_ULE_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// opaque handles used by outside world (i.e. indexer)
typedef struct ule *ULEHANDLE;	
typedef struct uxr *UXRHANDLE;

ULEHANDLE toku_ule_create(void * le_p);

void toku_ule_free(ULEHANDLE ule_p);

uint64_t ule_num_uxrs(ULEHANDLE ule);
uint32_t ule_get_num_committed(ULEHANDLE ule);
uint32_t ule_get_num_provisional(ULEHANDLE ule);
UXRHANDLE ule_get_uxr(ULEHANDLE ule, uint64_t ith);
int ule_is_committed(ULEHANDLE ule, uint64_t ith);
int ule_is_provisional(ULEHANDLE ule, uint64_t ith);
void *ule_get_key(ULEHANDLE ule);
uint32_t ule_get_keylen(ULEHANDLE ule);

BOOL uxr_is_insert(UXRHANDLE uxr);
BOOL uxr_is_delete(UXRHANDLE uxr);
BOOL uxr_is_placeholder(UXRHANDLE uxr);
void *uxr_get_val(UXRHANDLE uxr);
uint32_t uxr_get_vallen(UXRHANDLE uxr);
TXNID uxr_get_txnid(UXRHANDLE uxr);

//1 does much slower debugging
#define GARBAGE_COLLECTION_DEBUG 0

int apply_msg_to_leafentry(BRT_MSG   msg,
			   LEAFENTRY old_leafentry, // NULL if there was no stored data.
			   size_t *new_leafentry_memorysize, 
			   size_t *new_leafentry_disksize, 
			   LEAFENTRY *new_leafentry_p,
			   OMT omt, 
			   struct mempool *mp, 
			   void **maybe_free,
                           OMT snapshot_xids,
                           OMT live_list_reverse);


TXNID toku_get_youngest_live_list_txnid_for(TXNID xc, OMT live_list_reverse);

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif  // TOKU_ULE_H

