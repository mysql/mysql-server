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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#include <toku_stdint.h>

#include "ft.h"
#include "log-internal.h"
#include "rollback-ct-callbacks.h"

static void rollback_unpin_remove_callback(CACHEKEY* cachekey, bool for_checkpoint, void* extra) {
    FT CAST_FROM_VOIDP(h, extra);
    toku_free_blocknum(
        h->blocktable,
        cachekey,
        h,
        for_checkpoint
        );
}

void toku_rollback_log_unpin_and_remove(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    int r;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    FT CAST_FROM_VOIDP(h, toku_cachefile_get_userdata(cf));
    r = toku_cachetable_unpin_and_remove (cf, log->ct_pair, rollback_unpin_remove_callback, h);
    assert(r == 0);
}

int
toku_find_xid_by_xid (const TXNID &xid, const TXNID &xidfind) {
    if (xid<xidfind) return -1;
    if (xid>xidfind) return +1;
    return 0;
}

void *toku_malloc_in_rollback(ROLLBACK_LOG_NODE log, size_t size) {
    return malloc_in_memarena(log->rollentry_arena, size);
}

void *toku_memdup_in_rollback(ROLLBACK_LOG_NODE log, const void *v, size_t len) {
    void *r=toku_malloc_in_rollback(log, len);
    memcpy(r,v,len);
    return r;
}

static inline PAIR_ATTR make_rollback_pair_attr(long size) { 
    PAIR_ATTR result={
     .size = size, 
     .nonleaf_size = 0,
     .leaf_size = 0,
     .rollback_size = size,
     .cache_pressure_size = 0,
     .is_valid = true
    };
    return result;
}

PAIR_ATTR
rollback_memory_size(ROLLBACK_LOG_NODE log) {
    size_t size = sizeof(*log);
    if (log->rollentry_arena) {
        size += memarena_total_memory_size(log->rollentry_arena);
    }
    return make_rollback_pair_attr(size);
}

static void toku_rollback_node_save_ct_pair(CACHEKEY UU(key), void *value_data, PAIR p) {
    ROLLBACK_LOG_NODE CAST_FROM_VOIDP(log, value_data);
    log->ct_pair = p;
}

//
// initializes an empty rollback log node
// Does not touch the blocknum or hash, that is the
// responsibility of the caller
//
void rollback_empty_log_init(ROLLBACK_LOG_NODE log) {
    // Having a txnid set to TXNID_NONE is how we determine if the 
    // rollback log node is empty or in use.
    log->txnid.parent_id64 = TXNID_NONE;
    log->txnid.child_id64 = TXNID_NONE;
    
    log->layout_version                = FT_LAYOUT_VERSION;
    log->layout_version_original       = FT_LAYOUT_VERSION;
    log->layout_version_read_from_disk = FT_LAYOUT_VERSION;
    log->dirty = true;
    log->sequence = 0;
    log->previous = make_blocknum(0);
    log->previous_hash = 0;
    log->oldest_logentry = NULL;
    log->newest_logentry = NULL;
    log->rollentry_arena = NULL;
    log->rollentry_resident_bytecount = 0;
}



static void rollback_initialize_for_txn(
    ROLLBACK_LOG_NODE log,
    TOKUTXN txn,
    BLOCKNUM previous,
    uint32_t previous_hash
    )
{
    log->txnid = txn->txnid;
    log->sequence = txn->roll_info.num_rollback_nodes++;
    log->previous = previous;
    log->previous_hash = previous_hash;
    log->oldest_logentry = NULL;
    log->newest_logentry = NULL;
    log->rollentry_arena = memarena_create();
    log->rollentry_resident_bytecount = 0;
    log->dirty = true;
}

void make_rollback_log_empty(ROLLBACK_LOG_NODE log) {
    memarena_close(&log->rollentry_arena);
    rollback_empty_log_init(log);
}

// create and pin a new rollback log node. chain it to the other rollback nodes
// by providing a previous blocknum/ hash and assigning the new rollback log
// node the next sequence number
static void rollback_log_create (
    TOKUTXN txn,
    BLOCKNUM previous,
    uint32_t previous_hash,
    ROLLBACK_LOG_NODE *result
    )
{
    ROLLBACK_LOG_NODE XMALLOC(log);
    rollback_empty_log_init(log);

    CACHEFILE cf = txn->logger->rollback_cachefile;
    FT CAST_FROM_VOIDP(ft, toku_cachefile_get_userdata(cf));
    rollback_initialize_for_txn(log, txn, previous, previous_hash);
    toku_allocate_blocknum(ft->blocktable, &log->blocknum, ft);
    log->hash = toku_cachetable_hash(ft->cf, log->blocknum);
    *result = log;
    toku_cachetable_put(cf, log->blocknum, log->hash,
                       log, rollback_memory_size(log),
                       get_write_callbacks_for_rollback_log(ft),
                       toku_rollback_node_save_ct_pair);
    txn->roll_info.current_rollback = log->blocknum;
    txn->roll_info.current_rollback_hash = log->hash;
}

void toku_rollback_log_unpin(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    int r;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    r = toku_cachetable_unpin(
        cf, 
        log->ct_pair,
        (enum cachetable_dirty)log->dirty, 
        rollback_memory_size(log)
        );
    assert(r == 0);
}

//Requires: log is pinned
//          log is current
//After:
//  Maybe there is no current after (if it spilled)
void toku_maybe_spill_rollbacks(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    if (log->rollentry_resident_bytecount > txn->logger->write_block_size) {
        assert(log->blocknum.b == txn->roll_info.current_rollback.b);
        //spill
        if (!txn_has_spilled_rollback_logs(txn)) {
            //First spilled.  Copy to head.
            txn->roll_info.spilled_rollback_head      = txn->roll_info.current_rollback;
            txn->roll_info.spilled_rollback_head_hash = txn->roll_info.current_rollback_hash;
        }
        //Unconditionally copy to tail.  Old tail does not need to be cached anymore.
        txn->roll_info.spilled_rollback_tail      = txn->roll_info.current_rollback;
        txn->roll_info.spilled_rollback_tail_hash = txn->roll_info.current_rollback_hash;

        txn->roll_info.current_rollback      = ROLLBACK_NONE;
        txn->roll_info.current_rollback_hash = 0;
    }
}

int find_filenum (const FT &h, const FT &hfind);
int find_filenum (const FT &h, const FT &hfind) {
    FILENUM fnum     = toku_cachefile_filenum(h->cf);
    FILENUM fnumfind = toku_cachefile_filenum(hfind->cf);
    if (fnum.fileid<fnumfind.fileid) return -1;
    if (fnum.fileid>fnumfind.fileid) return +1;
    return 0;
}

//Notify a transaction that it has touched a brt.
void toku_txn_maybe_note_ft (TOKUTXN txn, FT ft) {
    toku_txn_lock(txn);
    FT ftv;
    uint32_t idx;
    int r = txn->open_fts.find_zero<FT, find_filenum>(ft, &ftv, &idx);
    if (r == 0) {
        // already there
        assert(ftv == ft);
        goto exit;
    }
    r = txn->open_fts.insert_at(ft, idx);
    assert_zero(r);
    // TODO(leif): if there's anything that locks the reflock and then
    // the txn lock, this may deadlock, because it grabs the reflock.
    toku_ft_add_txn_ref(ft);
exit:
    toku_txn_unlock(txn);
}

// Return the number of bytes that went into the rollback data structure (the uncompressed count if there is compression)
int toku_logger_txn_rollback_stats(TOKUTXN txn, struct txn_stat *txn_stat)
{
    toku_txn_lock(txn);
    txn_stat->rollback_raw_count = txn->roll_info.rollentry_raw_count;
    txn_stat->rollback_num_entries = txn->roll_info.num_rollentries;
    toku_txn_unlock(txn);
    return 0;
}

void toku_maybe_prefetch_previous_rollback_log(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    //Currently processing 'log'.  Prefetch the next (previous) log node.

    BLOCKNUM name = log->previous;
    int r = 0;
    if (name.b != ROLLBACK_NONE.b) {
        uint32_t hash = log->previous_hash;
        CACHEFILE cf = txn->logger->rollback_cachefile;
        FT CAST_FROM_VOIDP(h, toku_cachefile_get_userdata(cf));
        bool doing_prefetch = false;
        r = toku_cachefile_prefetch(cf, name, hash,
                                    get_write_callbacks_for_rollback_log(h),
                                    toku_rollback_fetch_callback,
                                    toku_rollback_pf_req_callback,
                                    toku_rollback_pf_callback,
                                    h,
                                    &doing_prefetch);
        assert(r == 0);
    }
}

void toku_rollback_verify_contents(ROLLBACK_LOG_NODE log, 
        TXNID_PAIR txnid, uint64_t sequence)
{
    assert(log->txnid.parent_id64 == txnid.parent_id64);
    assert(log->txnid.child_id64 == txnid.child_id64);
    assert(log->sequence == sequence);
}

void toku_get_and_pin_rollback_log(TOKUTXN txn, BLOCKNUM blocknum, uint32_t hash, ROLLBACK_LOG_NODE *log) {
    void * value;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    FT CAST_FROM_VOIDP(h, toku_cachefile_get_userdata(cf));
    int r = toku_cachetable_get_and_pin_with_dep_pairs(cf, blocknum, hash,
                                        &value, NULL,
                                        get_write_callbacks_for_rollback_log(h),
                                        toku_rollback_fetch_callback,
                                        toku_rollback_pf_req_callback,
                                        toku_rollback_pf_callback,
                                        PL_WRITE_CHEAP, // lock_type
                                        h,
                                        0, NULL, NULL
                                        );
    assert(r == 0);
    ROLLBACK_LOG_NODE CAST_FROM_VOIDP(pinned_log, value);
    assert(pinned_log->blocknum.b == blocknum.b);
    assert(pinned_log->hash == hash);
    *log = pinned_log;
}

void toku_get_and_pin_rollback_log_for_new_entry (TOKUTXN txn, ROLLBACK_LOG_NODE *log) {
    ROLLBACK_LOG_NODE pinned_log = NULL;
    invariant(txn->state == TOKUTXN_LIVE || txn->state == TOKUTXN_PREPARING); // hot indexing may call this function for prepared transactions
    if (txn_has_current_rollback_log(txn)) {
        toku_get_and_pin_rollback_log(txn, txn->roll_info.current_rollback, txn->roll_info.current_rollback_hash, &pinned_log);
        toku_rollback_verify_contents(pinned_log, txn->txnid, txn->roll_info.num_rollback_nodes - 1);
    } else {
        // For each transaction, we try to acquire the first rollback log
        // from the rollback log node cache, so that we avoid
        // putting something new into the cachetable. However,
        // if transaction has spilled rollbacks, that means we
        // have already done a lot of work for this transaction,
        // and subsequent rollback log nodes are created
        // and put into the cachetable. The idea is for
        // transactions that don't do a lot of work to (hopefully)
        // get a rollback log node from a cache, as opposed to
        // taking the more expensive route of creating a new one.
        if (!txn_has_spilled_rollback_logs(txn)) {
            txn->logger->rollback_cache.get_rollback_log_node(txn, &pinned_log);
            if (pinned_log != NULL) {
                rollback_initialize_for_txn(
                    pinned_log,
                    txn,
                    txn->roll_info.spilled_rollback_tail,
                    txn->roll_info.spilled_rollback_tail_hash
                    );
                txn->roll_info.current_rollback = pinned_log->blocknum;
                txn->roll_info.current_rollback_hash = pinned_log->hash;
            }
        }
        if (pinned_log == NULL) {
            rollback_log_create(txn, txn->roll_info.spilled_rollback_tail, txn->roll_info.spilled_rollback_tail_hash, &pinned_log);
        }
    }
    assert(pinned_log->txnid.parent_id64 == txn->txnid.parent_id64);
    assert(pinned_log->txnid.child_id64 == txn->txnid.child_id64);
    assert(pinned_log->blocknum.b != ROLLBACK_NONE.b);
    *log = pinned_log;
}
