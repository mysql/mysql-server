/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_id.c,v 1.2 2002/01/11 15:52:59 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

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
	*idp = getpid();
#endif
}
