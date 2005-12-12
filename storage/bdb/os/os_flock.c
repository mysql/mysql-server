/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_flock.c,v 12.4 2005/06/20 14:59:01 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <string.h>
#endif

#include "db_int.h"

/*
 * __os_fdlock --
 *	Acquire/release a lock on a byte in a file.
 *
 * PUBLIC: int __os_fdlock __P((DB_ENV *, DB_FH *, off_t, int, int));
 */
int
__os_fdlock(dbenv, fhp, offset, acquire, nowait)
	DB_ENV *dbenv;
	DB_FH *fhp;
	int acquire, nowait;
	off_t offset;
{
	struct flock fl;
	int ret;

	DB_ASSERT(F_ISSET(fhp, DB_FH_OPENED) && fhp->fd != -1);

#ifdef HAVE_FCNTL
	fl.l_start = offset;
	fl.l_len = 1;
	fl.l_type = acquire ? F_WRLCK : F_UNLCK;
	fl.l_whence = SEEK_SET;

	RETRY_CHK_EINTR_ONLY(
	    (fcntl(fhp->fd, nowait ? F_SETLK : F_SETLKW, &fl)), ret);

	if (ret != 0 && ret != EACCES && ret != EAGAIN)
		__db_err(dbenv, "fcntl: %s", strerror(ret));
	return (ret);
#else
	__db_err(dbenv,
	    "advisory file locking unavailable: %s", strerror(DB_OPNOTSUP));
	return (DB_OPNOTSUP);
#endif
}
