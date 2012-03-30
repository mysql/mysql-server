// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_DB_H)
#define TOKU_YDB_DB_H

#include "ydb_txn.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    YDB_LAYER_DIRECTORY_WRITE_LOCKS = 0,        /* total directory write locks taken */
    YDB_LAYER_DIRECTORY_WRITE_LOCKS_FAIL,   /* total directory write locks unable to be taken */
    YDB_LAYER_LOGSUPPRESS,                  /* number of times logs are suppressed for empty table (2440) */
    YDB_LAYER_LOGSUPPRESS_FAIL,             /* number of times unable to suppress logs for empty table (2440) */
    YDB_DB_LAYER_STATUS_NUM_ROWS              /* number of rows in this status array */
} ydb_db_lock_layer_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_DB_LAYER_STATUS_NUM_ROWS];
} YDB_DB_LAYER_STATUS_S, *YDB_DB_LAYER_STATUS;

void ydb_db_layer_get_status(YDB_DB_LAYER_STATUS statp);


/* db methods */
static inline int db_opened(DB *db) {
    return db->i->opened != 0;
}

static inline toku_dbt_cmp 
toku_db_get_compare_fun(DB* db) {
    return db->i->brt->compare_fun;
}

int toku_db_pre_acquire_fileops_lock(DB *db, DB_TXN *txn);
int db_open_iname(DB * db, DB_TXN * txn, const char *iname, u_int32_t flags, int mode);
int toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn, BOOL just_lock);
int toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags);
int toku_db_create(DB ** db, DB_ENV * env, u_int32_t flags);
int toku_db_close(DB * db, u_int32_t flags, bool oplsn_valid, LSN oplsn);
int db_close_before_brt(DB *db, u_int32_t UU(flags), bool oplsn_valid, LSN oplsn);
int toku_close_db_internal (DB * db, bool oplsn_valid, LSN oplsn);
int toku_setup_db_internal (DB **dbp, DB_ENV *env, u_int32_t flags, BRT brt, bool is_open);
int db_getf_set(DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
int autotxn_db_get(DB* db, DB_TXN* txn, DBT* key, DBT* data, u_int32_t flags);

//TODO: DB_AUTO_COMMIT.
//TODO: Nowait only conditionally?
//TODO: NOSYNC change to SYNC if DB_ENV has something in set_flags
static inline int 
toku_db_construct_autotxn(DB* db, DB_TXN **txn, BOOL* changed, BOOL force_auto_commit, BOOL holds_ydb_lock) {
    assert(db && txn && changed);
    DB_ENV* env = db->dbenv;
    if (*txn || !(env->i->open_flags & DB_INIT_TXN)) {
        *changed = FALSE;
        return 0;
    }
    BOOL nosync = (BOOL)(!force_auto_commit && !(env->i->open_flags & DB_AUTO_COMMIT));
    u_int32_t txn_flags = DB_TXN_NOWAIT | (nosync ? DB_TXN_NOSYNC : 0);
    int r = toku_txn_begin(env, NULL, txn, txn_flags, 1, holds_ydb_lock);
    if (r!=0) return r;
    *changed = TRUE;
    return 0;
}

static inline int 
toku_db_destruct_autotxn(DB_TXN *txn, int r, BOOL changed, BOOL holds_ydb_lock) {
    if (!changed) return r;
    if (!holds_ydb_lock) toku_ydb_lock();
    if (r==0) {
        r = toku_txn_commit(txn, 0, NULL, NULL, false);
    }
    else {
        toku_txn_abort(txn, NULL, NULL, false);
    }
    if (!holds_ydb_lock) toku_ydb_unlock();    
    return r; 
}



#if defined(__cplusplus)
}
#endif

#endif
