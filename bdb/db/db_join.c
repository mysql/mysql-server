/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_join.c,v 11.31 2000/12/20 22:41:54 krinsky Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_join.h"
#include "db_am.h"
#include "btree.h"

static int __db_join_close __P((DBC *));
static int __db_join_cmp __P((const void *, const void *));
static int __db_join_del __P((DBC *, u_int32_t));
static int __db_join_get __P((DBC *, DBT *, DBT *, u_int32_t));
static int __db_join_getnext __P((DBC *, DBT *, DBT *, u_int32_t));
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
 * The curslist is an array of existing, intialized cursors and primary
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
	int ret;
	u_int32_t i, ncurs, nslots;

	COMPQUIET(nslots, 0);

	PANIC_CHECK(primary->dbenv);

	if ((ret = __db_joinchk(primary, curslist, flags)) != 0)
		return (ret);

	dbc = NULL;
	jc = NULL;
	dbenv = primary->dbenv;

	if ((ret = __os_calloc(dbenv, 1, sizeof(DBC), &dbc)) != 0)
		goto err;

	if ((ret = __os_calloc(dbenv,
	    1, sizeof(JOIN_CURSOR), &jc)) != 0)
		goto err;

	if ((ret = __os_malloc(dbenv, 256, NULL, &jc->j_key.data)) != 0)
		goto err;
	jc->j_key.ulen = 256;
	F_SET(&jc->j_key, DB_DBT_USERMEM);

	for (jc->j_curslist = curslist;
	    *jc->j_curslist != NULL; jc->j_curslist++)
		;

	/*
	 * The number of cursor slots we allocate is one greater than
	 * the number of cursors involved in the join, because the
	 * list is NULL-terminated.
	 */
	ncurs = jc->j_curslist - curslist;
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
	jc->j_ncurs = ncurs;

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
	if ((ret = jc->j_curslist[0]->c_dup(jc->j_curslist[0], jc->j_workcurs,
	    DB_POSITIONI)) != 0)
		goto err;

	dbc->c_close = __db_join_close;
	dbc->c_del = __db_join_del;
	dbc->c_get = __db_join_get;
	dbc->c_put = __db_join_put;
	dbc->internal = (DBC_INTERNAL *) jc;
	dbc->dbp = primary;
	jc->j_primary = primary;

	*dbcp = dbc;

	MUTEX_THREAD_LOCK(dbenv, primary->mutexp);
	TAILQ_INSERT_TAIL(&primary->join_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbenv, primary->mutexp);

	return (0);

err:	if (jc != NULL) {
		if (jc->j_curslist != NULL)
			__os_free(jc->j_curslist, nslots * sizeof(DBC *));
		if (jc->j_workcurs != NULL) {
			if (jc->j_workcurs[0] != NULL)
				__os_free(jc->j_workcurs[0], sizeof(DBC));
			__os_free(jc->j_workcurs, nslots * sizeof(DBC *));
		}
		if (jc->j_fdupcurs != NULL)
			__os_free(jc->j_fdupcurs, nslots * sizeof(DBC *));
		if (jc->j_exhausted != NULL)
			__os_free(jc->j_exhausted, nslots * sizeof(u_int8_t));
		__os_free(jc, sizeof(JOIN_CURSOR));
	}
	if (dbc != NULL)
		__os_free(dbc, sizeof(DBC));
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
	int ret;
	u_int32_t i, j, operation;

	dbp = dbc->dbp;
	jc = (JOIN_CURSOR *)dbc->internal;

	PANIC_CHECK(dbp->dbenv);

	operation = LF_ISSET(DB_OPFLAGS_MASK);

	if ((ret = __db_joingetchk(dbp, key_arg, flags)) != 0)
		return (ret);

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

retry:	ret = jc->j_workcurs[0]->c_get(jc->j_workcurs[0],
	    &jc->j_key, key_n, jc->j_exhausted[0] ? DB_NEXT_DUP : DB_CURRENT);

	if (ret == ENOMEM) {
		jc->j_key.ulen <<= 1;
		if ((ret = __os_realloc(dbp->dbenv,
		    jc->j_key.ulen, NULL, &jc->j_key.data)) != 0)
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
		    (ret = jc->j_fdupcurs[i]->c_close(jc->j_fdupcurs[i])) != 0)
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
			if ((ret = jc->j_curslist[i]->c_dup(
			    jc->j_curslist[i], jc->j_workcurs + i,
			    DB_POSITIONI)) != 0)
				goto err;

retry2:		cp = jc->j_workcurs[i];

		if ((ret = __db_join_getnext(cp, &jc->j_key, key_n,
			    jc->j_exhausted[i])) == DB_NOTFOUND) {
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
					if ((ret = jc->j_workcurs[j]->c_close(
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
						if ((jc->j_fdupcurs[j]->c_dup(
						    jc->j_fdupcurs[j],
						    &jc->j_workcurs[j],
						    DB_POSITIONI)) != 0)
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
				if ((ret = jc->j_workcurs[j]->c_close(
				    jc->j_workcurs[j])) != 0)
					goto err;
				jc->j_exhausted[j] = 0;
				if (jc->j_fdupcurs[j] != NULL &&
				    (ret = jc->j_fdupcurs[j]->c_dup(
				    jc->j_fdupcurs[j], &jc->j_workcurs[j],
				    DB_POSITIONI)) != 0)
					goto err;
				else
					jc->j_workcurs[j] = NULL;
			}
			goto retry2;
			/* NOTREACHED */
		}

		if (ret == ENOMEM) {
			jc->j_key.ulen <<= 1;
			if ((ret = __os_realloc(dbp->dbenv, jc->j_key.ulen,
			    NULL, &jc->j_key.data)) != 0) {
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
		    cp->c_dup(cp, &jc->j_fdupcurs[i], DB_POSITIONI)) != 0)
			goto err;

	}

err:	if (ret != 0)
		return (ret);

	if (0) {
samekey:	/*
		 * Get the key we tried and failed to return last time;
		 * it should be the current datum of all the secondary cursors.
		 */
		if ((ret = jc->j_workcurs[0]->c_get(jc->j_workcurs[0],
		    &jc->j_key, key_n, DB_CURRENT)) != 0)
			return (ret);
		F_CLR(jc, JOIN_RETRY);
	}

	/*
	 * ret == 0;  we have a key to return.
	 *
	 * If DB_DBT_USERMEM or DB_DBT_MALLOC is set, we need to
	 * copy it back into the dbt we were given for the key;
	 * call __db_retcopy.
	 *
	 * Otherwise, assert that we do not in fact need to copy anything
	 * and simply proceed.
	 */
	if (F_ISSET(key_arg, DB_DBT_USERMEM) ||
	    F_ISSET(key_arg, DB_DBT_MALLOC)) {
		/*
		 * We need to copy the key back into our original
		 * datum.  Do so.
		 */
		if ((ret = __db_retcopy(dbp,
		    key_arg, key_n->data, key_n->size, NULL, NULL)) != 0) {
			/*
			 * The retcopy failed, most commonly because we
			 * have a user buffer for the key which is too small.
			 * Set things up to retry next time, and return.
			 */
			F_SET(jc, JOIN_RETRY);
			return (ret);
		}
	} else
		DB_ASSERT(key_n == key_arg);

	/*
	 * If DB_JOIN_ITEM is
	 * set, we return it;  otherwise we do the lookup in the
	 * primary and then return.
	 *
	 * Note that we use key_arg here;  it is safe (and appropriate)
	 * to do so.
	 */
	if (operation == DB_JOIN_ITEM)
		return (0);

	if ((ret = jc->j_primary->get(jc->j_primary,
	    jc->j_curslist[0]->txn, key_arg, data_arg, 0)) != 0)
		/*
		 * The get on the primary failed, most commonly because we're
		 * using a user buffer that's not big enough.  Flag our
		 * failure so we can return the same key next time.
		 */
		F_SET(jc, JOIN_RETRY);

	return (ret);
}

static int
__db_join_close(dbc)
	DBC *dbc;
{
	DB *dbp;
	JOIN_CURSOR *jc;
	int ret, t_ret;
	u_int32_t i;

	jc = (JOIN_CURSOR *)dbc->internal;
	dbp = dbc->dbp;
	ret = t_ret = 0;

	/*
	 * Remove from active list of join cursors.  Note that this
	 * must happen before any action that can fail and return, or else
	 * __db_close may loop indefinitely.
	 */
	MUTEX_THREAD_LOCK(dbp->dbenv, dbp->mutexp);
	TAILQ_REMOVE(&dbp->join_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);

	PANIC_CHECK(dbc->dbp->dbenv);

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
		if (jc->j_workcurs[i] != NULL && (t_ret =
		    jc->j_workcurs[i]->c_close(jc->j_workcurs[i])) != 0)
			ret = t_ret;
		if (jc->j_fdupcurs[i] != NULL && (t_ret =
		    jc->j_fdupcurs[i]->c_close(jc->j_fdupcurs[i])) != 0)
			ret = t_ret;
	}

	__os_free(jc->j_exhausted, 0);
	__os_free(jc->j_curslist, 0);
	__os_free(jc->j_workcurs, 0);
	__os_free(jc->j_fdupcurs, 0);
	__os_free(jc->j_key.data, jc->j_key.ulen);
	__os_free(jc, sizeof(JOIN_CURSOR));
	__os_free(dbc, sizeof(DBC));

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
__db_join_getnext(dbc, key, data, exhausted)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t exhausted;
{
	int ret, cmp;
	DB *dbp;
	DBT ldata;
	int (*func) __P((DB *, const DBT *, const DBT *));

	dbp = dbc->dbp;
	func = (dbp->dup_compare == NULL) ? __bam_defcmp : dbp->dup_compare;

	switch (exhausted) {
	case 0:
		memset(&ldata, 0, sizeof(DBT));
		/* We don't want to step on data->data;  malloc. */
		F_SET(&ldata, DB_DBT_MALLOC);
		if ((ret = dbc->c_get(dbc, key, &ldata, DB_CURRENT)) != 0)
			break;
		cmp = func(dbp, data, &ldata);
		if (cmp == 0) {
			/*
			 * We have to return the real data value.  Copy
			 * it into data, then free the buffer we malloc'ed
			 * above.
			 */
			if ((ret = __db_retcopy(dbp, data, ldata.data,
			    ldata.size, &data->data, &data->size)) != 0)
				return (ret);
			__os_free(ldata.data, 0);
			return (0);
		}

		/*
		 * Didn't match--we want to fall through and search future
		 * dups.  We just forget about ldata and free
		 * its buffer--data contains the value we're searching for.
		 */
		__os_free(ldata.data, 0);
		/* FALLTHROUGH */
	case 1:
		ret = dbc->c_get(dbc, key, data, DB_GET_BOTHC);
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

	/* In case c_count fails, pretend cursors are equal. */
	counta = countb = 0;

	dbca = *((DBC * const *)a);
	dbcb = *((DBC * const *)b);

	if (dbca->c_count(dbca, &counta, 0) != 0 ||
	    dbcb->c_count(dbcb, &countb, 0) != 0)
		return (0);

	return (counta - countb);
}
