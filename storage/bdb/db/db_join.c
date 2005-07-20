/*
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_join.c,v 11.75 2004/09/22 03:30:23 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_join.h"
#include "dbinc/btree.h"

static int __db_join_close_pp __P((DBC *));
static int __db_join_cmp __P((const void *, const void *));
static int __db_join_del __P((DBC *, u_int32_t));
static int __db_join_get __P((DBC *, DBT *, DBT *, u_int32_t));
static int __db_join_get_pp __P((DBC *, DBT *, DBT *, u_int32_t));
static int __db_join_getnext __P((DBC *, DBT *, DBT *, u_int32_t, u_int32_t));
static int __db_join_primget __P((DB *,
    DB_TXN *, u_int32_t, DBT *, DBT *, u_int32_t));
static int __db_join_put __P((DBC *, DBT *, DBT *, u_int32_t));

/*
 * Check to see if the Nth secondary cursor of join cursor jc is pointing
 * to a sorted duplicate set.
 */
#define	SORTED_SET(jc, n)   ((jc)->j_curslist[(n)]->dbp->dup_compare != NULL)

/*
 * This is the duplicate-assisted join functionality.  Right now we're
 * going to write it such that we return one item at a time, although
 * I think we may need to optimize it to return them all at once.
 * It should be easier to get it working this way, and I believe that
 * changing it should be fairly straightforward.
 *
 * We optimize the join by sorting cursors from smallest to largest
 * cardinality.  In most cases, this is indeed optimal.  However, if
 * a cursor with large cardinality has very few data in common with the
 * first cursor, it is possible that the join will be made faster by
 * putting it earlier in the cursor list.  Since we have no way to detect
 * cases like this, we simply provide a flag, DB_JOIN_NOSORT, which retains
 * the sort order specified by the caller, who may know more about the
 * structure of the data.
 *
 * The first cursor moves sequentially through the duplicate set while
 * the others search explicitly for the duplicate in question.
 *
 */

/*
 * __db_join --
 *	This is the interface to the duplicate-assisted join functionality.
 * In the same way that cursors mark a position in a database, a cursor
 * can mark a position in a join.  While most cursors are created by the
 * cursor method of a DB, join cursors are created through an explicit
 * call to DB->join.
 *
 * The curslist is an array of existing, initialized cursors and primary
 * is the DB of the primary file.  The data item that joins all the
 * cursors in the curslist is used as the key into the primary and that
 * key and data are returned.  When no more items are left in the join
 * set, the  c_next operation off the join cursor will return DB_NOTFOUND.
 *
 * PUBLIC: int __db_join __P((DB *, DBC **, DBC **, u_int32_t));
 */
int
__db_join(primary, curslist, dbcp, flags)
	DB *primary;
	DBC **curslist, **dbcp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *dbc;
	JOIN_CURSOR *jc;
	size_t ncurs, nslots;
	u_int32_t i;
	int ret;

	dbenv = primary->dbenv;
	dbc = NULL;
	jc = NULL;

	if ((ret = __os_calloc(dbenv, 1, sizeof(DBC), &dbc)) != 0)
		goto err;

	if ((ret = __os_calloc(dbenv, 1, sizeof(JOIN_CURSOR), &jc)) != 0)
		goto err;

	if ((ret = __os_malloc(dbenv, 256, &jc->j_key.data)) != 0)
		goto err;
	jc->j_key.ulen = 256;
	F_SET(&jc->j_key, DB_DBT_USERMEM);

	F_SET(&jc->j_rdata, DB_DBT_REALLOC);

	for (jc->j_curslist = curslist;
	    *jc->j_curslist != NULL; jc->j_curslist++)
		;

	/*
	 * The number of cursor slots we allocate is one greater than
	 * the number of cursors involved in the join, because the
	 * list is NULL-terminated.
	 */
	ncurs = (size_t)(jc->j_curslist - curslist);
	nslots = ncurs + 1;

	/*
	 * !!! -- A note on the various lists hanging off jc.
	 *
	 * j_curslist is the initial NULL-terminated list of cursors passed
	 * into __db_join.  The original cursors are not modified; pristine
	 * copies are required because, in databases with unsorted dups, we
	 * must reset all of the secondary cursors after the first each
	 * time the first one is incremented, or else we will lose data
	 * which happen to be sorted differently in two different cursors.
	 *
	 * j_workcurs is where we put those copies that we're planning to
	 * work with.  They're lazily c_dup'ed from j_curslist as we need
	 * them, and closed when the join cursor is closed or when we need
	 * to reset them to their original values (in which case we just
	 * c_dup afresh).
	 *
	 * j_fdupcurs is an array of cursors which point to the first
	 * duplicate in the duplicate set that contains the data value
	 * we're currently interested in.  We need this to make
	 * __db_join_get correctly return duplicate duplicates;  i.e., if a
	 * given data value occurs twice in the set belonging to cursor #2,
	 * and thrice in the set belonging to cursor #3, and once in all
	 * the other cursors, successive calls to __db_join_get need to
	 * return that data item six times.  To make this happen, each time
	 * cursor N is allowed to advance to a new datum, all cursors M
	 * such that M > N have to be reset to the first duplicate with
	 * that datum, so __db_join_get will return all the dup-dups again.
	 * We could just reset them to the original cursor from j_curslist,
	 * but that would be a bit slower in the unsorted case and a LOT
	 * slower in the sorted one.
	 *
	 * j_exhausted is a list of boolean values which represent
	 * whether or not their corresponding cursors are "exhausted",
	 * i.e. whether the datum under the corresponding cursor has
	 * been found not to exist in any unreturned combinations of
	 * later secondary cursors, in which case they are ready to be
	 * incremented.
	 */

	/* We don't want to free regions whose callocs have failed. */
	jc->j_curslist = NULL;
	jc->j_workcurs = NULL;
	jc->j_fdupcurs = NULL;
	jc->j_exhausted = NULL;

	if ((ret = __os_calloc(dbenv, nslots, sizeof(DBC *),
	    &jc->j_curslist)) != 0)
		goto err;
	if ((ret = __os_calloc(dbenv, nslots, sizeof(DBC *),
	    &jc->j_workcurs)) != 0)
		goto err;
	if ((ret = __os_calloc(dbenv, nslots, sizeof(DBC *),
	    &jc->j_fdupcurs)) != 0)
		goto err;
	if ((ret = __os_calloc(dbenv, nslots, sizeof(u_int8_t),
	    &jc->j_exhausted)) != 0)
		goto err;
	for (i = 0; curslist[i] != NULL; i++) {
		jc->j_curslist[i] = curslist[i];
		jc->j_workcurs[i] = NULL;
		jc->j_fdupcurs[i] = NULL;
		jc->j_exhausted[i] = 0;
	}
	jc->j_ncurs = (u_int32_t)ncurs;

	/*
	 * If DB_JOIN_NOSORT is not set, optimize secondary cursors by
	 * sorting in order of increasing cardinality.
	 */
	if (!LF_ISSET(DB_JOIN_NOSORT))
		qsort(jc->j_curslist, ncurs, sizeof(DBC *), __db_join_cmp);

	/*
	 * We never need to reset the 0th cursor, so there's no
	 * solid reason to use workcurs[0] rather than curslist[0] in
	 * join_get.  Nonetheless, it feels cleaner to do it for symmetry,
	 * and this is the most logical place to copy it.
	 *
	 * !!!
	 * There's no need to close the new cursor if we goto err only
	 * because this is the last thing that can fail.  Modifier of this
	 * function beware!
	 */
	if ((ret =
	    __db_c_dup(jc->j_curslist[0], jc->j_workcurs, DB_POSITION)) != 0)
		goto err;

	dbc->c_close = __db_join_close_pp;
	dbc->c_del = __db_join_del;
	dbc->c_get = __db_join_get_pp;
	dbc->c_put = __db_join_put;
	dbc->internal = (DBC_INTERNAL *)jc;
	dbc->dbp = primary;
	jc->j_primary = primary;

	/* Stash the first cursor's transaction here for easy access. */
	dbc->txn = curslist[0]->txn;

	*dbcp = dbc;

	MUTEX_THREAD_LOCK(dbenv, primary->mutexp);
	TAILQ_INSERT_TAIL(&primary->join_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbenv, primary->mutexp);

	return (0);

err:	if (jc != NULL) {
		if (jc->j_curslist != NULL)
			__os_free(dbenv, jc->j_curslist);
		if (jc->j_workcurs != NULL) {
			if (jc->j_workcurs[0] != NULL)
				(void)__db_c_close(jc->j_workcurs[0]);
			__os_free(dbenv, jc->j_workcurs);
		}
		if (jc->j_fdupcurs != NULL)
			__os_free(dbenv, jc->j_fdupcurs);
		if (jc->j_exhausted != NULL)
			__os_free(dbenv, jc->j_exhausted);
		__os_free(dbenv, jc);
	}
	if (dbc != NULL)
		__os_free(dbenv, dbc);
	return (ret);
}

/*
 * __db_join_close_pp --
 *	DBC->c_close pre/post processing for join cursors.
 */
static int
__db_join_close_pp(dbc)
	DBC *dbc;
{
	DB_ENV *dbenv;
	DB *dbp;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 0, 0, dbc->txn != NULL)) != 0)
		return (ret);

	ret = __db_join_close(dbc);

	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

static int
__db_join_put(dbc, key, data, flags)
	DBC *dbc;
	DBT *key;
	DBT *data;
	u_int32_t flags;
{
	PANIC_CHECK(dbc->dbp->dbenv);

	COMPQUIET(key, NULL);
	COMPQUIET(data, NULL);
	COMPQUIET(flags, 0);
	return (EINVAL);
}

static int
__db_join_del(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	PANIC_CHECK(dbc->dbp->dbenv);

	COMPQUIET(flags, 0);
	return (EINVAL);
}

/*
 * __db_join_get_pp --
 *	DBjoin->get pre/post processing.
 */
static int
__db_join_get_pp(dbc, key, data, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	u_int32_t handle_check, save_flags;
	int ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	/* Save the original flags value. */
	save_flags = flags;

	PANIC_CHECK(dbenv);

	if (LF_ISSET(DB_DIRTY_READ | DB_DEGREE_2 | DB_RMW)) {
		if (!LOCKING_ON(dbp->dbenv))
			return (__db_fnl(dbp->dbenv, "DBcursor->c_get"));

		LF_CLR(DB_DIRTY_READ | DB_DEGREE_2 | DB_RMW);
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

	handle_check = IS_REPLICATED(dbp->dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, dbc->txn != NULL)) != 0)
		return (ret);

	/* Restore the original flags value. */
	flags = save_flags;

	ret = __db_join_get(dbc, key, data, flags);

	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

static int
__db_join_get(dbc, key_arg, data_arg, flags)
	DBC *dbc;
	DBT *key_arg, *data_arg;
	u_int32_t flags;
{
	DBT *key_n, key_n_mem;
	DB *dbp;
	DBC *cp;
	JOIN_CURSOR *jc;
	int db_manage_data, ret;
	u_int32_t i, j, operation, opmods;

	dbp = dbc->dbp;
	jc = (JOIN_CURSOR *)dbc->internal;

	operation = LF_ISSET(DB_OPFLAGS_MASK);

	/* !!!
	 * If the set of flags here changes, check that __db_join_primget
	 * is updated to handle them properly.
	 */
	opmods = LF_ISSET(DB_RMW | DB_DEGREE_2 | DB_DIRTY_READ);

	/*
	 * Since we are fetching the key as a datum in the secondary indices,
	 * we must be careful of caller-specified DB_DBT_* memory
	 * management flags.  If necessary, use a stack-allocated DBT;
	 * we'll appropriately copy and/or allocate the data later.
	 */
	if (F_ISSET(key_arg, DB_DBT_USERMEM) ||
	    F_ISSET(key_arg, DB_DBT_MALLOC)) {
		/* We just use the default buffer;  no need to go malloc. */
		key_n = &key_n_mem;
		memset(key_n, 0, sizeof(DBT));
	} else {
		/*
		 * Either DB_DBT_REALLOC or the default buffer will work
		 * fine if we have to reuse it, as we do.
		 */
		key_n = key_arg;
	}

	/*
	 * If our last attempt to do a get on the primary key failed,
	 * short-circuit the join and try again with the same key.
	 */
	if (F_ISSET(jc, JOIN_RETRY))
		goto samekey;
	F_CLR(jc, JOIN_RETRY);

retry:	ret = __db_c_get(jc->j_workcurs[0], &jc->j_key, key_n,
	    opmods | (jc->j_exhausted[0] ? DB_NEXT_DUP : DB_CURRENT));

	if (ret == DB_BUFFER_SMALL) {
		jc->j_key.ulen <<= 1;
		if ((ret = __os_realloc(dbp->dbenv,
		    jc->j_key.ulen, &jc->j_key.data)) != 0)
			goto mem_err;
		goto retry;
	}

	/*
	 * If ret == DB_NOTFOUND, we're out of elements of the first
	 * secondary cursor.  This is how we finally finish the join
	 * if all goes well.
	 */
	if (ret != 0)
		goto err;

	/*
	 * If jc->j_exhausted[0] == 1, we've just advanced the first cursor,
	 * and we're going to want to advance all the cursors that point to
	 * the first member of a duplicate duplicate set (j_fdupcurs[1..N]).
	 * Close all the cursors in j_fdupcurs;  we'll reopen them the
	 * first time through the upcoming loop.
	 */
	for (i = 1; i < jc->j_ncurs; i++) {
		if (jc->j_fdupcurs[i] != NULL &&
		    (ret = __db_c_close(jc->j_fdupcurs[i])) != 0)
			goto err;
		jc->j_fdupcurs[i] = NULL;
	}

	/*
	 * If jc->j_curslist[1] == NULL, we have only one cursor in the join.
	 * Thus, we can safely increment that one cursor on each call
	 * to __db_join_get, and we signal this by setting jc->j_exhausted[0]
	 * right away.
	 *
	 * Otherwise, reset jc->j_exhausted[0] to 0, so that we don't
	 * increment it until we know we're ready to.
	 */
	if (jc->j_curslist[1] == NULL)
		jc->j_exhausted[0] = 1;
	else
		jc->j_exhausted[0] = 0;

	/* We have the first element; now look for it in the other cursors. */
	for (i = 1; i < jc->j_ncurs; i++) {
		DB_ASSERT(jc->j_curslist[i] != NULL);
		if (jc->j_workcurs[i] == NULL)
			/* If this is NULL, we need to dup curslist into it. */
			if ((ret = __db_c_dup(jc->j_curslist[i],
			    &jc->j_workcurs[i], DB_POSITION)) != 0)
				goto err;

retry2:		cp = jc->j_workcurs[i];

		if ((ret = __db_join_getnext(cp, &jc->j_key, key_n,
			    jc->j_exhausted[i], opmods)) == DB_NOTFOUND) {
			/*
			 * jc->j_workcurs[i] has no more of the datum we're
			 * interested in.  Go back one cursor and get
			 * a new dup.  We can't just move to a new
			 * element of the outer relation, because that way
			 * we might miss duplicate duplicates in cursor i-1.
			 *
			 * If this takes us back to the first cursor,
			 * -then- we can move to a new element of the outer
			 * relation.
			 */
			--i;
			jc->j_exhausted[i] = 1;

			if (i == 0) {
				for (j = 1; jc->j_workcurs[j] != NULL; j++) {
					/*
					 * We're moving to a new element of
					 * the first secondary cursor.  If
					 * that cursor is sorted, then any
					 * other sorted cursors can be safely
					 * reset to the first duplicate
					 * duplicate in the current set if we
					 * have a pointer to it (we can't just
					 * leave them be, or we'll miss
					 * duplicate duplicates in the outer
					 * relation).
					 *
					 * If the first cursor is unsorted, or
					 * if cursor j is unsorted, we can
					 * make no assumptions about what
					 * we're looking for next or where it
					 * will be, so we reset to the very
					 * beginning (setting workcurs NULL
					 * will achieve this next go-round).
					 *
					 * XXX: This is likely to break
					 * horribly if any two cursors are
					 * both sorted, but have different
					 * specified sort functions.  For,
					 * now, we dismiss this as pathology
					 * and let strange things happen--we
					 * can't make rope childproof.
					 */
					if ((ret = __db_c_close(
					    jc->j_workcurs[j])) != 0)
						goto err;
					if (!SORTED_SET(jc, 0) ||
					    !SORTED_SET(jc, j) ||
					    jc->j_fdupcurs[j] == NULL)
						/*
						 * Unsafe conditions;
						 * reset fully.
						 */
						jc->j_workcurs[j] = NULL;
					else
						/* Partial reset suffices. */
						if ((__db_c_dup(
						    jc->j_fdupcurs[j],
						    &jc->j_workcurs[j],
						    DB_POSITION)) != 0)
							goto err;
					jc->j_exhausted[j] = 0;
				}
				goto retry;
				/* NOTREACHED */
			}

			/*
			 * We're about to advance the cursor and need to
			 * reset all of the workcurs[j] where j>i, so that
			 * we don't miss any duplicate duplicates.
			 */
			for (j = i + 1;
			    jc->j_workcurs[j] != NULL;
			    j++) {
				if ((ret =
				    __db_c_close(jc->j_workcurs[j])) != 0)
					goto err;
				jc->j_exhausted[j] = 0;
				if (jc->j_fdupcurs[j] == NULL)
					jc->j_workcurs[j] = NULL;
				else if ((ret = __db_c_dup(jc->j_fdupcurs[j],
				    &jc->j_workcurs[j], DB_POSITION)) != 0)
					goto err;
			}
			goto retry2;
			/* NOTREACHED */
		}

		if (ret == DB_BUFFER_SMALL) {
			jc->j_key.ulen <<= 1;
			if ((ret = __os_realloc(dbp->dbenv, jc->j_key.ulen,
			    &jc->j_key.data)) != 0) {
mem_err:			__db_err(dbp->dbenv,
				    "Allocation failed for join key, len = %lu",
				    (u_long)jc->j_key.ulen);
				goto err;
			}
			goto retry2;
		}

		if (ret != 0)
			goto err;

		/*
		 * If we made it this far, we've found a matching
		 * datum in cursor i.  Mark the current cursor
		 * unexhausted, so we don't miss any duplicate
		 * duplicates the next go-round--unless this is the
		 * very last cursor, in which case there are none to
		 * miss, and we'll need that exhausted flag to finally
		 * get a DB_NOTFOUND and move on to the next datum in
		 * the outermost cursor.
		 */
		if (i + 1 != jc->j_ncurs)
			jc->j_exhausted[i] = 0;
		else
			jc->j_exhausted[i] = 1;

		/*
		 * If jc->j_fdupcurs[i] is NULL and the ith cursor's dups are
		 * sorted, then we're here for the first time since advancing
		 * cursor 0, and we have a new datum of interest.
		 * jc->j_workcurs[i] points to the beginning of a set of
		 * duplicate duplicates;  store this into jc->j_fdupcurs[i].
		 */
		if (SORTED_SET(jc, i) && jc->j_fdupcurs[i] == NULL && (ret =
		    __db_c_dup(cp, &jc->j_fdupcurs[i], DB_POSITION)) != 0)
			goto err;
	}

err:	if (ret != 0)
		return (ret);

	if (0) {
samekey:	/*
		 * Get the key we tried and failed to return last time;
		 * it should be the current datum of all the secondary cursors.
		 */
		if ((ret = __db_c_get(jc->j_workcurs[0],
		    &jc->j_key, key_n, DB_CURRENT | opmods)) != 0)
			return (ret);
		F_CLR(jc, JOIN_RETRY);
	}

	/*
	 * ret == 0;  we have a key to return.
	 *
	 * If DB_DBT_USERMEM or DB_DBT_MALLOC is set, we need to copy the key
	 * back into the dbt we were given for the key; call __db_retcopy.
	 * Otherwise, assert that we do not need to copy anything and proceed.
	 */
	DB_ASSERT(F_ISSET(
	    key_arg, DB_DBT_USERMEM | DB_DBT_MALLOC) || key_n == key_arg);

	if (F_ISSET(key_arg, DB_DBT_USERMEM | DB_DBT_MALLOC) &&
	    (ret = __db_retcopy(dbp->dbenv,
	    key_arg, key_n->data, key_n->size, NULL, NULL)) != 0) {
		/*
		 * The retcopy failed, most commonly because we have a user
		 * buffer for the key which is too small. Set things up to
		 * retry next time, and return.
		 */
		F_SET(jc, JOIN_RETRY);
		return (ret);
	}

	/*
	 * If DB_JOIN_ITEM is set, we return it; otherwise we do the lookup
	 * in the primary and then return.
	 *
	 * Note that we use key_arg here;  it is safe (and appropriate)
	 * to do so.
	 */
	if (operation == DB_JOIN_ITEM)
		return (0);

	/*
	 * If data_arg->flags == 0--that is, if DB is managing the
	 * data DBT's memory--it's not safe to just pass the DBT
	 * through to the primary get call, since we don't want that
	 * memory to belong to the primary DB handle (and if the primary
	 * is free-threaded, it can't anyway).
	 *
	 * Instead, use memory that is managed by the join cursor, in
	 * jc->j_rdata.
	 */
	if (!F_ISSET(data_arg, DB_DBT_MALLOC | DB_DBT_REALLOC | DB_DBT_USERMEM))
		db_manage_data = 1;
	else
		db_manage_data = 0;
	if ((ret = __db_join_primget(jc->j_primary,
	    jc->j_curslist[0]->txn, jc->j_curslist[0]->locker, key_arg,
	    db_manage_data ? &jc->j_rdata : data_arg, opmods)) != 0) {
		if (ret == DB_NOTFOUND)
			/*
			 * If ret == DB_NOTFOUND, the primary and secondary
			 * are out of sync;  every item in each secondary
			 * should correspond to something in the primary,
			 * or we shouldn't have done the join this way.
			 * Wail.
			 */
			ret = __db_secondary_corrupt(jc->j_primary);
		else
			/*
			 * The get on the primary failed for some other
			 * reason, most commonly because we're using a user
			 * buffer that's not big enough.  Flag our failure
			 * so we can return the same key next time.
			 */
			F_SET(jc, JOIN_RETRY);
	}
	if (db_manage_data && ret == 0) {
		data_arg->data = jc->j_rdata.data;
		data_arg->size = jc->j_rdata.size;
	}

	return (ret);
}

/*
 * __db_join_close --
 *	DBC->c_close for join cursors.
 *
 * PUBLIC: int __db_join_close __P((DBC *));
 */
int
__db_join_close(dbc)
	DBC *dbc;
{
	DB *dbp;
	DB_ENV *dbenv;
	JOIN_CURSOR *jc;
	int ret, t_ret;
	u_int32_t i;

	jc = (JOIN_CURSOR *)dbc->internal;
	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	ret = t_ret = 0;

	/*
	 * Remove from active list of join cursors.  Note that this
	 * must happen before any action that can fail and return, or else
	 * __db_close may loop indefinitely.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	TAILQ_REMOVE(&dbp->join_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	PANIC_CHECK(dbenv);

	/*
	 * Close any open scratch cursors.  In each case, there may
	 * not be as many outstanding as there are cursors in
	 * curslist, but we want to close whatever's there.
	 *
	 * If any close fails, there's no reason not to close everything else;
	 * we'll just return the error code of the last one to fail.  There's
	 * not much the caller can do anyway, since these cursors only exist
	 * hanging off a db-internal data structure that they shouldn't be
	 * mucking with.
	 */
	for (i = 0; i < jc->j_ncurs; i++) {
		if (jc->j_workcurs[i] != NULL &&
		    (t_ret = __db_c_close(jc->j_workcurs[i])) != 0)
			ret = t_ret;
		if (jc->j_fdupcurs[i] != NULL &&
		    (t_ret = __db_c_close(jc->j_fdupcurs[i])) != 0)
			ret = t_ret;
	}

	__os_free(dbenv, jc->j_exhausted);
	__os_free(dbenv, jc->j_curslist);
	__os_free(dbenv, jc->j_workcurs);
	__os_free(dbenv, jc->j_fdupcurs);
	__os_free(dbenv, jc->j_key.data);
	if (jc->j_rdata.data != NULL)
		__os_ufree(dbenv, jc->j_rdata.data);
	__os_free(dbenv, jc);
	__os_free(dbenv, dbc);

	return (ret);
}

/*
 * __db_join_getnext --
 *	This function replaces the DBC_CONTINUE and DBC_KEYSET
 *	functionality inside the various cursor get routines.
 *
 *	If exhausted == 0, we're not done with the current datum;
 *	return it if it matches "matching", otherwise search
 *	using DB_GET_BOTHC (which is faster than iteratively doing
 *	DB_NEXT_DUP) forward until we find one that does.
 *
 *	If exhausted == 1, we are done with the current datum, so just
 *	leap forward to searching NEXT_DUPs.
 *
 *	If no matching datum exists, returns DB_NOTFOUND, else 0.
 */
static int
__db_join_getnext(dbc, key, data, exhausted, opmods)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t exhausted, opmods;
{
	int ret, cmp;
	DB *dbp;
	DBT ldata;
	int (*func) __P((DB *, const DBT *, const DBT *));

	dbp = dbc->dbp;
	func = (dbp->dup_compare == NULL) ? __bam_defcmp : dbp->dup_compare;

	switch (exhausted) {
	case 0:
		/*
		 * We don't want to step on data->data;  use a new
		 * DBT and malloc so we don't step on dbc's rdata memory.
		 */
		memset(&ldata, 0, sizeof(DBT));
		F_SET(&ldata, DB_DBT_MALLOC);
		if ((ret = __db_c_get(dbc,
		    key, &ldata, opmods | DB_CURRENT)) != 0)
			break;
		cmp = func(dbp, data, &ldata);
		if (cmp == 0) {
			/*
			 * We have to return the real data value.  Copy
			 * it into data, then free the buffer we malloc'ed
			 * above.
			 */
			if ((ret = __db_retcopy(dbp->dbenv, data, ldata.data,
			    ldata.size, &data->data, &data->size)) != 0)
				return (ret);
			__os_ufree(dbp->dbenv, ldata.data);
			return (0);
		}

		/*
		 * Didn't match--we want to fall through and search future
		 * dups.  We just forget about ldata and free
		 * its buffer--data contains the value we're searching for.
		 */
		__os_ufree(dbp->dbenv, ldata.data);
		/* FALLTHROUGH */
	case 1:
		ret = __db_c_get(dbc, key, data, opmods | DB_GET_BOTHC);
		break;
	default:
		ret = EINVAL;
		break;
	}

	return (ret);
}

/*
 * __db_join_cmp --
 *	Comparison function for sorting DBCs in cardinality order.
 */
static int
__db_join_cmp(a, b)
	const void *a, *b;
{
	DBC *dbca, *dbcb;
	db_recno_t counta, countb;

	dbca = *((DBC * const *)a);
	dbcb = *((DBC * const *)b);

	if (__db_c_count(dbca, &counta) != 0 ||
	    __db_c_count(dbcb, &countb) != 0)
		return (0);

	return ((long)counta - (long)countb);
}

/*
 * __db_join_primget --
 *	Perform a DB->get in the primary, being careful not to use a new
 * locker ID if we're doing CDB locking.
 */
static int
__db_join_primget(dbp, txn, lockerid, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t lockerid;
	DBT *key, *data;
	u_int32_t flags;
{
	DBC *dbc;
	int ret, rmw, t_ret;

	if ((ret = __db_cursor_int(dbp,
	    txn, dbp->type, PGNO_INVALID, 0, lockerid, &dbc)) != 0)
		return (ret);

	/*
	 * The only allowable flags here are the two flags copied into
	 * "opmods" in __db_join_get, DB_RMW and DB_DIRTY_READ.  The former
	 * is an op on the c_get call, the latter on the cursor call.
	 * It's a DB bug if we allow any other flags down in here.
	 */
	rmw = LF_ISSET(DB_RMW);
	if (LF_ISSET(DB_DIRTY_READ) ||
	    (txn != NULL && F_ISSET(txn, TXN_DIRTY_READ)))
		F_SET(dbc, DBC_DIRTY_READ);

	if (LF_ISSET(DB_DEGREE_2) ||
	    (txn != NULL && F_ISSET(txn, TXN_DEGREE_2)))
		F_SET(dbc, DBC_DEGREE_2);

	LF_CLR(DB_RMW | DB_DIRTY_READ | DB_DEGREE_2);
	DB_ASSERT(flags == 0);

	F_SET(dbc, DBC_TRANSIENT);

	/*
	 * This shouldn't be necessary, thanks to the fact that join cursors
	 * swap in their own DB_DBT_REALLOC'ed buffers, but just for form's
	 * sake, we mirror what __db_get does.
	 */
	SET_RET_MEM(dbc, dbp);

	ret = __db_c_get(dbc, key, data, DB_SET | rmw);

	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_secondary_corrupt --
 *	Report that a secondary index appears corrupt, as it has a record
 * that does not correspond to a record in the primary or vice versa.
 *
 * PUBLIC: int __db_secondary_corrupt __P((DB *));
 */
int
__db_secondary_corrupt(dbp)
	DB *dbp;
{
	__db_err(dbp->dbenv,
	    "Secondary index corrupt: not consistent with primary");
	return (DB_SECONDARY_BAD);
}
