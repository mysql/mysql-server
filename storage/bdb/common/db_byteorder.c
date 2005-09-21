/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_byteorder.c,v 11.10 2004/01/28 03:35:52 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"

/*
 * __db_isbigendian --
 *	Return 1 if big-endian (Motorola and Sparc), not little-endian
 *	(Intel and Vax).  We do this work at run-time, rather than at
 *	configuration time so cross-compilation and general embedded
 *	system support is simpler.
 *
 * PUBLIC: int __db_isbigendian __P((void));
 */
int
__db_isbigendian()
{
	union {					/* From Harbison & Steele.  */
		long l;
		char c[sizeof(long)];
	} u;

	u.l = 1;
	return (u.c[sizeof(long) - 1] == 1);
}

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
	int is_bigendian;

	is_bigendian = __db_isbigendian();

	switch (lorder) {
	case 0:
		break;
	case 1234:
		if (is_bigendian)
			return (DB_SWAPBYTES);
		break;
	case 4321:
		if (!is_bigendian)
			return (DB_SWAPBYTES);
		break;
	default:
		__db_err(dbenv,
	    "unsupported byte order, only big and little-endian supported");
		return (EINVAL);
	}
	return (0);
}
