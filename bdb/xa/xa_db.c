/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: xa_db.c,v 11.9 2000/09/06 18:57:59 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "xa.h"
#include "xa_ext.h"
#include "txn.h"

static int __xa_close __P((DB *, u_int32_t));
static int __xa_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
static int __xa_del __P((DB *, DB_TXN *, DBT *, u_int32_t));
static int __xa_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int __xa_open __P((DB *,
	    const char *, const char *, DBTYPE, u_int32_t, int));
static int __xa_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));

typedef struct __xa_methods {
	int (*close) __P((DB *, u_int32_t));
	int (*cursor) __P((DB *, DB_TXN *, DBC **, u_int32_t));
	int (*del) __P((DB *, DB_TXN *, DBT *, u_int32_t));
	int (*get) __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
	int (*open) __P((DB *,
	    const char *, const char *, DBTYPE, u_int32_t, int));
	int (*put) __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
} XA_METHODS;

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
	 * Interpose XA routines in front of any method that takes a TXN
	 * ID as an argument.
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
__xa_open(dbp, name, subdb, type, flags, mode)
	DB *dbp;
	const char *name, *subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	XA_METHODS *xam;
	int ret;

	xam = (XA_METHODS *)dbp->xa_internal;

	if ((ret = xam->open(dbp, name, subdb, type, flags, mode)) != 0)
		return (ret);

	xam->cursor = dbp->cursor;
	xam->del = dbp->del;
	xam->get = dbp->get;
	xam->put = dbp->put;
	dbp->cursor = __xa_cursor;
	dbp->del = __xa_del;
	dbp->get = __xa_get;
	dbp->put = __xa_put;

	return (0);
}

static int
__xa_cursor(dbp, txn, dbcp, flags)
	DB *dbp;
	DB_TXN *txn;
	DBC **dbcp;
	u_int32_t flags;
{
	DB_TXN *t;

	t = txn != NULL && txn == dbp->open_txn ? txn : dbp->dbenv->xa_txn;
	if (t->txnid == TXN_INVALID)
		t = NULL;

	return (((XA_METHODS *)dbp->xa_internal)->cursor (dbp, t, dbcp, flags));
}

static int
__xa_del(dbp, txn, key, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	DB_TXN *t;

	t = txn != NULL && txn == dbp->open_txn ? txn : dbp->dbenv->xa_txn;
	if (t->txnid == TXN_INVALID)
		t = NULL;

	return (((XA_METHODS *)dbp->xa_internal)->del(dbp, t, key, flags));
}

static int
__xa_close(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	int (*real_close) __P((DB *, u_int32_t));

	real_close = ((XA_METHODS *)dbp->xa_internal)->close;

	__os_free(dbp->xa_internal, sizeof(XA_METHODS));
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
	DB_TXN *t;

	t = txn != NULL && txn == dbp->open_txn ? txn : dbp->dbenv->xa_txn;
	if (t->txnid == TXN_INVALID)
		t = NULL;

	return (((XA_METHODS *)dbp->xa_internal)->get
	    (dbp, t, key, data, flags));
}

static int
__xa_put(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DB_TXN *t;

	t = txn != NULL && txn == dbp->open_txn ? txn : dbp->dbenv->xa_txn;
	if (t->txnid == TXN_INVALID)
		t = NULL;

	return (((XA_METHODS *)dbp->xa_internal)->put
	    (dbp, t, key, data, flags));
}
