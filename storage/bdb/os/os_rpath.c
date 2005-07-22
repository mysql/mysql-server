/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_rpath.c,v 11.9 2004/01/28 03:36:18 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <string.h>
#endif

#include "db_int.h"
#ifdef HAVE_VXWORKS
#include "iosLib.h"
#endif

/*
 * __db_rpath --
 *	Return the last path separator in the path or NULL if none found.
 *
 * PUBLIC: char *__db_rpath __P((const char *));
 */
char *
__db_rpath(path)
	const char *path;
{
	const char *s, *last;
#ifdef HAVE_VXWORKS
	DEV_HDR *dummy;
	char *ptail;

	/*
	 * VxWorks devices can be rooted at any name.  We want to
	 * skip over the device name and not take into account any
	 * PATH_SEPARATOR characters that might be in that name.
	 *
	 * XXX [#2393]
	 * VxWorks supports having a filename directly follow a device
	 * name with no separator.  I.e. to access a file 'xxx' in
	 * the top level directory of a device mounted at "mydrive"
	 * you could say "mydrivexxx" or "mydrive/xxx" or "mydrive\xxx".
	 * We do not support the first usage here.
	 * XXX
	 */
	if ((dummy = iosDevFind((char *)path, &ptail)) == NULL)
		s = path;
	else
		s = ptail;
#else
	s = path;
#endif

	last = NULL;
	if (PATH_SEPARATOR[1] != '\0') {
		for (; s[0] != '\0'; ++s)
			if (strchr(PATH_SEPARATOR, s[0]) != NULL)
				last = s;
	} else
		for (; s[0] != '\0'; ++s)
			if (s[0] == PATH_SEPARATOR[0])
				last = s;
	return ((char *)last);
}
