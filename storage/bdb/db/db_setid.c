/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_setid.c,v 1.6 2004/09/24 13:41:08 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_swap.h"
#include "dbinc/db_am.h"

/*
 * __db_fileid_reset --
 *	Reset the file IDs for every database in the file.
 *
 * PUBLIC: int __db_fileid_reset __P((DB_ENV *, char *, int));
 */
int
__db_fileid_reset(dbenv, name, passwd)
	DB_ENV *dbenv;
	char *name;
	int passwd;
{
	DB *dbp;
	DBC *dbcp;
	DBT key, data;
	DB_MPOOLFILE *mpf;
	db_pgno_t pgno;
	int t_ret, ret;
	void *pagep;
	char *real_name;
	u_int8_t fileid[DB_FILE_ID_LEN];

	dbp = NULL;
	dbcp = NULL;
	real_name = NULL;

	/* Get the real backing file name. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		return (ret);

	/* Get a new file ID. */
	if ((ret = __os_fileid(dbenv, real_name, 1, fileid)) != 0) {
		dbenv->err(dbenv, ret, "unable to get new file ID");
		goto err;
	}

	/* Create the DB object. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		goto err;
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

	mpf = dbp->mpf;

	pgno = PGNO_BASE_MD;
	if ((ret = mpf->get(mpf, &pgno, 0, &pagep)) != 0) {
		dbp->err(dbp, ret,
		    "%s: DB_MPOOLFILE->get: %lu", name, (u_long)pgno);
		goto err;
	}
	memcpy(((DBMETA *)pagep)->uid, fileid, DB_FILE_ID_LEN);
	if ((ret = mpf->put(mpf, pagep, DB_MPOOL_DIRTY)) != 0) {
		dbp->err(dbp, ret,
		    "%s: DB_MPOOLFILE->put: %lu", name, (u_long)pgno);
		goto err;
	}

	/*
	 * If the database file doesn't support subdatabases, we only have
	 * to update a single metadata page.  Otherwise, we have to open a
	 * cursor and step through the master database, and update all of
	 * the subdatabases' metadata pages.
	 */
	if (!F_ISSET(dbp, DB_AM_SUBDB))
		goto err;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		goto err;
	}
	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
		/*
		 * XXX
		 * We're handling actual data, not on-page meta-data, so it
		 * hasn't been converted to/from opposite endian architectures.
		 * Do it explicitly, now.
		 */
		memcpy(&pgno, data.data, sizeof(db_pgno_t));
		DB_NTOHL(&pgno);
		if ((ret = mpf->get(mpf, &pgno, 0, &pagep)) != 0) {
			dbp->err(dbp, ret,
			    "%s: DB_MPOOLFILE->get: %lu", name, (u_long)pgno);
			goto err;
		}
		memcpy(((DBMETA *)pagep)->uid, fileid, DB_FILE_ID_LEN);
		if ((ret = mpf->put(mpf, pagep, DB_MPOOL_DIRTY)) != 0) {
			dbp->err(dbp, ret,
			    "%s: DB_MPOOLFILE->put: %lu", name, (u_long)pgno);
			goto err;
		}
	}
	if (ret == DB_NOTFOUND)
		ret = 0;
	else
		dbp->err(dbp, ret, "DBcursor->get");

err:	if (dbcp != NULL && (t_ret = dbcp->c_close(dbcp)) != 0) {
		dbp->err(dbp, ret, "DBcursor->close");
		if (ret == 0)
			ret = t_ret;
	}
	if (dbp != NULL && (t_ret = dbp->close(dbp, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->close");
		if (ret == 0)
			ret = t_ret;
	}
	if (real_name != NULL)
		__os_free(dbenv, real_name);

	return (ret);
}
