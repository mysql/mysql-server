/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_seek.c,v 11.12 2000/11/30 00:58:42 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "os_jump.h"

/*
 * __os_seek --
 *	Seek to a page/byte offset in the file.
 *
 * PUBLIC: int __os_seek __P((DB_ENV *,
 * PUBLIC:      DB_FH *, size_t, db_pgno_t, u_int32_t, int, DB_OS_SEEK));
 */
int
__os_seek(dbenv, fhp, pgsize, pageno, relative, isrewind, db_whence)
	DB_ENV *dbenv;
	DB_FH *fhp;
	size_t pgsize;
	db_pgno_t pageno;
	u_int32_t relative;
	int isrewind;
	DB_OS_SEEK db_whence;
{
	off_t offset;
	int ret, whence;

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

	if (__db_jump.j_seek != NULL)
		ret = __db_jump.j_seek(fhp->fd,
		    pgsize, pageno, relative, isrewind, whence);
	else {
		offset = (off_t)pgsize * pageno + relative;
		if (isrewind)
			offset = -offset;
		ret =
		    lseek(fhp->fd, offset, whence) == -1 ? __os_get_errno() : 0;
	}

	if (ret != 0)
		__db_err(dbenv, "seek: %lu %d %d: %s",
		    (u_long)pgsize * pageno + relative,
		    isrewind, db_whence, strerror(ret));

	return (ret);
}
