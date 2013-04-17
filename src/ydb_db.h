/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_DB_H)
#define TOKU_YDB_DB_H

#include "ydb_txn.h"


typedef enum {
    YDB_LAYER_DIRECTORY_WRITE_LOCKS = 0,        /* total directory write locks taken */
    YDB_LAYER_DIRECTORY_WRITE_LOCKS_FAIL,   /* total directory write locks unable to be taken */
    YDB_LAYER_LOGSUPPRESS,                  /* number of times logs are suppressed for empty table (2440) */
    YDB_LAYER_LOGSUPPRESS_FAIL,             /* number of times unable to suppress logs for empty table (2440) */
    YDB_DB_LAYER_STATUS_NUM_ROWS              /* number of rows in this status array */
} ydb_db_lock_layer_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_DB_LAYER_STATUS_NUM_ROWS];
} YDB_DB_LAYER_STATUS_S, *YDB_DB_LAYER_STATUS;

void ydb_db_layer_get_status(YDB_DB_LAYER_STATUS statp);


/* db methods */
static inline int db_opened(DB *db) {
    return db->i->opened != 0;
}

static inline toku_dbt_cmp 
toku_db_get_compare_fun(DB* db) {
    return db->i->ft_handle->ft->compare_fun;
}

int toku_db_pre_acquire_fileops_lock(DB *db, DB_TXN *txn);
int db_open_iname(DB * db, DB_TXN * txn, const char *iname, uint32_t flags, int mode);
int toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn);
int toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, uint32_t flags);
int toku_db_create(DB ** db, DB_ENV * env, uint32_t flags);
int toku_db_close(DB * db);
int toku_setup_db_internal (DB **dbp, DB_ENV *env, uint32_t flags, FT_HANDLE brt, bool is_open);
int db_getf_set(DB *db, DB_TXN *txn, uint32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
int autotxn_db_get(DB* db, DB_TXN* txn, DBT* key, DBT* data, uint32_t flags);

//TODO: DB_AUTO_COMMIT.
//TODO: Nowait only conditionally?
//TODO: NOSYNC change to SYNC if DB_ENV has something in set_flags
static inline int 
toku_db_construct_autotxn(DB* db, DB_TXN **txn, bool* changed, bool force_auto_commit) {
    assert(db && txn && changed);
    DB_ENV* env = db->dbenv;
    if (*txn || !(env->i->open_flags & DB_INIT_TXN)) {
        *changed = false;
        return 0;
    }
    bool nosync = (bool)(!force_auto_commit && !(env->i->open_flags & DB_AUTO_COMMIT));
    uint32_t txn_flags = DB_TXN_NOWAIT | (nosync ? DB_TXN_NOSYNC : 0);
    int r = toku_txn_begin(env, NULL, txn, txn_flags);
    if (r!=0) return r;
    *changed = true;
    return 0;
}

static inline int 
toku_db_destruct_autotxn(DB_TXN *txn, int r, bool changed) {
    if (!changed) return r;
    if (r==0) {
        r = locked_txn_commit(txn, 0);
    }
    else {
        locked_txn_abort(txn);
    }
    return r; 
}




#endif
