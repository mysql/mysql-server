/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: client.c,v 1.21 2000/11/30 00:58:44 ubell Exp $";
#endif /* not lint */

#ifdef HAVE_RPC
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <rpc/rpc.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif
#include "db_server.h"

#include "db_int.h"
#include "txn.h"
#include "gen_client_ext.h"
#include "rpc_client_ext.h"

/*
 * __dbclenv_server --
 *	Initialize an environment's server.
 *
 * PUBLIC: int __dbcl_envserver __P((DB_ENV *, char *, long, long, u_int32_t));
 */
int
__dbcl_envserver(dbenv, host, tsec, ssec, flags)
	DB_ENV *dbenv;
	char *host;
	long tsec, ssec;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_create_msg req;
	__env_create_reply *replyp;
	struct timeval tp;
	int ret;

	COMPQUIET(flags, 0);

#ifdef HAVE_VXWORKS
	if ((ret = rpcTaskInit()) != 0) {
		__db_err(dbenv, "Could not initialize VxWorks RPC");
		return (ERROR);
	}
#endif
	if ((cl =
	    clnt_create(host, DB_SERVERPROG, DB_SERVERVERS, "tcp")) == NULL) {
		__db_err(dbenv, clnt_spcreateerror(host));
		return (DB_NOSERVER);
	}
	dbenv->cl_handle = cl;

	if (tsec != 0) {
		tp.tv_sec = tsec;
		tp.tv_usec = 0;
		(void)clnt_control(cl, CLSET_TIMEOUT, (char *)&tp);
	}

	req.timeout = ssec;
	/*
	 * CALL THE SERVER
	 */
	if ((replyp = __db_env_create_1(&req, cl)) == NULL) {
		__db_err(dbenv, clnt_sperror(cl, "Berkeley DB"));
		return (DB_NOSERVER);
	}

	/*
	 * Process reply and free up our space from request
	 * SUCCESS:  Store ID from server.
	 */
	if ((ret = replyp->status) != 0)
		return (ret);

	dbenv->cl_id = replyp->envcl_id;
	return (0);
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
	if (cl != NULL)
		clnt_destroy(cl);
	dbenv->cl_handle = NULL;
	return (ret);
}

/*
 * __dbcl_txn_close --
 *	Clean up an environment's transactions.
 *
 * PUBLIC: int __dbcl_txn_close __P((DB_ENV *));
 */
int
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

	__os_free(tmgrp, sizeof(*tmgrp));
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
	__os_free(txnp, sizeof(*txnp));

	return;
}

/*
 * __dbcl_c_destroy --
 *	Destroy a cursor.
 *
 * PUBLIC: int __dbcl_c_destroy __P((DBC *));
 */
int
__dbcl_c_destroy(dbc)
	DBC *dbc;
{
	DB *dbp;

	dbp = dbc->dbp;

	TAILQ_REMOVE(&dbp->free_queue, dbc, links);
	__os_free(dbc, sizeof(*dbc));

	return (0);
}

/*
 * __dbcl_c_refresh --
 *	Refresh a cursor.  Move it from the active queue to the free queue.
 *
 * PUBLIC: void __dbcl_c_refresh __P((DBC *));
 */
void
__dbcl_c_refresh(dbcp)
	DBC *dbcp;
{
	DB *dbp;

	dbp = dbcp->dbp;
	dbcp->flags = 0;
	dbcp->cl_id = 0;

	/*
	 * If dbp->cursor fails locally, we use a local dbc so that
	 * we can close it.  In that case, dbp will be NULL.
	 */
	if (dbp != NULL) {
		TAILQ_REMOVE(&dbp->active_queue, dbcp, links);
		TAILQ_INSERT_TAIL(&dbp->free_queue, dbcp, links);
	}
	return;
}

/*
 * __dbcl_c_setup --
 *	Allocate a cursor.
 *
 * PUBLIC: int __dbcl_c_setup __P((long, DB *, DBC **));
 */
int
__dbcl_c_setup(cl_id, dbp, dbcpp)
	long cl_id;
	DB *dbp;
	DBC **dbcpp;
{
	DBC *dbc, tmpdbc;
	int ret, t_ret;

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
			t_ret = __dbcl_dbc_close(&tmpdbc);
			return (ret);
		}
		dbc->c_close = __dbcl_dbc_close;
		dbc->c_count = __dbcl_dbc_count;
		dbc->c_del = __dbcl_dbc_del;
		dbc->c_dup = __dbcl_dbc_dup;
		dbc->c_get = __dbcl_dbc_get;
		dbc->c_put = __dbcl_dbc_put;
		dbc->c_am_destroy = __dbcl_c_destroy;
	}
	dbc->cl_id = cl_id;
	dbc->dbp = dbp;
	TAILQ_INSERT_TAIL(&dbp->active_queue, dbc, links);
	*dbcpp = dbc;
	return (0);
}

/*
 * __dbcl_retcopy --
 *	Copy the returned data into the user's DBT, handling special flags
 *	as they apply to a client.  Modeled after __db_retcopy().
 *
 * PUBLIC: int __dbcl_retcopy __P((DB_ENV *, DBT *, void *, u_int32_t));
 */
int
__dbcl_retcopy(dbenv, dbt, data, len)
	DB_ENV *dbenv;
	DBT *dbt;
	void *data;
	u_int32_t len;
{
	int ret;

	/*
	 * No need to handle DB_DBT_PARTIAL here, server already did.
	 */
	dbt->size = len;

	/*
	 * Allocate memory to be owned by the application: DB_DBT_MALLOC
	 * and DB_DBT_REALLOC.  Always allocate even if we're copying 0 bytes.
	 * Or use memory specified by application: DB_DBT_USERMEM.
	 */
	if (F_ISSET(dbt, DB_DBT_MALLOC)) {
		if ((ret = __os_malloc(dbenv, len, NULL, &dbt->data)) != 0)
			return (ret);
	} else if (F_ISSET(dbt, DB_DBT_REALLOC)) {
		if ((ret = __os_realloc(dbenv, len, NULL, &dbt->data)) != 0)
			return (ret);
	} else if (F_ISSET(dbt, DB_DBT_USERMEM)) {
		if (len != 0 && (dbt->data == NULL || dbt->ulen < len))
			return (ENOMEM);
	} else {
		/*
		 * If no user flags, then set the DBT to point to the
		 * returned data pointer and return.
		 */
		dbt->data = data;
		return (0);
	}

	if (len != 0)
		memcpy(dbt->data, data, len);
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

	memset(dbp, CLEAR_BYTE, sizeof(*dbp));
	__os_free(dbp, sizeof(*dbp));
	return (ret);
}
#endif /* HAVE_RPC */
