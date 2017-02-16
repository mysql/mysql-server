/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <config.h>

#include "ft/logger/log-internal.h"
#include "ft/txn/rollback-apply.h"

static void poll_txn_progress_function(TOKUTXN txn, uint8_t is_commit, uint8_t stall_for_checkpoint) {
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
        poll_txn_progress_function(txn, true, false);
    }
    return r;
}

int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, LSN lsn) {
    int r=0;
    rolltype_dispatch_assign(item, toku_rollback_, r, txn, lsn);
    txn->roll_info.num_rollentries_processed++;
    if (txn->roll_info.num_rollentries_processed % 1024 == 0) {
        poll_txn_progress_function(txn, false, false);
    }
    return r;
}

int note_ft_used_in_txns_parent(const FT &ft, uint32_t UU(index), TOKUTXN const child);
int note_ft_used_in_txns_parent(const FT &ft, uint32_t UU(index), TOKUTXN const child) {
    TOKUTXN parent = child->parent;
    toku_txn_maybe_note_ft(parent, ft);
    return 0;
}

static int apply_txn(TOKUTXN txn, LSN lsn, apply_rollback_item func) {
    int r = 0;
    // do the commit/abort calls and free everything
    // we do the commit/abort calls in reverse order too.
    struct roll_entry *item;
    //printf("%s:%d abort\n", __FILE__, __LINE__);

    BLOCKNUM next_log      = ROLLBACK_NONE;

    bool is_current = false;
    if (txn_has_current_rollback_log(txn)) {
        next_log      = txn->roll_info.current_rollback;
        is_current = true;
    }
    else if (txn_has_spilled_rollback_logs(txn)) {
        next_log      = txn->roll_info.spilled_rollback_tail;
    }

    uint64_t last_sequence = txn->roll_info.num_rollback_nodes;
    bool found_head = false;
    while (next_log.b != ROLLBACK_NONE.b) {
        ROLLBACK_LOG_NODE log;
        //pin log
        toku_get_and_pin_rollback_log(txn, next_log, &log);
        toku_rollback_verify_contents(log, txn->txnid, last_sequence - 1);

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
            found_head = true;
            assert(log->sequence == 0);
        }
        next_log      = log->previous;
        {
            //Clean up transaction structure to prevent
            //toku_txn_close from double-freeing
            if (is_current) {
                txn->roll_info.current_rollback      = ROLLBACK_NONE;
                is_current = false;
            }
            else {
                txn->roll_info.spilled_rollback_tail      = next_log;
            }
            if (found_head) {
                assert(next_log.b == ROLLBACK_NONE.b);
                txn->roll_info.spilled_rollback_head      = next_log;
            }
        }
        bool give_back = false;
        // each txn tries to give back at most one rollback log node
        // to the cache.
        if (next_log.b == ROLLBACK_NONE.b) {
            give_back = txn->logger->rollback_cache.give_rollback_log_node(
                txn,
                log
                );
        }
        if (!give_back) {
            toku_rollback_log_unpin_and_remove(txn, log);
        }
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
            toku_logger_save_rollback_rollinclude(txn->parent, txn->txnid, num_nodes,
                                                      txn->roll_info.spilled_rollback_head,
                                                      txn->roll_info.spilled_rollback_tail);
            //Remove ownership from child.
            txn->roll_info.spilled_rollback_head      = ROLLBACK_NONE; 
            txn->roll_info.spilled_rollback_tail      = ROLLBACK_NONE; 
        }
        // if we're commiting a child rollback, put its entries into the parent
        // by pinning both child and parent and then linking the child log entry
        // list to the end of the parent log entry list.
        if (txn_has_current_rollback_log(txn)) {
            //Pin parent log
            toku_txn_lock(txn->parent);
            ROLLBACK_LOG_NODE parent_log;
            toku_get_and_pin_rollback_log_for_new_entry(txn->parent, &parent_log);

            //Pin child log
            ROLLBACK_LOG_NODE child_log;
            toku_get_and_pin_rollback_log(txn, txn->roll_info.current_rollback, &child_log);
            toku_rollback_verify_contents(child_log, txn->txnid, txn->roll_info.num_rollback_nodes - 1);

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
            if (child_log->rollentry_arena.total_size_in_use() > 0) {
                // If there are no bytes to move, then just leave things alone, and let the memory be reclaimed on txn is closed.
                child_log->rollentry_arena.move_memory(&parent_log->rollentry_arena);
            }
            // each txn tries to give back at most one rollback log node
            // to the cache. All other rollback log nodes for this child
            // transaction are included in the parent's rollback log,
            // so this is the only node we can give back to the cache
            bool give_back = txn->logger->rollback_cache.give_rollback_log_node(
                txn,
                child_log
                );
            if (!give_back) {
                toku_rollback_log_unpin_and_remove(txn, child_log);
            }
            txn->roll_info.current_rollback = ROLLBACK_NONE;

            toku_maybe_spill_rollbacks(txn->parent, parent_log);
            toku_rollback_log_unpin(txn->parent, parent_log);
            assert(r == 0);
            toku_txn_unlock(txn->parent);
        }

        // Note the open FTs, the omts must be merged
        r = txn->open_fts.iterate<struct tokutxn, note_ft_used_in_txns_parent>(txn);
        assert(r==0);

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

int toku_rollback_discard(TOKUTXN txn) {
    txn->roll_info.current_rollback = ROLLBACK_NONE;
    return 0;
}

