#ifndef TOKU_ROLLBACK_H
#define TOKU_ROLLBACK_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "omt.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// these routines in rollback.c

void toku_poll_txn_progress_function(TOKUTXN txn, uint8_t is_commit, uint8_t stall_for_checkpoint);
int toku_rollback_commit(TOKUTXN txn, YIELDF yield, void*yieldv, LSN lsn);
int toku_rollback_abort(TOKUTXN txn, YIELDF yield, void*yieldv, LSN lsn);
void toku_rollback_txn_close (TOKUTXN txn);
int toku_get_and_pin_rollback_log_for_new_entry (TOKUTXN txn, ROLLBACK_LOG_NODE *result);
int toku_get_and_pin_rollback_log(TOKUTXN txn, TXNID xid, uint64_t sequence, BLOCKNUM name, uint32_t hash, ROLLBACK_LOG_NODE *result);
int toku_maybe_prefetch_older_rollback_log(TOKUTXN txn, ROLLBACK_LOG_NODE log);
int toku_rollback_log_unpin(TOKUTXN txn, ROLLBACK_LOG_NODE log);
int toku_delete_rollback_log(TOKUTXN txn, ROLLBACK_LOG_NODE log);

typedef int(*apply_rollback_item)(TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv, LSN lsn);

int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv, LSN lsn);
int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv, LSN lsn);

void *toku_malloc_in_rollback(ROLLBACK_LOG_NODE log, size_t size);
void *toku_memdup_in_rollback(ROLLBACK_LOG_NODE log, const void *v, size_t len);
int toku_maybe_spill_rollbacks (TOKUTXN txn, ROLLBACK_LOG_NODE log);

int toku_txn_note_brt (TOKUTXN txn, BRT brt);
int toku_txn_note_swap_brt (BRT live, BRT zombie);
int toku_txn_note_close_brt (BRT brt);
int toku_logger_txn_rollback_raw_count(TOKUTXN txn, u_int64_t *raw_count);

int toku_txn_find_by_xid (BRT brt, TXNID xid, TOKUTXN *txnptr);

// these routines in roll.c
int toku_rollback_fileentries (int fd, TOKUTXN txn, YIELDF yield, void *yieldv, LSN lsn);
int toku_commit_fileentries (int fd, TOKUTXN txn, YIELDF yield,void *yieldv, LSN lsn);

//Heaviside function to find a TOKUTXN by TOKUTXN (used to find the index)
int find_xid (OMTVALUE v, void *txnv);

struct rollback_log_node {
    int                layout_version;
    int                layout_version_original;
    int                layout_version_read_from_disk;
    int                dirty;
    TXNID              txnid;         // Which transaction made this?
    uint64_t           sequence;      // Which rollback log in the sequence is this?
    BLOCKNUM           thislogname;   // Which block number is this chunk of the log?
    uint32_t           thishash;
    BLOCKNUM           older;         // Which block number is the next oldest chunk of the log?
    uint32_t           older_hash;
    struct roll_entry *oldest_logentry;
    struct roll_entry *newest_logentry;
    MEMARENA           rollentry_arena;
    size_t             rollentry_resident_bytecount; // How many bytes for the rollentries that are stored in main memory.
};

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif // TOKU_ROLLBACK_H
