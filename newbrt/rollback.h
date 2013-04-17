#ifndef TOKUROLLBACK_H
#define TOKUROLLBACK_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "omt.h"

// these routines in rollback.c

void toku_poll_txn_progress_function(TOKUTXN txn, uint8_t is_commit, uint8_t stall_for_checkpoint);
int toku_rollback_commit(TOKUTXN txn, YIELDF yield, void*yieldv, LSN lsn);
int toku_rollback_abort(TOKUTXN txn, YIELDF yield, void*yieldv, LSN lsn);
void toku_rollback_txn_close (TOKUTXN txn);

int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv, LSN lsn);
int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv, LSN lsn);

void *toku_malloc_in_rollback(TOKUTXN txn, size_t size);
void *toku_memdup_in_rollback(TOKUTXN txn, const void *v, size_t len);
char *toku_strdup_in_rollback(TOKUTXN txn, const char *s);
int toku_maybe_spill_rollbacks (TOKUTXN txn);

int toku_txn_note_brt (TOKUTXN txn, BRT brt);
int toku_txn_note_swap_brt (BRT live, BRT zombie);
int toku_txn_note_close_brt (BRT brt);
int toku_logger_txn_rolltmp_raw_count(TOKUTXN txn, u_int64_t *raw_count);

int toku_txn_find_by_xid (BRT brt, TXNID xid, TOKUTXN *txnptr);

// these routines in roll.c
int toku_rollback_fileentries (int fd, TOKUTXN txn, YIELDF yield, void *yieldv, LSN lsn);
int toku_commit_fileentries (int fd, TOKUTXN txn, YIELDF yield,void *yieldv, LSN lsn);

//Heaviside function to find a TOKUTXN by TOKUTXN (used to find the index)
int find_xid (OMTVALUE v, void *txnv);

#endif
