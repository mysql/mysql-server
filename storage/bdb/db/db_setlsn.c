/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_setlsn.c,v 1.2 2004/04/27 16:10:13 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"

/*
 * __db_lsn_reset --
 *	Reset the LSNs for every page in the file.
 *
 * PUBLIC: int __db_lsn_reset __P((DB_ENV *, char *, int));
 */
int
__db_lsn_reset(dbenv, name, passwd)
	DB_ENV *dbenv;
	char *name;
	int passwd;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	int t_ret, ret;

	/* Create the DB object. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (1);
	}

	/* If configured with a password, the databases are encrypted. */
	if (passwd && (ret = dbp->set_flags(dbp, DB_ENCRYPT)) != 0) {
		dbp->err(dbp, ret, "DB->set_flags: DB_ENCRYPT");
		goto err;
	}

	/*
	 * Open the DB file.
	 *
	 * !!!
	 * Note DB_RDWRMASTER flag, we need to open the master database file
	 * for writing in this case.
	 */
	if ((ret = dbp->open(dbp,
	    NULL, name, NULL, DB_UNKNOWN, DB_RDWRMASTER, 0)) != 0) {
		dbp->err(dbp, ret, "DB->open: %s", name);
		goto err;
	}

	/* Reset the LSN on every page of the database file. */
	mpf = dbp->mpf;
	for (pgno = 0; (ret = mpf->get(mpf, &pgno, 0, &pagep)) == 0; ++pgno) {
		LSN_NOT_LOGGED(pagep->lsn);
		if ((ret = mpf->put(mpf, pagep, DB_MPOOL_DIRTY)) != 0) {
			dbp->err(dbp, ret, "DB_MPOOLFILE->put: %s", name);
			goto err;
		}
	}

	if (ret == DB_PAGE_NOTFOUND)
		ret = 0;
	else
		dbp->err(dbp, ret, "DB_MPOOLFILE->get: %s", name);

err:	if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0)
		ret = t_ret;
	return (ret == 0 ? 0 : 1);
}
