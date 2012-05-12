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

int toku_brtheader_close (CACHEFILE cachefile, int fd, void *header_v, char **error_string, BOOL oplsn_valid, LSN oplsn) __attribute__((__warn_unused_result__));
int toku_brtheader_begin_checkpoint (LSN checkpoint_lsn, void *header_v) __attribute__((__warn_unused_result__));
int toku_brtheader_checkpoint (CACHEFILE cachefile, int fd, void *header_v) __attribute__((__warn_unused_result__));
int toku_brtheader_end_checkpoint (CACHEFILE cachefile, int fd, void *header_v) __attribute__((__warn_unused_result__));

int toku_create_new_brtheader(BRT t, CACHEFILE cf, TOKUTXN txn);
void toku_brtheader_free (struct brt_header *h);

int toku_brt_alloc_init_header(BRT t, TOKUTXN txn);
int toku_read_brt_header_and_store_in_cachefile (BRT brt, CACHEFILE cf, LSN max_acceptable_lsn, struct brt_header **header, BOOL* was_open);
void toku_brtheader_note_brt_open(BRT live);

int toku_brt_header_needed(struct brt_header* h);
int toku_remove_brtheader (struct brt_header* h, char **error_string, BOOL oplsn_valid, LSN oplsn)  __attribute__ ((warn_unused_result));

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

#endif
