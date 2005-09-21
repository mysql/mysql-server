/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: client.c,v 1.60 2004/09/21 16:09:54 sue Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_VXWORKS
#include <rpcLib.h>
#endif
#include <rpc/rpc.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_server.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/txn.h"
#include "dbinc_auto/rpc_client_ext.h"

static int __dbcl_c_destroy __P((DBC *));
static int __dbcl_txn_close __P((DB_ENV *));

/*
 * __dbcl_envrpcserver --
 *	Initialize an environment's server.
 *
 * PUBLIC: int __dbcl_envrpcserver
 * PUBLIC:     __P((DB_ENV *, void *, const char *, long, long, u_int32_t));
 */
int
__dbcl_envrpcserver(dbenv, clnt, host, tsec, ssec, flags)
	DB_ENV *dbenv;
	void *clnt;
	const char *host;
	long tsec, ssec;
	u_int32_t flags;
{
	CLIENT *cl;
	struct timeval tp;

	COMPQUIET(flags, 0);

#ifdef HAVE_VXWORKS
	if (rpcTaskInit() != 0) {
		__db_err(dbenv, "Could not initialize VxWorks RPC");
		return (ERROR);
	}
#endif
	if (RPC_ON(dbenv)) {
		__db_err(dbenv, "Already set an RPC handle");
		return (EINVAL);
	}
	/*
	 * Only create the client and set its timeout if the user
	 * did not pass us a client structure to begin with.
	 */
	if (clnt == NULL) {
		if ((cl = clnt_create((char *)host, DB_RPC_SERVERPROG,
		    DB_RPC_SERVERVERS, "tcp")) == NULL) {
			__db_err(dbenv, clnt_spcreateerror((char *)host));
			return (DB_NOSERVER);
		}
		if (tsec != 0) {
			tp.tv_sec = tsec;
			tp.tv_usec = 0;
			(void)clnt_control(cl, CLSET_TIMEOUT, (char *)&tp);
		}
	} else {
		cl = (CLIENT *)clnt;
		F_SET(dbenv, DB_ENV_RPCCLIENT_GIVEN);
	}
	dbenv->cl_handle = cl;

	return (__dbcl_env_create(dbenv, ssec));
}

/*
 * __dbcl_env_close_wrap --
 *	Wrapper function for DB_ENV->close function for clients.
 *	We need a wrapper function to deal with the case where we
 *	either don't call dbenv->open or close gets an error.
 *	We need to release the handle no matter what.
 *
 * PUBLIC: int __dbcl_env_close_wrap
 * PUBLIC:     __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_env_close_wrap(dbenv, flags)
	DB_ENV * dbenv;
	u_int32_t flags;
{
	int ret, t_ret;

	ret = __dbcl_env_close(dbenv, flags);
	t_ret = __dbcl_refresh(dbenv);
	if (ret == 0 && t_ret != 0)
		ret = t_ret;
	return (ret);
}

/*
 * __dbcl_env_open_wrap --
 *	Wrapper function for DB_ENV->open function for clients.
 *	We need a wrapper function to deal with DB_USE_ENVIRON* flags
 *	and we don't want to complicate the generated code for env_open.
 *
 * PUBLIC: int __dbcl_env_open_wrap
 * PUBLIC:     __P((DB_ENV *, const char *, u_int32_t, int));
 */
int
__dbcl_env_open_wrap(dbenv, home, flags, mode)
	DB_ENV * dbenv;
	const char * home;
	u_int32_t flags;
	int mode;
{
	int ret;

	if (LF_ISSET(DB_THREAD)) {
		__db_err(dbenv, "DB_THREAD not allowed on RPC clients");
		return (EINVAL);
	}
	if ((ret = __db_home(dbenv, home, flags)) != 0)
		return (ret);
	return (__dbcl_env_open(dbenv, dbenv->db_home, flags, mode));
}

/*
 * __dbcl_db_open_wrap --
 *	Wrapper function for DB->open function for clients.
 *	We need a wrapper function to error on DB_THREAD flag.
 *	and we don't want to complicate the generated code.
 *
 * PUBLIC: int __dbcl_db_open_wrap
 * PUBLIC:     __P((DB *, DB_TXN *, const char *, const char *,
 * PUBLIC:     DBTYPE, u_int32_t, int));
 */
int
__dbcl_db_open_wrap(dbp, txnp, name, subdb, type, flags, mode)
	DB * dbp;
	DB_TXN * txnp;
	const char * name;
	const char * subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	if (LF_ISSET(DB_THREAD)) {
		__db_err(dbp->dbenv, "DB_THREAD not allowed on RPC clients");
		return (EINVAL);
	}
	return (__dbcl_db_open(dbp, txnp, name, subdb, type, flags, mode));
}

/*
 * __dbcl_refresh --
 *	Clean up an environment.
 *
 * PUBLIC: int __dbcl_refresh __P((DB_ENV *));
 */
int
__dbcl_refresh(dbenv)
	DB_ENV *dbenv;
{
	CLIENT *cl;
	int ret;

	cl = (CLIENT *)dbenv->cl_handle;

	ret = 0;
	if (dbenv->tx_handle != NULL) {
		/*
		 * We only need to free up our stuff, the caller
		 * of this function will call the server who will
		 * do all the real work.
		 */
		ret = __dbcl_txn_close(dbenv);
		dbenv->tx_handle = NULL;
	}
	if (!F_ISSET(dbenv, DB_ENV_RPCCLIENT_GIVEN) && cl != NULL)
		clnt_destroy(cl);
	dbenv->cl_handle = NULL;
	if (dbenv->db_home != NULL) {
		__os_free(dbenv, dbenv->db_home);
		dbenv->db_home = NULL;
	}
	return (ret);
}

/*
 * __dbcl_retcopy --
 *	Copy the returned data into the user's DBT, handling allocation flags,
 *	but not DB_DBT_PARTIAL.
 *
 * PUBLIC: int __dbcl_retcopy __P((DB_ENV *, DBT *,
 * PUBLIC:    void *, u_int32_t, void **, u_int32_t *));
 */
int
__dbcl_retcopy(dbenv, dbt, data, len, memp, memsize)
	DB_ENV *dbenv;
	DBT *dbt;
	void *data;
	u_int32_t len;
	void **memp;
	u_int32_t *memsize;
{
	int ret;
	u_int32_t orig_flags;

	/*
	 * The RPC server handles DB_DBT_PARTIAL, so we mask it out here to
	 * avoid the handling of partials in __db_retcopy.  Check first whether
	 * the data has actually changed, so we don't try to copy over
	 * read-only keys, which the RPC server always returns regardless.
	 */
	orig_flags = dbt->flags;
	F_CLR(dbt, DB_DBT_PARTIAL);
	if (dbt->data != NULL && dbt->size == len &&
	    memcmp(dbt->data, data, len) == 0)
		ret = 0;
	else
		ret = __db_retcopy(dbenv, dbt, data, len, memp, memsize);
	dbt->flags = orig_flags;
	return (ret);
}

/*
 * __dbcl_txn_close --
 *	Clean up an environment's transactions.
 */
static int
__dbcl_txn_close(dbenv)
	DB_ENV *dbenv;
{
	DB_TXN *txnp;
	DB_TXNMGR *tmgrp;
	int ret;

	ret = 0;
	tmgrp = dbenv->tx_handle;

	/*
	 * This function can only be called once per process (i.e., not
	 * once per thread), so no synchronization is required.
	 * Also this function is called *after* the server has been called,
	 * so the server has already closed/aborted any transactions that
	 * were open on its side.  We only need to do local cleanup.
	 */
	while ((txnp = TAILQ_FIRST(&tmgrp->txn_chain)) != NULL)
		__dbcl_txn_end(txnp);

	__os_free(dbenv, tmgrp);
	return (ret);

}

/*
 * __dbcl_txn_end --
 *	Clean up an transaction.
 * RECURSIVE FUNCTION:  Clean up nested transactions.
 *
 * PUBLIC: void __dbcl_txn_end __P((DB_TXN *));
 */
void
__dbcl_txn_end(txnp)
	DB_TXN *txnp;
{
	DB_ENV *dbenv;
	DB_TXN *kids;
	DB_TXNMGR *mgr;

	mgr = txnp->mgrp;
	dbenv = mgr->dbenv;

	/*
	 * First take care of any kids we have
	 */
	for (kids = TAILQ_FIRST(&txnp->kids);
	    kids != NULL;
	    kids = TAILQ_FIRST(&txnp->kids))
		__dbcl_txn_end(kids);

	/*
	 * We are ending this transaction no matter what the parent
	 * may eventually do, if we have a parent.  All those details
	 * are taken care of by the server.  We only need to make sure
	 * that we properly release resources.
	 */
	if (txnp->parent != NULL)
		TAILQ_REMOVE(&txnp->parent->kids, txnp, klinks);
	TAILQ_REMOVE(&mgr->txn_chain, txnp, links);
	__os_free(dbenv, txnp);
}

/*
 * __dbcl_txn_setup --
 *	Setup a client transaction structure.
 *
 * PUBLIC: void __dbcl_txn_setup __P((DB_ENV *, DB_TXN *, DB_TXN *, u_int32_t));
 */
void
__dbcl_txn_setup(dbenv, txn, parent, id)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB_TXN *parent;
	u_int32_t id;
{
	txn->mgrp = dbenv->tx_handle;
	txn->parent = parent;
	txn->txnid = id;

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

	TAILQ_INIT(&txn->kids);

	if (parent != NULL)
		TAILQ_INSERT_HEAD(&parent->kids, txn, klinks);

	txn->abort = __dbcl_txn_abort;
	txn->commit = __dbcl_txn_commit;
	txn->discard = __dbcl_txn_discard;
	txn->id = __txn_id;
	txn->prepare = __dbcl_txn_prepare;
	txn->set_timeout = __dbcl_txn_timeout;

	txn->flags = TXN_MALLOC;
}

/*
 * __dbcl_c_destroy --
 *	Destroy a cursor.
 */
static int
__dbcl_c_destroy(dbc)
	DBC *dbc;
{
	DB *dbp;

	dbp = dbc->dbp;

	TAILQ_REMOVE(&dbp->free_queue, dbc, links);
	/* Discard any memory used to store returned data. */
	if (dbc->my_rskey.data != NULL)
		__os_free(dbc->dbp->dbenv, dbc->my_rskey.data);
	if (dbc->my_rkey.data != NULL)
		__os_free(dbc->dbp->dbenv, dbc->my_rkey.data);
	if (dbc->my_rdata.data != NULL)
		__os_free(dbc->dbp->dbenv, dbc->my_rdata.data);
	__os_free(NULL, dbc);

	return (0);
}

/*
 * __dbcl_c_refresh --
 *	Refresh a cursor.  Move it from the active queue to the free queue.
 *
 * PUBLIC: void __dbcl_c_refresh __P((DBC *));
 */
void
__dbcl_c_refresh(dbc)
	DBC *dbc;
{
	DB *dbp;

	dbp = dbc->dbp;
	dbc->flags = 0;
	dbc->cl_id = 0;

	/*
	 * If dbp->cursor fails locally, we use a local dbc so that
	 * we can close it.  In that case, dbp will be NULL.
	 */
	if (dbp != NULL) {
		TAILQ_REMOVE(&dbp->active_queue, dbc, links);
		TAILQ_INSERT_TAIL(&dbp->free_queue, dbc, links);
	}
}

/*
 * __dbcl_c_setup --
 *	Allocate a cursor.
 *
 * PUBLIC: int __dbcl_c_setup __P((u_int, DB *, DBC **));
 */
int
__dbcl_c_setup(cl_id, dbp, dbcp)
	u_int cl_id;
	DB *dbp;
	DBC **dbcp;
{
	DBC *dbc, tmpdbc;
	int ret;

	if ((dbc = TAILQ_FIRST(&dbp->free_queue)) != NULL)
		TAILQ_REMOVE(&dbp->free_queue, dbc, links);
	else {
		if ((ret =
		    __os_calloc(dbp->dbenv, 1, sizeof(DBC), &dbc)) != 0) {
			/*
			 * If we die here, set up a tmp dbc to call the
			 * server to shut down that cursor.
			 */
			tmpdbc.dbp = NULL;
			tmpdbc.cl_id = cl_id;
			(void)__dbcl_dbc_close(&tmpdbc);
			return (ret);
		}
		dbc->c_close = __dbcl_dbc_close;
		dbc->c_count = __dbcl_dbc_count;
		dbc->c_del = __dbcl_dbc_del;
		dbc->c_dup = __dbcl_dbc_dup;
		dbc->c_get = __dbcl_dbc_get;
		dbc->c_pget = __dbcl_dbc_pget;
		dbc->c_put = __dbcl_dbc_put;
		dbc->c_am_destroy = __dbcl_c_destroy;
	}
	dbc->cl_id = cl_id;
	dbc->dbp = dbp;
	TAILQ_INSERT_TAIL(&dbp->active_queue, dbc, links);
	*dbcp = dbc;
	return (0);
}

/*
 * __dbcl_dbclose_common --
 *	Common code for closing/cleaning a dbp.
 *
 * PUBLIC: int __dbcl_dbclose_common __P((DB *));
 */
int
__dbcl_dbclose_common(dbp)
	DB *dbp;
{
	int ret, t_ret;
	DBC *dbc;

	/*
	 * Go through the active cursors and call the cursor recycle routine,
	 * which resolves pending operations and moves the cursors onto the
	 * free list.  Then, walk the free list and call the cursor destroy
	 * routine.
	 *
	 * NOTE:  We do not need to use the join_queue for join cursors.
	 * See comment in __dbcl_dbjoin_ret.
	 */
	ret = 0;
	while ((dbc = TAILQ_FIRST(&dbp->active_queue)) != NULL)
		__dbcl_c_refresh(dbc);
	while ((dbc = TAILQ_FIRST(&dbp->free_queue)) != NULL)
		if ((t_ret = __dbcl_c_destroy(dbc)) != 0 && ret == 0)
			ret = t_ret;

	TAILQ_INIT(&dbp->free_queue);
	TAILQ_INIT(&dbp->active_queue);
	/* Discard any memory used to store returned data. */
	if (dbp->my_rskey.data != NULL)
		__os_free(dbp->dbenv, dbp->my_rskey.data);
	if (dbp->my_rkey.data != NULL)
		__os_free(dbp->dbenv, dbp->my_rkey.data);
	if (dbp->my_rdata.data != NULL)
		__os_free(dbp->dbenv, dbp->my_rdata.data);

	memset(dbp, CLEAR_BYTE, sizeof(*dbp));
	__os_free(NULL, dbp);
	return (ret);
}
