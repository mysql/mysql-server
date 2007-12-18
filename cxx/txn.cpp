#include <assert.h>
#include <db_cxx.h>

DbTxn::DbTxn(DB_TXN *txn)
    :  the_txn(txn)
{
    txn->api_internal = this;
}

