/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_finit.c,v 11.9 2000/03/29 20:50:52 ubell Exp $";
#endif /* not lint */

#include "db_int.h"

/*
 * __os_fpinit --
 *	Initialize a page in a regular file.
 *
 * PUBLIC: int __os_fpinit __P((DB_ENV *, DB_FH *, db_pgno_t, int, int));
 */
int
__os_fpinit(dbenv, fhp, pgno, pagecount, pagesize)
	DB_ENV *dbenv;
	DB_FH *fhp;
	db_pgno_t pgno;
	int pagecount, pagesize;
{
	size_t nw, totalbytes, curbytes;
	int ret;
	char buf[1024];

	/*
	 * Windows/NT zero-fills pages that were never explicitly written to
	 * the file.  Windows 95/98 gives you random garbage, and that breaks
	 * DB.
	 */
	if (__os_is_winnt())
		return (0);

	if ((ret = __os_seek(dbenv,
	    fhp, pagesize, pgno, 0, 0, DB_OS_SEEK_SET)) != 0)
		return (ret);

	memset(buf, 0, sizeof(buf));
	totalbytes = pagecount * pagesize;

	while (totalbytes > 0) {
		if (totalbytes > sizeof(buf))
			curbytes = sizeof(buf);
		else
			curbytes = totalbytes;
		if ((ret = __os_write(dbenv, fhp, buf, curbytes, &nw)) != 0)
			return (ret);
		if (nw != curbytes)
			return (EIO);
		totalbytes -= curbytes;
	}
	return (0);
}
