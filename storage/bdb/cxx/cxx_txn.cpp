/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: cxx_txn.cpp,v 12.2 2005/06/16 20:21:03 bostic Exp $
 */

#include "db_config.h"

#include <errno.h>

#include "db_cxx.h"
#include "dbinc/cxx_int.h"

#include "db_int.h"
#include "dbinc/txn.h"

// Helper macro for simple methods that pass through to the
// underlying C method. It may return an error or raise an exception.
// Note this macro expects that input _argspec is an argument
// list element (e.g., "char *arg") and that _arglist is the arguments
// that should be passed through to the C method (e.g., "(db, arg)")
//
#define	DBTXN_METHOD(_name, _delete, _argspec, _arglist)		   \
int DbTxn::_name _argspec						   \
{									   \
	int ret;							   \
	DB_TXN *txn = unwrap(this);					   \
	DbEnv *dbenv = DbEnv::get_DbEnv(txn->mgrp->dbenv);		   \
									   \
	ret = txn->_name _arglist;					   \
	/* Weird, but safe if we don't access this again. */		   \
	if (_delete)							   \
		delete this;						   \
	if (!DB_RETOK_STD(ret))						   \
		DB_ERROR(dbenv, "DbTxn::" # _name, ret, ON_ERROR_UNKNOWN); \
	return (ret);							   \
}

// private constructor, never called but needed by some C++ linkers
DbTxn::DbTxn()
:	imp_(0)
{
}

DbTxn::DbTxn(DB_TXN *txn)
:	imp_(txn)
{
	txn->api_internal = this;
}

DbTxn::~DbTxn()
{
}

DBTXN_METHOD(abort, 1, (), (txn))
DBTXN_METHOD(commit, 1, (u_int32_t flags), (txn, flags))
DBTXN_METHOD(discard, 1, (u_int32_t flags), (txn, flags))

u_int32_t DbTxn::id()
{
	DB_TXN *txn;

	txn = unwrap(this);
	return (txn->id(txn));		// no error
}

DBTXN_METHOD(get_name, 0, (const char **namep), (txn, namep))
DBTXN_METHOD(prepare, 0, (u_int8_t *gid), (txn, gid))
DBTXN_METHOD(set_name, 0, (const char *name), (txn, name))
DBTXN_METHOD(set_timeout, 0, (db_timeout_t timeout, u_int32_t flags),
    (txn, timeout, flags))

// static method
DbTxn *DbTxn::wrap_DB_TXN(DB_TXN *txn)
{
	DbTxn *wrapped_txn = get_DbTxn(txn);
	return (wrapped_txn != NULL) ?  wrapped_txn : new DbTxn(txn);
}
