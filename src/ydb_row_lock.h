/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_YDB_ROW_LOCK_H
#define TOKU_YDB_ROW_LOCK_H

#include <ydb-internal.h>

#include <locktree/lock_request.h>

// Expose the escalate callback to ydb.cc,
// so it can pass the function pointer to the locktree
void toku_db_txn_escalate_callback(TXNID txnid, const toku::locktree *lt, const toku::range_buffer &buffer, void *extra);

int toku_db_get_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key,
        toku::lock_request::type lock_type);

int toku_db_start_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key,
        toku::lock_request::type lock_type, toku::lock_request *lock_request);

int toku_db_wait_range_lock(DB *db, DB_TXN *txn, toku::lock_request *lock_request);

int toku_db_get_point_write_lock(DB *db, DB_TXN *txn, const DBT *key);

void toku_db_grab_write_lock(DB *db, DBT *key, TOKUTXN tokutxn);

void toku_db_release_lt_key_ranges(DB_TXN *txn, txn_lt_key_ranges *ranges);

#endif /* TOKU_YDB_ROW_LOCK_H */
