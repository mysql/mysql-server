/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_seek.c,v 11.26 2004/09/17 22:00:31 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_seek --
 *	Seek to a page/byte offset in the file.
 *
 * PUBLIC: int __os_seek __P((DB_ENV *,
 * PUBLIC:      DB_FH *, u_int32_t, db_pgno_t, u_int32_t, int, DB_OS_SEEK));
 */
int
__os_seek(dbenv, fhp, pgsize, pageno, relative, isrewind, db_whence)
	DB_ENV *dbenv;
	DB_FH *fhp;
	u_int32_t pgsize;
	db_pgno_t pageno;
	u_int32_t relative;
	int isrewind;
	DB_OS_SEEK db_whence;
{
	off_t offset;
	int ret, whence;

	/* Check for illegal usage. */
	DB_ASSERT(F_ISSET(fhp, DB_FH_OPENED) && fhp->fd != -1);

	switch (db_whence) {
	case DB_OS_SEEK_CUR:
		whence = SEEK_CUR;
		break;
	case DB_OS_SEEK_END:
		whence = SEEK_END;
		break;
	case DB_OS_SEEK_SET:
		whence = SEEK_SET;
		break;
	default:
		return (EINVAL);
	}

	offset = (off_t)pgsize * pageno + relative;
	if (isrewind)
		offset = -offset;

	if (DB_GLOBAL(j_seek) != NULL)
		ret = DB_GLOBAL(j_seek)(fhp->fd, offset, whence);
	else
		RETRY_CHK((lseek(fhp->fd, offset, whence) == -1 ? 1 : 0), ret);

	if (ret == 0) {
		fhp->pgsize = pgsize;
		fhp->pgno = pageno;
		fhp->offset = relative;
	} else
		__db_err(dbenv, "seek: %lu %d %d: %s",
		    (u_long)pgsize * pageno + relative,
		    isrewind, db_whence, strerror(ret));

	return (ret);
}
