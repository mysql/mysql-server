/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: fop_basic.c,v 1.23 2002/08/11 02:11:23 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <string.h>
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/db_page.h"
#include "dbinc/fop.h"
#include "dbinc/txn.h"
#include "dbinc/db_am.h"

/*
 * This file implements the basic file-level operations.  This code
 * ought to be fairly independent of DB, other than through its
 * error-reporting mechanism.
 */

/*
 * __fop_create --
 * Create a (transactionally protected) file system object.  This is used
 * to create DB files now, potentially blobs, queue extents and anything
 * else you wish to store in a file system object.
 *
 * PUBLIC: int __fop_create __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, DB_FH *, const char *, APPNAME, int));
 */
int
__fop_create(dbenv, txn, fhp, name, appname, mode)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB_FH *fhp;
	const char *name;
	APPNAME appname;
	int mode;
{
	DB_FH fh;
	DB_LSN lsn;
	DBT data;
	char *real_name;
	int do_close, ret;

	ret = 0;
	real_name = NULL;

	if (fhp != NULL)
		do_close = 0;
	else {
		fhp = &fh;
		memset(fhp, 0, sizeof(fh));
		do_close = 1;
	}

	if (mode == 0)
		mode = __db_omode("rw----");

	if ((ret =
	    __db_appname(dbenv, appname, name, 0, NULL, &real_name)) != 0)
		goto err;

	if (DBENV_LOGGING(dbenv)) {
		memset(&data, 0, sizeof(data));
		data.data = (void *)name;
		data.size = (u_int32_t)strlen(name) + 1;
		if ((ret = __fop_create_log(dbenv,
		    txn, &lsn, DB_FLUSH, &data, (u_int32_t)appname, mode)) != 0)
			goto err;
	}

	DB_ENV_TEST_RECOVERY(dbenv, DB_TEST_POSTLOG, ret, name);

	ret =
	    __os_open(dbenv, real_name, DB_OSO_CREATE | DB_OSO_EXCL, mode, fhp);

err:
DB_TEST_RECOVERY_LABEL
	if (do_close && F_ISSET(fhp, DB_FH_VALID))
		__os_closehandle(dbenv, fhp);
	if (real_name != NULL)
		__os_free(dbenv, real_name);
	return (ret);
}

/*
 * __fop_remove --
 *	Remove a file system object.
 *
 * PUBLIC: int __fop_remove __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, u_int8_t *, const char *, APPNAME));
 */
int
__fop_remove(dbenv, txn, fileid, name, appname)
	DB_ENV *dbenv;
	DB_TXN *txn;
	u_int8_t *fileid;
	const char *name;
	APPNAME appname;
{
	DB_LSN lsn;
	DBT fdbt, ndbt;
	char *real_name;
	int ret;

	real_name = NULL;

	if ((ret =
	    __db_appname(dbenv, appname, name, 0, NULL, &real_name)) != 0)
		goto err;

	if (txn == NULL) {
		if (fileid != NULL && (ret = dbenv->memp_nameop(
		    dbenv, fileid, NULL, real_name, NULL)) != 0)
			goto err;
	} else {
		if (DBENV_LOGGING(dbenv)) {
			memset(&fdbt, 0, sizeof(ndbt));
			fdbt.data = fileid;
			fdbt.size = fileid == NULL ? 0 : DB_FILE_ID_LEN;
			memset(&ndbt, 0, sizeof(ndbt));
			ndbt.data = (void *)name;
			ndbt.size = (u_int32_t)strlen(name) + 1;
			if ((ret = __fop_remove_log(dbenv,
			    txn, &lsn, 0, &ndbt, &fdbt, appname)) != 0)
				goto err;
		}
		ret = __txn_remevent(dbenv, txn, real_name, fileid);
	}

err:	if (real_name != NULL)
		__os_free(dbenv, real_name);
	return (ret);
}

/*
 * __fop_write
 *
 * Write "size" bytes from "buf" to file "name" beginning at offset "off."
 * If the file is open, supply a handle in fhp.  Istmp indicate if this is
 * an operation that needs to be undone in the face of failure (i.e., if
 * this is a write to a temporary file, we're simply going to remove the
 * file, so don't worry about undoing the write).
 *
 * Currently, we *only* use this with istmp true.  If we need more general
 * handling, then we'll have to zero out regions on abort (and possibly
 * log the before image of the data in the log record).
 *
 * PUBLIC: int __fop_write __P((DB_ENV *, DB_TXN *, const char *, APPNAME,
 * PUBLIC:     DB_FH *, u_int32_t, u_int8_t *, u_int32_t, u_int32_t));
 */
int
__fop_write(dbenv, txn, name, appname, fhp, off, buf, size, istmp)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *name;
	APPNAME appname;
	DB_FH *fhp;
	u_int32_t off;
	u_int8_t *buf;
	u_int32_t size, istmp;
{
	DB_FH fh;
	DB_LSN lsn;
	DBT data, namedbt;
	char *real_name;
	int ret, t_ret, we_opened;
	size_t nbytes;

	ret = 0;
	we_opened = 0;
	real_name = NULL;

	if ((ret =
	    __db_appname(dbenv, appname, name, 0, NULL, &real_name)) != 0)
		goto err;

	if (DBENV_LOGGING(dbenv)) {
		memset(&data, 0, sizeof(data));
		data.data = buf;
		data.size = size;
		memset(&namedbt, 0, sizeof(namedbt));
		namedbt.data = (void *)name;
		namedbt.size = (u_int32_t)strlen(name) + 1;
		if ((ret = __fop_write_log(dbenv,
		    txn, &lsn, 0, &namedbt, appname, off, &data, istmp)) != 0)
			goto err;
	}

	if (fhp == NULL) {
		/* File isn't open; we need to reopen it. */
		if ((ret = __os_open(dbenv, real_name, 0, 0, &fh)) != 0)
			goto err;
		fhp = &fh;
		we_opened = 1;
	} else
		we_opened = 0;

	/* Seek to offset. */
	if ((ret = __os_seek(dbenv, fhp, 0, 0, off, 0, DB_OS_SEEK_SET)) != 0)
		goto err;

	/* Now do the write. */
	if ((ret = __os_write(dbenv, fhp, buf, size, &nbytes)) != 0)
		goto err;

err:	if (we_opened)
		if ((t_ret = __os_closehandle(dbenv, fhp)) != 0 && ret == 0)
			ret = t_ret;

	if (real_name != NULL)
		__os_free(dbenv, real_name);
	return (ret);
}

/*
 * __fop_rename --
 *	Change a file's name.
 *
 * PUBLIC: int __fop_rename __P((DB_ENV *,
 * PUBLIC:      DB_TXN *, const char *, const char *, u_int8_t *, APPNAME));
 */
int
__fop_rename(dbenv, txn, oldname, newname, fid, appname)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *oldname;
	const char *newname;
	u_int8_t *fid;
	APPNAME appname;
{
	DB_LSN lsn;
	DBT fiddbt, new, old;
	int ret;
	char *n, *o;

	if ((ret = __db_appname(dbenv, appname, oldname, 0, NULL, &o)) != 0)
		goto err;
	if ((ret = __db_appname(dbenv, appname, newname, 0, NULL, &n)) != 0)
		goto err;

	if (DBENV_LOGGING(dbenv)) {
		memset(&old, 0, sizeof(old));
		memset(&new, 0, sizeof(new));
		memset(&fiddbt, 0, sizeof(fiddbt));
		old.data = (void *)oldname;
		old.size = (u_int32_t)strlen(oldname) + 1;
		new.data = (void *)newname;
		new.size = (u_int32_t)strlen(newname) + 1;
		fiddbt.data = fid;
		fiddbt.size = DB_FILE_ID_LEN;
		if ((ret = __fop_rename_log(dbenv, txn, &lsn,
		    DB_FLUSH, &old, &new, &fiddbt, (u_int32_t)appname)) != 0)
			goto err;
	}

	ret = dbenv->memp_nameop(dbenv, fid, newname, o, n);

err:	if (o != oldname)
		__os_free(dbenv, o);
	if (n != newname)
		__os_free(dbenv, n);
	return (ret);
}
