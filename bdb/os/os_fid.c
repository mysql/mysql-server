/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_fid.c,v 11.7 2000/10/26 14:17:05 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

#define	SERIAL_INIT	0
static u_int32_t fid_serial = SERIAL_INIT;

/*
 * __os_fileid --
 *	Return a unique identifier for a file.
 *
 * PUBLIC: int __os_fileid __P((DB_ENV *, const char *, int, u_int8_t *));
 */
int
__os_fileid(dbenv, fname, unique_okay, fidp)
	DB_ENV *dbenv;
	const char *fname;
	int unique_okay;
	u_int8_t *fidp;
{
	struct stat sb;
	size_t i;
	int ret;
	u_int32_t tmp;
	u_int8_t *p;

	/* Clear the buffer. */
	memset(fidp, 0, DB_FILE_ID_LEN);

	/* On POSIX/UNIX, use a dev/inode pair. */
#ifdef HAVE_VXWORKS
	if (stat((char *)fname, &sb)) {
#else
	if (stat(fname, &sb)) {
#endif
		ret = __os_get_errno();
		__db_err(dbenv, "%s: %s", fname, strerror(ret));
		return (ret);
	}

	/*
	 * Initialize/increment the serial number we use to help avoid
	 * fileid collisions.  Note that we don't bother with locking;
	 * it's unpleasant to do from down in here, and if we race on
	 * this no real harm will be done, since the finished fileid
	 * has so many other components.
	 *
	 * We increment by 100000 on each call as a simple way of
	 * randomizing;  simply incrementing seems potentially less useful
	 * if pids are also simply incremented, since this is process-local
	 * and we may be one of a set of processes starting up.  100000
	 * pushes us out of pid space on most platforms, and has few
	 * interesting properties in base 2.
	 */
	if (fid_serial == SERIAL_INIT)
		fid_serial = (u_int32_t)getpid();
	else
		fid_serial += 100000;

	/*
	 * !!!
	 * Nothing is ever big enough -- on Sparc V9, st_ino, st_dev and the
	 * time_t types are all 8 bytes.  As DB_FILE_ID_LEN is only 20 bytes,
	 * we convert to a (potentially) smaller fixed-size type and use it.
	 *
	 * We don't worry about byte sexing or the actual variable sizes.
	 *
	 * When this routine is called from the DB access methods, it's only
	 * called once -- whatever ID is generated when a database is created
	 * is stored in the database file's metadata, and that is what is
	 * saved in the mpool region's information to uniquely identify the
	 * file.
	 *
	 * When called from the mpool layer this routine will be called each
	 * time a new thread of control wants to share the file, which makes
	 * things tougher.  As far as byte sexing goes, since the mpool region
	 * lives on a single host, there's no issue of that -- the entire
	 * region is byte sex dependent.  As far as variable sizes go, we make
	 * the simplifying assumption that 32-bit and 64-bit processes will
	 * get the same 32-bit values if we truncate any returned 64-bit value
	 * to a 32-bit value.  When we're called from the mpool layer, though,
	 * we need to be careful not to include anything that isn't
	 * reproducible for a given file, such as the timestamp or serial
	 * number.
	 */
	tmp = (u_int32_t)sb.st_ino;
	for (p = (u_int8_t *)&tmp, i = sizeof(u_int32_t); i > 0; --i)
		*fidp++ = *p++;

	tmp = (u_int32_t)sb.st_dev;
	for (p = (u_int8_t *)&tmp, i = sizeof(u_int32_t); i > 0; --i)
		*fidp++ = *p++;

	if (unique_okay) {
		/*
		 * We want the number of seconds, not the high-order 0 bits,
		 * so convert the returned time_t to a (potentially) smaller
		 * fixed-size type.
		 */
		tmp = (u_int32_t)time(NULL);
		for (p = (u_int8_t *)&tmp, i = sizeof(u_int32_t); i > 0; --i)
			*fidp++ = *p++;

		for (p = (u_int8_t *)&fid_serial, i = sizeof(u_int32_t);
		    i > 0; --i)
			*fidp++ = *p++;
	}

	return (0);
}
