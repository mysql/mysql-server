/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log_compare.c,v 11.3 2000/02/14 02:59:59 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"

/*
 * log_compare --
 *	Compare two LSN's; return 1, 0, -1 if first is >, == or < second.
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
