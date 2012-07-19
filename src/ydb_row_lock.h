/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_ROWLOCK_H)
#define TOKU_YDB_ROWLOCK_H


int
get_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key, toku_lock_type lock_type);

int
start_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key, toku_lock_type lock_type, toku_lock_request *lock_request);

int 
get_point_write_lock(DB *db, DB_TXN *txn, const DBT *key);

int
toku_grab_write_lock (DB *db, DBT *key, TOKUTXN tokutxn);



#endif
