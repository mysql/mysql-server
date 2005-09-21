/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_stat.c,v 11.32 2004/10/07 14:00:11 carol Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#endif

#include "db_int.h"

/*
 * __os_exists --
 *	Return if the file exists.
 */
int
__os_exists(path, isdirp)
	const char *path;
	int *isdirp;
{
	int ret;
	DWORD attrs;
	_TCHAR *tpath;

	if (DB_GLOBAL(j_exists) != NULL)
		return (DB_GLOBAL(j_exists)(path, isdirp));

	TO_TSTRING(NULL, path, tpath, ret);
	if (ret != 0)
		return (ret);

	RETRY_CHK(
	    ((attrs = GetFileAttributes(tpath)) == (DWORD)-1 ? 1 : 0), ret);

	if (ret == 0 && isdirp != NULL)
		*isdirp = (attrs & FILE_ATTRIBUTE_DIRECTORY);

	FREE_STRING(NULL, tpath);
	return (ret);
}

/*
 * __os_ioinfo --
 *	Return file size and I/O size; abstracted to make it easier
 *	to replace.
 */
int
__os_ioinfo(dbenv, path, fhp, mbytesp, bytesp, iosizep)
	DB_ENV *dbenv;
	const char *path;
	DB_FH *fhp;
	u_int32_t *mbytesp, *bytesp, *iosizep;
{
	int ret;
	BY_HANDLE_FILE_INFORMATION bhfi;
	unsigned __int64 filesize;

	if (DB_GLOBAL(j_ioinfo) != NULL)
		return (DB_GLOBAL(j_ioinfo)(path,
		    fhp->fd, mbytesp, bytesp, iosizep));

	RETRY_CHK((!GetFileInformationByHandle(fhp->handle, &bhfi)), ret);
	if (ret != 0) {
		__db_err(dbenv,
		    "GetFileInformationByHandle: %s", strerror(ret));
		return (ret);
	}

	filesize = ((unsigned __int64)bhfi.nFileSizeHigh << 32) +
	    bhfi.nFileSizeLow;

	/* Return the size of the file. */
	if (mbytesp != NULL)
		*mbytesp = (u_int32_t)(filesize / MEGABYTE);
	if (bytesp != NULL)
		*bytesp = (u_int32_t)(filesize % MEGABYTE);

	/*
	 * The filesystem blocksize is not easily available.  In particular,
	 * the values returned by GetDiskFreeSpace() are not very helpful
	 * (NTFS volumes often report 512B clusters, which are too small to
	 * be a useful default).
	 */
	if (iosizep != NULL)
		*iosizep = DB_DEF_IOSIZE;
	return (0);
}
