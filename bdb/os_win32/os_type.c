/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_type.c,v 11.3 2000/02/14 03:00:07 bostic Exp $";
#endif /* not lint */

/*
 * __os_is_winnt --
 *	Return 1 if Windows/NT, otherwise 0.
 *
 * PUBLIC: int __os_is_winnt __P((void));
 */
int
__os_is_winnt()
{
	static int __os_type = -1;

	/*
	 * The value of __os_type is computed only once, and cached to
	 * avoid the overhead of repeated calls to GetVersion().
	 */
	if (__os_type == -1)
		if ((GetVersion() & 0x80000000) == 0)
			__os_type = 1;
		else
			__os_type = 0;
	return (__os_type);
}
