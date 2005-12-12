/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_setid.c,v 12.8 2005/10/18 14:17:08 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_swap.h"
#include "dbinc/db_am.h"
#include "dbinc/mp.h"

static int __env_fileid_reset __P((DB_ENV *, const char *, int));

/*
 * __env_fileid_reset_pp --
 *	DB_ENV->fileid_reset pre/post processing.
 *
 * PUBLIC: int __env_fileid_reset_pp __P((DB_ENV *, const char *, u_int32_t));
 */
int
__env_fileid_reset_pp(dbenv, name, flags)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int handle_check, ret, t_ret;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->fileid_reset");

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline, outside of
	 * the replication block.
	 */
	if (flags != 0 && flags != DB_ENCRYPT)
		return (__db_ferr(dbenv, "DB_ENV->fileid_reset", 0));

	ENV_ENTER(dbenv, ip);

	/* Check for replication block. */
	handle_check = IS_ENV_REPLICATED(dbenv);
	if (handle_check && (ret = __env_rep_enter(dbenv, 1)) != 0)
		goto err;

	ret = __env_fileid_reset(dbenv, name, LF_ISSET(DB_ENCRYPT) ? 1 : 0);

	if (handle_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && ret == 0)
		ret = t_ret;

err:	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __env_fileid_reset --
 *	Reset the file IDs for every database in the file.
 */
static int
__env_fileid_reset(dbenv, name, encrypted)
	DB_ENV *dbenv;
	const char *name;
	int encrypted;
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
	if ((ret =
	    __db_appname(dbenv, DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		return (ret);

	/* Get a new file ID. */
	if ((ret = __os_fileid(dbenv, real_name, 1, fileid)) != 0)
		goto err;

	/* Create the DB object. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		goto err;

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

	mpf = dbp->mpf;

	pgno = PGNO_BASE_MD;
	if ((ret = __memp_fget(mpf, &pgno, 0, &pagep)) != 0)
		goto err;
	memcpy(((DBMETA *)pagep)->uid, fileid, DB_FILE_ID_LEN);
	if ((ret = __memp_fput(mpf, pagep, DB_MPOOL_DIRTY)) != 0)
		goto err;

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
	if ((ret = __db_cursor(dbp, NULL, &dbcp, 0)) != 0)
		goto err;
	while ((ret = __db_c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
		/*
		 * XXX
		 * We're handling actual data, not on-page meta-data, so it
		 * hasn't been converted to/from opposite endian architectures.
		 * Do it explicitly, now.
		 */
		memcpy(&pgno, data.data, sizeof(db_pgno_t));
		DB_NTOHL(&pgno);
		if ((ret = __memp_fget(mpf, &pgno, 0, &pagep)) != 0)
			goto err;
		memcpy(((DBMETA *)pagep)->uid, fileid, DB_FILE_ID_LEN);
		if ((ret = __memp_fput(mpf, pagep, DB_MPOOL_DIRTY)) != 0)
			goto err;
	}
	if (ret == DB_NOTFOUND)
		ret = 0;

err:	if (dbcp != NULL && (t_ret = __db_c_close(dbcp)) != 0 && ret == 0)
		ret = t_ret;
	if (dbp != NULL && (t_ret = __db_close(dbp, NULL, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (real_name != NULL)
		__os_free(dbenv, real_name);

	return (ret);
}
