/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include <db.h>
#include "ydb-internal.h"
#include "ydb_row_lock.h"
#include <lock_tree/lth.h>

static int 
toku_txn_add_lt(DB_TXN* txn, toku_lock_tree* lt) {
    int r = ENOSYS;
    assert(txn && lt);
    toku_mutex_lock(&db_txn_struct_i(txn)->txn_mutex);
    toku_lth* lth = db_txn_struct_i(txn)->lth;
    // we used to initialize the transaction's lth during begin.
    // Now we initialize the lth only if the transaction needs the lth, here
    if (!lth) {
        r = toku_lth_create(&db_txn_struct_i(txn)->lth);
        assert_zero(r);
        lth = db_txn_struct_i(txn)->lth;
    }

    toku_lock_tree* find = toku_lth_find(lth, lt);
    if (find) {
        assert(find == lt);
        r = 0;
        goto cleanup;
    }
    r = toku_lth_insert(lth, lt);
    if (r != 0) { goto cleanup; }
    
    toku_lt_add_ref(lt);
    r = 0;
cleanup:
    toku_mutex_unlock(&db_txn_struct_i(txn)->txn_mutex);
    return r;
}

/*
    Used for partial implementation of nested transactions.
    Work is done by children as normal, but all locking is done by the
    root of the nested txn tree.
    This may hold extra locks, and will not work as expected when
    a node has two non-completed txns at any time.
*/
static inline DB_TXN* 
toku_txn_ancestor(DB_TXN* txn) {
    while (txn && txn->parent) txn = txn->parent;

    return txn;
}

// Get a range lock.
// Return when the range lock is acquired or the default lock tree timeout has expired.  
// The ydb mutex must be held when called and may be released when waiting in the lock tree.
int
get_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key, toku_lock_type lock_type) {
    int r;
    DB_TXN *txn_anc = toku_txn_ancestor(txn);
    r = toku_txn_add_lt(txn_anc, db->i->lt);
    if (r == 0) {
        TXNID txn_anc_id = toku_txn_get_txnid(db_txn_struct_i(txn_anc)->tokutxn);
        toku_lock_request lock_request;
        toku_lock_request_init(&lock_request, txn_anc_id, left_key, right_key, lock_type);
        r = toku_lt_acquire_lock_request_with_default_timeout(db->i->lt, &lock_request);
        toku_lock_request_destroy(&lock_request);
    }
    return r;
}

// Setup and start an asynchronous lock request.
int
start_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key, toku_lock_type lock_type, toku_lock_request *lock_request) {
    int r;
    DB_TXN *txn_anc = toku_txn_ancestor(txn);
    r = toku_txn_add_lt(txn_anc, db->i->lt);
    if (r == 0) {
        TXNID txn_anc_id = toku_txn_get_txnid(db_txn_struct_i(txn_anc)->tokutxn);
        toku_lock_request_set(lock_request, txn_anc_id, left_key, right_key, lock_type);
        r = toku_lock_request_start(lock_request, db->i->lt, true);
    }
    return r;
}

int 
get_point_write_lock(DB *db, DB_TXN *txn, const DBT *key) {
    int r = get_range_lock(db, txn, key, key, LOCK_REQUEST_WRITE);
    return r;
}

// acquire a point write lock on the key for a given txn.
// this does not block the calling thread.
int
toku_grab_write_lock (DB *db, DBT *key, TOKUTXN tokutxn) {
    DB_TXN *txn = toku_txn_get_container_db_txn(tokutxn);
    DB_TXN *txn_anc = toku_txn_ancestor(txn);
    int r = toku_txn_add_lt(txn_anc, db->i->lt);
    if (r == 0) {
        TXNID txn_anc_id = toku_txn_get_txnid(db_txn_struct_i(txn_anc)->tokutxn);
        r = toku_lt_acquire_write_lock(db->i->lt, txn_anc_id, key);
    }
    return r;
}


