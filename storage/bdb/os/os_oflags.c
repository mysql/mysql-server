/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_oflags.c,v 11.14 2004/07/09 18:39:10 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SHMGET
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <fcntl.h>
#endif

#include "db_int.h"

/*
 * __db_oflags --
 *	Convert open(2) flags to DB flags.
 *
 * PUBLIC: u_int32_t __db_oflags __P((int));
 */
u_int32_t
__db_oflags(oflags)
	int oflags;
{
	u_int32_t dbflags;

	dbflags = 0;

	if (oflags & O_CREAT)
		dbflags |= DB_CREATE;

	if (oflags & O_TRUNC)
		dbflags |= DB_TRUNCATE;

	/*
	 * !!!
	 * Convert POSIX 1003.1 open(2) mode flags to DB flags.  This isn't
	 * an exact science as few POSIX implementations have a flag value
	 * for O_RDONLY, it's simply the lack of a write flag.
	 */
#ifndef	O_ACCMODE
#define	O_ACCMODE	(O_RDONLY | O_RDWR | O_WRONLY)
#endif
	switch (oflags & O_ACCMODE) {
	case O_RDWR:
	case O_WRONLY:
		break;
	default:
		dbflags |= DB_RDONLY;
		break;
	}
	return (dbflags);
}

#ifdef DB_WIN32
#ifndef	S_IRUSR
#define	S_IRUSR	S_IREAD		/* R for owner */
#endif
#ifndef	S_IWUSR
#define	S_IWUSR	S_IWRITE	/* W for owner */
#endif
#ifndef	S_IRGRP
#define	S_IRGRP	0		/* R for group */
#endif
#ifndef	S_IWGRP
#define	S_IWGRP	0		/* W for group */
#endif
#ifndef	S_IROTH
#define	S_IROTH	0		/* R for other */
#endif
#ifndef	S_IWOTH
#define	S_IWOTH	0		/* W for other */
#endif
#else
#ifndef	S_IRUSR
#define	S_IRUSR	0000400		/* R for owner */
#endif
#ifndef	S_IWUSR
#define	S_IWUSR	0000200		/* W for owner */
#endif
#ifndef	S_IRGRP
#define	S_IRGRP	0000040		/* R for group */
#endif
#ifndef	S_IWGRP
#define	S_IWGRP	0000020		/* W for group */
#endif
#ifndef	S_IROTH
#define	S_IROTH	0000004		/* R for other */
#endif
#ifndef	S_IWOTH
#define	S_IWOTH	0000002		/* W for other */
#endif
#endif /* DB_WIN32 */

/*
 * __db_omode --
 *	Convert a permission string to the correct open(2) flags.
 *
 * PUBLIC: int __db_omode __P((const char *));
 */
int
__db_omode(perm)
	const char *perm;
{
	int mode;
	mode = 0;
	if (perm[0] == 'r')
		mode |= S_IRUSR;
	if (perm[1] == 'w')
		mode |= S_IWUSR;
	if (perm[2] == 'r')
		mode |= S_IRGRP;
	if (perm[3] == 'w')
		mode |= S_IWGRP;
	if (perm[4] == 'r')
		mode |= S_IROTH;
	if (perm[5] == 'w')
		mode |= S_IWOTH;
	return (mode);
}

#ifdef HAVE_SHMGET

#ifndef SHM_R
#define	SHM_R	0400
#endif
#ifndef SHM_W
#define	SHM_W	0200
#endif

/*
 * __db_shm_mode --
 *	Map the DbEnv::open method file mode permissions to shmget call
 *	permissions.
 *
 * PUBLIC: int __db_shm_mode __P((DB_ENV *));
 */
int
__db_shm_mode(dbenv)
	DB_ENV *dbenv;
{
	int mode;

	/* Default to r/w owner, r/w group. */
	if (dbenv->db_mode == 0)
		return (SHM_R | SHM_W | SHM_R >> 3 | SHM_W >> 3);

	mode = 0;
	if (dbenv->db_mode & S_IRUSR)
		mode |= SHM_R;
	if (dbenv->db_mode & S_IWUSR)
		mode |= SHM_W;
	if (dbenv->db_mode & S_IRGRP)
		mode |= SHM_R >> 3;
	if (dbenv->db_mode & S_IWGRP)
		mode |= SHM_W >> 3;
	if (dbenv->db_mode & S_IROTH)
		mode |= SHM_R >> 6;
	if (dbenv->db_mode & S_IWOTH)
		mode |= SHM_W >> 6;
	return (mode);
}
#endif
