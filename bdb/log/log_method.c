/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log_method.c,v 11.14 2000/11/30 00:58:40 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "log.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int __log_set_lg_max __P((DB_ENV *, u_int32_t));
static int __log_set_lg_bsize __P((DB_ENV *, u_int32_t));
static int __log_set_lg_dir __P((DB_ENV *, const char *));

/*
 * __log_dbenv_create --
 *	Log specific initialization of the DB_ENV structure.
 *
 * PUBLIC: void __log_dbenv_create __P((DB_ENV *));
 */
void
__log_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	dbenv->lg_bsize = LG_BSIZE_DEFAULT;
	dbenv->set_lg_bsize = __log_set_lg_bsize;

	dbenv->lg_max = LG_MAX_DEFAULT;
	dbenv->set_lg_max = __log_set_lg_max;

	dbenv->set_lg_dir = __log_set_lg_dir;
#ifdef	HAVE_RPC
	/*
	 * If we have a client, overwrite what we just setup to
	 * point to client functions.
	 */
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->set_lg_bsize = __dbcl_set_lg_bsize;
		dbenv->set_lg_max = __dbcl_set_lg_max;
		dbenv->set_lg_dir = __dbcl_set_lg_dir;
	}
#endif
}

/*
 * __log_set_lg_bsize --
 *	Set the log buffer size.
 */
static int
__log_set_lg_bsize(dbenv, lg_bsize)
	DB_ENV *dbenv;
	u_int32_t lg_bsize;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lg_bsize");

					/* Let's not be silly. */
	if (lg_bsize > dbenv->lg_max / 4) {
		__db_err(dbenv, "log buffer size must be <= log file size / 4");
		return (EINVAL);
	}

	dbenv->lg_bsize = lg_bsize;
	return (0);
}

/*
 * __log_set_lg_max --
 *	Set the maximum log file size.
 */
static int
__log_set_lg_max(dbenv, lg_max)
	DB_ENV *dbenv;
	u_int32_t lg_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lg_max");

					/* Let's not be silly. */
	if (lg_max < dbenv->lg_bsize * 4) {
		__db_err(dbenv, "log file size must be >= log buffer size * 4");
		return (EINVAL);
	}

	dbenv->lg_max = lg_max;
	return (0);
}

/*
 * __log_set_lg_dir --
 *	Set the log file directory.
 */
static int
__log_set_lg_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	if (dbenv->db_log_dir != NULL)
		__os_freestr(dbenv->db_log_dir);
	return (__os_strdup(dbenv, dir, &dbenv->db_log_dir));
}
