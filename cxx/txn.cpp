#include <db_cxx.h>

DbTxn::DbTxn(DB_TXN *txn)
    :  the_txn(txn)
{
    txn->api_internal = this;
}

int DbTxn::commit (u_int32_t flags) {
    DB_TXN *txn = get_DB_TXN();
    int ret = txn->commit(txn, flags);
    return ret;
}
