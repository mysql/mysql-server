/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_WRITE_H)
#define TOKU_YDB_WRITE_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    YDB_LAYER_NUM_INSERTS = 0,
    YDB_LAYER_NUM_INSERTS_FAIL,
    YDB_LAYER_NUM_DELETES,
    YDB_LAYER_NUM_DELETES_FAIL,
    YDB_LAYER_NUM_UPDATES,
    YDB_LAYER_NUM_UPDATES_FAIL,
    YDB_LAYER_NUM_UPDATES_BROADCAST,
    YDB_LAYER_NUM_UPDATES_BROADCAST_FAIL,
    YDB_LAYER_NUM_MULTI_INSERTS,
    YDB_LAYER_NUM_MULTI_INSERTS_FAIL,
    YDB_LAYER_NUM_MULTI_DELETES,
    YDB_LAYER_NUM_MULTI_DELETES_FAIL,
    YDB_LAYER_NUM_MULTI_UPDATES,
    YDB_LAYER_NUM_MULTI_UPDATES_FAIL,
    YDB_WRITE_LAYER_STATUS_NUM_ROWS              /* number of rows in this status array */
} ydb_write_lock_layer_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_WRITE_LAYER_STATUS_NUM_ROWS];
} YDB_WRITE_LAYER_STATUS_S, *YDB_WRITE_LAYER_STATUS;

void ydb_write_layer_get_status(YDB_WRITE_LAYER_STATUS statp);


int toku_db_del(DB *db, DB_TXN *txn, DBT *key, u_int32_t flags, BOOL holds_ydb_lock);
int toku_db_put(DB *db, DB_TXN *txn, DBT *key, DBT *val, u_int32_t flags, BOOL holds_ydb_lock);
int autotxn_db_del(DB* db, DB_TXN* txn, DBT* key, u_int32_t flags);
int autotxn_db_put(DB* db, DB_TXN* txn, DBT* key, DBT* data, u_int32_t flags);
int autotxn_db_update(DB *db, DB_TXN *txn, const DBT *key, const DBT *update_function_extra, u_int32_t flags);
int autotxn_db_update_broadcast(DB *db, DB_TXN *txn, const DBT *update_function_extra, u_int32_t flags);
int env_put_multiple(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn, 
    const DBT *src_key, const DBT *src_val, 
    uint32_t num_dbs, 
    DB **db_array, 
    DBT *keys, DBT *vals, 
    uint32_t *flags_array
    );
int env_del_multiple(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn, 
    const DBT *src_key, 
    const DBT *src_val, 
    uint32_t num_dbs, 
    DB **db_array, 
    DBT *keys, 
    uint32_t *flags_array
    ); 
int env_update_multiple(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn,                                
    DBT *old_src_key, DBT *old_src_data,
    DBT *new_src_key, DBT *new_src_data,
    uint32_t num_dbs, 
    DB **db_array, 
    uint32_t* flags_array, 
    uint32_t num_keys, DBT keys[], 
    uint32_t num_vals, DBT vals[]
    );



#if defined(__cplusplus)
}
#endif

#endif
