/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_handle.c,v 11.30 2002/07/12 18:56:54 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_openhandle --
 *	Open a file, using POSIX 1003.1 open flags.
 *
 * PUBLIC: int __os_openhandle __P((DB_ENV *, const char *, int, int, DB_FH *));
 */
int
__os_openhandle(dbenv, name, flags, mode, fhp)
	DB_ENV *dbenv;
	const char *name;
	int flags, mode;
	DB_FH *fhp;
{
	int ret, nrepeat;

	memset(fhp, 0, sizeof(*fhp));
	fhp->handle = INVALID_HANDLE_VALUE;

	/* If the application specified an interface, use it. */
	if (DB_GLOBAL(j_open) != NULL) {
		if ((fhp->fd = DB_GLOBAL(j_open)(name, flags, mode)) == -1)
			return (__os_get_errno());
		F_SET(fhp, DB_FH_VALID);
		return (0);
	}

	for (nrepeat = 1; nrepeat < 4; ++nrepeat) {
		ret = 0;
		fhp->fd = open(name, flags, mode);

		if (fhp->fd == -1) {
			/*
			 * If it's a "temporary" error, we retry up to 3 times,
			 * waiting up to 12 seconds.  While it's not a problem
			 * if we can't open a database, an inability to open a
			 * log file is cause for serious dismay.
			 */
			ret = __os_get_errno();
			if (ret == ENFILE || ret == EMFILE || ret == ENOSPC) {
				(void)__os_sleep(dbenv, nrepeat * 2, 0);
				continue;
			}

			/*
			 * If it was an EINTR it's reasonable to retry
			 * immediately, and arbitrarily often.
			 */
			if (ret == EINTR) {
				--nrepeat;
				continue;
			}
		} else {
			F_SET(fhp, DB_FH_VALID);
		}
		break;
	}

	return (ret);
}

/*
 * __os_closehandle --
 *	Close a file.
 *
 * PUBLIC: int __os_closehandle __P((DB_ENV *, DB_FH *));
 */
int
__os_closehandle(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	BOOL success;
	int ret;

	COMPQUIET(dbenv, NULL);
	/* Don't close file descriptors that were never opened. */
	DB_ASSERT(F_ISSET(fhp, DB_FH_VALID) &&
	    ((fhp->fd != -1) || (fhp->handle != INVALID_HANDLE_VALUE)));

	ret = 0;

	do {
		if (DB_GLOBAL(j_close) != NULL)
			success = (DB_GLOBAL(j_close)(fhp->fd) == 0);
		else if (fhp->handle != INVALID_HANDLE_VALUE) {
			success = CloseHandle(fhp->handle);
			if (!success)
				__os_set_errno(__os_win32_errno());
		}
		else
			success = (close(fhp->fd) == 0);
	} while (!success && (ret = __os_get_errno()) == EINTR);

	/*
	 * Smash the POSIX file descriptor -- it's never tested, but we want
	 * to catch any mistakes.
	 */
	fhp->fd = -1;
	fhp->handle = INVALID_HANDLE_VALUE;
	F_CLR(fhp, DB_FH_VALID);

	return (ret);
}
