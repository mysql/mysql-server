/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRTHEADER_H
#define BRTHEADER_H
#ident "$Id: brt_header.h 43422 2012-05-12 17:51:02Z zardosht $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brttypes.h"
#include "ybt.h"
#include <db.h>
#include "cachetable.h"
#include "log.h"
#include "brt-search.h"
#include "compress.h"

void toku_brt_header_suppress_rollbacks(struct brt_header *h, TOKUTXN txn);
//Effect: suppresses rollback logs

void toku_brtheader_init_treelock(struct brt_header* h);
void toku_brtheader_destroy_treelock(struct brt_header* h);
void toku_brtheader_grab_treelock(struct brt_header* h);
void toku_brtheader_release_treelock(struct brt_header* h);

int toku_create_new_brtheader(BRT t, CACHEFILE cf, TOKUTXN txn);
void toku_brtheader_free (struct brt_header *h);

int toku_read_brt_header_and_store_in_cachefile (BRT brt, CACHEFILE cf, LSN max_acceptable_lsn, struct brt_header **header, BOOL* was_open);
void toku_brtheader_note_brt_open(BRT live);

int toku_brt_header_needed(struct brt_header* h);
int toku_remove_brtheader (struct brt_header* h, char **error_string, BOOL oplsn_valid, LSN oplsn)  __attribute__ ((warn_unused_result));

BRT toku_brtheader_get_some_existing_brt(struct brt_header* h);

void toku_brt_header_note_hot_begin(BRT brt);
void toku_brt_header_note_hot_complete(BRT brt, BOOL success, MSN msn_at_start_of_hot);

void 
toku_brt_header_init(
    struct brt_header *h,
    BLOCKNUM root_blocknum_on_disk, 
    LSN checkpoint_lsn, 
    TXNID root_xid_that_created, 
    uint32_t target_nodesize, 
    uint32_t target_basementnodesize, 
    enum toku_compression_method compression_method
    );

int toku_dictionary_redirect_abort(struct brt_header *old_h, struct brt_header *new_h, TOKUTXN txn) __attribute__ ((warn_unused_result));
int toku_dictionary_redirect (const char *dst_fname_in_env, BRT old_brt, TOKUTXN txn);
void toku_reset_root_xid_that_created(struct brt_header* h, TXNID new_root_xid_that_created);
// Reset the root_xid_that_created field to the given value.  
// This redefines which xid created the dictionary.


BOOL
toku_brtheader_maybe_add_txn_ref(struct brt_header* h, TOKUTXN txn);
void
toku_brtheader_remove_txn_ref(struct brt_header* h, TOKUTXN txn);

CACHEKEY* toku_calculate_root_offset_pointer (struct brt_header* h, u_int32_t *root_hash);

#endif
