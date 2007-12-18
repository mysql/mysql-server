#include "db_cxx.h"

int DbTxn::commit (u_int32_t flags) {
    DB_TXN *txn = get_DB_TXN();
    int ret = txn->commit(txn, flags);
    return ret;
}
