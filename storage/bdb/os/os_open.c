/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_open.c,v 11.37 2002/06/21 20:35:16 sandstro Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <string.h>
#endif

#include "db_int.h"

#ifdef HAVE_QNX
static int __os_region_open __P((DB_ENV *, const char *, int, int, DB_FH *));
#endif

/*
 * __os_open --
 *	Open a file.
 *
 * PUBLIC: int __os_open __P((DB_ENV *, const char *, u_int32_t, int, DB_FH *));
 */
int
__os_open(dbenv, name, flags, mode, fhp)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags;
	int mode;
	DB_FH *fhp;
{
	int oflags, ret;

	oflags = 0;

#ifdef DIAGNOSTIC
#define	OKFLAGS								\
	(DB_OSO_CREATE | DB_OSO_DIRECT | DB_OSO_EXCL | DB_OSO_LOG |	\
	 DB_OSO_RDONLY | DB_OSO_REGION | DB_OSO_SEQ | DB_OSO_TEMP |	\
	 DB_OSO_TRUNC)
	if ((ret = __db_fchk(dbenv, "__os_open", flags, OKFLAGS)) != 0)
		return (ret);
#endif

#if defined(O_BINARY)
	/*
	 * If there's a binary-mode open flag, set it, we never want any
	 * kind of translation.  Some systems do translations by default,
	 * e.g., with Cygwin, the default mode for an open() is set by the
	 * mode of the mount that underlies the file.
	 */
	oflags |= O_BINARY;
#endif

	/*
	 * DB requires the POSIX 1003.1 semantic that two files opened at the
	 * same time with DB_OSO_CREATE/O_CREAT and DB_OSO_EXCL/O_EXCL flags
	 * set return an EEXIST failure in at least one.
	 */
	if (LF_ISSET(DB_OSO_CREATE))
		oflags |= O_CREAT;

	if (LF_ISSET(DB_OSO_EXCL))
		oflags |= O_EXCL;

#if defined(O_DSYNC) && defined(XXX_NEVER_SET)
	/*
	 * !!!
	 * We should get better performance if we push the log files to disk
	 * immediately instead of waiting for the sync.  However, Solaris
	 * (and likely any other system based on the 4BSD filesystem releases),
	 * doesn't implement O_DSYNC correctly, only flushing data blocks and
	 * not inode or indirect blocks.
	 */
	if (LF_ISSET(DB_OSO_LOG))
		oflags |= O_DSYNC;
#endif

	if (LF_ISSET(DB_OSO_RDONLY))
		oflags |= O_RDONLY;
	else
		oflags |= O_RDWR;

	if (LF_ISSET(DB_OSO_TRUNC))
		oflags |= O_TRUNC;

#ifdef HAVE_O_DIRECT
	if (LF_ISSET(DB_OSO_DIRECT))
		oflags |= O_DIRECT;
#endif

#ifdef HAVE_QNX
	if (LF_ISSET(DB_OSO_REGION))
		return (__os_region_open(dbenv, name, oflags, mode, fhp));
#endif
	/* Open the file. */
	if ((ret = __os_openhandle(dbenv, name, oflags, mode, fhp)) != 0)
		return (ret);

#ifdef HAVE_DIRECTIO
	if (LF_ISSET(DB_OSO_DIRECT))
		(void)directio(fhp->fd, DIRECTIO_ON);
#endif

	/*
	 * Delete any temporary file.
	 *
	 * !!!
	 * There's a race here, where we've created a file and we crash before
	 * we can unlink it.  Temporary files aren't common in DB, regardless,
	 * it's not a security problem because the file is empty.  There's no
	 * reasonable way to avoid the race (playing signal games isn't worth
	 * the portability nightmare), so we just live with it.
	 */
	if (LF_ISSET(DB_OSO_TEMP)) {
#if defined(HAVE_UNLINK_WITH_OPEN_FAILURE) || defined(CONFIG_TEST)
		if ((ret = __os_strdup(dbenv, name, &fhp->name)) != 0) {
			(void)__os_closehandle(dbenv, fhp);
			(void)__os_unlink(dbenv, name);
			return (ret);
		}
		F_SET(fhp, DB_FH_UNLINK);
#else
		(void)__os_unlink(dbenv, name);
#endif
	}

	return (0);
}

#ifdef HAVE_QNX
/*
 * __os_region_open --
 *	Open a shared memory region file using POSIX shm_open.
 */
static int
__os_region_open(dbenv, name, oflags, mode, fhp)
	DB_ENV *dbenv;
	const char *name;
	int oflags;
	int mode;
	DB_FH *fhp;
{
	int ret;
	char *newname;

	if ((ret = __os_shmname(dbenv, name, &newname)) != 0)
		goto err;
	memset(fhp, 0, sizeof(*fhp));
	fhp->fd = shm_open(newname, oflags, mode);
	if (fhp->fd == -1)
		ret = __os_get_errno();
	else {
#ifdef HAVE_FCNTL_F_SETFD
		/* Deny file descriptor acces to any child process. */
		if (fcntl(fhp->fd, F_SETFD, 1) == -1) {
			ret = __os_get_errno();
			__db_err(dbenv, "fcntl(F_SETFD): %s", strerror(ret));
			__os_closehandle(dbenv, fhp);
		} else
#endif
		F_SET(fhp, DB_FH_VALID);
	}
	/*
	 * Once we have created the object, we don't need the name
	 * anymore.  Other callers of this will convert themselves.
	 */
err:
	if (newname != NULL)
		__os_free(dbenv, newname);
	return (ret);
}

/*
 * __os_shmname --
 *	Translate a pathname into a shm_open memory object name.
 *
 * PUBLIC: #ifdef HAVE_QNX
 * PUBLIC: int __os_shmname __P((DB_ENV *, const char *, char **));
 * PUBLIC: #endif
 */
int
__os_shmname(dbenv, name, newnamep)
	DB_ENV *dbenv;
	const char *name;
	char **newnamep;
{
	int ret;
	size_t size;
	char *p, *q, *tmpname;

	*newnamep = NULL;

	/*
	 * POSIX states that the name for a shared memory object
	 * may begin with a slash '/' and support for subsequent
	 * slashes is implementation-dependent.  The one implementation
	 * we know of right now, QNX, forbids subsequent slashes.
	 * We don't want to be parsing pathnames for '.' and '..' in
	 * the middle.  In order to allow easy conversion, just take
	 * the last component as the shared memory name.  This limits
	 * the namespace a bit, but makes our job a lot easier.
	 *
	 * We should not be modifying user memory, so we use our own.
	 * Caller is responsible for freeing the memory we give them.
	 */
	if ((ret = __os_strdup(dbenv, name, &tmpname)) != 0)
		return (ret);
	/*
	 * Skip over filename component.
	 * We set that separator to '\0' so that we can do another
	 * __db_rpath.  However, we immediately set it then to ':'
	 * so that we end up with the tailing directory:filename.
	 * We require a home directory component.  Return an error
	 * if there isn't one.
	 */
	p = __db_rpath(tmpname);
	if (p == NULL)
		return (EINVAL);
	if (p != tmpname) {
		*p = '\0';
		q = p;
		p = __db_rpath(tmpname);
		*q = ':';
	}
	if (p != NULL) {
		/*
		 * If we have a path component, copy and return it.
		 */
		ret = __os_strdup(dbenv, p, newnamep);
		__os_free(dbenv, tmpname);
		return (ret);
	}

	/*
	 * We were given just a directory name with no path components.
	 * Add a leading slash, and copy the remainder.
	 */
	size = strlen(tmpname) + 2;
	if ((ret = __os_malloc(dbenv, size, &p)) != 0)
		return (ret);
	p[0] = '/';
	memcpy(&p[1], tmpname, size-1);
	__os_free(dbenv, tmpname);
	*newnamep = p;
	return (0);
}
#endif
