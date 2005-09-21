/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_handle.c,v 11.40 2004/08/19 17:59:22 sue Exp $
 */

#include "db_config.h"

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
 * PUBLIC: int __os_openhandle
 * PUBLIC:     __P((DB_ENV *, const char *, int, int, DB_FH **));
 */
int
__os_openhandle(dbenv, name, flags, mode, fhpp)
	DB_ENV *dbenv;
	const char *name;
	int flags, mode;
	DB_FH **fhpp;
{
	DB_FH *fhp;
	u_int nrepeat, retries;
	int ret;
#ifdef HAVE_VXWORKS
	int newflags;
#endif

	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), fhpp)) != 0)
		return (ret);
	fhp = *fhpp;

	/* If the application specified an interface, use it. */
	if (DB_GLOBAL(j_open) != NULL) {
		if ((fhp->fd = DB_GLOBAL(j_open)(name, flags, mode)) == -1) {
			ret = __os_get_errno();
			goto err;
		}
		F_SET(fhp, DB_FH_OPENED);
		return (0);
	}

	retries = 0;
	for (nrepeat = 1; nrepeat < 4; ++nrepeat) {
		ret = 0;
#ifdef	HAVE_VXWORKS
		/*
		 * VxWorks does not support O_CREAT on open, you have to use
		 * creat() instead.  (It does not support O_EXCL or O_TRUNC
		 * either, even though they are defined "for future support".)
		 * We really want the POSIX behavior that if O_CREAT is set,
		 * we open if it exists, or create it if it doesn't exist.
		 * If O_CREAT is specified, single thread and try to open the
		 * file.  If successful, and O_EXCL return EEXIST.  If
		 * unsuccessful call creat and then end single threading.
		 */
		if (LF_ISSET(O_CREAT)) {
			DB_BEGIN_SINGLE_THREAD;
			newflags = flags & ~(O_CREAT | O_EXCL);
			if ((fhp->fd = open(name, newflags, mode)) != -1) {
				/*
				 * We need to mark the file opened at this
				 * point so that if we get any error below
				 * we will properly close the fd we just
				 * opened on the error path.
				 */
				F_SET(fhp, DB_FH_OPENED);
				if (LF_ISSET(O_EXCL)) {
					/*
					 * If we get here, want O_EXCL create,
					 * and the file exists.  Close and
					 * return EEXISTS.
					 */
					DB_END_SINGLE_THREAD;
					ret = EEXIST;
					goto err;
				}
				/*
				 * XXX
				 * Assume any error means non-existence.
				 * Unfortunately return values (even for
				 * non-existence) are driver specific so
				 * there is no single error we can use to
				 * verify we truly got the equivalent of
				 * ENOENT.
				 */
			} else
				fhp->fd = creat(name, newflags);
			DB_END_SINGLE_THREAD;
		} else
		/* FALLTHROUGH */
#endif
#ifdef __VMS
		/*
		 * !!!
		 * Open with full sharing on VMS.
		 *
		 * We use these flags because they are the ones set by the VMS
		 * CRTL mmap() call when it opens a file, and we have to be
		 * able to open files that mmap() has previously opened, e.g.,
		 * when we're joining already existing DB regions.
		 */
		fhp->fd = open(name, flags, mode, "shr=get,put,upd,del,upi");
#else
		fhp->fd = open(name, flags, mode);
#endif
		if (fhp->fd != -1) {
			F_SET(fhp, DB_FH_OPENED);

#if defined(HAVE_FCNTL_F_SETFD)
			/* Deny file descriptor access to any child process. */
			if (fcntl(fhp->fd, F_SETFD, 1) == -1) {
				ret = __os_get_errno();
				__db_err(dbenv,
				    "fcntl(F_SETFD): %s", strerror(ret));
				goto err;
			}
#endif
			break;
		}

		switch (ret = __os_get_errno()) {
		case EMFILE:
		case ENFILE:
		case ENOSPC:
			/*
			 * If it's a "temporary" error, we retry up to 3 times,
			 * waiting up to 12 seconds.  While it's not a problem
			 * if we can't open a database, an inability to open a
			 * log file is cause for serious dismay.
			 */
			__os_sleep(dbenv, nrepeat * 2, 0);
			break;
		case EAGAIN:
		case EBUSY:
		case EINTR:
			/*
			 * If an EAGAIN, EBUSY or EINTR, retry immediately for
			 * DB_RETRY times.
			 */
			if (++retries < DB_RETRY)
				--nrepeat;
			break;
		default:
			break;
		}
	}

err:	if (ret != 0) {
		(void)__os_closehandle(dbenv, fhp);
		*fhpp = NULL;
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
	int ret;

	ret = 0;

	/*
	 * If we have a valid handle, close it and unlink any temporary
	 * file.
	 */
	if (F_ISSET(fhp, DB_FH_OPENED)) {
		if (DB_GLOBAL(j_close) != NULL)
			ret = DB_GLOBAL(j_close)(fhp->fd);
		else
			RETRY_CHK((close(fhp->fd)), ret);

		if (ret != 0)
			__db_err(dbenv, "close: %s", strerror(ret));

		/* Unlink the file if we haven't already done so. */
		if (F_ISSET(fhp, DB_FH_UNLINK)) {
			(void)__os_unlink(dbenv, fhp->name);
			__os_free(dbenv, fhp->name);
		}
	}

	__os_free(dbenv, fhp);

	return (ret);
}
