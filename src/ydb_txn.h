/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_TXN_H)
#define TOKU_YDB_TXN_H


// begin, commit, and abort use the multi operation lock 
// internally to synchronize with begin checkpoint. callers
// should not hold the multi operation lock.

int toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, uint32_t flags);

int locked_txn_commit(DB_TXN *txn, uint32_t flags);

int locked_txn_abort(DB_TXN *txn);

void toku_keep_prepared_txn_callback(DB_ENV *env, TOKUTXN tokutxn);

// Test-only function
extern "C" void toku_increase_last_xid(DB_ENV *env, uint64_t increment) __attribute__((__visibility__("default")));

#endif
