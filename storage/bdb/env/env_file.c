/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_file.c,v 1.11 2004/03/24 20:51:38 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"

static int __db_overwrite_pass __P((DB_ENV *,
	       const char *, DB_FH *, u_int32_t, u_int32_t, int));

/*
 * __db_fileinit --
 *	Initialize a regular file, optionally zero-filling it as well.
 *
 * PUBLIC: int __db_fileinit __P((DB_ENV *, DB_FH *, size_t, int));
 */
int
__db_fileinit(dbenv, fhp, size, zerofill)
	DB_ENV *dbenv;
	DB_FH *fhp;
	size_t size;
	int zerofill;
{
	db_pgno_t pages;
	size_t i;
	size_t nw;
	u_int32_t relative;
	int ret;
	char buf[OS_VMPAGESIZE];

	/* Write nuls to the new bytes. */
	memset(buf, 0, sizeof(buf));

	/*
	 * Extend the region by writing the last page.  If the region is >4Gb,
	 * increment may be larger than the maximum possible seek "relative"
	 * argument, as it's an unsigned 32-bit value.  Break the offset into
	 * pages of 1MB each so that we don't overflow (2^20 + 2^32 is bigger
	 * than any memory I expect to see for awhile).
	 */
	if ((ret = __os_seek(dbenv, fhp, 0, 0, 0, 0, DB_OS_SEEK_END)) != 0)
		return (ret);
	pages = (db_pgno_t)((size - OS_VMPAGESIZE) / MEGABYTE);
	relative = (u_int32_t)((size - OS_VMPAGESIZE) % MEGABYTE);
	if ((ret = __os_seek(dbenv,
	    fhp, MEGABYTE, pages, relative, 0, DB_OS_SEEK_CUR)) != 0)
		return (ret);
	if ((ret = __os_write(dbenv, fhp, buf, sizeof(buf), &nw)) != 0)
		return (ret);

	/*
	 * We may want to guarantee that there is enough disk space for the
	 * file, so we also write a byte to each page.  We write the byte
	 * because reading it is insufficient on systems smart enough not to
	 * instantiate disk pages to satisfy a read (e.g., Solaris).
	 */
	if (zerofill) {
		pages = (db_pgno_t)(size / MEGABYTE);
		relative = (u_int32_t)(size % MEGABYTE);
		if ((ret = __os_seek(dbenv, fhp,
		    MEGABYTE, pages, relative, 1, DB_OS_SEEK_END)) != 0)
			return (ret);

		/* Write a byte to each page. */
		for (i = 0; i < size; i += OS_VMPAGESIZE) {
			if ((ret = __os_write(dbenv, fhp, buf, 1, &nw)) != 0)
				return (ret);
			if ((ret = __os_seek(dbenv, fhp,
			    0, 0, OS_VMPAGESIZE - 1, 0, DB_OS_SEEK_CUR)) != 0)
				return (ret);
		}
	}
	return (0);
}

/*
 * __db_overwrite  --
 *	Overwrite a file.
 *
 * PUBLIC: int __db_overwrite __P((DB_ENV *, const char *));
 */
int
__db_overwrite(dbenv, path)
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
		if ((ret = __db_overwrite_pass(
		    dbenv, path, fhp, mbytes, bytes, 255)) != 0)
			goto err;
		if ((ret = __db_overwrite_pass(
		    dbenv, path, fhp, mbytes, bytes, 0)) != 0)
			goto err;
		if ((ret = __db_overwrite_pass(
		    dbenv, path, fhp, mbytes, bytes, 255)) != 0)
			goto err;
	} else
		__db_err(dbenv, "%s: %s", path, db_strerror(ret));

err:	if (fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);
	return (ret);
}

/*
 * __db_overwrite_pass --
 *	A single pass over the file, writing the specified byte pattern.
 */
static int
__db_overwrite_pass(dbenv, path, fhp, mbytes, bytes, pattern)
	DB_ENV *dbenv;
	const char *path;
	DB_FH *fhp;
	int pattern;
	u_int32_t mbytes, bytes;
{
	size_t len, nw;
	int i, ret;
	char buf[8 * 1024];

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
