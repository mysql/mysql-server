/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_fsync.c,v 11.15 2002/07/12 18:56:54 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>			/* XXX: Required by __hp3000s900 */
#include <unistd.h>
#include <string.h>
#endif

#include "db_int.h"

/*
 * __os_fsync --
 *	Flush a file descriptor.
 *
 * PUBLIC: int __os_fsync __P((DB_ENV *, DB_FH *));
 */
int
__os_fsync(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	BOOL success;
	int ret;

	/*
	 * Do nothing if the file descriptor has been marked as not requiring
	 * any sync to disk.
	 */
	if (F_ISSET(fhp, DB_FH_NOSYNC))
		return (0);

	ret = 0;
	do {
		if (DB_GLOBAL(j_fsync) != NULL)
			success = (DB_GLOBAL(j_fsync)(fhp->fd) == 0);
		else {
			success = FlushFileBuffers(fhp->handle);
			if (!success)
				__os_set_errno(__os_win32_errno());
		}
	} while (!success && (ret = __os_get_errno()) == EINTR);

	if (ret != 0)
		__db_err(dbenv, "fsync %s", strerror(ret));
	return (ret);
}
