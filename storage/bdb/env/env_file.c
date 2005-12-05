/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_file.c,v 12.3 2005/06/16 20:21:56 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"

/*
 * __db_file_extend --
 *	Initialize a regular file by writing the last page of the file.
 *
 * PUBLIC: int __db_file_extend __P((DB_ENV *, DB_FH *, size_t));
 */
int
__db_file_extend(dbenv, fhp, size)
	DB_ENV *dbenv;
	DB_FH *fhp;
	size_t size;
{
	db_pgno_t pages;
	size_t nw;
	u_int32_t relative;
	int ret;
	char buf[8 * 1024];

	/*
	 * Extend the file by writing the last page.  If the region is >4Gb,
	 * increment may be larger than the maximum possible seek "relative"
	 * argument, as it's an unsigned 32-bit value.  Break the offset into
	 * pages of 1MB each so we don't overflow -- (2^20 + 2^32 is bigger
	 * than any memory I expect to see for awhile).
	 */
	memset(buf, 0, sizeof(buf));

	if ((ret = __os_seek(dbenv, fhp, 0, 0, 0, 0, DB_OS_SEEK_END)) != 0)
		return (ret);
	pages = (db_pgno_t)((size - sizeof(buf)) / MEGABYTE);
	relative = (u_int32_t)((size - sizeof(buf)) % MEGABYTE);
	if ((ret = __os_seek(dbenv,
	    fhp, MEGABYTE, pages, relative, 0, DB_OS_SEEK_CUR)) != 0)
		return (ret);
	if ((ret = __os_write(dbenv, fhp, buf, sizeof(buf), &nw)) != 0)
		return (ret);

	return (0);
}

/*
 * __db_file_multi_write  --
 *	Overwrite a file with multiple passes to corrupt the data.
 *
 * PUBLIC: int __db_file_multi_write __P((DB_ENV *, const char *));
 */
int
__db_file_multi_write(dbenv, path)
	DB_ENV *dbenv;
	const char *path;
{
	DB_FH *fhp;
	u_int32_t mbytes, bytes;
	int ret;

	if ((ret = __os_open(dbenv, path, DB_OSO_REGION, 0, &fhp)) == 0 &&
	    (ret = __os_ioinfo(dbenv, path, fhp, &mbytes, &bytes, NULL)) == 0) {
		/*
		 * !!!
		 * Overwrite a regular file with alternating 0xff, 0x00 and 0xff
		 * byte patterns.  Implies a fixed-block filesystem, journaling
		 * or logging filesystems will require operating system support.
		 */
		if ((ret = __db_file_write(
		    dbenv, path, fhp, mbytes, bytes, 255)) != 0)
			goto err;
		if ((ret = __db_file_write(
		    dbenv, path, fhp, mbytes, bytes, 0)) != 0)
			goto err;
		if ((ret = __db_file_write(
		    dbenv, path, fhp, mbytes, bytes, 255)) != 0)
			goto err;
	} else
		__db_err(dbenv, "%s: %s", path, db_strerror(ret));

err:	if (fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);
	return (ret);
}

/*
 * __db_file_write --
 *	A single pass over the file, writing the specified byte pattern.
 *
 * PUBLIC: int __db_file_write __P((DB_ENV *,
 * PUBLIC:     const char *, DB_FH *, u_int32_t, u_int32_t, int));
 */
int
__db_file_write(dbenv, path, fhp, mbytes, bytes, pattern)
	DB_ENV *dbenv;
	const char *path;
	DB_FH *fhp;
	int pattern;
	u_int32_t mbytes, bytes;
{
	size_t len, nw;
	int i, ret;
	char buf[32 * 1024];

	if ((ret = __os_seek(dbenv, fhp, 0, 0, 0, 0, DB_OS_SEEK_SET)) != 0)
		goto err;

	memset(buf, pattern, sizeof(buf));

	for (; mbytes > 0; --mbytes)
		for (i = MEGABYTE / sizeof(buf); i > 0; --i)
			if ((ret =
			    __os_write(dbenv, fhp, buf, sizeof(buf), &nw)) != 0)
				goto err;
	for (; bytes > 0; bytes -= (u_int32_t)len) {
		len = bytes < sizeof(buf) ? bytes : sizeof(buf);
		if ((ret = __os_write(dbenv, fhp, buf, len, &nw)) != 0)
			goto err;
	}

	if ((ret = __os_fsync(dbenv, fhp)) != 0)
err:		__db_err(dbenv, "%s: %s", path, db_strerror(ret));

	return (ret);
}
