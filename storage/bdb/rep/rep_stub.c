/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_stub.c,v 1.22 2004/09/29 15:36:04 bostic Exp $
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
static int __rep_elect
    __P((DB_ENV *, int, int, int, u_int32_t, int *, u_int32_t));
static int __rep_flush __P((DB_ENV *));
static int __rep_start __P((DB_ENV *, DBT *, u_int32_t));
static int __rep_get_limit __P((DB_ENV *, u_int32_t *, u_int32_t *));
static int __rep_set_limit __P((DB_ENV *, u_int32_t, u_int32_t));
static int __rep_set_request __P((DB_ENV *, u_int32_t, u_int32_t));
static int __rep_set_rep_transport __P((DB_ENV *, int, int (*)
    (DB_ENV *, const DBT *, const DBT *, const DB_LSN *, int, u_int32_t)));

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

void
__env_rep_enter(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return;
}

void
__env_db_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return;
}

void
__op_rep_enter(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return;
}

void
__op_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return;
}

int
__rep_dbenv_close(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

void
__rep_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	dbenv->rep_elect = __rep_elect;
	dbenv->rep_flush = __rep_flush;
	dbenv->rep_process_message = __rep_process_message;
	dbenv->rep_start = __rep_start;
	dbenv->rep_stat = __rep_stat_pp;
	dbenv->rep_stat_print = __rep_stat_print_pp;
	dbenv->get_rep_limit = __rep_get_limit;
	dbenv->set_rep_limit = __rep_set_limit;
	dbenv->set_rep_request = __rep_set_request;
	dbenv->set_rep_transport = __rep_set_rep_transport;
}

void
__rep_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return;
}

static int
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

static int
__rep_flush(dbenv)
	DB_ENV *dbenv;
{
	return (__db_norep(dbenv));
}

static int
__rep_get_limit(dbenv, gbytesp, bytesp)
	DB_ENV *dbenv;
	u_int32_t *gbytesp, *bytesp;
{
	COMPQUIET(gbytesp, NULL);
	COMPQUIET(bytesp, NULL);
	return (__db_norep(dbenv));
}

void
__rep_get_gen(dbenv, genp)
	DB_ENV *dbenv;
	u_int32_t *genp;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(genp, NULL);
	return;
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
__rep_preclose(dbenv, do_closefiles)
	DB_ENV *dbenv;
	int do_closefiles;
{
	COMPQUIET(do_closefiles, 0);
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
__rep_send_message(dbenv, eid, rtype, lsnp, dbtp, flags)
	DB_ENV *dbenv;
	int eid;
	u_int32_t rtype;
	DB_LSN *lsnp;
	const DBT *dbtp;
	u_int32_t flags;
{
	COMPQUIET(eid, 0);
	COMPQUIET(rtype, 0);
	COMPQUIET(lsnp, NULL);
	COMPQUIET(dbtp, NULL);
	COMPQUIET(flags, 0);
	return (__db_norep(dbenv));
}

static int
__rep_set_limit(dbenv, gbytes, bytes)
	DB_ENV *dbenv;
	u_int32_t gbytes, bytes;
{
	COMPQUIET(gbytes, 0);
	COMPQUIET(bytes, 0);
	return (__db_norep(dbenv));
}

static int
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

static int
__rep_set_request(dbenv, min, max)
	DB_ENV *dbenv;
	u_int32_t min, max;
{
	COMPQUIET(min, 0);
	COMPQUIET(max, 0);
	return (__db_norep(dbenv));
}

static int
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
#endif /* !HAVE_REPLICATION */
