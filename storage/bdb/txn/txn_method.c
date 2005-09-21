/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: txn_method.c,v 11.72 2004/03/23 17:24:18 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_RPC
#include <rpc/rpc.h>
#endif

#include <string.h>
#endif

#ifdef HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "dbinc/txn.h"

#ifdef HAVE_RPC
#include "dbinc_auto/rpc_client_ext.h"
#endif

static int __txn_get_tx_max __P((DB_ENV *, u_int32_t *));
static int __txn_get_tx_timestamp __P((DB_ENV *, time_t *));
static int __txn_set_tx_timestamp __P((DB_ENV *, time_t *));

/*
 * __txn_dbenv_create --
 *	Transaction specific initialization of the DB_ENV structure.
 *
 * PUBLIC: void __txn_dbenv_create __P((DB_ENV *));
 */
void
__txn_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 */

	dbenv->tx_max = DEF_MAX_TXNS;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->get_tx_max = __dbcl_get_tx_max;
		dbenv->set_tx_max = __dbcl_set_tx_max;
		dbenv->get_tx_timestamp = __dbcl_get_tx_timestamp;
		dbenv->set_tx_timestamp = __dbcl_set_tx_timestamp;

		dbenv->txn_checkpoint = __dbcl_txn_checkpoint;
		dbenv->txn_recover = __dbcl_txn_recover;
		dbenv->txn_stat = __dbcl_txn_stat;
		dbenv->txn_stat_print = NULL;
		dbenv->txn_begin = __dbcl_txn_begin;
	} else
#endif
	{
		dbenv->get_tx_max = __txn_get_tx_max;
		dbenv->set_tx_max = __txn_set_tx_max;
		dbenv->get_tx_timestamp = __txn_get_tx_timestamp;
		dbenv->set_tx_timestamp = __txn_set_tx_timestamp;

		dbenv->txn_checkpoint = __txn_checkpoint_pp;
		dbenv->txn_recover = __txn_recover_pp;
		dbenv->txn_stat = __txn_stat_pp;
		dbenv->txn_stat_print = __txn_stat_print_pp;
		dbenv->txn_begin = __txn_begin_pp;
	}
}

static int
__txn_get_tx_max(dbenv, tx_maxp)
	DB_ENV *dbenv;
	u_int32_t *tx_maxp;
{
	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->tx_handle, "DB_ENV->get_tx_max", DB_INIT_TXN);

	if (TXN_ON(dbenv)) {
		/* Cannot be set after open, no lock required to read. */
		*tx_maxp = ((DB_TXNREGION *)
		    ((DB_TXNMGR *)dbenv->tx_handle)->reginfo.primary)->maxtxns;
	} else
		*tx_maxp = dbenv->tx_max;
	return (0);
}

/*
 * __txn_set_tx_max --
 *	DB_ENV->set_tx_max.
 *
 * PUBLIC: int __txn_set_tx_max __P((DB_ENV *, u_int32_t));
 */
int
__txn_set_tx_max(dbenv, tx_max)
	DB_ENV *dbenv;
	u_int32_t tx_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_tx_max");

	dbenv->tx_max = tx_max;
	return (0);
}

static int
__txn_get_tx_timestamp(dbenv, timestamp)
	DB_ENV *dbenv;
	time_t *timestamp;
{
	*timestamp = dbenv->tx_timestamp;
	return (0);
}

/*
 * __txn_set_tx_timestamp --
 *	Set the transaction recovery timestamp.
 */
static int
__txn_set_tx_timestamp(dbenv, timestamp)
	DB_ENV *dbenv;
	time_t *timestamp;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_tx_timestamp");

	dbenv->tx_timestamp = *timestamp;
	return (0);
}
