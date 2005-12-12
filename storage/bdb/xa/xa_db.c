/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: xa_db.c,v 12.4 2005/10/20 18:57:16 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/txn.h"

static int __xa_close __P((DB *, u_int32_t));
static int __xa_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
static int __xa_del __P((DB *, DB_TXN *, DBT *, u_int32_t));
static int __xa_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int __xa_open __P((DB *, DB_TXN *,
	    const char *, const char *, DBTYPE, u_int32_t, int));
static int __xa_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int __xa_set_txn __P((DB *, DB_TXN **, int));
static int __xa_truncate __P((DB *, DB_TXN *, u_int32_t *, u_int32_t));

typedef struct __xa_methods {
	int (*close) __P((DB *, u_int32_t));
	int (*cursor) __P((DB *, DB_TXN *, DBC **, u_int32_t));
	int (*del) __P((DB *, DB_TXN *, DBT *, u_int32_t));
	int (*get) __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
	int (*open) __P((DB *, DB_TXN *,
	    const char *, const char *, DBTYPE, u_int32_t, int));
	int (*put) __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
	int (*truncate) __P((DB *, DB_TXN *, u_int32_t *, u_int32_t));
} XA_METHODS;

/*
 * __xa_set_txn --
 *	Find a transaction handle.
 */
static int
__xa_set_txn(dbp, txnpp, no_xa_txn)
	DB *dbp;
	DB_TXN **txnpp;
	int no_xa_txn;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * It doesn't make sense for a server to specify a DB_TXN handle.
	 * As the server can't know if other operations it has done have
	 * committed/aborted, it can self-deadlock.  If the server wants
	 * other transactions, it can open other DB handles and use them.
	 * Disallow specified DB_TXN handles.
	 */
	if (*txnpp != NULL) {
		__db_err(dbenv,
    "transaction handles should not be directly specified to XA interfaces");
		return (EINVAL);
	}

	/* See if the TM has declared a transaction. */
	if ((ret = __xa_get_txn(dbenv, txnpp, 0)) != 0)
		return (ret);
	if ((*txnpp)->txnid != TXN_INVALID)
		return (0);

	/*
	 * We may be opening databases in the server initialization routine.
	 * In that case, it's reasonable not to have an XA transaction.  It's
	 * also reasonable to open a database as part of an XA transaction,
	 * allow both.
	 */
	if (no_xa_txn) {
		*txnpp = NULL;
		return (0);
	}

	__db_err(dbenv, "no XA transaction declared");
	return (EINVAL);
}

/*
 * __db_xa_create --
 *	DB XA constructor.
 *
 * PUBLIC: int __db_xa_create __P((DB *));
 */
int
__db_xa_create(dbp)
	DB *dbp;
{
	XA_METHODS *xam;
	int ret;

	/*
	 * Allocate the XA internal structure, and wrap the open and close
	 * calls.
	 */
	if ((ret = __os_calloc(dbp->dbenv, 1, sizeof(XA_METHODS), &xam)) != 0)
		return (ret);

	dbp->xa_internal = xam;
	xam->open = dbp->open;
	dbp->open = __xa_open;
	xam->close = dbp->close;
	dbp->close = __xa_close;

	return (0);
}

/*
 * __xa_open --
 *	XA open wrapper.
 */
static int
__xa_open(dbp, txn, name, subdb, type, flags, mode)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	XA_METHODS *xam;
	int ret;

	xam = (XA_METHODS *)dbp->xa_internal;

	if ((ret =
	    __xa_set_txn(dbp, &txn, LF_ISSET(DB_AUTO_COMMIT) ? 1 : 0)) != 0)
		return (ret);
	if ((ret = xam->open(dbp, txn, name, subdb, type, flags, mode)) != 0)
		return (ret);

	/* Wrap any DB handle method that takes a TXN ID as an argument. */
	xam->cursor = dbp->cursor;
	xam->del = dbp->del;
	xam->get = dbp->get;
	xam->put = dbp->put;
	xam->truncate = dbp->truncate;
	dbp->cursor = __xa_cursor;
	dbp->del = __xa_del;
	dbp->get = __xa_get;
	dbp->put = __xa_put;
	dbp->truncate = __xa_truncate;

	return (0);
}

static int
__xa_cursor(dbp, txn, dbcp, flags)
	DB *dbp;
	DB_TXN *txn;
	DBC **dbcp;
	u_int32_t flags;
{
	int ret;

	if ((ret = __xa_set_txn(dbp, &txn, 0)) != 0)
		return (ret);
	return (((XA_METHODS *)
	    dbp->xa_internal)->cursor(dbp, txn, dbcp, flags));
}

static int
__xa_del(dbp, txn, key, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	int ret;

	if ((ret = __xa_set_txn(dbp, &txn, 0)) != 0)
		return (ret);
	return (((XA_METHODS *)dbp->xa_internal)->del(dbp, txn, key, flags));
}

static int
__xa_close(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	int (*real_close) __P((DB *, u_int32_t));

	real_close = ((XA_METHODS *)dbp->xa_internal)->close;

	__os_free(dbp->dbenv, dbp->xa_internal);
	dbp->xa_internal = NULL;

	return (real_close(dbp, flags));
}

static int
__xa_get(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	int ret;

	if ((ret = __xa_set_txn(dbp, &txn, 0)) != 0)
		return (ret);
	return (((XA_METHODS *)
	    dbp->xa_internal)->get(dbp, txn, key, data, flags));
}

static int
__xa_put(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	int ret;

	if ((ret = __xa_set_txn(dbp, &txn, 0)) != 0)
		return (ret);
	return (((XA_METHODS *)
	    dbp->xa_internal)->put(dbp, txn, key, data, flags));
}

static int
__xa_truncate(dbp, txn, countp, flags)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t *countp, flags;
{
	int ret;

	if ((ret = __xa_set_txn(dbp, &txn, 0)) != 0)
		return (ret);
	return (((XA_METHODS *)
	    dbp->xa_internal)->truncate(dbp, txn, countp, flags));
}
