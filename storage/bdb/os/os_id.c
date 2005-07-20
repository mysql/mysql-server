/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_id.c,v 1.9 2004/09/22 16:27:54 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_id --
 *	Return a 32-bit value identifying the current thread of control.
 *
 * PUBLIC: void __os_id __P((u_int32_t *));
 */
void
__os_id(idp)
	u_int32_t *idp;
{
	/*
	 * By default, use the process ID.
	 *
	 * getpid() returns a pid_t which we convert to a u_int32_t.  I have
	 * not yet seen a system where a pid_t has 64-bits, but I'm sure they
	 * exist.  Since we're returning only the bottom 32-bits, you cannot
	 * use the return of __os_id to reference a process (for example, you
	 * cannot send a signal to the value returned by __os_id).  To send a
	 * signal to the current process, use raise(3) instead.
	 */
#ifdef	HAVE_VXWORKS
	*idp = taskIdSelf();
#else
	*idp = (u_int32_t)getpid();
#endif
}

/*
 * __os_unique_id --
 *	Return a unique 32-bit value.
 *
 * PUBLIC: void __os_unique_id __P((DB_ENV *, u_int32_t *));
 */
void
__os_unique_id(dbenv, idp)
	DB_ENV *dbenv;
	u_int32_t *idp;
{
	static int first = 1;
	u_int32_t id, pid, sec, usec;

	*idp = 0;

	/*
	 * Our randomized value is comprised of our process ID, the current
	 * time of day and a couple of a stack addresses, all XOR'd together.
	 */
	__os_id(&pid);
	__os_clock(dbenv, &sec, &usec);

	id = pid ^ sec ^ usec ^ P_TO_UINT32(&pid);

	/*
	 * We could try and find a reasonable random-number generator, but
	 * that's not all that easy to do.  Seed and use srand()/rand(), if
	 * we can find them.
	 */
#if HAVE_SRAND
	if (first == 1)
		srand((u_int)id);
#endif
	first = 0;

#if HAVE_RAND
	id ^= (u_int)rand();
#endif

	*idp = id;
}
