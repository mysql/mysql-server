/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_setlsn.c,v 12.8 2005/10/21 19:17:40 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/mp.h"

static int __env_lsn_reset __P((DB_ENV *, const char *, int));

/*
 * __env_lsn_reset_pp --
 *	DB_ENV->lsn_reset pre/post processing.
 *
 * PUBLIC: int __env_lsn_reset_pp __P((DB_ENV *, const char *, u_int32_t));
 */
int
__env_lsn_reset_pp(dbenv, name, flags)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int handle_check, ret, t_ret;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->lsn_reset");

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline, outside of
	 * the replication block.
	 */
	if (flags != 0 && flags != DB_ENCRYPT)
		return (__db_ferr(dbenv, "DB_ENV->lsn_reset", 0));

	ENV_ENTER(dbenv, ip);

	/* Check for replication block. */
	handle_check = IS_ENV_REPLICATED(dbenv);
	if (handle_check && (ret = __env_rep_enter(dbenv, 1)) != 0)
		goto err;

	ret = __env_lsn_reset(dbenv, name, LF_ISSET(DB_ENCRYPT) ? 1 : 0);

	if (handle_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && ret == 0)
		ret = t_ret;

err:	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __env_lsn_reset --
 *	Reset the LSNs for every page in the file.
 */
static int
__env_lsn_reset(dbenv, name, encrypted)
	DB_ENV *dbenv;
	const char *name;
	int encrypted;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	int t_ret, ret;

	/* Create the DB object. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		return (ret);

	/* If configured with a password, the databases are encrypted. */
	if (encrypted && (ret = __db_set_flags(dbp, DB_ENCRYPT)) != 0)
		goto err;

	/*
	 * Open the DB file.
	 *
	 * !!!
	 * Note DB_RDWRMASTER flag, we need to open the master database file
	 * for writing in this case.
	 */
	if ((ret = __db_open(dbp, NULL,
	    name, NULL, DB_UNKNOWN, DB_RDWRMASTER, 0, PGNO_BASE_MD)) != 0)
		goto err;

	/* Reset the LSN on every page of the database file. */
	mpf = dbp->mpf;
	for (pgno = 0;
	    (ret = __memp_fget(mpf, &pgno, 0, &pagep)) == 0; ++pgno) {
		LSN_NOT_LOGGED(pagep->lsn);
		if ((ret = __memp_fput(mpf, pagep, DB_MPOOL_DIRTY)) != 0)
			goto err;
	}

	if (ret == DB_PAGE_NOTFOUND)
		ret = 0;

err:	if ((t_ret = __db_close(dbp, NULL, 0)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}
