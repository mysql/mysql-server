/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <db_cxx.h>

DbTxn::DbTxn(DB_TXN *txn)
    :  the_txn(txn)
{
    txn->api_internal = this;
}

DbTxn::~DbTxn()
{
    if (the_txn) {
	the_txn->abort(0);
    }
}

int DbTxn::commit (u_int32_t flags) {
    DbEnv *env = (DbEnv*)the_txn->mgrp->api1_internal; // Must grab the env before committing the txn (because that releases the txn memory.)
    int ret = the_txn->commit(the_txn, flags);
    the_txn=0; // So we don't touch it my mistake.
    return env->maybe_throw_error(ret);
}

int DbTxn::abort () {
    DbEnv *env = (DbEnv*)the_txn->mgrp->api1_internal; // Must grab the env before committing the txn (because that releases the txn memory.)
    int ret = the_txn->abort(the_txn);
    the_txn=0; // So we don't touch it my mistake.
    return env->maybe_throw_error(ret);
}
