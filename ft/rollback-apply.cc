/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "fttypes.h"
#include "log-internal.h"
#include "rollback-apply.h"

static void
poll_txn_progress_function(TOKUTXN txn, uint8_t is_commit, uint8_t stall_for_checkpoint) {
    if (txn->progress_poll_fun) {
        TOKU_TXN_PROGRESS_S progress = {
            .entries_total     = txn->roll_info.num_rollentries,
            .entries_processed = txn->roll_info.num_rollentries_processed,
            .is_commit = is_commit,
            .stalled_on_checkpoint = stall_for_checkpoint};
        txn->progress_poll_fun(&progress, txn->progress_poll_fun_extra);
    }
}

int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item, LSN lsn) {
    int r=0;
    rolltype_dispatch_assign(item, toku_commit_, r, txn, lsn);
    txn->roll_info.num_rollentries_processed++;
    if (txn->roll_info.num_rollentries_processed % 1024 == 0) {
        poll_txn_progress_function(txn, TRUE, FALSE);
    }
    return r;
}

int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, LSN lsn) {
    int r=0;
    rolltype_dispatch_assign(item, toku_rollback_, r, txn, lsn);
    txn->roll_info.num_rollentries_processed++;
    if (txn->roll_info.num_rollentries_processed % 1024 == 0) {
        poll_txn_progress_function(txn, FALSE, FALSE);
    }
    return r;
}

static int
note_ft_used_in_txns_parent(OMTVALUE ftv, u_int32_t UU(index), void *txnv) {
    TOKUTXN CAST_FROM_VOIDP(child, txnv);
    TOKUTXN parent = child->parent;
    FT CAST_FROM_VOIDP(ft, ftv);
    toku_txn_maybe_note_ft(parent, ft);
    if (ft->txnid_that_created_or_locked_when_empty == toku_txn_get_txnid(child)) {
        //Pass magic "no rollback needed" flag to parent.
        ft->txnid_that_created_or_locked_when_empty = toku_txn_get_txnid(parent);
    }
    if (ft->txnid_that_suppressed_recovery_logs == toku_txn_get_txnid(child)) {
        //Pass magic "no recovery needed" flag to parent.
        ft->txnid_that_suppressed_recovery_logs = toku_txn_get_txnid(parent);
    }
    return 0;
}

static int
apply_txn(TOKUTXN txn, LSN lsn, apply_rollback_item func) {
    int r = 0;
    // do the commit/abort calls and free everything
    // we do the commit/abort calls in reverse order too.
    struct roll_entry *item;
    //printf("%s:%d abort\n", __FILE__, __LINE__);

    BLOCKNUM next_log      = ROLLBACK_NONE;
    uint32_t next_log_hash = 0;

    BOOL is_current = FALSE;
    if (txn_has_current_rollback_log(txn)) {
        next_log      = txn->roll_info.current_rollback;
        next_log_hash = txn->roll_info.current_rollback_hash;
        is_current = TRUE;
    }
    else if (txn_has_spilled_rollback_logs(txn)) {
        next_log      = txn->roll_info.spilled_rollback_tail;
        next_log_hash = txn->roll_info.spilled_rollback_tail_hash;
    }

    uint64_t last_sequence = txn->roll_info.num_rollback_nodes;
    BOOL found_head = FALSE;
    while (next_log.b != ROLLBACK_NONE.b) {
        ROLLBACK_LOG_NODE log;
        //pin log
        toku_get_and_pin_rollback_log(txn, next_log, next_log_hash, &log);
        toku_rollback_verify_contents(log, txn->txnid64, last_sequence - 1);

        toku_maybe_prefetch_previous_rollback_log(txn, log);

        last_sequence = log->sequence;
        if (func) {
            while ((item=log->newest_logentry)) {
                log->newest_logentry = item->prev;
                r = func(txn, item, lsn);
                if (r!=0) return r;
            }
        }
        if (next_log.b == txn->roll_info.spilled_rollback_head.b) {
            assert(!found_head);
            found_head = TRUE;
            assert(log->sequence == 0);
        }
        next_log      = log->previous;
        next_log_hash = log->previous_hash;
        {
            //Clean up transaction structure to prevent
            //toku_txn_close from double-freeing
            if (is_current) {
                txn->roll_info.current_rollback      = ROLLBACK_NONE;
                txn->roll_info.current_rollback_hash = 0;
                is_current = FALSE;
            }
            else {
                txn->roll_info.spilled_rollback_tail      = next_log;
                txn->roll_info.spilled_rollback_tail_hash = next_log_hash;
            }
            if (found_head) {
                assert(next_log.b == ROLLBACK_NONE.b);
                txn->roll_info.spilled_rollback_head      = next_log;
                txn->roll_info.spilled_rollback_head_hash = next_log_hash;
            }
        }
        toku_rollback_log_unpin_and_remove(txn, log);
    }
    return r;
}

//Commit each entry in the rollback log.
//If the transaction has a parent, it just promotes its information to its parent.
int toku_rollback_commit(TOKUTXN txn, LSN lsn) {
    int r=0;
    if (txn->parent!=0) {
        // First we must put a rollinclude entry into the parent if we spilled

        if (txn_has_spilled_rollback_logs(txn)) {
            uint64_t num_nodes = txn->roll_info.num_rollback_nodes;
            if (txn_has_current_rollback_log(txn)) {
                num_nodes--; //Don't count the in-progress rollback log.
            }
            r = toku_logger_save_rollback_rollinclude(txn->parent, txn->txnid64, num_nodes,
                                                      txn->roll_info.spilled_rollback_head, txn->roll_info.spilled_rollback_head_hash,
                                                      txn->roll_info.spilled_rollback_tail, txn->roll_info.spilled_rollback_tail_hash);
            if (r!=0) return r;
            //Remove ownership from child.
            txn->roll_info.spilled_rollback_head      = ROLLBACK_NONE; 
            txn->roll_info.spilled_rollback_head_hash = 0; 
            txn->roll_info.spilled_rollback_tail      = ROLLBACK_NONE; 
            txn->roll_info.spilled_rollback_tail_hash = 0; 
        }
        // if we're commiting a child rollback, put its entries into the parent
        // by pinning both child and parent and then linking the child log entry
        // list to the end of the parent log entry list.
        if (txn_has_current_rollback_log(txn)) {
            //Pin parent log
            ROLLBACK_LOG_NODE parent_log;
            toku_get_and_pin_rollback_log_for_new_entry(txn->parent, &parent_log);

            //Pin child log
            ROLLBACK_LOG_NODE child_log;
            toku_get_and_pin_rollback_log(txn, txn->roll_info.current_rollback, 
                    txn->roll_info.current_rollback_hash, &child_log);
            toku_rollback_verify_contents(child_log, txn->txnid64, txn->roll_info.num_rollback_nodes - 1);

            // Append the list to the front of the parent.
            if (child_log->oldest_logentry) {
                // There are some entries, so link them in.
                child_log->oldest_logentry->prev = parent_log->newest_logentry;
                if (!parent_log->oldest_logentry) {
                    parent_log->oldest_logentry = child_log->oldest_logentry;
                }
                parent_log->newest_logentry = child_log->newest_logentry;
                parent_log->rollentry_resident_bytecount += child_log->rollentry_resident_bytecount;
                txn->parent->roll_info.rollentry_raw_count         += txn->roll_info.rollentry_raw_count;
                child_log->rollentry_resident_bytecount = 0;
            }
            if (parent_log->oldest_logentry==NULL) {
                parent_log->oldest_logentry = child_log->oldest_logentry;
            }
            child_log->newest_logentry = child_log->oldest_logentry = 0;
            // Put all the memarena data into the parent.
            if (memarena_total_size_in_use(child_log->rollentry_arena) > 0) {
                // If there are no bytes to move, then just leave things alone, and let the memory be reclaimed on txn is closed.
                memarena_move_buffers(parent_log->rollentry_arena, child_log->rollentry_arena);
            }
            toku_rollback_log_unpin_and_remove(txn, child_log);
            txn->roll_info.current_rollback = ROLLBACK_NONE;
            txn->roll_info.current_rollback_hash = 0;

            toku_maybe_spill_rollbacks(txn->parent, parent_log);
            toku_rollback_log_unpin(txn->parent, parent_log);
            assert(r == 0);
        }

        // Note the open brts, the omts must be merged
        r = toku_omt_iterate(txn->open_fts, note_ft_used_in_txns_parent, txn);
        assert(r==0);

        // Merge the list of headers that must be checkpointed before commit
        if (txn->checkpoint_needed_before_commit) {
            txn->parent->checkpoint_needed_before_commit = TRUE;
        }

        //If this transaction needs an fsync (if it commits)
        //save that in the parent.  Since the commit really happens in the root txn.
        txn->parent->force_fsync_on_commit |= txn->force_fsync_on_commit;
        txn->parent->roll_info.num_rollentries       += txn->roll_info.num_rollentries;
    } else {
        r = apply_txn(txn, lsn, toku_commit_rollback_item);
        assert(r==0);
    }

    return r;
}

int toku_rollback_abort(TOKUTXN txn, LSN lsn) {
    int r;
    r = apply_txn(txn, lsn, toku_abort_rollback_item);
    assert(r==0);
    return r;
}
