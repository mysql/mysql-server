// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_CURSOR_H)
#define TOKU_YDB_CURSOR_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    YDB_C_LAYER_NUM_POINT_QUERIES = 0,
    YDB_C_LAYER_NUM_SEQUENTIAL_QUERIES,
    YDB_C_LAYER_STATUS_NUM_ROWS              /* number of rows in this status array */
} ydb_c_lock_layer_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_C_LAYER_STATUS_NUM_ROWS];
} YDB_C_LAYER_STATUS_S, *YDB_C_LAYER_STATUS;

void ydb_c_layer_get_status(YDB_C_LAYER_STATUS statp);

int toku_c_get(DBC * c, DBT * key, DBT * data, u_int32_t flag);
int toku_c_getf_set(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
int toku_c_close(DBC * c);
int toku_db_cursor_internal(DB *db, DB_TXN * txn, DBC **c, u_int32_t flags, int is_temporary_cursor);
int toku_db_cursor(DB *db, DB_TXN *txn, DBC **c, u_int32_t flags);


#if defined(__cplusplus)
}
#endif

#endif
