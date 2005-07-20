/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_am.h,v 11.78 2004/09/22 21:14:56 ubell Exp $
 */
#ifndef _DB_AM_H_
#define	_DB_AM_H_

/*
 * IS_AUTO_COMMIT --
 *	Test for local auto-commit flag or global flag with no local DbTxn
 *	handle.
 */
#define	IS_AUTO_COMMIT(dbenv, txn, flags)				\
	(LF_ISSET(DB_AUTO_COMMIT) ||					\
	    ((txn) == NULL && F_ISSET((dbenv), DB_ENV_AUTO_COMMIT) &&	\
	    !LF_ISSET(DB_NO_AUTO_COMMIT)))

/* DB recovery operation codes. */
#define	DB_ADD_DUP	1
#define	DB_REM_DUP	2
#define	DB_ADD_BIG	3
#define	DB_REM_BIG	4

/*
 * Standard initialization and shutdown macros for all recovery functions.
 */
#define	REC_INTRO(func, inc_count) do {					\
	argp = NULL;							\
	dbc = NULL;							\
	file_dbp = NULL;						\
	/* mpf isn't used by all of the recovery functions. */		\
	COMPQUIET(mpf, NULL);						\
	if ((ret = func(dbenv, dbtp->data, &argp)) != 0)		\
		goto out;						\
	if ((ret = __dbreg_id_to_db(dbenv, argp->txnid,			\
	    &file_dbp, argp->fileid, inc_count)) != 0) {		\
		if (ret	== DB_DELETED) {				\
			ret = 0;					\
			goto done;					\
		}							\
		goto out;						\
	}								\
	if ((ret = __db_cursor(file_dbp, NULL, &dbc, 0)) != 0)		\
		goto out;						\
	F_SET(dbc, DBC_RECOVER);					\
	mpf = file_dbp->mpf;						\
} while (0)

#define	REC_CLOSE {							\
	int __t_ret;							\
	if (argp != NULL)						\
		__os_free(dbenv, argp);					\
	if (dbc != NULL &&						\
	    (__t_ret = __db_c_close(dbc)) != 0 && ret == 0)		\
		ret = __t_ret;						\
	}								\
	return (ret)

/*
 * No-op versions of the same macros.
 */
#define	REC_NOOP_INTRO(func) do {					\
	argp = NULL;							\
	if ((ret = func(dbenv, dbtp->data, &argp)) != 0)		\
		return (ret);						\
} while (0)
#define	REC_NOOP_CLOSE							\
	if (argp != NULL)						\
		__os_free(dbenv, argp);					\
	return (ret)

/*
 * Macro for reading pages during recovery.  In most cases we
 * want to avoid an error if the page is not found during rollback
 * or if we are using truncate to remove pages from the file.
 */
#ifndef HAVE_FTRUNCATE
#define	REC_FGET(mpf, pgno, pagep, cont)				\
	if ((ret = __memp_fget(mpf, &(pgno), 0, pagep)) != 0) {		\
		if (ret != DB_PAGE_NOTFOUND || DB_REDO(op)) {		\
			ret = __db_pgerr(file_dbp, pgno, ret);		\
			goto out;					\
		} else							\
			goto cont;					\
	}
#else
#define	REC_FGET(mpf, pgno, pagep, cont)				\
	if ((ret = __memp_fget(mpf, &(pgno), 0, pagep)) != 0) {		\
		if (ret != DB_PAGE_NOTFOUND) {				\
			ret = __db_pgerr(file_dbp, pgno, ret);		\
			goto out;					\
		} else							\
			goto cont;					\
	}
#endif

/*
 * Standard debugging macro for all recovery functions.
 */
#ifdef DEBUG_RECOVER
#define	REC_PRINT(func)							\
	(void)func(dbenv, dbtp, lsnp, op, info);
#else
#define	REC_PRINT(func)
#endif

/*
 * Actions to __db_lget
 */
#define	LCK_ALWAYS		1	/* Lock even for off page dup cursors */
#define	LCK_COUPLE		2	/* Lock Couple */
#define	LCK_COUPLE_ALWAYS	3	/* Lock Couple even in txn. */
#define	LCK_DOWNGRADE		4	/* Downgrade the lock. (internal) */
#define	LCK_ROLLBACK		5	/* Lock even if in rollback */

/*
 * If doing transactions we have to hold the locks associated with a data item
 * from a page for the entire transaction.  However, we don't have to hold the
 * locks associated with walking the tree.  Distinguish between the two so that
 * we don't tie up the internal pages of the tree longer than necessary.
 */
#define	__LPUT(dbc, lock)						\
	__ENV_LPUT((dbc)->dbp->dbenv, 					\
	     lock, F_ISSET((dbc)->dbp, DB_AM_DIRTY) ? DB_LOCK_DOWNGRADE : 0)
#define	__ENV_LPUT(dbenv, lock, flags)					\
	(LOCK_ISSET(lock) ? __lock_put(dbenv, &(lock), flags) : 0)

/*
 * __TLPUT -- transactional lock put
 *	If the lock is valid then
 *	   If we are not in a transaction put the lock.
 *	   Else if the cursor is doing dirty reads and this was a read then
 *		put the lock.
 *	   Else if the db is supporting dirty reads and this is a write then
 *		downgrade it.
 *	Else do nothing.
 */
#define	__TLPUT(dbc, lock)						\
	(LOCK_ISSET(lock) ? __db_lput(dbc, &(lock)) : 0)

typedef struct {
	DBC *dbc;
	u_int32_t count;
} db_trunc_param;

#include "dbinc/db_dispatch.h"
#include "dbinc_auto/db_auto.h"
#include "dbinc_auto/crdel_auto.h"
#include "dbinc_auto/db_ext.h"
#endif /* !_DB_AM_H_ */
