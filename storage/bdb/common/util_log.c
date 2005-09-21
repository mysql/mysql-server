/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: util_log.c,v 1.14 2004/01/28 03:35:52 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
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
#endif

#include "db_int.h"

/*
 * __db_util_logset --
 *	Log that we're running.
 *
 * PUBLIC: int __db_util_logset __P((const char *, char *));
 */
int
__db_util_logset(progname, fname)
	const char *progname;
	char *fname;
{
	FILE *fp;
	time_t now;
	u_int32_t id;

	if ((fp = fopen(fname, "w")) == NULL)
		goto err;

	(void)time(&now);
	__os_id(&id);
	fprintf(fp, "%s: %lu %s", progname, (u_long)id, ctime(&now));

	if (fclose(fp) == EOF)
		goto err;

	return (0);

err:	fprintf(stderr, "%s: %s: %s\n", progname, fname, strerror(errno));
	return (1);
}
