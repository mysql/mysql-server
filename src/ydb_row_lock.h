/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_ROWLOCK_H)
#define TOKU_YDB_ROWLOCK_H

#if defined(__cplusplus)
extern "C" {
#endif

int
get_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key, toku_lock_type lock_type);

int
start_range_lock(DB *db, DB_TXN *txn, const DBT *left_key, const DBT *right_key, toku_lock_type lock_type, toku_lock_request *lock_request);

int 
get_point_write_lock(DB *db, DB_TXN *txn, const DBT *key);

int
toku_grab_write_lock (DB *db, DBT *key, TOKUTXN tokutxn);


#if defined(__cplusplus)
}
#endif

#endif
