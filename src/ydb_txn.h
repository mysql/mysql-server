/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_TXN_H)
#define TOKU_YDB_TXN_H

#if defined(__cplusplus)
extern "C" {
#endif

// begin, commit, and abort use the multi operation lock 
// internally to synchronize with begin checkpoint. callers
// should not hold the multi operation lock.

int locked_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags);

int locked_txn_commit(DB_TXN *txn, u_int32_t flags);

int locked_txn_abort(DB_TXN *txn);

void toku_keep_prepared_txn_callback(DB_ENV *env, TOKUTXN tokutxn);

#if defined(__cplusplus)
}
#endif

#endif
