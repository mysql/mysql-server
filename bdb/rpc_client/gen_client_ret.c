/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *      Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: gen_client_ret.c,v 1.29 2000/12/31 19:26:23 bostic Exp $";
#endif /* not lint */

#ifdef HAVE_RPC
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>

#include <string.h>
#endif
#include "db_server.h"

#include "db_int.h"
#include "db_page.h"
#include "txn.h"
#include "db_ext.h"
#include "rpc_client_ext.h"

static void __db_db_stat_statsfree __P((u_int32_t *));
static int __db_db_stat_statslist __P((__db_stat_statsreplist *, u_int32_t **));

int
__dbcl_env_close_ret(dbenv, flags, replyp)
	DB_ENV *dbenv;
	u_int32_t flags;
	__env_close_reply *replyp;
{
	int ret;

	COMPQUIET(flags, 0);

	ret = __dbcl_refresh(dbenv);
	__os_free(dbenv, sizeof(*dbenv));
	if (replyp->status == 0 && ret != 0)
		return (ret);
	else
		return (replyp->status);
}

int
__dbcl_env_open_ret(dbenv, home, flags, mode, replyp)
	DB_ENV *dbenv;
	const char *home;
	u_int32_t flags;
	int mode;
	__env_open_reply *replyp;
{
	DB_TXNMGR *tmgrp;
	int ret;

	COMPQUIET(home, NULL);
	COMPQUIET(mode, 0);

	/*
	 * If error, return it.
	 */
	if (replyp->status != 0)
		return (replyp->status);

	/*
	 * If the user requested transactions, then we have some
	 * local client-side setup to do also.
	 */
	if (LF_ISSET(DB_INIT_TXN)) {
		if ((ret = __os_calloc(dbenv,
		    1, sizeof(DB_TXNMGR), &tmgrp)) != 0)
			return (ret);
		TAILQ_INIT(&tmgrp->txn_chain);
		tmgrp->dbenv = dbenv;
		dbenv->tx_handle = tmgrp;
	}

	return (replyp->status);
}

int
__dbcl_env_remove_ret(dbenv, home, flags, replyp)
	DB_ENV *dbenv;
	const char *home;
	u_int32_t flags;
	__env_remove_reply *replyp;
{
	int ret;

	COMPQUIET(home, NULL);
	COMPQUIET(flags, 0);

	ret = __dbcl_refresh(dbenv);
	__os_free(dbenv, sizeof(*dbenv));
	if (replyp->status == 0 && ret != 0)
		return (ret);
	else
		return (replyp->status);
}

int
__dbcl_txn_abort_ret(txnp, replyp)
	DB_TXN *txnp;
	__txn_abort_reply *replyp;
{
	__dbcl_txn_end(txnp);
	return (replyp->status);
}

int
__dbcl_txn_begin_ret(envp, parent, txnpp, flags, replyp)
	DB_ENV *envp;
	DB_TXN *parent, **txnpp;
	u_int32_t flags;
	__txn_begin_reply *replyp;
{
	DB_TXN *txn;
	int ret;

	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);

	if ((ret = __os_calloc(envp, 1, sizeof(DB_TXN), &txn)) != 0)
		return (ret);
	txn->txnid = replyp->txnidcl_id;
	txn->mgrp = envp->tx_handle;
	txn->parent = parent;
	TAILQ_INIT(&txn->kids);
	txn->flags = TXN_MALLOC;
	if (parent != NULL)
		TAILQ_INSERT_HEAD(&parent->kids, txn, klinks);

	/*
	 * XXX
	 * In DB library the txn_chain is protected by the mgrp->mutexp.
	 * However, that mutex is implemented in the environments shared
	 * memory region.  The client library does not support all of the
	 * region - that just get forwarded to the server.  Therefore,
	 * the chain is unprotected here, but properly protected on the
	 * server.
	 */
	TAILQ_INSERT_TAIL(&txn->mgrp->txn_chain, txn, links);

	*txnpp = txn;
	return (replyp->status);
}

int
__dbcl_txn_commit_ret(txnp, flags, replyp)
	DB_TXN *txnp;
	u_int32_t flags;
	__txn_commit_reply *replyp;
{
	COMPQUIET(flags, 0);

	__dbcl_txn_end(txnp);
	return (replyp->status);
}

int
__dbcl_db_close_ret(dbp, flags, replyp)
	DB *dbp;
	u_int32_t flags;
	__db_close_reply *replyp;
{
	int ret;

	COMPQUIET(flags, 0);

	ret = __dbcl_dbclose_common(dbp);

	if (replyp->status != 0)
		return (replyp->status);
	else
		return (ret);
}

int
__dbcl_db_get_ret(dbp, txnp, key, data, flags, replyp)
	DB *dbp;
	DB_TXN *txnp;
	DBT *key, *data;
	u_int32_t flags;
	__db_get_reply *replyp;
{
	DB_ENV *dbenv;
	int ret;
	void *oldkey;

	COMPQUIET(txnp, NULL);
	COMPQUIET(flags, 0);

	ret = 0;
	if (replyp->status != 0)
		return (replyp->status);

	dbenv = dbp->dbenv;

	oldkey = key->data;
	ret = __dbcl_retcopy(dbenv, key, replyp->keydata.keydata_val,
	    replyp->keydata.keydata_len);
	if (ret)
		return (ret);
	ret = __dbcl_retcopy(dbenv, data, replyp->datadata.datadata_val,
	    replyp->datadata.datadata_len);
	/*
	 * If an error on copying 'data' and we allocated for 'key'
	 * free it before returning the error.
	 */
	if (ret && oldkey != NULL)
		__os_free(key->data, key->size);
	return (ret);
}

int
__dbcl_db_key_range_ret(dbp, txnp, key, range, flags, replyp)
	DB *dbp;
	DB_TXN *txnp;
	DBT *key;
	DB_KEY_RANGE *range;
	u_int32_t flags;
	__db_key_range_reply *replyp;
{
	COMPQUIET(dbp, NULL);
	COMPQUIET(txnp, NULL);
	COMPQUIET(key, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);
	range->less = replyp->less;
	range->equal = replyp->equal;
	range->greater = replyp->greater;
	return (replyp->status);
}

int
__dbcl_db_open_ret(dbp, name, subdb, type, flags, mode, replyp)
	DB *dbp;
	const char *name, *subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
	__db_open_reply *replyp;
{
	COMPQUIET(name, NULL);
	COMPQUIET(subdb, NULL);
	COMPQUIET(type, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(mode, 0);

	dbp->type = replyp->type;

	/*
	 * XXX
	 * This is only for Tcl which peeks at the dbp flags.
	 * When dbp->get_flags exists, this should go away.
	 */
	dbp->flags = replyp->dbflags;
	return (replyp->status);
}

int
__dbcl_db_put_ret(dbp, txnp, key, data, flags, replyp)
	DB *dbp;
	DB_TXN *txnp;
	DBT *key, *data;
	u_int32_t flags;
	__db_put_reply *replyp;
{
	int ret;

	COMPQUIET(dbp, NULL);
	COMPQUIET(txnp, NULL);
	COMPQUIET(data, NULL);

	ret = replyp->status;
	if (replyp->status == 0 && (flags == DB_APPEND))
		*(db_recno_t *)key->data =
		    *(db_recno_t *)replyp->keydata.keydata_val;
	return (ret);
}

int
__dbcl_db_remove_ret(dbp, name, subdb, flags, replyp)
	DB *dbp;
	const char *name, *subdb;
	u_int32_t flags;
	__db_remove_reply *replyp;
{
	int ret;

	COMPQUIET(name, 0);
	COMPQUIET(subdb, 0);
	COMPQUIET(flags, 0);

	ret = __dbcl_dbclose_common(dbp);

	if (replyp->status != 0)
		return (replyp->status);
	else
		return (ret);
}

int
__dbcl_db_rename_ret(dbp, name, subdb, newname, flags, replyp)
	DB *dbp;
	const char *name, *subdb, *newname;
	u_int32_t flags;
	__db_remove_reply *replyp;
{
	int ret;

	COMPQUIET(name, 0);
	COMPQUIET(subdb, 0);
	COMPQUIET(newname, 0);
	COMPQUIET(flags, 0);

	ret = __dbcl_dbclose_common(dbp);

	if (replyp->status != 0)
		return (replyp->status);
	else
		return (ret);
}

int
__dbcl_db_stat_ret(dbp, sp, func, flags, replyp)
	DB *dbp;
	void *sp;
	void *(*func) __P((size_t));
	u_int32_t flags;
	__db_stat_reply *replyp;
{
	int ret;
	u_int32_t *__db_statslist;

	COMPQUIET(dbp, NULL);
	COMPQUIET(func, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);

	if ((ret =
	    __db_db_stat_statslist(replyp->statslist, &__db_statslist)) != 0)
		return (ret);

	if (sp == NULL)
		__db_db_stat_statsfree(__db_statslist);
	else
		*(u_int32_t **)sp = __db_statslist;
	return (replyp->status);
}

static int
__db_db_stat_statslist(locp, ppp)
	__db_stat_statsreplist *locp;
	u_int32_t **ppp;
{
	u_int32_t *pp;
	int cnt, ret, size;
	__db_stat_statsreplist *nl;

	for (cnt = 0, nl = locp; nl != NULL; cnt++, nl = nl->next)
		;

	if (cnt == 0) {
		*ppp = NULL;
		return (0);
	}
	size = sizeof(*pp) * cnt;
	if ((ret = __os_malloc(NULL, size, NULL, ppp)) != 0)
		return (ret);
	memset(*ppp, 0, size);
	for (pp = *ppp, nl = locp; nl != NULL; nl = nl->next, pp++) {
		*pp = *(u_int32_t *)nl->ent.ent_val;
	}
	return (0);
}

static void
__db_db_stat_statsfree(pp)
	u_int32_t *pp;
{
	size_t size;
	u_int32_t *p;

	if (pp == NULL)
		return;
	size = sizeof(*p);
	for (p = pp; *p != 0; p++)
		size += sizeof(*p);

	__os_free(pp, size);
}

int
__dbcl_db_cursor_ret(dbp, txnp, dbcpp, flags, replyp)
	DB *dbp;
	DB_TXN *txnp;
	DBC **dbcpp;
	u_int32_t flags;
	__db_cursor_reply *replyp;
{
	COMPQUIET(txnp, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);

	return (__dbcl_c_setup(replyp->dbcidcl_id, dbp, dbcpp));
}

int
__dbcl_db_join_ret(dbp, curs, dbcpp, flags, replyp)
	DB *dbp;
	DBC **curs, **dbcpp;
	u_int32_t flags;
	__db_join_reply *replyp;
{
	COMPQUIET(curs, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);

	/*
	 * We set this up as a normal cursor.  We do not need
	 * to treat a join cursor any differently than a normal
	 * cursor, even though DB itself must.  We only need the
	 * client-side cursor/db relationship to know what cursors
	 * are open in the db, and to store their ID.  Nothing else.
	 */
	return (__dbcl_c_setup(replyp->dbcidcl_id, dbp, dbcpp));
}

int
__dbcl_dbc_close_ret(dbcp, replyp)
	DBC *dbcp;
	__dbc_close_reply *replyp;
{
	DB *dbp;

	dbp = dbcp->dbp;
	__dbcl_c_refresh(dbcp);
	return (replyp->status);
}

int
__dbcl_dbc_count_ret(dbc, countp, flags, replyp)
	DBC *dbc;
	db_recno_t *countp;
	u_int32_t flags;
	__dbc_count_reply *replyp;
{
	COMPQUIET(dbc, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);
	*countp = replyp->dupcount;

	return (replyp->status);
}

int
__dbcl_dbc_dup_ret(dbcp, dbcpp, flags, replyp)
	DBC *dbcp, **dbcpp;
	u_int32_t flags;
	__dbc_dup_reply *replyp;
{
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);

	return (__dbcl_c_setup(replyp->dbcidcl_id, dbcp->dbp, dbcpp));
}

int
__dbcl_dbc_get_ret(dbcp, key, data, flags, replyp)
	DBC *dbcp;
	DBT *key, *data;
	u_int32_t flags;
	__dbc_get_reply *replyp;
{
	DB_ENV *dbenv;
	int ret;
	void *oldkey;

	COMPQUIET(flags, 0);

	ret = 0;
	if (replyp->status != 0)
		return (replyp->status);

	dbenv = dbcp->dbp->dbenv;
	oldkey = key->data;
	ret = __dbcl_retcopy(dbenv, key, replyp->keydata.keydata_val,
	    replyp->keydata.keydata_len);
	if (ret)
		return (ret);
	ret = __dbcl_retcopy(dbenv, data, replyp->datadata.datadata_val,
	    replyp->datadata.datadata_len);

	/*
	 * If an error on copying 'data' and we allocated for 'key'
	 * free it before returning the error.
	 */
	if (ret && oldkey != NULL)
		__os_free(key->data, key->size);
	return (ret);
}

int
__dbcl_dbc_put_ret(dbcp, key, data, flags, replyp)
	DBC *dbcp;
	DBT *key, *data;
	u_int32_t flags;
	__dbc_put_reply *replyp;
{
	COMPQUIET(data, NULL);

	if (replyp->status != 0)
		return (replyp->status);

	if (replyp->status == 0 && dbcp->dbp->type == DB_RECNO &&
	    (flags == DB_AFTER || flags == DB_BEFORE))
		*(db_recno_t *)key->data =
		    *(db_recno_t *)replyp->keydata.keydata_val;
	return (replyp->status);
}
#endif /* HAVE_RPC */
