/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_vrfy_stub.c,v 11.6 2004/06/14 15:23:32 bostic Exp $
 */

#include "db_config.h"

#ifndef HAVE_VERIFY
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/db_verify.h"

/*
 * If the library wasn't compiled with the verification support, various
 * routines aren't available.  Stub them here, returning an appropriate
 * error.
 */

static int __db_novrfy __P((DB_ENV *));

/*
 * __db_novrfy --
 *	Error when a Berkeley DB build doesn't include the access method.
 */
static int
__db_novrfy(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv,
	    "library build did not include support for database verification");
	return (DB_OPNOTSUP);
}

int
__db_verify_pp(dbp, file, database, outfile, flags)
	DB *dbp;
	const char *file, *database;
	FILE *outfile;
	u_int32_t flags;
{
	int ret;

	COMPQUIET(file, NULL);
	COMPQUIET(database, NULL);
	COMPQUIET(outfile, NULL);
	COMPQUIET(flags, 0);

	ret = __db_novrfy(dbp->dbenv);

	/* The verify method is a destructor. */
	(void)__db_close(dbp, NULL, 0);

	return (ret);
}

int
__db_verify_internal(dbp, name, subdb, handle, callback, flags)
	DB *dbp;
	const char *name, *subdb;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
{
	COMPQUIET(dbp, NULL);
	COMPQUIET(name, NULL);
	COMPQUIET(subdb, NULL);
	COMPQUIET(handle, NULL);
	COMPQUIET(callback, NULL);
	COMPQUIET(flags, 0);
	return (0);
}

int
__db_vrfy_getpageinfo(vdp, pgno, pipp)
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	VRFY_PAGEINFO **pipp;
{
	COMPQUIET(pgno, 0);
	COMPQUIET(pipp, NULL);
	return (__db_novrfy(vdp->pgdbp->dbenv));
}

int
__db_vrfy_putpageinfo(dbenv, vdp, pip)
	DB_ENV *dbenv;
	VRFY_DBINFO *vdp;
	VRFY_PAGEINFO *pip;
{
	COMPQUIET(vdp, NULL);
	COMPQUIET(pip, NULL);
	return (__db_novrfy(dbenv));
}
#endif /* !HAVE_VERIFY */
