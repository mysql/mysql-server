/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_clock.c,v 12.1 2005/06/16 20:23:28 bostic Exp $
 */

#include "db_config.h"

#include <sys/types.h>
#include <sys/timeb.h>
#include <string.h>

#include "db_int.h"

/*
 * __os_clock --
 *	Return the current time-of-day clock in seconds and microseconds.
 */
void
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
}
