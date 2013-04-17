/* -*- mode: C; c-basic-offset: 4 -*- */
/*
 * Copyright (c) 2010 Tokutek Inc.  All rights reserved." 
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
 */

#ifndef TOKU_INDEXER_INTERNAL_H
#define TOKU_INDEXER_INTERNAL_H

struct __toku_indexer_internal {
    DB_ENV *env;
    DB_TXN *txn;
    DB *src_db;
    int N;
    DB **dest_dbs; /* [N] */
    uint32_t indexer_flags;
    void (*error_callback)(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra);
    void *error_extra;
    int  (*poll_func)(void *poll_extra, float progress);
    void *poll_extra;
    uint64_t estimated_rows; // current estimate of table size
    uint64_t loop_mod;       // how often to call poll_func
    LE_CURSOR lec;
    FILENUM  *fnums; /* [N] */
    FILENUMS filenums;

    // test functions
    int (*undo_do)(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule);
    int (*test_is_xid_live)(DB_INDEXER *indexer, TXNID xid);
    int (*test_maybe_lock_provisional_key)(DB_INDEXER *indexer, TXNID xid, DB *hotdb, DBT *key);
    int (*test_delete_provisional)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
    int (*test_delete_committed)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
    int (*test_insert_provisional)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
    int (*test_insert_committed)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
    int (*test_commit_any)(DB_INDEXER *indexer, DB *db, DBT *key, XIDS xids);

    // test flags
    int test_only_flags;
};

int indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule);

#endif
