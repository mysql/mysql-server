/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: log_compare.c,v 11.8 2004/01/28 03:36:17 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"

/*
 * log_compare --
 *	Compare two LSN's; return 1, 0, -1 if first is >, == or < second.
 *
 * EXTERN: int log_compare __P((const DB_LSN *, const DB_LSN *));
 */
int
log_compare(lsn0, lsn1)
	const DB_LSN *lsn0, *lsn1;
{
	if (lsn0->file != lsn1->file)
		return (lsn0->file < lsn1->file ? -1 : 1);

	if (lsn0->offset != lsn1->offset)
		return (lsn0->offset < lsn1->offset ? -1 : 1);

	return (0);
}
