/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_iface.c,v 11.77 2002/08/08 03:57:47 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"

static int __db_curinval __P((const DB_ENV *));
static int __db_fnl __P((const DB_ENV *, const char *));
static int __db_rdonly __P((const DB_ENV *, const char *));
static int __dbt_ferr __P((const DB *, const char *, const DBT *, int));

/*
 * A database should be required to be readonly if it's been explicitly
 * specified as such or if we're a client in a replicated environment and
 * we don't have the special "client-writer" designation.
 */
#define	IS_READONLY(dbp)						\
    (F_ISSET(dbp, DB_AM_RDONLY) ||					\
    (F_ISSET((dbp)->dbenv, DB_ENV_REP_CLIENT) &&			\
    !F_ISSET((dbp), DB_AM_CL_WRITER)))

/*
 * __db_cursorchk --
 *	Common cursor argument checking routine.
 *
 * PUBLIC: int __db_cursorchk __P((const DB *, u_int32_t));
 */
int
__db_cursorchk(dbp, flags)
	const DB *dbp;
	u_int32_t flags;
{
	/* DB_DIRTY_READ is the only valid bit-flag and requires locking. */
	if (LF_ISSET(DB_DIRTY_READ)) {
		if (!LOCKING_ON(dbp->dbenv))
			return (__db_fnl(dbp->dbenv, "DB->cursor"));
		LF_CLR(DB_DIRTY_READ);
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	case DB_WRITECURSOR:
		if (IS_READONLY(dbp))
			return (__db_rdonly(dbp->dbenv, "DB->cursor"));
		if (!CDB_LOCKING(dbp->dbenv))
			return (__db_ferr(dbp->dbenv, "DB->cursor", 0));
		break;
	case DB_WRITELOCK:
		if (IS_READONLY(dbp))
			return (__db_rdonly(dbp->dbenv, "DB->cursor"));
		break;
	default:
		return (__db_ferr(dbp->dbenv, "DB->cursor", 0));
	}

	return (0);
}

/*
 * __db_ccountchk --
 *	Common cursor count argument checking routine.
 *
 * PUBLIC: int __db_ccountchk __P((const DB *, u_int32_t, int));
 */
int
__db_ccountchk(dbp, flags, isvalid)
	const DB *dbp;
	u_int32_t flags;
	int isvalid;
{
	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	default:
		return (__db_ferr(dbp->dbenv, "DBcursor->c_count", 0));
	}

	/*
	 * The cursor must be initialized, return EINVAL for an invalid cursor,
	 * otherwise 0.
	 */
	return (isvalid ? 0 : __db_curinval(dbp->dbenv));
}

/*
 * __db_cdelchk --
 *	Common cursor delete argument checking routine.
 *
 * PUBLIC: int __db_cdelchk __P((const DB *, u_int32_t, int));
 */
int
__db_cdelchk(dbp, flags, isvalid)
	const DB *dbp;
	u_int32_t flags;
	int isvalid;
{
	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbp->dbenv, "c_del"));

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	case DB_UPDATE_SECONDARY:
		DB_ASSERT(F_ISSET(dbp, DB_AM_SECONDARY));
		break;
	default:
		return (__db_ferr(dbp->dbenv, "DBcursor->c_del", 0));
	}

	/*
	 * The cursor must be initialized, return EINVAL for an invalid cursor,
	 * otherwise 0.
	 */
	return (isvalid ? 0 : __db_curinval(dbp->dbenv));
}

/*
 * __db_cgetchk --
 *	Common cursor get argument checking routine.
 *
 * PUBLIC: int __db_cgetchk __P((const DB *, DBT *, DBT *, u_int32_t, int));
 */
int
__db_cgetchk(dbp, key, data, flags, isvalid)
	const DB *dbp;
	DBT *key, *data;
	u_int32_t flags;
	int isvalid;
{
	int dirty, multi, ret;

	/*
	 * Check for read-modify-write validity.  DB_RMW doesn't make sense
	 * with CDB cursors since if you're going to write the cursor, you
	 * had to create it with DB_WRITECURSOR.  Regardless, we check for
	 * LOCKING_ON and not STD_LOCKING, as we don't want to disallow it.
	 * If this changes, confirm that DB does not itself set the DB_RMW
	 * flag in a path where CDB may have been configured.
	 */
	dirty = 0;
	if (LF_ISSET(DB_DIRTY_READ | DB_RMW)) {
		if (!LOCKING_ON(dbp->dbenv))
			return (__db_fnl(dbp->dbenv, "DBcursor->c_get"));
		if (LF_ISSET(DB_DIRTY_READ))
			dirty = 1;
		LF_CLR(DB_DIRTY_READ | DB_RMW);
	}

	multi = 0;
	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		multi = 1;
		if (LF_ISSET(DB_MULTIPLE) && LF_ISSET(DB_MULTIPLE_KEY))
			goto multi_err;
		LF_CLR(DB_MULTIPLE | DB_MULTIPLE_KEY);
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		if (dirty) {
			__db_err(dbp->dbenv,
    "DB_DIRTY_READ is not supported with DB_CONSUME or DB_CONSUME_WAIT");
			return (EINVAL);
		}
		if (dbp->type != DB_QUEUE)
			goto err;
		break;
	case DB_CURRENT:
	case DB_FIRST:
	case DB_GET_BOTH:
	case DB_GET_BOTH_RANGE:
	case DB_NEXT:
	case DB_NEXT_DUP:
	case DB_NEXT_NODUP:
	case DB_SET:
	case DB_SET_RANGE:
		break;
	case DB_LAST:
	case DB_PREV:
	case DB_PREV_NODUP:
		if (multi)
multi_err:		return (__db_ferr(dbp->dbenv, "DBcursor->c_get", 1));
		break;
	case DB_GET_BOTHC:
		if (dbp->type == DB_QUEUE)
			goto err;
		break;
	case DB_GET_RECNO:
		/*
		 * The one situation in which this might be legal with a
		 * non-RECNUM dbp is if dbp is a secondary and its primary is
		 * DB_AM_RECNUM.
		 */
		if (!F_ISSET(dbp, DB_AM_RECNUM) &&
		    (!F_ISSET(dbp, DB_AM_SECONDARY) ||
		    !F_ISSET(dbp->s_primary, DB_AM_RECNUM)))
			goto err;
		break;
	case DB_SET_RECNO:
		if (!F_ISSET(dbp, DB_AM_RECNUM))
			goto err;
		break;
	default:
err:		return (__db_ferr(dbp->dbenv, "DBcursor->c_get", 0));
	}

	/* Check for invalid key/data flags. */
	if ((ret = __dbt_ferr(dbp, "key", key, 0)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 0)) != 0)
		return (ret);

	if (multi && !F_ISSET(data, DB_DBT_USERMEM)) {
		__db_err(dbp->dbenv,
		    "DB_MULTIPLE(_KEY) requires that DB_DBT_USERMEM be set");
		return (EINVAL);
	}
	if (multi &&
	    (F_ISSET(key, DB_DBT_PARTIAL) || F_ISSET(data, DB_DBT_PARTIAL))) {
		__db_err(dbp->dbenv,
		    "DB_DBT_PARTIAL forbidden with DB_MULTIPLE(_KEY)");
		return (EINVAL);
	}

	/*
	 * The cursor must be initialized for DB_CURRENT, DB_GET_RECNO and
	 * DB_NEXT_DUP.  Return EINVAL for an invalid cursor, otherwise 0.
	 */
	if (isvalid || (flags != DB_CURRENT &&
	    flags != DB_GET_RECNO && flags != DB_NEXT_DUP))
		return (0);

	return (__db_curinval(dbp->dbenv));
}

/*
 * __db_cputchk --
 *	Common cursor put argument checking routine.
 *
 * PUBLIC: int __db_cputchk __P((const DB *,
 * PUBLIC:    const DBT *, DBT *, u_int32_t, int));
 */
int
__db_cputchk(dbp, key, data, flags, isvalid)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int32_t flags;
	int isvalid;
{
	int key_flags, ret;

	key_flags = 0;

	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbp->dbenv, "c_put"));

	/* Check for puts on a secondary. */
	if (F_ISSET(dbp, DB_AM_SECONDARY)) {
		if (flags == DB_UPDATE_SECONDARY)
			flags = DB_KEYLAST;
		else {
			__db_err(dbp->dbenv,
		    "DBcursor->c_put forbidden on secondary indices");
			return (EINVAL);
		}
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case DB_AFTER:
	case DB_BEFORE:
		switch (dbp->type) {
		case DB_BTREE:
		case DB_HASH:		/* Only with unsorted duplicates. */
			if (!F_ISSET(dbp, DB_AM_DUP))
				goto err;
			if (dbp->dup_compare != NULL)
				goto err;
			break;
		case DB_QUEUE:		/* Not permitted. */
			goto err;
		case DB_RECNO:		/* Only with mutable record numbers. */
			if (!F_ISSET(dbp, DB_AM_RENUMBER))
				goto err;
			key_flags = 1;
			break;
		default:
			goto err;
		}
		break;
	case DB_CURRENT:
		/*
		 * If there is a comparison function, doing a DB_CURRENT
		 * must not change the part of the data item that is used
		 * for the comparison.
		 */
		break;
	case DB_NODUPDATA:
		if (!F_ISSET(dbp, DB_AM_DUPSORT))
			goto err;
		/* FALLTHROUGH */
	case DB_KEYFIRST:
	case DB_KEYLAST:
		key_flags = 1;
		break;
	default:
err:		return (__db_ferr(dbp->dbenv, "DBcursor->c_put", 0));
	}

	/* Check for invalid key/data flags. */
	if (key_flags && (ret = __dbt_ferr(dbp, "key", key, 0)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 0)) != 0)
		return (ret);

	/*
	 * The cursor must be initialized for anything other than DB_KEYFIRST
	 * and DB_KEYLAST, return EINVAL for an invalid cursor, otherwise 0.
	 */
	if (isvalid || flags == DB_KEYFIRST ||
	    flags == DB_KEYLAST || flags == DB_NODUPDATA)
		return (0);

	return (__db_curinval(dbp->dbenv));
}

/*
 * __db_pgetchk --
 *	DB->pget flag check.
 *
 * PUBLIC: int __db_pgetchk __P((const DB *, const DBT *, DBT *, DBT *,
 * PUBLIC:	u_int32_t));
 */
int
__db_pgetchk(dbp, skey, pkey, data, flags)
	const DB *dbp;
	const DBT *skey;
	DBT *pkey, *data;
	u_int32_t flags;
{
	int ret;
	u_int32_t save_flags;

	save_flags = flags;

	if (!F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbp->dbenv,
		    "DB->pget may only be used on secondary indices");
		return (EINVAL);
	}

	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		__db_err(dbp->dbenv,
	"DB_MULTIPLE and DB_MULTIPLE_KEY may not be used on secondary indices");
		return (EINVAL);
	}

	/* DB_CONSUME makes no sense on a secondary index. */
	LF_CLR(DB_RMW);
	switch (flags) {
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		return (__db_ferr(dbp->dbenv, "DB->pget", 0));
	default:
		/* __db_getchk will catch the rest. */
		break;
	}

	/*
	 * We allow the pkey field to be NULL, so that we can make the
	 * two-DBT get calls into wrappers for the three-DBT ones.
	 */
	if (pkey != NULL &&
	    (ret = __dbt_ferr(dbp, "primary key", pkey, 1)) != 0)
		return (ret);

	/* But the pkey field can't be NULL if we're doing a DB_GET_BOTH. */
	if (pkey == NULL && flags == DB_GET_BOTH) {
		__db_err(dbp->dbenv,
		    "DB_GET_BOTH on a secondary index requires a primary key");
		return (EINVAL);
	}

	return (__db_getchk(dbp, skey, data, save_flags));
}

/*
 * __db_cpgetchk --
 *	Secondary-index cursor get argument checking routine.
 *
 * PUBLIC: int __db_cpgetchk __P((const DB *,
 * PUBLIC:     DBT *, DBT *, DBT *, u_int32_t, int));
 */
int
__db_cpgetchk(dbp, skey, pkey, data, flags, isvalid)
	const DB *dbp;
	DBT *skey, *pkey, *data;
	u_int32_t flags;
	int isvalid;
{
	int ret;
	u_int32_t save_flags;

	save_flags = flags;

	if (!F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbp->dbenv,
		    "DBcursor->c_pget may only be used on secondary indices");
		return (EINVAL);
	}

	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		__db_err(dbp->dbenv,
	"DB_MULTIPLE and DB_MULTIPLE_KEY may not be used on secondary indices");
		return (EINVAL);
	}

	LF_CLR(DB_RMW);
	switch (flags) {
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		/* DB_CONSUME makes no sense on a secondary index. */
		return (__db_ferr(dbp->dbenv, "DBcursor->c_pget", 0));
	case DB_GET_BOTH:
		/* DB_GET_BOTH is "get both the primary and the secondary". */
		if (pkey == NULL) {
			__db_err(dbp->dbenv,
		    "DB_GET_BOTH requires both a secondary and a primary key");
			return (EINVAL);
		}
		break;
	default:
		/* __db_cgetchk will catch the rest. */
		break;
	}

	/*
	 * We allow the pkey field to be NULL, so that we can make the
	 * two-DBT get calls into wrappers for the three-DBT ones.
	 */
	if (pkey != NULL &&
	    (ret = __dbt_ferr(dbp, "primary key", pkey, 0)) != 0)
		return (ret);

	/* But the pkey field can't be NULL if we're doing a DB_GET_BOTH. */
	if (pkey == NULL && flags == DB_GET_BOTH) {
		__db_err(dbp->dbenv,
		    "DB_GET_BOTH on a secondary index requires a primary key");
		return (EINVAL);
	}

	return (__db_cgetchk(dbp, skey, data, save_flags, isvalid));
}

/*
 * __db_delchk --
 *	Common delete argument checking routine.
 *
 * PUBLIC: int __db_delchk __P((const DB *, DBT *, u_int32_t));
 */
int
__db_delchk(dbp, key, flags)
	const DB *dbp;
	DBT *key;
	u_int32_t flags;
{
	COMPQUIET(key, NULL);

	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbp->dbenv, "delete"));

	/* Check for invalid function flags. */
	LF_CLR(DB_AUTO_COMMIT);
	switch (flags) {
	case 0:
		break;
	default:
		return (__db_ferr(dbp->dbenv, "DB->del", 0));
	}

	return (0);
}

/*
 * __db_getchk --
 *	Common get argument checking routine.
 *
 * PUBLIC: int __db_getchk __P((const DB *, const DBT *, DBT *, u_int32_t));
 */
int
__db_getchk(dbp, key, data, flags)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int32_t flags;
{
	int dirty, multi, ret;

	/*
	 * Check for read-modify-write validity.  DB_RMW doesn't make sense
	 * with CDB cursors since if you're going to write the cursor, you
	 * had to create it with DB_WRITECURSOR.  Regardless, we check for
	 * LOCKING_ON and not STD_LOCKING, as we don't want to disallow it.
	 * If this changes, confirm that DB does not itself set the DB_RMW
	 * flag in a path where CDB may have been configured.
	 */
	dirty = 0;
	if (LF_ISSET(DB_DIRTY_READ | DB_RMW)) {
		if (!LOCKING_ON(dbp->dbenv))
			return (__db_fnl(dbp->dbenv, "DB->get"));
		if (LF_ISSET(DB_DIRTY_READ))
			dirty = 1;
		LF_CLR(DB_DIRTY_READ | DB_RMW);
	}

	multi = 0;
	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		if (LF_ISSET(DB_MULTIPLE_KEY))
			goto multi_err;
		multi = LF_ISSET(DB_MULTIPLE) ? 1 : 0;
		LF_CLR(DB_MULTIPLE);
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
	case DB_GET_BOTH:
		break;
	case DB_SET_RECNO:
		if (!F_ISSET(dbp, DB_AM_RECNUM))
			goto err;
		break;
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		if (dirty) {
			__db_err(dbp->dbenv,
    "DB_DIRTY_READ is not supported with DB_CONSUME or DB_CONSUME_WAIT");
			return (EINVAL);
		}
		if (multi)
multi_err:		return (__db_ferr(dbp->dbenv, "DB->get", 1));
		if (dbp->type == DB_QUEUE)
			break;
		/* FALLTHROUGH */
	default:
err:		return (__db_ferr(dbp->dbenv, "DB->get", 0));
	}

	/*
	 * Check for invalid key/data flags.
	 *
	 * XXX: Dave Krinsky
	 * Remember to modify this when we fix the flag-returning problem.
	 */
	if ((ret = __dbt_ferr(dbp, "key", key, flags == DB_SET_RECNO)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 1)) != 0)
		return (ret);

	if (multi && !F_ISSET(data, DB_DBT_USERMEM)) {
		__db_err(dbp->dbenv,
		    "DB_MULTIPLE requires that DB_DBT_USERMEM be set");
		return (EINVAL);
	}
	if (multi &&
	    (F_ISSET(key, DB_DBT_PARTIAL) || F_ISSET(data, DB_DBT_PARTIAL))) {
		__db_err(dbp->dbenv,
		    "DB_DBT_PARTIAL forbidden with DB_MULTIPLE(_KEY)");
		return (EINVAL);
	}

	return (0);
}

/*
 * __db_joinchk --
 *	Common join argument checking routine.
 *
 * PUBLIC: int __db_joinchk __P((const DB *, DBC * const *, u_int32_t));
 */
int
__db_joinchk(dbp, curslist, flags)
	const DB *dbp;
	DBC * const *curslist;
	u_int32_t flags;
{
	DB_TXN *txn;
	int i;

	switch (flags) {
	case 0:
	case DB_JOIN_NOSORT:
		break;
	default:
		return (__db_ferr(dbp->dbenv, "DB->join", 0));
	}

	if (curslist == NULL || curslist[0] == NULL) {
		__db_err(dbp->dbenv,
	    "At least one secondary cursor must be specified to DB->join");
		return (EINVAL);
	}

	txn = curslist[0]->txn;
	for (i = 1; curslist[i] != NULL; i++)
		if (curslist[i]->txn != txn) {
			__db_err(dbp->dbenv,
		    "All secondary cursors must share the same transaction");
			return (EINVAL);
		}

	return (0);
}

/*
 * __db_joingetchk --
 *	Common join_get argument checking routine.
 *
 * PUBLIC: int __db_joingetchk __P((const DB *, DBT *, u_int32_t));
 */
int
__db_joingetchk(dbp, key, flags)
	const DB *dbp;
	DBT *key;
	u_int32_t flags;
{

	if (LF_ISSET(DB_DIRTY_READ | DB_RMW)) {
		if (!LOCKING_ON(dbp->dbenv))
			return (__db_fnl(dbp->dbenv, "DBcursor->c_get"));

		LF_CLR(DB_DIRTY_READ | DB_RMW);
	}

	switch (flags) {
	case 0:
	case DB_JOIN_ITEM:
		break;
	default:
		return (__db_ferr(dbp->dbenv, "DBcursor->c_get", 0));
	}

	/*
	 * A partial get of the key of a join cursor don't make much sense;
	 * the entire key is necessary to query the primary database
	 * and find the datum, and so regardless of the size of the key
	 * it would not be a performance improvement.  Since it would require
	 * special handling, we simply disallow it.
	 *
	 * A partial get of the data, however, potentially makes sense (if
	 * all possible data are a predictable large structure, for instance)
	 * and causes us no headaches, so we permit it.
	 */
	if (F_ISSET(key, DB_DBT_PARTIAL)) {
		__db_err(dbp->dbenv,
		    "DB_DBT_PARTIAL may not be set on key during join_get");
		return (EINVAL);
	}

	return (0);
}

/*
 * __db_putchk --
 *	Common put argument checking routine.
 *
 * PUBLIC: int __db_putchk
 * PUBLIC:    __P((const DB *, DBT *, const DBT *, u_int32_t, int));
 */
int
__db_putchk(dbp, key, data, flags, isdup)
	const DB *dbp;
	DBT *key;
	const DBT *data;
	u_int32_t flags;
	int isdup;
{
	int ret, returnkey;

	returnkey = 0;

	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbp->dbenv, "put"));

	/* Check for puts on a secondary. */
	if (F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbp->dbenv, "DB->put forbidden on secondary indices");
		return (EINVAL);
	}

	/* Check for invalid function flags. */
	LF_CLR(DB_AUTO_COMMIT);
	switch (flags) {
	case 0:
	case DB_NOOVERWRITE:
		break;
	case DB_APPEND:
		if (dbp->type != DB_RECNO && dbp->type != DB_QUEUE)
			goto err;
		returnkey = 1;
		break;
	case DB_NODUPDATA:
		if (F_ISSET(dbp, DB_AM_DUPSORT))
			break;
		/* FALLTHROUGH */
	default:
err:		return (__db_ferr(dbp->dbenv, "DB->put", 0));
	}

	/* Check for invalid key/data flags. */
	if ((ret = __dbt_ferr(dbp, "key", key, returnkey)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 0)) != 0)
		return (ret);

	/* Check for partial puts in the presence of duplicates. */
	if (isdup && F_ISSET(data, DB_DBT_PARTIAL)) {
		__db_err(dbp->dbenv,
"a partial put in the presence of duplicates requires a cursor operation");
		return (EINVAL);
	}

	return (0);
}

/*
 * __db_statchk --
 *	Common stat argument checking routine.
 *
 * PUBLIC: int __db_statchk __P((const DB *, u_int32_t));
 */
int
__db_statchk(dbp, flags)
	const DB *dbp;
	u_int32_t flags;
{
	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
	case DB_FAST_STAT:
	case DB_CACHED_COUNTS:		/* Deprecated and undocumented. */
		break;
	case DB_RECORDCOUNT:		/* Deprecated and undocumented. */
		if (dbp->type == DB_RECNO)
			break;
		if (dbp->type == DB_BTREE && F_ISSET(dbp, DB_AM_RECNUM))
			break;
		goto err;
	default:
err:		return (__db_ferr(dbp->dbenv, "DB->stat", 0));
	}

	return (0);
}

/*
 * __db_syncchk --
 *	Common sync argument checking routine.
 *
 * PUBLIC: int __db_syncchk __P((const DB *, u_int32_t));
 */
int
__db_syncchk(dbp, flags)
	const DB *dbp;
	u_int32_t flags;
{
	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	default:
		return (__db_ferr(dbp->dbenv, "DB->sync", 0));
	}

	return (0);
}

/*
 * __dbt_ferr --
 *	Check a DBT for flag errors.
 */
static int
__dbt_ferr(dbp, name, dbt, check_thread)
	const DB *dbp;
	const char *name;
	const DBT *dbt;
	int check_thread;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * Check for invalid DBT flags.  We allow any of the flags to be
	 * specified to any DB or DBcursor call so that applications can
	 * set DB_DBT_MALLOC when retrieving a data item from a secondary
	 * database and then specify that same DBT as a key to a primary
	 * database, without having to clear flags.
	 */
	if ((ret = __db_fchk(dbenv, name, dbt->flags, DB_DBT_APPMALLOC |
	    DB_DBT_MALLOC | DB_DBT_DUPOK | DB_DBT_REALLOC | DB_DBT_USERMEM |
	    DB_DBT_PARTIAL)) != 0)
		return (ret);
	switch (F_ISSET(dbt, DB_DBT_MALLOC | DB_DBT_REALLOC | DB_DBT_USERMEM)) {
	case 0:
	case DB_DBT_MALLOC:
	case DB_DBT_REALLOC:
	case DB_DBT_USERMEM:
		break;
	default:
		return (__db_ferr(dbenv, name, 1));
	}

	if (check_thread && DB_IS_THREADED(dbp) &&
	    !F_ISSET(dbt, DB_DBT_MALLOC | DB_DBT_REALLOC | DB_DBT_USERMEM)) {
		__db_err(dbenv,
		    "DB_THREAD mandates memory allocation flag on DBT %s",
		    name);
		return (EINVAL);
	}
	return (0);
}

/*
 * __db_rdonly --
 *	Common readonly message.
 */
static int
__db_rdonly(dbenv, name)
	const DB_ENV *dbenv;
	const char *name;
{
	__db_err(dbenv, "%s: attempt to modify a read-only tree", name);
	return (EACCES);
}

/*
 * __db_fnl --
 *	Common flag-needs-locking message.
 */
static int
__db_fnl(dbenv, name)
	const DB_ENV *dbenv;
	const char *name;
{
	__db_err(dbenv,
	    "%s: the DB_DIRTY_READ and DB_RMW flags require locking", name);
	return (EINVAL);
}

/*
 * __db_curinval
 *	Report that a cursor is in an invalid state.
 */
static int
__db_curinval(dbenv)
	const DB_ENV *dbenv;
{
	__db_err(dbenv,
	    "Cursor position must be set before performing this operation");
	return (EINVAL);
}

/*
 * __db_secondary_corrupt --
 *	Report that a secondary index appears corrupt, as it has a record
 * that does not correspond to a record in the primary.
 *
 * PUBLIC: int __db_secondary_corrupt __P((DB *));
 */
int
__db_secondary_corrupt(dbp)
	DB *dbp;
{

	__db_err(dbp->dbenv,
	    "Secondary index corrupt: item in secondary not found in primary");
	return (DB_SECONDARY_BAD);
}

/*
 * __db_associatechk --
 *	Argument checking routine for DB->associate().
 *
 * PUBLIC: int __db_associatechk __P((DB *, DB *,
 * PUBLIC:     int (*)(DB *, const DBT *, const DBT *, DBT *), u_int32_t));
 */
int
__db_associatechk(dbp, sdbp, callback, flags)
	DB *dbp, *sdbp;
	int (*callback) __P((DB *, const DBT *, const DBT *, DBT *));
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	if (F_ISSET(sdbp, DB_AM_SECONDARY)) {
		__db_err(dbenv,
		    "Secondary index handles may not be re-associated");
		return (EINVAL);
	}
	if (F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbenv,
		    "Secondary indices may not be used as primary databases");
		return (EINVAL);
	}
	if (F_ISSET(dbp, DB_AM_DUP)) {
		__db_err(dbenv,
		    "Primary databases may not be configured with duplicates");
		return (EINVAL);
	}
	if (F_ISSET(dbp, DB_AM_RENUMBER)) {
		__db_err(dbenv,
	    "Renumbering recno databases may not be used as primary databases");
		return (EINVAL);
	}
	if (callback == NULL &&
	    (!F_ISSET(dbp, DB_AM_RDONLY) || !F_ISSET(sdbp, DB_AM_RDONLY))) {
		__db_err(dbenv,
    "Callback function may be NULL only when database handles are read-only");
		return (EINVAL);
	}

	return (__db_fchk(dbenv,
	    "DB->associate", flags, DB_CREATE | DB_AUTO_COMMIT));
}

/*
 * __db_txn_auto --
 *	Handle DB_AUTO_COMMIT initialization.
 *
 * PUBLIC: int __db_txn_auto __P((DB *, DB_TXN **));
 */
int
__db_txn_auto(dbp, txnidp)
	DB *dbp;
	DB_TXN **txnidp;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	if (*txnidp != NULL) {
		__db_err(dbenv,
    "DB_AUTO_COMMIT may not be specified along with a transaction handle");
		return (EINVAL);
	}

	if (!TXN_ON(dbenv)) {
		__db_err(dbenv,
    "DB_AUTO_COMMIT may not be specified in non-transactional environment");
		return (EINVAL);
	}

	return (dbenv->txn_begin(dbenv, NULL, txnidp, 0));
}
