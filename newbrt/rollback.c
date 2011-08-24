/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

static void note_txn_closing (TOKUTXN txn);

void
toku_poll_txn_progress_function(TOKUTXN txn, uint8_t is_commit, uint8_t stall_for_checkpoint) {
    if (txn->progress_poll_fun) {
        TOKU_TXN_PROGRESS_S progress = {
            .entries_total     = txn->num_rollentries,
            .entries_processed = txn->num_rollentries_processed,
            .is_commit = is_commit,
            .stalled_on_checkpoint = stall_for_checkpoint};
        txn->progress_poll_fun(&progress, txn->progress_poll_fun_extra);
    }
}

int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv, LSN lsn) {
    int r=0;
    rolltype_dispatch_assign(item, toku_commit_, r, txn, yield, yieldv, lsn);
    txn->num_rollentries_processed++;
    if (txn->num_rollentries_processed % 1024 == 0)
        toku_poll_txn_progress_function(txn, TRUE, FALSE);
    return r;
}

int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv, LSN lsn) {
    int r=0;
    rolltype_dispatch_assign(item, toku_rollback_, r, txn, yield, yieldv, lsn);
    txn->num_rollentries_processed++;
    if (txn->num_rollentries_processed % 1024 == 0)
        toku_poll_txn_progress_function(txn, FALSE, FALSE);
    return r;
}

static inline int
txn_has_inprogress_rollback_log(TOKUTXN txn) {
    return txn->current_rollback.b != ROLLBACK_NONE.b;
}

static inline int
txn_has_spilled_rollback_logs(TOKUTXN txn) {
    return txn->spilled_rollback_tail.b != ROLLBACK_NONE.b;
}

int
toku_delete_rollback_log(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    int r;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    struct brt_header *h = toku_cachefile_get_userdata(cf);
    BLOCKNUM to_free = log->thislogname;
    if (txn->pinned_inprogress_rollback_log == log) {
        txn->pinned_inprogress_rollback_log = NULL;
    }
    r = toku_cachetable_unpin_and_remove (cf, log->thislogname);
    assert(r==0);
    toku_free_blocknum(h->blocktable, &to_free, h);
    return r;
}

static int
toku_apply_txn (TOKUTXN txn, YIELDF yield, void*yieldv, LSN lsn,
                apply_rollback_item func) {
    int r = 0;
    // do the commit/abort calls and free everything
    // we do the commit/abort calls in reverse order too.
    struct roll_entry *item;
    //printf("%s:%d abort\n", __FILE__, __LINE__);
    int count=0;

    BLOCKNUM next_log      = ROLLBACK_NONE;
    uint32_t next_log_hash = 0;

    BOOL is_current = FALSE;
    if (txn_has_inprogress_rollback_log(txn)) {
        next_log      = txn->current_rollback;
        next_log_hash = txn->current_rollback_hash;
        is_current = TRUE;
    }
    else if (txn_has_spilled_rollback_logs(txn)) {
        next_log      = txn->spilled_rollback_tail;
        next_log_hash = txn->spilled_rollback_tail_hash;
    }

    uint64_t last_sequence = txn->num_rollback_nodes;
    BOOL found_head = FALSE;
    while (next_log.b != ROLLBACK_NONE.b) {
        ROLLBACK_LOG_NODE log;
        //pin log
        r = toku_get_and_pin_rollback_log(txn, txn->txnid64, last_sequence-1, next_log, next_log_hash, &log);
        assert(r==0);

        r = toku_maybe_prefetch_older_rollback_log(txn, log);
        assert(r==0);
        
        last_sequence = log->sequence;
        if (func) {
            while ((item=log->newest_logentry)) {
                log->newest_logentry = item->prev;
                r = func(txn, item, yield, yieldv, lsn);
                if (r!=0) return r;
                count++;
                if (count%2 == 0) yield(NULL, NULL, yieldv);
            }
        }
        if (next_log.b == txn->spilled_rollback_head.b) {
            assert(!found_head);
            found_head = TRUE;
            assert(log->sequence == 0);
        }
        next_log      = log->older;
        next_log_hash = log->older_hash;
        {
            //Clean up transaction structure to prevent
            //toku_txn_close from double-freeing
            if (is_current) {
                txn->current_rollback      = ROLLBACK_NONE;
                txn->current_rollback_hash = 0;
                is_current = FALSE;
            }
            else {
                txn->spilled_rollback_tail      = next_log;
                txn->spilled_rollback_tail_hash = next_log_hash;
            }
            if (found_head) {
                assert(next_log.b == ROLLBACK_NONE.b);
                txn->spilled_rollback_head      = next_log;
                txn->spilled_rollback_head_hash = next_log_hash;
            }
        }
        //Unpins log
        r = toku_delete_rollback_log(txn, log);
        assert(r==0);
    }
    return r;
}

int
toku_find_xid_by_xid (OMTVALUE v, void *xidv) {
    TXNID *xid = v;
    TXNID *xidfind = xidv;
    if (*xid<*xidfind) return -1;
    if (*xid>*xidfind) return +1;
    return 0;
}

int
toku_find_pair_by_xid (OMTVALUE v, void *xidv) {
    XID_PAIR pair = v;
    TXNID *xidfind = xidv;
    if (pair->xid1<*xidfind) return -1;
    if (pair->xid1>*xidfind) return +1;
    return 0;
}


// For each xid on the closing txn's live list, find the corresponding entry in the reverse live list.
// There must be one.
// If the second xid in the pair is not the xid of the closing transaction, then the second xid must be newer
// than the closing txn, and there is nothing to be done (except to assert the invariant).
// If the second xid in the pair is the xid of the closing transaction, then we need to find the next oldest
// txn.  If the live_xid is in the live list of the next oldest txn, then set the next oldest txn as the 
// second xid in the pair, otherwise delete the entry from the reverse live list.
static int
live_list_reverse_note_txn_end_iter(OMTVALUE live_xidv, u_int32_t UU(index), void*txnv) {
    TOKUTXN txn = txnv;
    TXNID xid   = txn->txnid64;       // xid of txn that is closing
    TXNID *live_xid = live_xidv;      // xid on closing txn's live list
    OMTVALUE pairv;
    XID_PAIR pair;
    uint32_t idx;

    int r;
    OMT reverse = txn->logger->live_list_reverse;
    r = toku_omt_find_zero(reverse, toku_find_pair_by_xid, live_xid, &pairv, &idx);
    invariant(r==0);
    pair = pairv;
    invariant(pair->xid1 == *live_xid); //sanity check
    if (pair->xid2 == xid) {
        //There is a record that needs to be either deleted or updated
        TXNID *olderxid;
        OMTVALUE olderv;
        uint32_t olderidx;
        OMT snapshot = txn->logger->snapshot_txnids;
        BOOL should_delete = TRUE;
        // find the youngest txn in snapshot that is older than xid
        r = toku_omt_find(snapshot, toku_find_xid_by_xid, &xid, -1, &olderv, &olderidx);
        if (r==0) {
            //There is an older txn
            olderxid = olderv;
            invariant(*olderxid < xid);
            if (*olderxid >= *live_xid) {
                //Older txn is new enough, we need to update.
                pair->xid2 = *olderxid;
                should_delete = FALSE;
            }
        }
        else {
            invariant(r==DB_NOTFOUND);
        }
        if (should_delete) {
            //Delete record
            toku_free(pair);
            r = toku_omt_delete_at(reverse, idx);
            invariant(r==0);
        }
    }
    else {
        invariant(pair->xid2 > xid);
    }
    return r;
}


// When txn ends, update reverse live list.  To do that, examine each txn in this (closing) txn's live list.
static int
live_list_reverse_note_txn_end(TOKUTXN txn) {
    int r;

    r = toku_omt_iterate(txn->live_root_txn_list, live_list_reverse_note_txn_end_iter, txn);
    invariant(r==0);
    return r;
}


void toku_rollback_txn_close (TOKUTXN txn) {
    assert(txn->spilled_rollback_head.b == ROLLBACK_NONE.b);
    assert(txn->spilled_rollback_tail.b == ROLLBACK_NONE.b);
    assert(txn->current_rollback.b == ROLLBACK_NONE.b);
    int r;
    {
        {
            //Remove txn from list (omt) of live transactions
            OMTVALUE txnagain;
            u_int32_t idx;
            r = toku_omt_find_zero(txn->logger->live_txns, find_xid, txn, &txnagain, &idx);
            assert(r==0);
            assert(txn==txnagain);
            r = toku_omt_delete_at(txn->logger->live_txns, idx);
            assert(r==0);
        }

        if (txn->parent==NULL) {
            OMTVALUE txnagain;
            u_int32_t idx;
            //Remove txn from list of live root txns
            r = toku_omt_find_zero(txn->logger->live_root_txns, find_xid, txn, &txnagain, &idx);
            assert(r==0);
            assert(txn==txnagain);
            r = toku_omt_delete_at(txn->logger->live_root_txns, idx);
            assert(r==0);
        }
        //
        // if this txn created a snapshot, make necessary modifications to list of snapshot txnids and live_list_reverse
        // the order of operations is important. We first remove the txnid from the list of snapshot txnids. This is
        // necessary because root snapshot transactions are in their own live lists. If we do not remove 
        // the txnid from the snapshot txnid list first, then when we go to make the modifications to 
        // live_list_reverse, we have trouble. We end up never removing (id, id) from live_list_reverse
        //
        if (txn->snapshot_type != TXN_SNAPSHOT_NONE && (txn->parent==NULL || txn->snapshot_type == TXN_SNAPSHOT_CHILD)) {
            {
                u_int32_t idx;
                OMTVALUE v;
                //Free memory used for snapshot_txnids
                r = toku_omt_find_zero(txn->logger->snapshot_txnids, toku_find_xid_by_xid, &txn->txnid64, &v, &idx);
                invariant(r==0);
                TXNID *xid = v;
                invariant(*xid == txn->txnid64);
                r = toku_omt_delete_at(txn->logger->snapshot_txnids, idx);
                invariant(r==0);
                toku_free(v);
            }
            live_list_reverse_note_txn_end(txn);
            {
                //Free memory used for live root txns local list
                invariant(toku_omt_size(txn->live_root_txn_list) > 0);
                OMTVALUE v;
                //store a single array of txnids
                r = toku_omt_fetch(txn->live_root_txn_list, 0, &v);
                invariant(r==0);
                toku_free(v);
                toku_omt_destroy(&txn->live_root_txn_list);
            }
        }
    }

    assert(txn->logger->oldest_living_xid <= txn->txnid64);
    if (txn->txnid64 == txn->logger->oldest_living_xid) {
        TOKULOGGER logger = txn->logger;

        OMTVALUE oldest_txnv;
        r = toku_omt_fetch(logger->live_txns, 0, &oldest_txnv);
        if (r==0) {
            TOKUTXN oldest_txn = oldest_txnv;
            assert(oldest_txn != txn); // We just removed it
            assert(oldest_txn->txnid64 > txn->logger->oldest_living_xid); //Must be newer than the previous oldest
            txn->logger->oldest_living_xid = oldest_txn->txnid64;
            txn->logger->oldest_living_starttime = oldest_txn->starttime;
        }
        else {
            //No living transactions
            assert(r==EINVAL);
            txn->logger->oldest_living_xid = TXNID_NONE_LIVING;
            txn->logger->oldest_living_starttime = 0;
        }
    }

    note_txn_closing(txn);
    xids_destroy(&txn->xids);
    toku_txn_ignore_free(txn); // 2954
    toku_free(txn);
    return;
}

void* toku_malloc_in_rollback(ROLLBACK_LOG_NODE log, size_t size) {
    return malloc_in_memarena(log->rollentry_arena, size);
}

void *toku_memdup_in_rollback(ROLLBACK_LOG_NODE log, const void *v, size_t len) {
    void *r=toku_malloc_in_rollback(log, len);
    memcpy(r,v,len);
    return r;
}

static int note_brt_used_in_txns_parent(OMTVALUE brtv, u_int32_t UU(index), void*txnv) {
    TOKUTXN child  = txnv;
    TOKUTXN parent = child->parent;
    BRT brt = brtv;
    int r = toku_txn_note_brt(parent, brt);
    if (r==0 &&
        brt->h->txnid_that_created_or_locked_when_empty == toku_txn_get_txnid(child)) {
        //Pass magic "no rollback needed" flag to parent.
        brt->h->txnid_that_created_or_locked_when_empty = toku_txn_get_txnid(parent);
    }
    if (r==0 &&
        brt->h->txnid_that_suppressed_recovery_logs == toku_txn_get_txnid(child)) {
        //Pass magic "no recovery needed" flag to parent.
        brt->h->txnid_that_suppressed_recovery_logs = toku_txn_get_txnid(parent);
    }
    return r;
}

//Commit each entry in the rollback log.
//If the transaction has a parent, it just promotes its information to its parent.
int toku_rollback_commit(TOKUTXN txn, YIELDF yield, void*yieldv, LSN lsn) {
    int r=0;
    if (txn->parent!=0) {
        // First we must put a rollinclude entry into the parent if we spilled

        if (txn_has_spilled_rollback_logs(txn)) {
            uint64_t num_nodes = txn->num_rollback_nodes;
            if (txn_has_inprogress_rollback_log(txn)) {
                num_nodes--; //Don't count the in-progress rollback log.
            }
            r = toku_logger_save_rollback_rollinclude(txn->parent, txn->txnid64, num_nodes,
                                                      txn->spilled_rollback_head, txn->spilled_rollback_head_hash,
                                                      txn->spilled_rollback_tail, txn->spilled_rollback_tail_hash);
            if (r!=0) return r;
            //Remove ownership from child.
            txn->spilled_rollback_head      = ROLLBACK_NONE; 
            txn->spilled_rollback_head_hash = 0; 
            txn->spilled_rollback_tail      = ROLLBACK_NONE; 
            txn->spilled_rollback_tail_hash = 0; 
        }
        if (txn_has_inprogress_rollback_log(txn)) {
            ROLLBACK_LOG_NODE parent_log;
            //Pin parent log
            r = toku_get_and_pin_rollback_log_for_new_entry(txn->parent, &parent_log);
            assert(r==0);

            ROLLBACK_LOG_NODE child_log;
            //Pin child log
            r = toku_get_and_pin_rollback_log(txn, txn->txnid64, txn->num_rollback_nodes - 1,
                                              txn->current_rollback, txn->current_rollback_hash,
                                              &child_log);
            assert(r==0);

            // Append the list to the front of the parent.
            if (child_log->oldest_logentry) {
                // There are some entries, so link them in.
                child_log->oldest_logentry->prev = parent_log->newest_logentry;
                if (!parent_log->oldest_logentry) {
                    parent_log->oldest_logentry = child_log->oldest_logentry;
                }
                parent_log->newest_logentry = child_log->newest_logentry;
                parent_log->rollentry_resident_bytecount += child_log->rollentry_resident_bytecount;
                txn->parent->rollentry_raw_count         += txn->rollentry_raw_count;
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
            //Delete child log (unpins child_log)
            r = toku_delete_rollback_log(txn, child_log);
            assert(r==0);
            txn->current_rollback = ROLLBACK_NONE;
            txn->current_rollback_hash = 0;

            r = toku_maybe_spill_rollbacks(txn->parent, parent_log); //unpins parent_log
            assert(r==0);
        }

        // Note the open brts, the omts must be merged
        r = toku_omt_iterate(txn->open_brts, note_brt_used_in_txns_parent, txn);
        assert(r==0);

        // Merge the list of headers that must be checkpointed before commit
        while (!toku_list_empty(&txn->checkpoint_before_commit)) {
            struct toku_list *list = toku_list_pop(&txn->checkpoint_before_commit);
            toku_list_push(&txn->parent->checkpoint_before_commit, list);
        }

        //If this transaction needs an fsync (if it commits)
        //save that in the parent.  Since the commit really happens in the root txn.
        txn->parent->force_fsync_on_commit |= txn->force_fsync_on_commit;
        txn->parent->num_rollentries       += txn->num_rollentries;
    } else {
        r = toku_apply_txn(txn, yield, yieldv, lsn, toku_commit_rollback_item);
        assert(r==0);
    }

    return r;
}

int toku_rollback_abort(TOKUTXN txn, YIELDF yield, void*yieldv, LSN lsn) {
    int r;
    //Empty the list
    while (!toku_list_empty(&txn->checkpoint_before_commit)) {
        toku_list_pop(&txn->checkpoint_before_commit);
    }

    r = toku_apply_txn(txn, yield, yieldv, lsn, toku_abort_rollback_item);
    assert(r==0);
    return r;
}
                         
// Write something out.  Keep trying even if partial writes occur.
// On error: Return negative with errno set.
// On success return nbytes.

static size_t
rollback_memory_size(ROLLBACK_LOG_NODE log) {
    size_t size = sizeof(*log);
    size += memarena_total_memory_size(log->rollentry_arena);
    return size;
}

static void
toku_rollback_log_free(ROLLBACK_LOG_NODE *log_p) {
    ROLLBACK_LOG_NODE log = *log_p;
    *log_p = NULL; //Sanitize

    // Cleanup the rollback memory
    memarena_close(&log->rollentry_arena);
    toku_free(log);
}

static void toku_rollback_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM logname,
                                          void *rollback_v, void *extraargs, long UU(size),
                                          BOOL write_me, BOOL keep_me, BOOL for_checkpoint) {
    int r;
    ROLLBACK_LOG_NODE  log = rollback_v;
    struct brt_header *h   = extraargs;

    assert(h->cf == cachefile);
    assert(log->thislogname.b==logname.b);

    if (write_me && !h->panic) {
        int n_workitems, n_threads; 
        toku_cachefile_get_workqueue_load(cachefile, &n_workitems, &n_threads);

        r = toku_serialize_rollback_log_to(fd, log->thislogname, log, h, n_workitems, n_threads, for_checkpoint);
        if (r) {
            if (h->panic==0) {
                char *e = strerror(r);
                int   l = 200 + strlen(e);
                char s[l];
                h->panic=r;
                snprintf(s, l-1, "While writing data to disk, error %d (%s)", r, e);
                h->panic_string = toku_strdup(s);
            }
        }
    }
    if (!keep_me) {
        toku_rollback_log_free(&log);
    }
}

static int toku_rollback_fetch_callback (CACHEFILE cachefile, int fd, BLOCKNUM logname, u_int32_t fullhash,
					 void **rollback_pv, long *sizep, int * UU(dirtyp), void *extraargs) {
    int r;
    struct brt_header *h = extraargs;
    assert(h->cf == cachefile);

    ROLLBACK_LOG_NODE *result = (ROLLBACK_LOG_NODE*)rollback_pv;
    r = toku_deserialize_rollback_log_from(fd, logname, fullhash, result, h);
    if (r==0) {
        *sizep = rollback_memory_size(*result);
    }
    return r;
}

static void toku_rollback_pe_est_callback(
    void* rollback_v, 
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    assert(rollback_v != NULL);
    *bytes_freed_estimate = 0;
    *cost = PE_CHEAP;
}

// callback for partially evicting a cachetable entry
static int toku_rollback_pe_callback (
    void *rollback_v, 
    long UU(bytes_to_free), 
    long* bytes_freed, 
    void* UU(extraargs)
    ) 
{
    assert(rollback_v != NULL);
    *bytes_freed = 0;
    return 0;
}
static BOOL toku_rollback_pf_req_callback(void* UU(brtnode_pv), void* UU(read_extraargs)) {
    return FALSE;
}

static int toku_rollback_pf_callback(void* UU(brtnode_pv), void* UU(read_extraargs), int UU(fd), long* UU(sizep)) {
    // should never be called, given that toku_rollback_pf_req_callback always returns false
    assert(FALSE);
    return 0;
}



static int toku_create_new_rollback_log (TOKUTXN txn, BLOCKNUM older, uint32_t older_hash, ROLLBACK_LOG_NODE *result) {
    ROLLBACK_LOG_NODE MALLOC(log);
    assert(log);

    int r;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    struct brt_header *h = toku_cachefile_get_userdata(cf);

    log->layout_version                = BRT_LAYOUT_VERSION;
    log->layout_version_original       = BRT_LAYOUT_VERSION;
    log->layout_version_read_from_disk = BRT_LAYOUT_VERSION;
    log->dirty = TRUE;
    log->txnid = txn->txnid64;
    log->sequence = txn->num_rollback_nodes++;
    toku_allocate_blocknum(h->blocktable, &log->thislogname, h);
    log->thishash   = toku_cachetable_hash(cf, log->thislogname);
    log->older      = older;
    log->older_hash = older_hash;
    log->oldest_logentry = NULL;
    log->newest_logentry = NULL;
    log->rollentry_arena = memarena_create();
    log->rollentry_resident_bytecount = 0;

    *result = log;
    r=toku_cachetable_put(cf, log->thislogname, log->thishash,
                          log, rollback_memory_size(log),
                          toku_rollback_flush_callback, 
                          toku_rollback_pe_est_callback,
                          toku_rollback_pe_callback,
                          h);
    assert(r==0);
    txn->current_rollback      = log->thislogname;
    txn->current_rollback_hash = log->thishash;
    txn->pinned_inprogress_rollback_log = log;
    return 0;
}

int
toku_rollback_log_unpin(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    int r;
    CACHEFILE cf = txn->logger->rollback_cachefile;
    if (txn->pinned_inprogress_rollback_log == log) {
        txn->pinned_inprogress_rollback_log = NULL;
    }
    r = toku_cachetable_unpin(cf, log->thislogname, log->thishash,
                              (enum cachetable_dirty)log->dirty, rollback_memory_size(log));
    assert(r==0);
    return r;
}

//Requires: log is pinned
//          log is current
//After:
//  log is unpinned if a spill happened
//  Maybe there is no current after (if it spilled)
int toku_maybe_spill_rollbacks (TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    int r = 0;
    if (log->rollentry_resident_bytecount > txn->logger->write_block_size) {
        assert(log->thislogname.b == txn->current_rollback.b);
        //spill
        if (!txn_has_spilled_rollback_logs(txn)) {
            //First spilled.  Copy to head.
            txn->spilled_rollback_head      = txn->current_rollback;
            txn->spilled_rollback_head_hash = txn->current_rollback_hash;
        }
        //Unconditionally copy to tail.  Old tail does not need to be cached anymore.
        txn->spilled_rollback_tail      = txn->current_rollback;
        txn->spilled_rollback_tail_hash = txn->current_rollback_hash;

        txn->current_rollback      = ROLLBACK_NONE;
        txn->current_rollback_hash = 0;
        //Unpin
        r = toku_rollback_log_unpin(txn, log);
        assert(r==0);
    }
    return r;
}

//Heaviside function to find a TOKUTXN by TOKUTXN (used to find the index)
int find_xid (OMTVALUE v, void *txnv) {
    TOKUTXN txn = v;
    TOKUTXN txnfind = txnv;
    if (txn->txnid64<txnfind->txnid64) return -1;
    if (txn->txnid64>txnfind->txnid64) return +1;
    return 0;
}

static int find_filenum (OMTVALUE v, void *brtv) {
    BRT brt     = v;
    BRT brtfind = brtv;
    FILENUM fnum     = toku_cachefile_filenum(brt    ->cf);
    FILENUM fnumfind = toku_cachefile_filenum(brtfind->cf);
    if (fnum.fileid<fnumfind.fileid) return -1;
    if (fnum.fileid>fnumfind.fileid) return +1;
    if (brt < brtfind) return -1;
    if (brt > brtfind) return +1;
    return 0;
}

//Notify a transaction that it has touched a brt.
int toku_txn_note_brt (TOKUTXN txn, BRT brt) {
    OMTVALUE txnv;
    u_int32_t index;
    // Does brt already know about transaction txn?
    int r = toku_omt_find_zero(brt->txns, find_xid, txn, &txnv, &index);
    if (r==0) {
	// It's already there.
	assert((TOKUTXN)txnv==txn);
	return 0;
    }
    // Otherwise it's not there.
    // Insert reference to transaction into brt
    r = toku_omt_insert_at(brt->txns, txn, index);
    assert(r==0);
    // Insert reference to brt into transaction
    r = toku_omt_insert(txn->open_brts, brt, find_filenum, brt, 0);
    assert(r==0);
    return 0;
}

struct swap_brt_extra {
    BRT live;
    BRT zombie;
};

static int swap_brt (OMTVALUE txnv, u_int32_t UU(idx), void *extra) {
    struct swap_brt_extra *info = extra;

    TOKUTXN txn = txnv;
    OMTVALUE zombie_again=NULL;
    u_int32_t index;

    int r;
    r = toku_txn_note_brt(txn, info->live); //Add new brt.
    assert(r==0);
    r = toku_omt_find_zero(txn->open_brts, find_filenum, info->zombie, &zombie_again, &index);
    assert(r==0);
    assert((void*)zombie_again==info->zombie);
    r = toku_omt_delete_at(txn->open_brts, index); //Delete old brt.
    assert(r==0);
    return 0;
}

int toku_txn_note_swap_brt (BRT live, BRT zombie) {
    if (zombie->pinned_by_checkpoint) {
        //Swap checkpoint responsibility.
        assert(!live->pinned_by_checkpoint); //Pin only uses one brt.
        live->pinned_by_checkpoint = 1;
        zombie->pinned_by_checkpoint = 0;
    }

    struct swap_brt_extra swap = {.live = live, .zombie = zombie};
    int r = toku_omt_iterate(zombie->txns, swap_brt, &swap);
    assert(r==0);
    toku_omt_clear(zombie->txns);

    //Close immediately.
    assert(zombie->close_db);
    assert(!toku_brt_zombie_needed(zombie));
    r = zombie->close_db(zombie->db, zombie->close_flags);
    return r;
}


static int remove_brt (OMTVALUE txnv, u_int32_t UU(idx), void *brtv) {
    TOKUTXN txn = txnv;
    BRT     brt = brtv;
    OMTVALUE brtv_again=NULL;
    u_int32_t index;
    int r = toku_omt_find_zero(txn->open_brts, find_filenum, brt, &brtv_again, &index);
    assert(r==0);
    assert((void*)brtv_again==brtv);
    r = toku_omt_delete_at(txn->open_brts, index);
    assert(r==0);
    return 0;
}

int toku_txn_note_close_brt (BRT brt) {
    assert(toku_omt_size(brt->txns)==0);
    int r = toku_omt_iterate(brt->txns, remove_brt, brt);
    assert(r==0);
    return 0;
}

static int remove_txn (OMTVALUE brtv, u_int32_t UU(idx), void *txnv) {
    BRT brt     = brtv;
    TOKUTXN txn = txnv;
    OMTVALUE txnv_again=NULL;
    u_int32_t index;
    int r = toku_omt_find_zero(brt->txns, find_xid, txn, &txnv_again, &index);
    assert(r==0);
    assert((void*)txnv_again==txnv);
    r = toku_omt_delete_at(brt->txns, index);
    assert(r==0);
    if (txn->txnid64==brt->h->txnid_that_created_or_locked_when_empty) {
        brt->h->txnid_that_created_or_locked_when_empty = TXNID_NONE;
        brt->h->root_that_created_or_locked_when_empty  = TXNID_NONE;
    }
    if (txn->txnid64==brt->h->txnid_that_suppressed_recovery_logs) {
        brt->h->txnid_that_suppressed_recovery_logs = TXNID_NONE;
    }
    if (!toku_brt_zombie_needed(brt) && brt->was_closed) {
        //Close immediately.
        assert(brt->close_db);
        r = brt->close_db(brt->db, brt->close_flags);
    }
    return r;
}

// for every BRT in txn, remove it.
static void note_txn_closing (TOKUTXN txn) {
    toku_omt_iterate(txn->open_brts, remove_txn, txn);
    toku_omt_destroy(&txn->open_brts);
}

// Return the number of bytes that went into the rollback data structure (the uncompressed count if there is compression)
int toku_logger_txn_rollback_raw_count(TOKUTXN txn, u_int64_t *raw_count)
{
    *raw_count = txn->rollentry_raw_count;
    return 0;
}

int toku_txn_find_by_xid (BRT brt, TXNID xid, TOKUTXN *txnptr) {
    struct tokutxn fake_txn; fake_txn.txnid64 = xid;
    u_int32_t index;
    OMTVALUE txnv;
    int r = toku_omt_find_zero(brt->txns, find_xid, &fake_txn, &txnv, &index);
    if (r == 0) *txnptr = txnv;
    return r;
}

int
toku_maybe_prefetch_older_rollback_log(TOKUTXN txn, ROLLBACK_LOG_NODE log) {
    //Currently processing 'log'.  Prefetch the next (older) log node.

    BLOCKNUM name = log->older;
    int r = 0;
    if (name.b != ROLLBACK_NONE.b) {
        uint32_t hash = log->older_hash;
        CACHEFILE cf = txn->logger->rollback_cachefile;
        struct brt_header *h = toku_cachefile_get_userdata(cf);
        BOOL doing_prefetch = FALSE;
        r = toku_cachefile_prefetch(cf, name, hash,
                                    toku_rollback_flush_callback,
                                    toku_rollback_fetch_callback,
                                    toku_rollback_pe_est_callback,
                                    toku_rollback_pe_callback,
                                    toku_brtnode_pf_req_callback,
                                    toku_brtnode_pf_callback,
                                    h,
                                    h,
                                    &doing_prefetch);
        assert(r==0);
    }
    return r;
}

int toku_get_and_pin_rollback_log(TOKUTXN txn, TXNID xid, uint64_t sequence, BLOCKNUM name, uint32_t hash, ROLLBACK_LOG_NODE *result) {
    BOOL save_inprogress_node = FALSE;
    assert(name.b != ROLLBACK_NONE.b);
    int r = 0;
    ROLLBACK_LOG_NODE log = NULL;

    if (name.b == txn->current_rollback.b) {
        assert(hash == txn->current_rollback_hash);
        log = txn->pinned_inprogress_rollback_log;
        save_inprogress_node = TRUE;
    }
    if (!log) {
        CACHEFILE cf = txn->logger->rollback_cachefile;
        void * log_v;
        struct brt_header *h = toku_cachefile_get_userdata(cf);
        r = toku_cachetable_get_and_pin(cf, name, hash,
                                        &log_v, NULL,
                                        toku_rollback_flush_callback, 
                                        toku_rollback_fetch_callback,
                                        toku_rollback_pe_est_callback,
                                        toku_rollback_pe_callback,
                                        toku_rollback_pf_req_callback,
                                        toku_rollback_pf_callback,
                                        h,
                                        h);
        assert(r==0);
        log = (ROLLBACK_LOG_NODE)log_v;
    }
    if (r==0) {
        assert(log->thislogname.b == name.b);
        assert(log->txnid    == xid);
        assert(log->sequence == sequence);
        if (save_inprogress_node) {
            txn->pinned_inprogress_rollback_log = log;
        }
        *result = log;
    }
    return r;
}

int toku_get_and_pin_rollback_log_for_new_entry (TOKUTXN txn, ROLLBACK_LOG_NODE *result) {
    invariant(txn->state == TOKUTXN_LIVE); // #3258
    int r;
    ROLLBACK_LOG_NODE log;
    if (txn_has_inprogress_rollback_log(txn)) {
        r = toku_get_and_pin_rollback_log(txn, txn->txnid64, txn->num_rollback_nodes-1,
                                          txn->current_rollback, txn->current_rollback_hash, &log);
        assert(r==0);
    }
    else {
        //Generate new one.
        //tail will be ROLLBACK_NONE if this is the very first
        r = toku_create_new_rollback_log(txn, txn->spilled_rollback_tail, txn->spilled_rollback_tail_hash, &log);
        assert(r==0);
    }
    if (r==0) {
        assert(log->txnid == txn->txnid64);
	assert(log->thislogname.b != ROLLBACK_NONE.b);	
        *result = log;
    }
    return r;
}


