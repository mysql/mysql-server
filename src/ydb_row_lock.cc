/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
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

static int find_key_ranges_by_lt(const txn_lt_key_ranges &ranges,
        toku::locktree *const &find_lt) {
    return ranges.lt->compare(find_lt);
}

static void db_txn_note_row_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key) {
    toku::locktree *lt = db->i->lt;

    toku_mutex_lock(&db_txn_struct_i(txn)->txn_mutex);

    uint32_t idx;
    txn_lt_key_ranges ranges;
    toku::omt<txn_lt_key_ranges> *map = &db_txn_struct_i(txn)->lt_map;

    // if this txn has not yet already referenced this
    // locktree, then add it to this txn's locktree map
    int r = map->find_zero<toku::locktree *, find_key_ranges_by_lt>(lt, &ranges, &idx);
    if (r == DB_NOTFOUND) {
        ranges.lt = lt;
        XMALLOC(ranges.buffer);
        ranges.buffer->create();
        map->insert_at(ranges, idx);

        // let the manager know we're referencing this lt
        toku::locktree::manager *ltm = &txn->mgrp->i->ltm;
        ltm->reference_lt(lt);
    } else {
        invariant_zero(r);
    }

    // add a new lock range to this txn's row lock buffer
    ranges.buffer->append(left_key, right_key);

    toku_mutex_unlock(&db_txn_struct_i(txn)->txn_mutex);
}


// Get a range lock.
// Return when the range lock is acquired or the default lock tree timeout has expired.  
int toku_db_get_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key,
        toku::lock_request::type lock_type) {
    uint64_t wait_time = txn->mgrp->i->ltm.get_lock_wait_time();
    toku::lock_request request;
    request.create(wait_time);

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
    request->set(db->i->lt, txn_anc_id, left_key, right_key, lock_type);

    int r = request->start();
    if (r == 0) {
        db_txn_note_row_lock(db, txn_anc, left_key, right_key);
    }
    return r;
}

// Complete a lock request by waiting until the request is ready
// and then storing the acquired lock if successful.
int toku_db_wait_range_lock(DB *db, DB_TXN *txn, toku::lock_request *request) {
    int r = request->wait();
    if (r == 0) {
        DB_TXN *txn_anc = txn_oldest_ancester(txn);
        const DBT *left_key = request->get_left_key();
        const DBT *right_key = request->get_right_key();
        db_txn_note_row_lock(db, txn_anc, left_key, right_key);
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
    const uint64_t lock_wait_time = 0;
    toku::lock_request request;

    request.create(lock_wait_time);
    request.set(db->i->lt, txn_anc_id, key, key, toku::lock_request::type::WRITE);
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
    ranges->buffer->destroy();
    toku_free(ranges->buffer);

    // all of our locks have been released, so first try to wake up
    // pending lock requests, then release our reference on the lt
    toku::lock_request::retry_all_lock_requests(lt);

    // Release our reference on this locktree
    toku::locktree::manager *ltm = &txn->mgrp->i->ltm;
    ltm->release_lt(lt);
}
