/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_errno.c,v 11.5 2000/11/30 00:58:43 ubell Exp $";
#endif /* not lint */

#include "db_int.h"

/*
 * __os_get_errno --
 *	Return the value of errno.
 */
int
__os_get_errno()
{
	/* This routine must be able to return the same value repeatedly. */
	return (errno);
}

/*
 * __os_set_errno --
 *	Set the value of errno.
 */
void
__os_set_errno(evalue)
	int evalue;
{
	errno = evalue;
}

/*
 * __os_win32_errno --
 *	Return the last Windows error as an errno.
 *	We give generic error returns:
 *
 *	EFAULT means Win* call failed,
 *	  and GetLastError provided no extra info.
 *
 *	EIO means error on Win* call.
 *	  and we were unable to provide a meaningful errno for this Windows
 *	  error.  More information is only available by setting a breakpoint
 *	  here.
 *
 * PUBLIC: #if defined(DB_WIN32)
 * PUBLIC: int __os_win32_errno __P((void));
 * PUBLIC: #endif
 */
int
__os_win32_errno(void)
{
	DWORD last_error;
	int ret;

	/*
	 * It's possible that errno was set after the error.
	 * The caller must take care to set it to 0 before
	 * any system operation.
	 */
	if (__os_get_errno() != 0)
		return (__os_get_errno());

	last_error = GetLastError();

	/*
	 * Take our best guess at translating some of the Windows error
	 * codes.  We really care about only a few of these.
	 */
	switch (last_error) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_INVALID_DRIVE:
	case ERROR_PATH_NOT_FOUND:
		ret = ENOENT;
		break;

	case ERROR_NO_MORE_FILES:
	case ERROR_TOO_MANY_OPEN_FILES:
		ret = EMFILE;
		break;

	case ERROR_ACCESS_DENIED:
		ret = EPERM;
		break;

	case ERROR_INVALID_HANDLE:
		ret = EBADF;
		break;

	case ERROR_NOT_ENOUGH_MEMORY:
		ret = ENOMEM;
		break;

	case ERROR_DISK_FULL:
		ret = ENOSPC;

	case ERROR_ARENA_TRASHED:
	case ERROR_BAD_COMMAND:
	case ERROR_BAD_ENVIRONMENT:
	case ERROR_BAD_FORMAT:
	case ERROR_GEN_FAILURE:
	case ERROR_INVALID_ACCESS:
	case ERROR_INVALID_BLOCK:
	case ERROR_INVALID_DATA:
	case ERROR_READ_FAULT:
	case ERROR_WRITE_FAULT:
		ret = EFAULT;
		break;

	case ERROR_FILE_EXISTS:
		ret = EEXIST;
		break;

	case ERROR_NOT_SAME_DEVICE:
		ret = EXDEV;
		break;

	case ERROR_WRITE_PROTECT:
		ret = EACCES;
		break;

	case ERROR_NOT_READY:
		ret = EBUSY;
		break;

	case ERROR_LOCK_VIOLATION:
	case ERROR_SHARING_VIOLATION:
		ret = EBUSY;
		break;

	case 0:
		ret = EFAULT;
		break;

	default:
		ret = EIO;			/* Generic error. */
		break;
	}

	return (ret);
}
