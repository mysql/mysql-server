/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_clock.c,v 1.7 2002/07/12 18:56:53 bostic Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/timeb.h>
#include <string.h>

#include "db_int.h"

/*
 * __os_clock --
 *	Return the current time-of-day clock in seconds and microseconds.
 */
int
__os_clock(dbenv, secsp, usecsp)
	DB_ENV *dbenv;
	u_int32_t *secsp, *usecsp;	/* Seconds and microseconds. */
{
	struct _timeb now;

	_ftime(&now);
	if (secsp != NULL)
		*secsp = (u_int32_t)now.time;
	if (usecsp != NULL)
		*usecsp = now.millitm * 1000;
	return (0);
}
