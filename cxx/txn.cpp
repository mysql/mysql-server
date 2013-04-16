/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

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
