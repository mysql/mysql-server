/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_mkdir.c,v 12.8 2005/11/02 03:12:17 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "db_int.h"

/*
 * __os_mkdir --
 *	Create a directory.
 *
 * PUBLIC: int __os_mkdir __P((DB_ENV *, const char *, int));
 */
int
__os_mkdir(dbenv, name, mode)
	DB_ENV *dbenv;
	const char *name;
	int mode;
{
	int ret;

	COMPQUIET(dbenv, NULL);

	/* Make the directory, with paranoid permissions. */
#ifdef HAVE_VXWORKS
	RETRY_CHK((mkdir((char *)name)), ret);
#else
#ifdef DB_WIN32
	RETRY_CHK((_mkdir(name)), ret);
#else
	RETRY_CHK((mkdir(name, 0600)), ret);
#endif
	if (ret != 0)
		return (ret);

	/* Set the absolute permissions, if specified. */
#ifndef DB_WIN32
	if (mode != 0)
		RETRY_CHK((chmod(name, mode)), ret);
#endif
#endif
	return (ret);
}
