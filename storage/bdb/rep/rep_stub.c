/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_stub.c,v 12.11 2005/10/09 16:12:07 bostic Exp $
 */

#include "db_config.h"

#ifndef HAVE_REPLICATION
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"

/*
 * If the library wasn't compiled with replication support, various routines
 * aren't available.  Stub them here, returning an appropriate error.
 */
static int __db_norep __P((DB_ENV *));

/*
 * __db_norep --
 *	Error when a Berkeley DB build doesn't include the access method.
 */
static int
__db_norep(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv,
	    "library build did not include support for replication");
	return (DB_OPNOTSUP);
}

int
__db_rep_enter(dbp, checkgen, checklock, return_now)
	DB *dbp;
	int checkgen, checklock, return_now;
{
	COMPQUIET(checkgen, 0);
	COMPQUIET(checklock, 0);
	COMPQUIET(return_now, 0);
	return (__db_norep(dbp->dbenv));
}

int
__env_rep_enter(dbenv, checklock)
	DB_ENV *dbenv;
	int checklock;
{
	COMPQUIET(checklock, 0);
	return (__db_norep(dbenv));
}

int
__env_db_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	return (__db_norep(dbenv));
}

int
__op_rep_enter(dbenv)
	DB_ENV *dbenv;
{
	return (__db_norep(dbenv));
}

int
__op_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	return (__db_norep(dbenv));
}

int
__rep_bulk_message(dbenv, bulkp, repth, lsnp, dbt, flags)
	DB_ENV *dbenv;
	REP_BULK *bulkp;
	REP_THROTTLE *repth;
	DB_LSN *lsnp;
	const DBT *dbt;
	u_int32_t flags;
{
	COMPQUIET(bulkp, NULL);
	COMPQUIET(repth, NULL);
	COMPQUIET(lsnp, NULL);
	COMPQUIET(dbt, NULL);
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}

int
__rep_dbenv_close(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

void
__rep_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return;
}

int
__rep_elect(dbenv, nsites, nvotes, priority, timeout, eidp, flags)
	DB_ENV *dbenv;
	int nsites, nvotes, priority;
	u_int32_t timeout, flags;
	int *eidp;
{
	COMPQUIET(nsites, 0);
	COMPQUIET(nvotes, 0);
	COMPQUIET(priority, 0);
	COMPQUIET(timeout, 0);
	COMPQUIET(eidp, NULL);
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}

int
__rep_flush(dbenv)
	DB_ENV *dbenv;
{
	return (__db_norep(dbenv));
}

int
__rep_get_config(dbenv, which, onp)
	DB_ENV *dbenv;
	u_int32_t which;
	int *onp;
{
	COMPQUIET(which, 0);
	COMPQUIET(onp, NULL);
	return (__db_norep(dbenv));
}

int
__rep_set_config(dbenv, which, on)
	DB_ENV *dbenv;
	u_int32_t which;
	int on;
{
	COMPQUIET(which, 0);
	COMPQUIET(on, 0);
	return (__db_norep(dbenv));
}

int
__rep_get_limit(dbenv, gbytesp, bytesp)
	DB_ENV *dbenv;
	u_int32_t *gbytesp, *bytesp;
{
	COMPQUIET(gbytesp, NULL);
	COMPQUIET(bytesp, NULL);
	return (__db_norep(dbenv));
}

int
__rep_get_gen(dbenv, genp)
	DB_ENV *dbenv;
	u_int32_t *genp;
{
	COMPQUIET(genp, NULL);
	return (__db_norep(dbenv));
}

int
__rep_is_client(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

int
__rep_noarchive(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

int
__rep_open(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

int
__rep_preclose(dbenv)
	DB_ENV *dbenv;
{
	return (__db_norep(dbenv));
}

int
__rep_process_message(dbenv, control, rec, eidp, ret_lsnp)
	DB_ENV *dbenv;
	DBT *control, *rec;
	int *eidp;
	DB_LSN *ret_lsnp;
{
	COMPQUIET(control, NULL);
	COMPQUIET(rec, NULL);
	COMPQUIET(eidp, NULL);
	COMPQUIET(ret_lsnp, NULL);
	return (__db_norep(dbenv));
}

int
__rep_region_destroy(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

int
__rep_region_init(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

int
__rep_send_message(dbenv, eid, rtype, lsnp, dbtp, logflags, repflags)
	DB_ENV *dbenv;
	int eid;
	u_int32_t rtype;
	DB_LSN *lsnp;
	const DBT *dbtp;
	u_int32_t logflags, repflags;
{
	COMPQUIET(eid, 0);
	COMPQUIET(rtype, 0);
	COMPQUIET(lsnp, NULL);
	COMPQUIET(dbtp, NULL);
	COMPQUIET(logflags, 0);
	COMPQUIET(repflags, 0);
	return (__db_norep(dbenv));
}

int
__rep_set_limit(dbenv, gbytes, bytes)
	DB_ENV *dbenv;
	u_int32_t gbytes, bytes;
{
	COMPQUIET(gbytes, 0);
	COMPQUIET(bytes, 0);
	return (__db_norep(dbenv));
}

int
__rep_set_rep_transport(dbenv, eid, f_send)
	DB_ENV *dbenv;
	int eid;
	int (*f_send) __P((DB_ENV *, const DBT *, const DBT *, const DB_LSN *,
	    int, u_int32_t));
{
	COMPQUIET(eid, 0);
	COMPQUIET(f_send, NULL);
	return (__db_norep(dbenv));
}

int
__rep_set_request(dbenv, min, max)
	DB_ENV *dbenv;
	u_int32_t min, max;
{
	COMPQUIET(min, 0);
	COMPQUIET(max, 0);
	return (__db_norep(dbenv));
}

int
__rep_start(dbenv, dbt, flags)
	DB_ENV *dbenv;
	DBT *dbt;
	u_int32_t flags;
{
	COMPQUIET(dbt, NULL);
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}

int
__rep_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_REP_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}

int
__rep_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}

int
__rep_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}

int
__rep_sync(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}
#endif /* !HAVE_REPLICATION */
