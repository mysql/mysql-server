/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_errno.c,v 11.14 2004/07/06 21:06:38 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_get_errno_ret_zero --
 *	Return the value of errno, even if it's zero.
 */
int
__os_get_errno_ret_zero()
{
	/* This routine must be able to return the same value repeatedly. */
	return (errno);
}

/*
 * __os_get_errno --
 *	Return the last Windows error as an errno.
 *	We give generic error returns:
 *
 *	EFAULT means Win* call failed,
 *	  and GetLastError provided no extra info.
 *
 *	EIO means error on Win* call,
 *	  and we were unable to provide a meaningful errno for this Windows
 *	  error.  More information is only available by setting a breakpoint
 *	  here.
 */
int
__os_get_errno()
{
	DWORD last_error;
	int ret;

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
		break;

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
	case ERROR_ALREADY_EXISTS:
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

	case ERROR_RETRY:
		ret = EINTR;
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
