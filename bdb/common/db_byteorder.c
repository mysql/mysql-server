/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_byteorder.c,v 11.4 2000/11/30 00:58:31 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#if BYTE_ORDER == BIG_ENDIAN
#define	WORDS_BIGENDIAN	1
#endif
#endif

#endif

#include "db_int.h"
#include "common_ext.h"

/*
 * __db_byteorder --
 *	Return if we need to do byte swapping, checking for illegal
 *	values.
 *
 * PUBLIC: int __db_byteorder __P((DB_ENV *, int));
 */
int
__db_byteorder(dbenv, lorder)
	DB_ENV *dbenv;
	int lorder;
{
	switch (lorder) {
	case 0:
		break;
	case 1234:
#if defined(WORDS_BIGENDIAN)
		return (DB_SWAPBYTES);
#else
		break;
#endif
	case 4321:
#if defined(WORDS_BIGENDIAN)
		break;
#else
		return (DB_SWAPBYTES);
#endif
	default:
		__db_err(dbenv,
	    "unsupported byte order, only big and little-endian supported");
		return (EINVAL);
	}
	return (0);
}
