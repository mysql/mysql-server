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

#include "ft/cachetable/checkpoint.h"
#include "ft/ft.h"
#include "ft/logger/log-internal.h"
#include "ft/ule.h"
#include "ft/txn/rollback-apply.h"
#include "ft/txn/txn.h"
#include "ft/txn/txn_manager.h"
#include "util/status.h"

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static TXN_STATUS_S txn_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUFT_STATUS_INIT(txn_status, k, c, t, "txn: " l, inc)

void
txn_status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(TXN_BEGIN,            TXN_BEGIN, PARCOUNT,   "begin", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(TXN_READ_BEGIN,       TXN_BEGIN_READ_ONLY, PARCOUNT,   "begin read only", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(TXN_COMMIT,           TXN_COMMITS, PARCOUNT,   "successful commits", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(TXN_ABORT,            TXN_ABORTS, PARCOUNT,   "aborts", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    txn_status.initialized = true;
}

void txn_status_destroy(void) {
    for (int i = 0; i < TXN_STATUS_NUM_ROWS; ++i) {
        if (txn_status.status[i].type == PARCOUNT) {
            destroy_partitioned_counter(txn_status.status[i].value.parcount);
        }
    }
}

#undef STATUS_INIT

#define STATUS_INC(x, d) increment_partitioned_counter(txn_status.status[x].value.parcount, d)

void 
toku_txn_get_status(TXN_STATUS s) {
    *s = txn_status;
}

void
toku_txn_lock(TOKUTXN txn)
{
    toku_mutex_lock(&txn->txn_lock);
}

void
toku_txn_unlock(TOKUTXN txn)
{
    toku_mutex_unlock(&txn->txn_lock);
}

uint64_t
toku_txn_get_root_id(TOKUTXN txn)
{
    return txn->txnid.parent_id64;
}

bool txn_declared_read_only(TOKUTXN txn) {
    return txn->declared_read_only;
}

int 
toku_txn_begin_txn (
    DB_TXN  *container_db_txn,
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn,
    TOKULOGGER logger, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    bool read_only
    ) 
{
    int r = toku_txn_begin_with_xid(
        parent_tokutxn, 
        tokutxn, 
        logger, 
        TXNID_PAIR_NONE, 
        snapshot_type, 
        container_db_txn, 
        false, // for_recovery
        read_only
        );
    return r;
}


static void
txn_create_xids(TOKUTXN txn, TOKUTXN parent) {
    XIDS xids;
    XIDS parent_xids;
    if (parent == NULL) {
        parent_xids = toku_xids_get_root_xids();
    } else {
        parent_xids = parent->xids;
    }
    toku_xids_create_unknown_child(parent_xids, &xids);
    TXNID finalized_xid = (parent == NULL) ? txn->txnid.parent_id64 : txn->txnid.child_id64;
    toku_xids_finalize_with_child(xids, finalized_xid);
    txn->xids = xids;
}

// Allocate and initialize a txn
static void toku_txn_create_txn(TOKUTXN *txn_ptr, TOKUTXN parent, TOKULOGGER logger, TXN_SNAPSHOT_TYPE snapshot_type, DB_TXN *container_db_txn, bool for_checkpoint, bool read_only);

int 
toku_txn_begin_with_xid (
    TOKUTXN parent, 
    TOKUTXN *txnp, 
    TOKULOGGER logger, 
    TXNID_PAIR xid, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn,
    bool for_recovery,
    bool read_only
    ) 
{   
    int r = 0;
    TOKUTXN txn;
    // check for case where we are trying to 
    // create too many nested transactions
    if (!read_only && parent && !toku_xids_can_create_child(parent->xids)) {
        r = EINVAL;
        goto exit;
    }
    if (read_only && parent) {
        invariant(txn_declared_read_only(parent));
    }
    toku_txn_create_txn(&txn, parent, logger, snapshot_type, container_db_txn, for_recovery, read_only);
    // txnid64, snapshot_txnid64 
    // will be set in here.
    if (for_recovery) {
        if (parent == NULL) {
            invariant(xid.child_id64 == TXNID_NONE);
            toku_txn_manager_start_txn_for_recovery(
                txn,
                logger->txn_manager,
                xid.parent_id64
                );
        }
        else {
            parent->child_manager->start_child_txn_for_recovery(txn, parent, xid);
        }
    }
    else {
        assert(xid.parent_id64 == TXNID_NONE);
        assert(xid.child_id64 == TXNID_NONE);
        if (parent == NULL) {
            toku_txn_manager_start_txn(
                txn, 
                logger->txn_manager, 
                snapshot_type,
                read_only
                );
        }
        else {
            parent->child_manager->start_child_txn(txn, parent);
            toku_txn_manager_handle_snapshot_create_for_child_txn(
                txn, 
                logger->txn_manager, 
                snapshot_type
                );
        }
    }
    if (!read_only) {
        // this call will set txn->xids
        txn_create_xids(txn, parent);
    }
    *txnp = txn;
exit:
    return r;
}

DB_TXN *
toku_txn_get_container_db_txn (TOKUTXN tokutxn) {
    DB_TXN * container = tokutxn->container_db_txn;
    return container;
}

void toku_txn_set_container_db_txn (TOKUTXN tokutxn, DB_TXN*container) {
    tokutxn->container_db_txn = container;
}

static void invalidate_xa_xid (TOKU_XA_XID *xid) {
    TOKU_ANNOTATE_NEW_MEMORY(xid, sizeof(*xid)); // consider it to be all invalid for valgrind
    xid->formatID = -1; // According to the XA spec, -1 means "invalid data"
}

static void toku_txn_create_txn (
    TOKUTXN *tokutxn, 
    TOKUTXN parent_tokutxn, 
    TOKULOGGER logger, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn,
    bool for_recovery,
    bool read_only
    )
{
    assert(logger->rollback_cachefile);

    omt<FT> open_fts;
    open_fts.create_no_array();
        
    struct txn_roll_info roll_info = {
        .num_rollback_nodes = 0,
        .num_rollentries = 0,
        .num_rollentries_processed = 0,
        .rollentry_raw_count = 0,
        .spilled_rollback_head = ROLLBACK_NONE,
        .spilled_rollback_tail = ROLLBACK_NONE,
        .current_rollback = ROLLBACK_NONE,
    };

static txn_child_manager tcm;

    struct tokutxn new_txn = {
        .txnid = {.parent_id64 = TXNID_NONE, .child_id64 = TXNID_NONE },
        .snapshot_txnid64 = TXNID_NONE,
        .snapshot_type = for_recovery ? TXN_SNAPSHOT_NONE : snapshot_type,
        .for_recovery = for_recovery,
        .logger = logger,
        .parent = parent_tokutxn,
        .child = NULL,
        .child_manager_s = tcm,
        .child_manager = NULL,
        .container_db_txn = container_db_txn,
        .live_root_txn_list = nullptr,
        .xids = NULL,
        .snapshot_next = NULL,
        .snapshot_prev = NULL,
        .begin_was_logged = false,
        .declared_read_only = read_only,
        .do_fsync = false,
        .force_fsync_on_commit = false,
        .do_fsync_lsn = ZERO_LSN,
        .xa_xid = {0, 0, 0, {}},
        .progress_poll_fun = NULL,
        .progress_poll_fun_extra = NULL,
        .txn_lock = TOKU_MUTEX_INITIALIZER,
        .open_fts = open_fts,
        .roll_info = roll_info,
        .state_lock = TOKU_MUTEX_INITIALIZER,
        .state_cond = TOKU_COND_INITIALIZER,
        .state = TOKUTXN_LIVE,
        .num_pin = 0,
        .client_id = 0,
        .start_time = time(NULL),
    };

    TOKUTXN result = NULL;
    XMEMDUP(result, &new_txn);
    invalidate_xa_xid(&result->xa_xid);
    if (parent_tokutxn == NULL) {
        result->child_manager = &result->child_manager_s;
        result->child_manager->init(result);
    }
    else {
        result->child_manager = parent_tokutxn->child_manager;
    }

    toku_mutex_init(&result->txn_lock, nullptr);

    toku_pthread_mutexattr_t attr;
    toku_mutexattr_init(&attr);
    toku_mutexattr_settype(&attr, TOKU_MUTEX_ADAPTIVE);
    toku_mutex_init(&result->state_lock, &attr);
    toku_mutexattr_destroy(&attr);

    toku_cond_init(&result->state_cond, nullptr);

    *tokutxn = result;

    if (read_only) {
        STATUS_INC(TXN_READ_BEGIN, 1);
    }
    else {
        STATUS_INC(TXN_BEGIN, 1);
    }
}

void
toku_txn_update_xids_in_txn(TOKUTXN txn, TXNID xid)
{
    // these should not have been set yet
    invariant(txn->txnid.parent_id64 == TXNID_NONE);
    invariant(txn->txnid.child_id64 == TXNID_NONE);
    txn->txnid.parent_id64 = xid;
    txn->txnid.child_id64 = TXNID_NONE;
}

//Used on recovery to recover a transaction.
int
toku_txn_load_txninfo (TOKUTXN txn, struct txninfo *info) {
    txn->roll_info.rollentry_raw_count = info->rollentry_raw_count;
    uint32_t i;
    for (i = 0; i < info->num_fts; i++) {
        FT ft = info->open_fts[i];
        toku_txn_maybe_note_ft(txn, ft);
    }
    txn->force_fsync_on_commit = info->force_fsync_on_commit;
    txn->roll_info.num_rollback_nodes = info->num_rollback_nodes;
    txn->roll_info.num_rollentries = info->num_rollentries;

    txn->roll_info.spilled_rollback_head = info->spilled_rollback_head;
    txn->roll_info.spilled_rollback_tail = info->spilled_rollback_tail;
    txn->roll_info.current_rollback = info->current_rollback;
    return 0;
}

int toku_txn_commit_txn(TOKUTXN txn, int nosync,
                        TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
// Effect: Doesn't close the txn, just performs the commit operations.
//  If release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    return toku_txn_commit_with_lsn(txn, nosync, ZERO_LSN,
                                    poll, poll_extra);
}

struct xcommit_info {
    int r;
    TOKUTXN txn;
};

static void txn_note_commit(TOKUTXN txn) {
    // Purpose:
    //  Delay until any indexer is done pinning this transaction.
    //  Update status of a transaction from live->committing (or prepared->committing)
    //  Do so in a thread-safe manner that does not conflict with hot indexing or
    //  begin checkpoint.
    if (toku_txn_is_read_only(txn)) {
        // Neither hot indexing nor checkpoint do any work with readonly txns,
        // so we can skip taking the txn_manager lock here.
        invariant(txn->state==TOKUTXN_LIVE);
        txn->state = TOKUTXN_COMMITTING;
        goto done;
    }
    if (txn->state==TOKUTXN_PREPARING) {
        invalidate_xa_xid(&txn->xa_xid);
    }
    // for hot indexing, if hot index is processing
    // this transaction in some leafentry, then we cannot change
    // the state to commit or abort until
    // hot index is done with that leafentry
    toku_txn_lock_state(txn);
    while (txn->num_pin > 0) {
        toku_cond_wait(
            &txn->state_cond,
            &txn->state_lock
            );
    }
    txn->state = TOKUTXN_COMMITTING;
    toku_txn_unlock_state(txn);
done:
    return;
}

int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) 
{
    // there should be no child when we commit or abort a TOKUTXN
    invariant(txn->child == NULL);
    txn_note_commit(txn);

    // Child transactions do not actually 'commit'.  They promote their 
    // changes to parent, so no need to fsync if this txn has a parent. The
    // do_sync state is captured in the txn for txn_maybe_fsync_log function
    // Additionally, if the transaction was first prepared, we do not need to 
    // fsync because the prepare caused an fsync of the log. In this case, 
    // we do not need an additional of the log. We rely on the client running 
    // recovery to properly recommit this transaction if the commit 
    // does not make it to disk. In the case of MySQL, that would be the
    // binary log.
    txn->do_fsync = !txn->parent && (txn->force_fsync_on_commit || (!nosync && txn->roll_info.num_rollentries>0));

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;

    if (!toku_txn_is_read_only(txn)) {
        toku_log_xcommit(txn->logger, &txn->do_fsync_lsn, 0, txn, txn->txnid);
    }
    // If !txn->begin_was_logged, we could skip toku_rollback_commit
    // but it's cheap (only a number of function calls that return immediately)
    // since there were no writes.  Skipping it would mean we would need to be careful
    // in case we added any additional required cleanup into those functions in the future.
    int r = toku_rollback_commit(txn, oplsn);
    STATUS_INC(TXN_COMMIT, 1);
    return r;
}

int toku_txn_abort_txn(TOKUTXN txn,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
// Effect: Doesn't close the txn, just performs the abort operations.
// If release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    return toku_txn_abort_with_lsn(txn, ZERO_LSN, poll, poll_extra);
}

static void txn_note_abort(TOKUTXN txn) {
    // Purpose:
    //  Delay until any indexer is done pinning this transaction.
    //  Update status of a transaction from live->aborting (or prepared->aborting)
    //  Do so in a thread-safe manner that does not conflict with hot indexing or
    //  begin checkpoint.
    if (toku_txn_is_read_only(txn)) {
        // Neither hot indexing nor checkpoint do any work with readonly txns,
        // so we can skip taking the state lock here.
        invariant(txn->state==TOKUTXN_LIVE);
        txn->state = TOKUTXN_ABORTING;
        goto done;
    }
    if (txn->state==TOKUTXN_PREPARING) {
        invalidate_xa_xid(&txn->xa_xid);
    }
    // for hot indexing, if hot index is processing
    // this transaction in some leafentry, then we cannot change
    // the state to commit or abort until
    // hot index is done with that leafentry
    toku_txn_lock_state(txn);
    while (txn->num_pin > 0) {
        toku_cond_wait(
            &txn->state_cond,
            &txn->state_lock
            );
    }
    txn->state = TOKUTXN_ABORTING;
    toku_txn_unlock_state(txn);
done:
    return;
}

int toku_txn_abort_with_lsn(TOKUTXN txn, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
{
    // there should be no child when we commit or abort a TOKUTXN
    invariant(txn->child == NULL);
    txn_note_abort(txn);

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;
    txn->do_fsync = false;

    if (!toku_txn_is_read_only(txn)) {
        toku_log_xabort(txn->logger, &txn->do_fsync_lsn, 0, txn, txn->txnid);
    }
    // If !txn->begin_was_logged, we could skip toku_rollback_abort
    // but it's cheap (only a number of function calls that return immediately)
    // since there were no writes.  Skipping it would mean we would need to be careful
    // in case we added any additional required cleanup into those functions in the future.
    int r = toku_rollback_abort(txn, oplsn);
    STATUS_INC(TXN_ABORT, 1);
    return r;
}

static void copy_xid (TOKU_XA_XID *dest, TOKU_XA_XID *source) {
    TOKU_ANNOTATE_NEW_MEMORY(dest, sizeof(*dest));
    dest->formatID     = source->formatID;
    dest->gtrid_length = source->gtrid_length;
    dest->bqual_length = source->bqual_length;
    memcpy(dest->data, source->data, source->gtrid_length+source->bqual_length);
}

void toku_txn_prepare_txn (TOKUTXN txn, TOKU_XA_XID *xa_xid, int nosync) {
    if (txn->parent || toku_txn_is_read_only(txn)) {
        // We do not prepare children.
        //
        // Readonly transactions do the same if they commit or abort, so
        // XA guarantees are free.  No need to pay for overhead of prepare.
        return;
    }
    assert(txn->state==TOKUTXN_LIVE);
    // This state transition must be protected against begin_checkpoint
    // Therefore, the caller must have the mo lock held
    toku_txn_lock_state(txn);
    txn->state = TOKUTXN_PREPARING; 
    toku_txn_unlock_state(txn);
    // Do we need to do an fsync?
    txn->do_fsync = txn->force_fsync_on_commit || (!nosync && txn->roll_info.num_rollentries>0);
    copy_xid(&txn->xa_xid, xa_xid);
    // This list will go away with #4683, so we wn't need the ydb lock for this anymore.
    toku_log_xprepare(txn->logger, &txn->do_fsync_lsn, 0, txn, txn->txnid, xa_xid);
}

void toku_txn_get_prepared_xa_xid (TOKUTXN txn, TOKU_XA_XID *xid) {
    copy_xid(xid, &txn->xa_xid);
}

int toku_logger_recover_txn (TOKULOGGER logger, struct tokulogger_preplist preplist[/*count*/], long count, /*out*/ long *retp, uint32_t flags) {
    return toku_txn_manager_recover_root_txn(
        logger->txn_manager,
        preplist,
        count,
        retp,
        flags
        );
}

void toku_txn_maybe_fsync_log(TOKULOGGER logger, LSN do_fsync_lsn, bool do_fsync) {
    if (logger && do_fsync) {
        toku_logger_fsync_if_lsn_not_fsynced(logger, do_fsync_lsn);
    }
}

void toku_txn_get_fsync_info(TOKUTXN ttxn, bool* do_fsync, LSN* do_fsync_lsn) {
    *do_fsync = ttxn->do_fsync;
    *do_fsync_lsn = ttxn->do_fsync_lsn;
}

void toku_txn_close_txn(TOKUTXN txn) {
    toku_txn_complete_txn(txn);
    toku_txn_destroy_txn(txn);
}

int remove_txn (const FT &h, const uint32_t UU(idx), TOKUTXN const txn);
int remove_txn (const FT &h, const uint32_t UU(idx), TOKUTXN const UU(txn))
// Effect:  This function is called on every open FT that a transaction used.
//  This function removes the transaction from that FT.
{
    toku_ft_remove_txn_ref(h);

    return 0;
}

// for every ft in txn, remove it.
static void note_txn_closing (TOKUTXN txn) {
    txn->open_fts.iterate<struct tokutxn, remove_txn>(txn);
}

void toku_txn_complete_txn(TOKUTXN txn) {
    assert(txn->roll_info.spilled_rollback_head.b == ROLLBACK_NONE.b);
    assert(txn->roll_info.spilled_rollback_tail.b == ROLLBACK_NONE.b);
    assert(txn->roll_info.current_rollback.b == ROLLBACK_NONE.b);
    assert(txn->num_pin == 0);
    assert(txn->state == TOKUTXN_COMMITTING || txn->state == TOKUTXN_ABORTING || txn->state == TOKUTXN_PREPARING);
    if (txn->parent) {
        toku_txn_manager_handle_snapshot_destroy_for_child_txn(
            txn,
            txn->logger->txn_manager,
            txn->snapshot_type
            );
        txn->parent->child_manager->finish_child_txn(txn);
    }
    else {
        toku_txn_manager_finish_txn(txn->logger->txn_manager, txn);
        txn->child_manager->destroy();
    }
    // note that here is another place we depend on
    // this function being called with the multi operation lock
    note_txn_closing(txn);
}

void toku_txn_destroy_txn(TOKUTXN txn) {
    txn->open_fts.destroy();
    if (txn->xids) {
        toku_xids_destroy(&txn->xids);
    }
    toku_mutex_destroy(&txn->txn_lock);
    toku_mutex_destroy(&txn->state_lock);
    toku_cond_destroy(&txn->state_cond);
    toku_free(txn);
}

XIDS toku_txn_get_xids (TOKUTXN txn) {
    if (txn==0) return toku_xids_get_root_xids();
    else return txn->xids;
}

void toku_txn_force_fsync_on_commit(TOKUTXN txn) {
    txn->force_fsync_on_commit = true;
}

TXNID toku_get_oldest_in_live_root_txn_list(TOKUTXN txn) {
    TXNID xid;
    if (txn->live_root_txn_list->size()>0) {
        int r = txn->live_root_txn_list->fetch(0, &xid);
        assert_zero(r);
    }
    else {
        xid = TXNID_NONE;
    }
    return xid;
}

bool toku_is_txn_in_live_root_txn_list(const xid_omt_t &live_root_txn_list, TXNID xid) {
    TXNID txnid;
    bool retval = false;
    int r = live_root_txn_list.find_zero<TXNID, toku_find_xid_by_xid>(xid, &txnid, nullptr);
    if (r==0) {
        invariant(txnid == xid);
        retval = true;
    }
    else {
        invariant(r==DB_NOTFOUND);
    }
    return retval;
}

TOKUTXN_STATE
toku_txn_get_state(TOKUTXN txn) {
    return txn->state;
}

static void
maybe_log_begin_txn_for_write_operation_unlocked(TOKUTXN txn) {
    // We now hold the lock.
    if (txn->begin_was_logged) {
        return;
    }
    TOKUTXN parent;
    parent = txn->parent;
    TXNID_PAIR xid;
    xid = txn->txnid;
    TXNID_PAIR pxid;
    pxid = TXNID_PAIR_NONE;
    if (parent) {
        // Recursively log parent first if necessary.
        // Transactions cannot do work if they have children,
        // so the lowest level child's lock is sufficient for ancestors.
        maybe_log_begin_txn_for_write_operation_unlocked(parent);
        pxid = parent->txnid;
    }

    toku_log_xbegin(txn->logger, NULL, 0, xid, pxid);
    txn->begin_was_logged = true;
}

void
toku_maybe_log_begin_txn_for_write_operation(TOKUTXN txn) {
    toku_txn_lock(txn);
    maybe_log_begin_txn_for_write_operation_unlocked(txn);
    toku_txn_unlock(txn);
}

bool
toku_txn_is_read_only(TOKUTXN txn) {
    // No need to recursively check children because parents are
    // recursively logged before children.
    if (!txn->begin_was_logged) {
        // Did no work.
        invariant(txn->roll_info.num_rollentries == 0);
        invariant(txn->do_fsync_lsn.lsn == ZERO_LSN.lsn);
        invariant(txn->open_fts.size() == 0);
        invariant(txn->num_pin==0);
        return true;
    }
    return false;
}

// needed for hot indexing
void toku_txn_lock_state(TOKUTXN txn) {
    toku_mutex_lock(&txn->state_lock);
}
void toku_txn_unlock_state(TOKUTXN txn){
    toku_mutex_unlock(&txn->state_lock);
}


// prevents a client thread from transitioning txn from LIVE|PREPARING -> COMMITTING|ABORTING
// hot indexing may need a transactions to stay in the LIVE|PREPARING state while it processes
// a leafentry.
void toku_txn_pin_live_txn_unlocked(TOKUTXN txn) {
    assert(txn->state == TOKUTXN_LIVE || txn->state == TOKUTXN_PREPARING);
    assert(!toku_txn_is_read_only(txn));
    txn->num_pin++;
}

// allows a client thread to go back to being able to transition txn
// from LIVE|PREPARING -> COMMITTING|ABORTING
void toku_txn_unpin_live_txn(TOKUTXN txn) {
    assert(txn->state == TOKUTXN_LIVE || txn->state == TOKUTXN_PREPARING);
    assert(txn->num_pin > 0);
    toku_txn_lock_state(txn);
    txn->num_pin--;
    if (txn->num_pin == 0) {
        toku_cond_broadcast(&txn->state_cond);
    }
    toku_txn_unlock_state(txn);
}

bool toku_txn_has_spilled_rollback(TOKUTXN txn) {
    return txn_has_spilled_rollback_logs(txn);
}

uint64_t toku_txn_get_client_id(TOKUTXN txn) {
    return txn->client_id;
}

void toku_txn_set_client_id(TOKUTXN txn, uint64_t client_id) {
    txn->client_id = client_id;
}

time_t toku_txn_get_start_time(struct tokutxn *txn) {
    return txn->start_time;
}

int toku_txn_reads_txnid(TXNID txnid, TOKUTXN txn) {
    int r = 0;
    TXNID oldest_live_in_snapshot = toku_get_oldest_in_live_root_txn_list(txn);
    if (oldest_live_in_snapshot == TXNID_NONE && txnid < txn->snapshot_txnid64) {
        r = TOKUDB_ACCEPT;
    } else if (txnid < oldest_live_in_snapshot || txnid == txn->txnid.parent_id64) {
        r = TOKUDB_ACCEPT;
    } else if (txnid > txn->snapshot_txnid64 || toku_is_txn_in_live_root_txn_list(*txn->live_root_txn_list, txnid)) {
        r = 0;
    } else {
        r = TOKUDB_ACCEPT;
    }
    return r;
}

int toku_txn_discard_txn(TOKUTXN txn) {
    int r = toku_rollback_discard(txn);
    return r;
}

#include <toku_race_tools.h>
void __attribute__((__constructor__)) toku_txn_status_helgrind_ignore(void);
void toku_txn_status_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&txn_status, sizeof txn_status);
}

#undef STATUS_VALUE
