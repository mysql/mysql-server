/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
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
    r = toku_cachetable_unpin_and_remove (cf, log->blocknum, rollback_unpin_remove_callback, h);
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
    size += memarena_total_memory_size(log->rollentry_arena);
    return make_rollback_pair_attr(size);
}

// create and pin a new rollback log node. chain it to the other rollback nodes
// by providing a previous blocknum/ hash and assigning the new rollback log
// node the next sequence number
static void rollback_log_create (TOKUTXN txn, BLOCKNUM previous, uint32_t previous_hash, ROLLBACK_LOG_NODE *result) {
    ROLLBACK_LOG_NODE MALLOC(log);
    assert(log);

    int r;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    FT CAST_FROM_VOIDP(h, toku_cachefile_get_userdata(cf));

    log->layout_version                = FT_LAYOUT_VERSION;
    log->layout_version_original       = FT_LAYOUT_VERSION;
    log->layout_version_read_from_disk = FT_LAYOUT_VERSION;
    log->dirty = true;
    log->txnid = txn->txnid64;
    log->sequence = txn->roll_info.num_rollback_nodes++;
    toku_allocate_blocknum(h->blocktable, &log->blocknum, h);
    log->hash   = toku_cachetable_hash(cf, log->blocknum);
    log->previous      = previous;
    log->previous_hash = previous_hash;
    log->oldest_logentry = NULL;
    log->newest_logentry = NULL;
    log->rollentry_arena = memarena_create();
    log->rollentry_resident_bytecount = 0;

    *result = log;
    r = toku_cachetable_put(cf, log->blocknum, log->hash,
                          log, rollback_memory_size(log),
                          get_write_callbacks_for_rollback_log(h));
    assert(r == 0);
    txn->roll_info.current_rollback      = log->blocknum;
    txn->roll_info.current_rollback_hash = log->hash;
}

void toku_rollback_log_unpin(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    int r;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    r = toku_cachetable_unpin(cf, log->blocknum, log->hash,
                              (enum cachetable_dirty)log->dirty, rollback_memory_size(log));
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

static int find_filenum (const FT &h, const FT &hfind) {
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
int toku_logger_txn_rollback_raw_count(TOKUTXN txn, uint64_t *raw_count)
{
    toku_txn_lock(txn);
    *raw_count = txn->roll_info.rollentry_raw_count;
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
        TXNID txnid, uint64_t sequence)
{
    assert(log->txnid == txnid);
    assert(log->sequence == sequence);
}

void toku_get_and_pin_rollback_log(TOKUTXN txn, BLOCKNUM blocknum, uint32_t hash, ROLLBACK_LOG_NODE *log) {
    void * value;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    FT CAST_FROM_VOIDP(h, toku_cachefile_get_userdata(cf));
    int r = toku_cachetable_get_and_pin(cf, blocknum, hash,
                                        &value, NULL,
                                        get_write_callbacks_for_rollback_log(h),
                                        toku_rollback_fetch_callback,
                                        toku_rollback_pf_req_callback,
                                        toku_rollback_pf_callback,
                                        true, // may_modify_value
                                        h
                                        );
    assert(r == 0);
    ROLLBACK_LOG_NODE CAST_FROM_VOIDP(pinned_log, value);
    assert(pinned_log->blocknum.b == blocknum.b);
    *log = pinned_log;
}

void toku_get_and_pin_rollback_log_for_new_entry (TOKUTXN txn, ROLLBACK_LOG_NODE *log) {
    ROLLBACK_LOG_NODE pinned_log;
    invariant(txn->state == TOKUTXN_LIVE || txn->state == TOKUTXN_PREPARING); // hot indexing may call this function for prepared transactions
    if (txn_has_current_rollback_log(txn)) {
        toku_get_and_pin_rollback_log(txn, txn->roll_info.current_rollback, txn->roll_info.current_rollback_hash, &pinned_log);
        toku_rollback_verify_contents(pinned_log, txn->txnid64, txn->roll_info.num_rollback_nodes - 1);
    } else {
        // create a new log for this transaction to use.
        // this call asserts success internally
        rollback_log_create(txn, txn->roll_info.spilled_rollback_tail, txn->roll_info.spilled_rollback_tail_hash, &pinned_log);
    }
    assert(pinned_log->txnid == txn->txnid64);
    assert(pinned_log->blocknum.b != ROLLBACK_NONE.b);
    *log = pinned_log;
}
