#include "le_cursor.h"

static inline int le_cursor_create_db_txn(LE_CURSOR *le_cursor_result, DB *db, DB_TXN *txn) {
    return le_cursor_create(le_cursor_result, db_struct_i(db)->brt, db_txn_struct_i(txn)->tokutxn);
}

