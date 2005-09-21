/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: gen_client_ret.c,v 1.69 2004/09/22 16:29:51 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>

#include <string.h>
#endif

#include "db_server.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/txn.h"
#include "dbinc_auto/rpc_client_ext.h"

#define	FREE_IF_CHANGED(dbtp, orig)	do {				\
	if ((dbtp)->data != NULL && (dbtp)->data != orig) {		\
		__os_free(dbenv, (dbtp)->data);				\
		(dbtp)->data = NULL;					\
	}								\
} while (0)

/*
 * PUBLIC: int __dbcl_env_create_ret
 * PUBLIC:     __P((DB_ENV *, long, __env_create_reply *));
 */
int
__dbcl_env_create_ret(dbenv, timeout, replyp)
	DB_ENV * dbenv;
	long timeout;
	__env_create_reply *replyp;
{

	COMPQUIET(timeout, 0);

	if (replyp->status != 0)
		return (replyp->status);
	dbenv->cl_id = replyp->envcl_id;
	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_env_open_ret __P((DB_ENV *,
 * PUBLIC:     const char *, u_int32_t, int, __env_open_reply *));
 */
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

	dbenv->cl_id = replyp->envcl_id;
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

/*
 * PUBLIC: int __dbcl_env_remove_ret
 * PUBLIC:     __P((DB_ENV *, const char *, u_int32_t, __env_remove_reply *));
 */
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
	__os_free(NULL, dbenv);
	if (replyp->status == 0 && ret != 0)
		return (ret);
	else
		return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_txn_abort_ret __P((DB_TXN *, __txn_abort_reply *));
 */
int
__dbcl_txn_abort_ret(txnp, replyp)
	DB_TXN *txnp;
	__txn_abort_reply *replyp;
{
	__dbcl_txn_end(txnp);
	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_txn_begin_ret __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, DB_TXN **, u_int32_t, __txn_begin_reply *));
 */
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
	/*
	 * !!!
	 * Cast the txnidcl_id to 32-bits.  We don't want to change the
	 * size of the txn structure.  But if we're running on 64-bit
	 * machines, we could overflow.  Ignore for now.
	 */
	__dbcl_txn_setup(envp, txn, parent, (u_int32_t)replyp->txnidcl_id);
	*txnpp = txn;
	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_txn_commit_ret
 * PUBLIC:     __P((DB_TXN *, u_int32_t, __txn_commit_reply *));
 */
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

/*
 * PUBLIC: int __dbcl_txn_discard_ret __P((DB_TXN *, u_int32_t,
 * PUBLIC:      __txn_discard_reply *));
 */
int
__dbcl_txn_discard_ret(txnp, flags, replyp)
	DB_TXN * txnp;
	u_int32_t flags;
	__txn_discard_reply *replyp;
{
	COMPQUIET(flags, 0);

	__dbcl_txn_end(txnp);
	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_txn_recover_ret __P((DB_ENV *, DB_PREPLIST *, long,
 * PUBLIC:      long *, u_int32_t, __txn_recover_reply *));
 */
int
__dbcl_txn_recover_ret(dbenv, preplist, count, retp, flags, replyp)
	DB_ENV * dbenv;
	DB_PREPLIST * preplist;
	long count;
	long * retp;
	u_int32_t flags;
	__txn_recover_reply *replyp;
{
	DB_PREPLIST *prep;
	DB_TXN *txnarray, *txn;
	u_int32_t i, *txnid;
	int ret;
	u_int8_t *gid;

	COMPQUIET(flags, 0);
	COMPQUIET(count, 0);

	if (replyp->status != 0)
		return (replyp->status);

	*retp = (long) replyp->retcount;

	if (replyp->retcount == 0)
		return (replyp->status);

	if ((ret = __os_calloc(dbenv, replyp->retcount, sizeof(DB_TXN),
	    &txnarray)) != 0)
		return (ret);
	/*
	 * We have a bunch of arrays that need to iterate in
	 * lockstep with each other.
	 */
	i = 0;
	txn = txnarray;
	txnid = (u_int32_t *)replyp->txn.txn_val;
	gid = (u_int8_t *)replyp->gid.gid_val;
	prep = preplist;
	while (i++ < replyp->retcount) {
		__dbcl_txn_setup(dbenv, txn, NULL, *txnid);
		prep->txn = txn;
		memcpy(prep->gid, gid, DB_XIDDATASIZE);
		/*
		 * Now increment all our array pointers.
		 */
		txn++;
		gid += DB_XIDDATASIZE;
		txnid++;
		prep++;
	}

	return (0);
}

/*
 * PUBLIC: int __dbcl_db_close_ret __P((DB *, u_int32_t, __db_close_reply *));
 */
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

/*
 * PUBLIC: int __dbcl_db_create_ret
 * PUBLIC:     __P((DB *, DB_ENV *, u_int32_t, __db_create_reply *));
 */
int
__dbcl_db_create_ret(dbp, dbenv, flags, replyp)
	DB * dbp;
	DB_ENV * dbenv;
	u_int32_t flags;
	__db_create_reply *replyp;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);
	dbp->cl_id = replyp->dbcl_id;
	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_db_get_ret
 * PUBLIC:     __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t, __db_get_reply *));
 */
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
	    replyp->keydata.keydata_len, &dbp->my_rkey.data,
	    &dbp->my_rkey.ulen);
	if (ret)
		return (ret);
	ret = __dbcl_retcopy(dbenv, data, replyp->datadata.datadata_val,
	    replyp->datadata.datadata_len, &dbp->my_rdata.data,
	    &dbp->my_rdata.ulen);
	/*
	 * If an error on copying 'data' and we allocated for 'key'
	 * free it before returning the error.
	 */
	if (ret)
		FREE_IF_CHANGED(key, oldkey);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_key_range_ret __P((DB *, DB_TXN *,
 * PUBLIC:     DBT *, DB_KEY_RANGE *, u_int32_t, __db_key_range_reply *));
 */
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

/*
 * PUBLIC: int __dbcl_db_open_ret __P((DB *, DB_TXN *, const char *,
 * PUBLIC:     const char *, DBTYPE, u_int32_t, int, __db_open_reply *));
 */
int
__dbcl_db_open_ret(dbp, txn, name, subdb, type, flags, mode, replyp)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
	__db_open_reply *replyp;
{
	COMPQUIET(txn, NULL);
	COMPQUIET(name, NULL);
	COMPQUIET(subdb, NULL);
	COMPQUIET(type, DB_UNKNOWN);
	COMPQUIET(flags, 0);
	COMPQUIET(mode, 0);

	if (replyp->status == 0) {
		dbp->cl_id = replyp->dbcl_id;
		dbp->type = (DBTYPE)replyp->type;
		/*
		 * We get back the database's byteorder on the server.
		 * Determine if our byteorder is the same or not by
		 * calling __db_set_lorder.
		 *
		 * XXX
		 * This MUST come before we set the flags because
		 * __db_set_lorder checks that it is called before
		 * the open flag is set.
		 */
		(void)__db_set_lorder(dbp, replyp->lorder);

		/*
		 * Explicitly set DB_AM_OPEN_CALLED since open is now
		 * successfully completed.
		 */
		F_SET(dbp, DB_AM_OPEN_CALLED);
	}
	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_db_pget_ret __P((DB *, DB_TXN *, DBT *, DBT *, DBT *,
 * PUBLIC:      u_int32_t, __db_pget_reply *));
 */
int
__dbcl_db_pget_ret(dbp, txnp, skey, pkey, data, flags, replyp)
	DB * dbp;
	DB_TXN * txnp;
	DBT * skey;
	DBT * pkey;
	DBT * data;
	u_int32_t flags;
	__db_pget_reply *replyp;
{
	DB_ENV *dbenv;
	int ret;
	void *oldskey, *oldpkey;

	COMPQUIET(txnp, NULL);
	COMPQUIET(flags, 0);

	ret = 0;
	if (replyp->status != 0)
		return (replyp->status);

	dbenv = dbp->dbenv;

	oldskey = skey->data;
	ret = __dbcl_retcopy(dbenv, skey, replyp->skeydata.skeydata_val,
	    replyp->skeydata.skeydata_len, &dbp->my_rskey.data,
	    &dbp->my_rskey.ulen);
	if (ret)
		return (ret);

	oldpkey = pkey->data;
	if ((ret = __dbcl_retcopy(dbenv, pkey, replyp->pkeydata.pkeydata_val,
	    replyp->pkeydata.pkeydata_len, &dbp->my_rkey.data,
	    &dbp->my_rkey.ulen)) != 0)
		goto err;
	ret = __dbcl_retcopy(dbenv, data, replyp->datadata.datadata_val,
	    replyp->datadata.datadata_len, &dbp->my_rdata.data,
	    &dbp->my_rdata.ulen);

	if (ret) {
err:		FREE_IF_CHANGED(skey, oldskey);
		FREE_IF_CHANGED(pkey, oldpkey);
	}
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_put_ret
 * PUBLIC:     __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t, __db_put_reply *));
 */
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

/*
 * PUBLIC: int __dbcl_db_remove_ret __P((DB *,
 * PUBLIC:     const char *, const char *, u_int32_t, __db_remove_reply *));
 */
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

/*
 * PUBLIC: int __dbcl_db_rename_ret __P((DB *, const char *,
 * PUBLIC:     const char *, const char *, u_int32_t, __db_rename_reply *));
 */
int
__dbcl_db_rename_ret(dbp, name, subdb, newname, flags, replyp)
	DB *dbp;
	const char *name, *subdb, *newname;
	u_int32_t flags;
	__db_rename_reply *replyp;
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

/*
 * PUBLIC: int __dbcl_db_stat_ret
 * PUBLIC:     __P((DB *, DB_TXN *, void *, u_int32_t, __db_stat_reply *));
 */
int
__dbcl_db_stat_ret(dbp, txnp, sp, flags, replyp)
	DB *dbp;
	DB_TXN *txnp;
	void *sp;
	u_int32_t flags;
	__db_stat_reply *replyp;
{
	size_t len;
	u_int32_t i, *q, *p, *retsp;
	int ret;

	COMPQUIET(flags, 0);
	COMPQUIET(txnp, NULL);

	if (replyp->status != 0 || sp == NULL)
		return (replyp->status);

	len = replyp->stats.stats_len * sizeof(u_int32_t);
	if ((ret = __os_umalloc(dbp->dbenv, len, &retsp)) != 0)
		return (ret);
	for (i = 0, q = retsp, p = (u_int32_t *)replyp->stats.stats_val;
	    i < replyp->stats.stats_len; i++, q++, p++)
		*q = *p;
	*(u_int32_t **)sp = retsp;
	return (0);
}

/*
 * PUBLIC: int __dbcl_db_truncate_ret __P((DB *, DB_TXN *, u_int32_t  *,
 * PUBLIC:      u_int32_t, __db_truncate_reply *));
 */
int
__dbcl_db_truncate_ret(dbp, txnp, countp, flags, replyp)
	DB *dbp;
	DB_TXN *txnp;
	u_int32_t *countp, flags;
	__db_truncate_reply *replyp;
{
	COMPQUIET(dbp, NULL);
	COMPQUIET(txnp, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);
	*countp = replyp->count;

	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_db_cursor_ret
 * PUBLIC:     __P((DB *, DB_TXN *, DBC **, u_int32_t, __db_cursor_reply *));
 */
int
__dbcl_db_cursor_ret(dbp, txnp, dbcp, flags, replyp)
	DB *dbp;
	DB_TXN *txnp;
	DBC **dbcp;
	u_int32_t flags;
	__db_cursor_reply *replyp;
{
	COMPQUIET(txnp, NULL);
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);

	return (__dbcl_c_setup(replyp->dbcidcl_id, dbp, dbcp));
}

/*
 * PUBLIC: int __dbcl_db_join_ret
 * PUBLIC:     __P((DB *, DBC **, DBC **, u_int32_t, __db_join_reply *));
 */
int
__dbcl_db_join_ret(dbp, curs, dbcp, flags, replyp)
	DB *dbp;
	DBC **curs, **dbcp;
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
	return (__dbcl_c_setup(replyp->dbcidcl_id, dbp, dbcp));
}

/*
 * PUBLIC: int __dbcl_dbc_close_ret __P((DBC *, __dbc_close_reply *));
 */
int
__dbcl_dbc_close_ret(dbc, replyp)
	DBC *dbc;
	__dbc_close_reply *replyp;
{
	__dbcl_c_refresh(dbc);
	return (replyp->status);
}

/*
 * PUBLIC: int __dbcl_dbc_count_ret
 * PUBLIC:     __P((DBC *, db_recno_t *, u_int32_t, __dbc_count_reply *));
 */
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

/*
 * PUBLIC: int __dbcl_dbc_dup_ret
 * PUBLIC:     __P((DBC *, DBC **, u_int32_t, __dbc_dup_reply *));
 */
int
__dbcl_dbc_dup_ret(dbc, dbcp, flags, replyp)
	DBC *dbc, **dbcp;
	u_int32_t flags;
	__dbc_dup_reply *replyp;
{
	COMPQUIET(flags, 0);

	if (replyp->status != 0)
		return (replyp->status);

	return (__dbcl_c_setup(replyp->dbcidcl_id, dbc->dbp, dbcp));
}

/*
 * PUBLIC: int __dbcl_dbc_get_ret
 * PUBLIC:     __P((DBC *, DBT *, DBT *, u_int32_t, __dbc_get_reply *));
 */
int
__dbcl_dbc_get_ret(dbc, key, data, flags, replyp)
	DBC *dbc;
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

	dbenv = dbc->dbp->dbenv;
	oldkey = key->data;
	ret = __dbcl_retcopy(dbenv, key, replyp->keydata.keydata_val,
	    replyp->keydata.keydata_len, &dbc->my_rkey.data,
	    &dbc->my_rkey.ulen);
	if (ret)
		return (ret);
	ret = __dbcl_retcopy(dbenv, data, replyp->datadata.datadata_val,
	    replyp->datadata.datadata_len, &dbc->my_rdata.data,
	    &dbc->my_rdata.ulen);

	/*
	 * If an error on copying 'data' and we allocated for 'key'
	 * free it before returning the error.
	 */
	if (ret)
		FREE_IF_CHANGED(key, oldkey);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_pget_ret __P((DBC *, DBT *, DBT *, DBT *, u_int32_t,
 * PUBLIC:      __dbc_pget_reply *));
 */
int
__dbcl_dbc_pget_ret(dbc, skey, pkey, data, flags, replyp)
	DBC * dbc;
	DBT * skey;
	DBT * pkey;
	DBT * data;
	u_int32_t flags;
	__dbc_pget_reply *replyp;
{
	DB_ENV *dbenv;
	int ret;
	void *oldskey, *oldpkey;

	COMPQUIET(flags, 0);

	ret = 0;
	if (replyp->status != 0)
		return (replyp->status);

	dbenv = dbc->dbp->dbenv;

	oldskey = skey->data;
	ret = __dbcl_retcopy(dbenv, skey, replyp->skeydata.skeydata_val,
	    replyp->skeydata.skeydata_len, &dbc->my_rskey.data,
	    &dbc->my_rskey.ulen);
	if (ret)
		return (ret);

	oldpkey = pkey->data;
	if ((ret = __dbcl_retcopy(dbenv, pkey, replyp->pkeydata.pkeydata_val,
	    replyp->pkeydata.pkeydata_len, &dbc->my_rkey.data,
	    &dbc->my_rkey.ulen)) != 0)
		goto err;
	ret = __dbcl_retcopy(dbenv, data, replyp->datadata.datadata_val,
	    replyp->datadata.datadata_len, &dbc->my_rdata.data,
	    &dbc->my_rdata.ulen);

	/*
	 * If an error on copying 'data' and we allocated for '*key'
	 * free it before returning the error.
	 */
	if (ret) {
err:		FREE_IF_CHANGED(skey, oldskey);
		FREE_IF_CHANGED(pkey, oldpkey);
	}
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_put_ret
 * PUBLIC:     __P((DBC *, DBT *, DBT *, u_int32_t, __dbc_put_reply *));
 */
int
__dbcl_dbc_put_ret(dbc, key, data, flags, replyp)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
	__dbc_put_reply *replyp;
{
	COMPQUIET(data, NULL);

	if (replyp->status != 0)
		return (replyp->status);

	if (replyp->status == 0 && dbc->dbp->type == DB_RECNO &&
	    (flags == DB_AFTER || flags == DB_BEFORE))
		*(db_recno_t *)key->data =
		    *(db_recno_t *)replyp->keydata.keydata_val;
	return (replyp->status);
}
