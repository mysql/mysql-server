/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_truncate.c,v 11.185 2002/08/07 16:16:48 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/qam.h"

/*
 * __db_truncate
 *	truncate method for DB.
 *
 * PUBLIC: int __db_truncate __P((DB *, DB_TXN *, u_int32_t *, u_int32_t));
 */
int
__db_truncate(dbp, txn, countp, flags)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t *countp, flags;
{
	DB_ENV *dbenv;
	int ret, t_ret, txn_local;

	dbenv = dbp->dbenv;
	ret = txn_local = 0;

	PANIC_CHECK(dbenv);

	/* Check for invalid flags. */
	if ((ret =
	    __db_fchk(dbenv, "DB->truncate", flags, DB_AUTO_COMMIT)) != 0)
		return (ret);

	/*
	 * Create local transaction as necessary, check for consistent
	 * transaction usage.
	 */
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto(dbp, &txn)) != 0)
			return (ret);
		txn_local = 1;
	} else
		if (txn != NULL && !TXN_ON(dbenv))
			return (__db_not_txn_env(dbenv));

	DB_TEST_RECOVERY(dbp, DB_TEST_PREDESTROY, ret, NULL);
	switch (dbp->type) {
		case DB_BTREE:
		case DB_RECNO:
			if ((ret = __bam_truncate(dbp, txn, countp)) != 0)
				goto err;
			break;
		case DB_HASH:
			if ((ret = __ham_truncate(dbp, txn, countp)) != 0)
				goto err;
			break;
		case DB_QUEUE:
			if ((ret = __qam_truncate(dbp, txn, countp)) != 0)
				goto err;
			break;
		default:
			ret = __db_unknown_type(
			    dbenv, "__db_truncate", dbp->type);
			goto err;
	}
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTDESTROY, ret, NULL);

DB_TEST_RECOVERY_LABEL
err:
	/* Commit for DB_AUTO_COMMIT. */
	if (txn_local) {
		if (ret == 0)
			ret = txn->commit(txn, 0);
		else
			if ((t_ret = txn->abort(txn)) != 0)
				ret = __db_panic(dbenv, t_ret);
	}

	return (ret);
}
