/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: bt_reclaim.c,v 11.15 2004/01/28 03:35:49 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"

/*
 * __bam_reclaim --
 *	Free a database.
 *
 * PUBLIC: int __bam_reclaim __P((DB *, DB_TXN *));
 */
int
__bam_reclaim(dbp, txn)
	DB *dbp;
	DB_TXN *txn;
{
	DBC *dbc;
	int ret, t_ret;

	/* Acquire a cursor. */
	if ((ret = __db_cursor(dbp, txn, &dbc, 0)) != 0)
		return (ret);

	/* Walk the tree, freeing pages. */
	ret = __bam_traverse(dbc,
	    DB_LOCK_WRITE, dbc->internal->root, __db_reclaim_callback, dbc);

	/* Discard the cursor. */
	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __bam_truncate --
 *	Truncate a database.
 *
 * PUBLIC: int __bam_truncate __P((DBC *, u_int32_t *));
 */
int
__bam_truncate(dbc, countp)
	DBC *dbc;
	u_int32_t *countp;
{
	db_trunc_param trunc;
	int ret;

	trunc.count = 0;
	trunc.dbc = dbc;

	/* Walk the tree, freeing pages. */
	ret = __bam_traverse(dbc,
	    DB_LOCK_WRITE, dbc->internal->root, __db_truncate_callback, &trunc);

	*countp = trunc.count;

	return (ret);
}
