/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: fop_basic.c,v 12.8 2005/10/12 17:52:16 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <string.h>
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/fop.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"
#include "dbinc/db_am.h"

/*
 * The transactional guarantees Berkeley DB provides for file
 * system level operations (database physical file create, delete,
 * rename) are based on our understanding of current file system
 * semantics; a system that does not provide these semantics and
 * guarantees could be in danger.
 *
 * First, as in standard database changes, fsync and fdatasync must
 * work: when applied to the log file, the records written into the
 * log must be transferred to stable storage.
 *
 * Second, it must not be possible for the log file to be removed
 * without previous file system level operations being flushed to
 * stable storage.  Berkeley DB applications write log records
 * describing file system operations into the log, then perform the
 * file system operation, then commit the enclosing transaction
 * (which flushes the log file to stable storage).  Subsequently,
 * a database environment checkpoint may make it possible for the
 * application to remove the log file containing the record of the
 * file system operation.  DB's transactional guarantees for file
 * system operations require the log file removal not succeed until
 * all previous filesystem operations have been flushed to stable
 * storage.  In other words, the flush of the log file, or the
 * removal of the log file, must block until all previous
 * filesystem operations have been flushed to stable storage.  This
 * semantic is not, as far as we know, required by any existing
 * standards document, but we have never seen a filesystem where
 * it does not apply.
 */

/*
 * __fop_create --
 * Create a (transactionally protected) file system object.  This is used
 * to create DB files now, potentially blobs, queue extents and anything
 * else you wish to store in a file system object.
 *
 * PUBLIC: int __fop_create __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, DB_FH **, const char *, APPNAME, int, u_int32_t));
 */
int
__fop_create(dbenv, txn, fhpp, name, appname, mode, flags)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB_FH **fhpp;
	const char *name;
	APPNAME appname;
	int mode;
	u_int32_t flags;
{
	DB_FH *fhp;
	DB_LSN lsn;
	DBT data;
	int ret;
	char *real_name;

	real_name = NULL;
	fhp = NULL;

	if ((ret =
	    __db_appname(dbenv, appname, name, 0, NULL, &real_name)) != 0)
		return (ret);

	if (mode == 0)
		mode = __db_omode(OWNER_RW);

	if (DBENV_LOGGING(dbenv)) {
		memset(&data, 0, sizeof(data));
		data.data = (void *)name;
		data.size = (u_int32_t)strlen(name) + 1;
		if ((ret = __fop_create_log(dbenv, txn, &lsn,
		    flags | DB_FLUSH,
		    &data, (u_int32_t)appname, (u_int32_t)mode)) != 0)
			goto err;
	}

	DB_ENV_TEST_RECOVERY(dbenv, DB_TEST_POSTLOG, ret, name);

	if (fhpp == NULL)
		fhpp = &fhp;
	ret = __os_open(
	    dbenv, real_name, DB_OSO_CREATE | DB_OSO_EXCL, mode, fhpp);

err:
DB_TEST_RECOVERY_LABEL
	if (fhpp == &fhp && fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);
	if (real_name != NULL)
		__os_free(dbenv, real_name);
	return (ret);
}

/*
 * __fop_remove --
 *	Remove a file system object.
 *
 * PUBLIC: int __fop_remove __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, u_int8_t *, const char *, APPNAME, u_int32_t));
 */
int
__fop_remove(dbenv, txn, fileid, name, appname, flags)
	DB_ENV *dbenv;
	DB_TXN *txn;
	u_int8_t *fileid;
	const char *name;
	APPNAME appname;
	u_int32_t flags;
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
		if (fileid != NULL && (ret = __memp_nameop(
		    dbenv, fileid, NULL, real_name, NULL, 0)) != 0)
			goto err;
	} else {
		if (DBENV_LOGGING(dbenv)) {
			memset(&fdbt, 0, sizeof(ndbt));
			fdbt.data = fileid;
			fdbt.size = fileid == NULL ? 0 : DB_FILE_ID_LEN;
			memset(&ndbt, 0, sizeof(ndbt));
			ndbt.data = (void *)name;
			ndbt.size = (u_int32_t)strlen(name) + 1;
			if ((ret = __fop_remove_log(dbenv, txn, &lsn,
			    flags, &ndbt, &fdbt, (u_int32_t)appname)) != 0)
				goto err;
		}
		ret = __txn_remevent(dbenv, txn, real_name, fileid, 0);
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
 * PUBLIC: int __fop_write __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, const char *, APPNAME, DB_FH *, u_int32_t, db_pgno_t,
 * PUBLIC:     u_int32_t, u_int8_t *, u_int32_t, u_int32_t, u_int32_t));
 */
int
__fop_write(dbenv,
    txn, name, appname, fhp, pgsize, pageno, off, buf, size, istmp, flags)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *name;
	APPNAME appname;
	DB_FH *fhp;
	u_int32_t pgsize;
	db_pgno_t pageno;
	u_int32_t off;
	u_int8_t *buf;
	u_int32_t size, istmp, flags;
{
	DB_LSN lsn;
	DBT data, namedbt;
	size_t nbytes;
	int local_open, ret, t_ret;
	char *real_name;

	DB_ASSERT(istmp != 0);

	ret = local_open = 0;
	real_name = NULL;

	if ((ret =
	    __db_appname(dbenv, appname, name, 0, NULL, &real_name)) != 0)
		return (ret);

	if (DBENV_LOGGING(dbenv)) {
		memset(&data, 0, sizeof(data));
		data.data = buf;
		data.size = size;
		memset(&namedbt, 0, sizeof(namedbt));
		namedbt.data = (void *)name;
		namedbt.size = (u_int32_t)strlen(name) + 1;
		if ((ret = __fop_write_log(dbenv, txn,
		    &lsn, flags, &namedbt, (u_int32_t)appname,
		    pgsize, pageno, off, &data, istmp)) != 0)
			goto err;
	}

	if (fhp == NULL) {
		/* File isn't open; we need to reopen it. */
		if ((ret = __os_open(dbenv, real_name, 0, 0, &fhp)) != 0)
			goto err;
		local_open = 1;
	}

	/* Seek to offset. */
	if ((ret = __os_seek(dbenv,
	    fhp, pgsize, pageno, off, 0, DB_OS_SEEK_SET)) != 0)
		goto err;

	/* Now do the write. */
	if ((ret = __os_write(dbenv, fhp, buf, size, &nbytes)) != 0)
		goto err;

err:	if (local_open &&
	    (t_ret = __os_closehandle(dbenv, fhp)) != 0 && ret == 0)
			ret = t_ret;

	if (real_name != NULL)
		__os_free(dbenv, real_name);
	return (ret);
}

/*
 * __fop_rename --
 *	Change a file's name.
 *
 * PUBLIC: int __fop_rename __P((DB_ENV *, DB_TXN *,
 * PUBLIC:      const char *, const char *, u_int8_t *, APPNAME, u_int32_t));
 */
int
__fop_rename(dbenv, txn, oldname, newname, fid, appname, flags)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *oldname;
	const char *newname;
	u_int8_t *fid;
	APPNAME appname;
	u_int32_t flags;
{
	DB_LSN lsn;
	DBT fiddbt, new, old;
	int ret;
	char *n, *o;

	o = n = NULL;
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
		if ((ret = __fop_rename_log(dbenv, txn, &lsn, flags | DB_FLUSH,
		    &old, &new, &fiddbt, (u_int32_t)appname)) != 0)
			goto err;
	}

	ret = __memp_nameop(dbenv, fid, newname, o, n, 0);

err:	if (o != NULL)
		__os_free(dbenv, o);
	if (n != NULL)
		__os_free(dbenv, n);
	return (ret);
}
