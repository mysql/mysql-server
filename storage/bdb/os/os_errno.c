/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_errno.c,v 11.11 2004/01/28 03:36:18 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_get_errno_ret_zero --
 *	Return the value of errno, even if it's zero.
 *
 * PUBLIC: int __os_get_errno_ret_zero __P((void));
 */
int
__os_get_errno_ret_zero()
{
	/* This routine must be able to return the same value repeatedly. */
	return (errno);
}

/*
 * __os_get_errno --
 *	Return the value of errno, or EAGAIN if errno is zero.
 *
 * PUBLIC: int __os_get_errno __P((void));
 */
int
__os_get_errno()
{
	/*
	 * This routine must be able to return the same value repeatedly.
	 *
	 * We've seen cases where system calls failed but errno was never set.
	 * This version of __os_get_errno() sets errno to EAGAIN if it's not
	 * already set, to work around that problem.  For obvious reasons, we
	 * can only call this function if we know an error has occurred, that
	 * is, we can't test errno for a non-zero value after this call.
	 */
	if (errno == 0)
		__os_set_errno(EAGAIN);

	return (errno);
}

/*
 * __os_set_errno --
 *	Set the value of errno.
 *
 * PUBLIC: void __os_set_errno __P((int));
 */
void
__os_set_errno(evalue)
	int evalue;
{
	/*
	 * This routine is called by the compatibility interfaces (DB 1.85,
	 * dbm and hsearch).  Force values > 0, that is, not one of DB 2.X
	 * and later's public error returns.  If something bad has happened,
	 * default to EFAULT -- a nasty return.  Otherwise, default to EINVAL.
	 * As the compatibility APIs aren't included on Windows, the Windows
	 * version of this routine doesn't need this behavior.
	 */
	errno =
	    evalue >= 0 ? evalue : (evalue == DB_RUNRECOVERY ? EFAULT : EINVAL);
}
