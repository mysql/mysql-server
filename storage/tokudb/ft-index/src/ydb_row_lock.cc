/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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
#ident "$Id$"

#include <db.h>

#include <locktree/lock_request.h>

#include "ydb-internal.h"
#include "ydb_txn.h"
#include "ydb_row_lock.h"

/*
    Used for partial implementation of nested transactions.
    Work is done by children as normal, but all locking is done by the
    root of the nested txn tree.
    This may hold extra locks, and will not work as expected when
    a node has two non-completed txns at any time.
*/
static DB_TXN *txn_oldest_ancester(DB_TXN* txn) {
    while (txn && txn->parent) {
        txn = txn->parent;
    }
    return txn;
}

int find_key_ranges_by_lt(const txn_lt_key_ranges &ranges,
        const toku::locktree *const &find_lt);
int find_key_ranges_by_lt(const txn_lt_key_ranges &ranges,
        const toku::locktree *const &find_lt) {
    return ranges.lt->compare(find_lt);
}

static void db_txn_note_row_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key) {
    const toku::locktree *lt = db->i->lt;

    toku_mutex_lock(&db_txn_struct_i(txn)->txn_mutex);

    uint32_t idx;
    txn_lt_key_ranges ranges;
    toku::omt<txn_lt_key_ranges> *map = &db_txn_struct_i(txn)->lt_map;

    // if this txn has not yet already referenced this
    // locktree, then add it to this txn's locktree map
    int r = map->find_zero<const toku::locktree *, find_key_ranges_by_lt>(lt, &ranges, &idx);
    if (r == DB_NOTFOUND) {
        ranges.lt = db->i->lt;
        XMALLOC(ranges.buffer);
        ranges.buffer->create();
        map->insert_at(ranges, idx);

        // let the manager know we're referencing this lt
        toku::locktree_manager *ltm = &txn->mgrp->i->ltm;
        ltm->reference_lt(ranges.lt);
    } else {
        invariant_zero(r);
    }

    // add a new lock range to this txn's row lock buffer
    size_t old_num_bytes = ranges.buffer->get_num_bytes();
    ranges.buffer->append(left_key, right_key);
    size_t new_num_bytes = ranges.buffer->get_num_bytes();
    invariant(new_num_bytes > old_num_bytes);
    lt->get_manager()->note_mem_used(new_num_bytes - old_num_bytes);

    toku_mutex_unlock(&db_txn_struct_i(txn)->txn_mutex);
}

void toku_db_txn_escalate_callback(TXNID txnid, const toku::locktree *lt, const toku::range_buffer &buffer, void *extra) {
    DB_ENV *CAST_FROM_VOIDP(env, extra);

    // Get the TOKUTXN and DB_TXN for this txnid from the environment's txn manager.
    // Only the parent id is used in the search.
    TOKUTXN ttxn;
    TXNID_PAIR txnid_pair = { .parent_id64 = txnid, .child_id64 = 0 };
    TXN_MANAGER txn_manager = toku_logger_get_txn_manager(env->i->logger);

    toku_txn_manager_suspend(txn_manager);
    toku_txn_manager_id2txn_unlocked(txn_manager, txnid_pair, &ttxn);

    // We are still holding the txn manager lock. If we couldn't find the txn,
    // then we lost a race with a committing transaction that got removed
    // from the txn manager before it released its locktree locks. In this
    // case we do nothing - that transaction has or is just about to release
    // its locks and be gone, so there's not point in updating its lt_map
    // with the new escalated ranges. It will go about releasing the old
    // locks it thinks it had, and will succeed as if nothing happened.
    //
    // If we did find the transaction, then it has not yet been removed
    // from the manager and therefore has not yet released its locks.
    // We must try to replace the range buffer associated with this locktree,
    // if it exists. This is impotant, otherwise it can grow out of
    // control (ticket 5961).

    if (ttxn != nullptr) {
        DB_TXN *txn = toku_txn_get_container_db_txn(ttxn);

        // One subtle point is that if the transaction is still live, it is impossible
        // to deadlock on the txn mutex, even though we are holding the locktree's root
        // mutex and release locks takes them in the opposite order.
        //
        // Proof: releasing locks takes the txn mutex and then acquires the locktree's
        // root mutex, escalation takes the root mutex and possibly takes the txn mutex.
        // releasing locks implies the txn is not live, and a non-live txn implies we
        // will not need to take the txn mutex, so the deadlock is avoided.
        toku_mutex_lock(&db_txn_struct_i(txn)->txn_mutex);

        uint32_t idx;
        txn_lt_key_ranges ranges;
        toku::omt<txn_lt_key_ranges> *map = &db_txn_struct_i(txn)->lt_map;
        int r = map->find_zero<const toku::locktree *, find_key_ranges_by_lt>(lt, &ranges, &idx);
        if (r == 0) {
            // Destroy the old range buffer, create a new one, and insert the new ranges.
            //
            // We could theoretically steal the memory from the caller instead of copying
            // it, but it's simpler to have a callback API that doesn't transfer memory ownership.
            lt->get_manager()->note_mem_released(ranges.buffer->get_num_bytes());
            ranges.buffer->destroy();
            ranges.buffer->create();
            toku::range_buffer::iterator iter;
            toku::range_buffer::iterator::record rec;
            iter.create(&buffer);
            while (iter.current(&rec)) {
                ranges.buffer->append(rec.get_left_key(), rec.get_right_key());
                iter.next();
            }
            lt->get_manager()->note_mem_used(ranges.buffer->get_num_bytes());
        } else {
            // In rare cases, we may not find the associated locktree, because we are
            // racing with the transaction trying to add this locktree to the lt map
            // after acquiring its first lock. The escalated lock set must be the single
            // lock that this txnid just acquired. Do nothing here and let the txn
            // take care of adding this locktree and range to its lt map as usual.
            invariant(buffer.get_num_ranges() == 1);
        }

        toku_mutex_unlock(&db_txn_struct_i(txn)->txn_mutex);
    }

    toku_txn_manager_resume(txn_manager);
}

// Get a range lock.
// Return when the range lock is acquired or the default lock tree timeout has expired.  
int toku_db_get_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key,
        toku::lock_request::type lock_type) {
    toku::lock_request request;
    request.create();
    int r = toku_db_start_range_lock(db, txn, left_key, right_key, lock_type, &request);
    if (r == DB_LOCK_NOTGRANTED) {
        r = toku_db_wait_range_lock(db, txn, &request);
    }

    request.destroy();
    return r;
}

// Setup and start an asynchronous lock request.
int toku_db_start_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key,
        toku::lock_request::type lock_type, toku::lock_request *request) {
    DB_TXN *txn_anc = txn_oldest_ancester(txn);
    TXNID txn_anc_id = txn_anc->id64(txn_anc);
    request->set(db->i->lt, txn_anc_id, left_key, right_key, lock_type, toku_is_big_txn(txn_anc));

    const int r = request->start();
    if (r == 0) {
        db_txn_note_row_lock(db, txn_anc, left_key, right_key);
    } else if (r == DB_LOCK_DEADLOCK) {
        lock_timeout_callback callback = txn->mgrp->i->lock_wait_timeout_callback;
        if (callback != nullptr) {
            callback(db, txn_anc_id, left_key, right_key,
                     request->get_conflicting_txnid());
        }
    }
    return r;
}

// Complete a lock request by waiting until the request is ready
// and then storing the acquired lock if successful.
int toku_db_wait_range_lock(DB *db, DB_TXN *txn, toku::lock_request *request) {
    DB_TXN *txn_anc = txn_oldest_ancester(txn);
    const DBT *left_key = request->get_left_key();
    const DBT *right_key = request->get_right_key();
    DB_ENV *env = db->dbenv;
    uint64_t wait_time_msec = env->i->default_lock_timeout_msec;
    if (env->i->get_lock_timeout_callback)
        wait_time_msec = env->i->get_lock_timeout_callback(wait_time_msec);
    uint64_t killed_time_msec = env->i->default_killed_time_msec;
    if (env->i->get_killed_time_callback)
        killed_time_msec = env->i->get_killed_time_callback(killed_time_msec);
    const int r = request->wait(wait_time_msec, killed_time_msec, env->i->killed_callback);
    if (r == 0) {
        db_txn_note_row_lock(db, txn_anc, left_key, right_key);
    } else if (r == DB_LOCK_NOTGRANTED) {
        lock_timeout_callback callback = txn->mgrp->i->lock_wait_timeout_callback;
        if (callback != nullptr) {
            callback(db, txn_anc->id64(txn_anc), left_key, right_key,
                     request->get_conflicting_txnid());
        }
    }
    return r;
}

int toku_db_get_point_write_lock(DB *db, DB_TXN *txn, const DBT *key) {
    return toku_db_get_range_lock(db, txn, key, key, toku::lock_request::type::WRITE);
}

// acquire a point write lock on the key for a given txn.
// this does not block the calling thread.
void toku_db_grab_write_lock (DB *db, DBT *key, TOKUTXN tokutxn) {
    DB_TXN *txn = toku_txn_get_container_db_txn(tokutxn);
    DB_TXN *txn_anc = txn_oldest_ancester(txn);
    TXNID txn_anc_id = txn_anc->id64(txn_anc);

    // This lock request must succeed, so we do not want to wait
    toku::lock_request request;
    request.create();
    request.set(db->i->lt, txn_anc_id, key, key, toku::lock_request::type::WRITE, toku_is_big_txn(txn_anc));
    int r = request.start();
    invariant_zero(r);
    db_txn_note_row_lock(db, txn_anc, key, key);
}

void toku_db_release_lt_key_ranges(DB_TXN *txn, txn_lt_key_ranges *ranges) {
    toku::locktree *lt = ranges->lt;
    TXNID txnid = txn->id64(txn);

    // release all of the locks this txn has ever successfully
    // acquired and stored in the range buffer for this locktree
    lt->release_locks(txnid, ranges->buffer);
    lt->get_manager()->note_mem_released(ranges->buffer->get_num_bytes());
    ranges->buffer->destroy();
    toku_free(ranges->buffer);

    // all of our locks have been released, so first try to wake up
    // pending lock requests, then release our reference on the lt
    toku::lock_request::retry_all_lock_requests(lt);

    // Release our reference on this locktree
    toku::locktree_manager *ltm = &txn->mgrp->i->ltm;
    ltm->release_lt(lt);
}
