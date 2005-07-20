/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_am.c,v 11.120 2004/10/07 17:33:32 sue Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/qam.h"

static int __db_append_primary __P((DBC *, DBT *, DBT *));
static int __db_secondary_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));

/*
 * __db_cursor_int --
 *	Internal routine to create a cursor.
 *
 * PUBLIC: int __db_cursor_int
 * PUBLIC:     __P((DB *, DB_TXN *, DBTYPE, db_pgno_t, int, u_int32_t, DBC **));
 */
int
__db_cursor_int(dbp, txn, dbtype, root, is_opd, lockerid, dbcp)
	DB *dbp;
	DB_TXN *txn;
	DBTYPE dbtype;
	db_pgno_t root;
	int is_opd;
	u_int32_t lockerid;
	DBC **dbcp;
{
	DBC *dbc;
	DBC_INTERNAL *cp;
	DB_ENV *dbenv;
	int allocated, ret;

	dbenv = dbp->dbenv;
	allocated = 0;

	/*
	 * If dbcp is non-NULL it is assumed to point to an area to initialize
	 * as a cursor.
	 *
	 * Take one from the free list if it's available.  Take only the
	 * right type.  With off page dups we may have different kinds
	 * of cursors on the queue for a single database.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	for (dbc = TAILQ_FIRST(&dbp->free_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links))
		if (dbtype == dbc->dbtype) {
			TAILQ_REMOVE(&dbp->free_queue, dbc, links);
			F_CLR(dbc, ~DBC_OWN_LID);
			break;
		}
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	if (dbc == NULL) {
		if ((ret = __os_calloc(dbenv, 1, sizeof(DBC), &dbc)) != 0)
			return (ret);
		allocated = 1;
		dbc->flags = 0;

		dbc->dbp = dbp;

		/* Set up locking information. */
		if (LOCKING_ON(dbenv)) {
			/*
			 * If we are not threaded, we share a locker ID among
			 * all cursors opened in the environment handle,
			 * allocating one if this is the first cursor.
			 *
			 * This relies on the fact that non-threaded DB handles
			 * always have non-threaded environment handles, since
			 * we set DB_THREAD on DB handles created with threaded
			 * environment handles.
			 */
			if (!DB_IS_THREADED(dbp)) {
				if (dbp->dbenv->env_lid == DB_LOCK_INVALIDID &&
				    (ret =
				    __lock_id(dbenv,&dbp->dbenv->env_lid)) != 0)
					goto err;
				dbc->lid = dbp->dbenv->env_lid;
			} else {
				if ((ret = __lock_id(dbenv, &dbc->lid)) != 0)
					goto err;
				F_SET(dbc, DBC_OWN_LID);
			}

			/*
			 * In CDB, secondary indices should share a lock file
			 * ID with the primary;  otherwise we're susceptible
			 * to deadlocks.  We also use __db_cursor_int rather
			 * than __db_cursor to create secondary update cursors
			 * in c_put and c_del; these won't acquire a new lock.
			 *
			 * !!!
			 * Since this is in the one-time cursor allocation
			 * code, we need to be sure to destroy, not just
			 * close, all cursors in the secondary when we
			 * associate.
			 */
			if (CDB_LOCKING(dbenv) &&
			    F_ISSET(dbp, DB_AM_SECONDARY))
				memcpy(dbc->lock.fileid,
				    dbp->s_primary->fileid, DB_FILE_ID_LEN);
			else
				memcpy(dbc->lock.fileid,
				    dbp->fileid, DB_FILE_ID_LEN);

			if (CDB_LOCKING(dbenv)) {
				if (F_ISSET(dbenv, DB_ENV_CDB_ALLDB)) {
					/*
					 * If we are doing a single lock per
					 * environment, set up the global
					 * lock object just like we do to
					 * single thread creates.
					 */
					DB_ASSERT(sizeof(db_pgno_t) ==
					    sizeof(u_int32_t));
					dbc->lock_dbt.size = sizeof(u_int32_t);
					dbc->lock_dbt.data = &dbc->lock.pgno;
					dbc->lock.pgno = 0;
				} else {
					dbc->lock_dbt.size = DB_FILE_ID_LEN;
					dbc->lock_dbt.data = dbc->lock.fileid;
				}
			} else {
				dbc->lock.type = DB_PAGE_LOCK;
				dbc->lock_dbt.size = sizeof(dbc->lock);
				dbc->lock_dbt.data = &dbc->lock;
			}
		}
		/* Init the DBC internal structure. */
		switch (dbtype) {
		case DB_BTREE:
		case DB_RECNO:
			if ((ret = __bam_c_init(dbc, dbtype)) != 0)
				goto err;
			break;
		case DB_HASH:
			if ((ret = __ham_c_init(dbc)) != 0)
				goto err;
			break;
		case DB_QUEUE:
			if ((ret = __qam_c_init(dbc)) != 0)
				goto err;
			break;
		case DB_UNKNOWN:
		default:
			ret = __db_unknown_type(dbenv, "DB->cursor", dbtype);
			goto err;
		}

		cp = dbc->internal;
	}

	/* Refresh the DBC structure. */
	dbc->dbtype = dbtype;
	RESET_RET_MEM(dbc);

	if ((dbc->txn = txn) == NULL) {
		/*
		 * There are certain cases in which we want to create a
		 * new cursor with a particular locker ID that is known
		 * to be the same as (and thus not conflict with) an
		 * open cursor.
		 *
		 * The most obvious case is cursor duplication;  when we
		 * call DBC->c_dup or __db_c_idup, we want to use the original
		 * cursor's locker ID.
		 *
		 * Another case is when updating secondary indices.  Standard
		 * CDB locking would mean that we might block ourself:  we need
		 * to open an update cursor in the secondary while an update
		 * cursor in the primary is open, and when the secondary and
		 * primary are subdatabases or we're using env-wide locking,
		 * this is disastrous.
		 *
		 * In these cases, our caller will pass a nonzero locker ID
		 * into this function.  Use this locker ID instead of dbc->lid
		 * as the locker ID for our new cursor.
		 */
		if (lockerid != DB_LOCK_INVALIDID)
			dbc->locker = lockerid;
		else
			dbc->locker = dbc->lid;
	} else
		dbc->locker = txn->txnid;

	/*
	 * These fields change when we are used as a secondary index, so
	 * if the DB is a secondary, make sure they're set properly just
	 * in case we opened some cursors before we were associated.
	 *
	 * __db_c_get is used by all access methods, so this should be safe.
	 */
	if (F_ISSET(dbp, DB_AM_SECONDARY))
		dbc->c_get = __db_c_secondary_get_pp;

	if (is_opd)
		F_SET(dbc, DBC_OPD);
	if (F_ISSET(dbp, DB_AM_RECOVER))
		F_SET(dbc, DBC_RECOVER);
	if (F_ISSET(dbp, DB_AM_COMPENSATE))
		F_SET(dbc, DBC_COMPENSATE);

	/* Refresh the DBC internal structure. */
	cp = dbc->internal;
	cp->opd = NULL;

	cp->indx = 0;
	cp->page = NULL;
	cp->pgno = PGNO_INVALID;
	cp->root = root;

	switch (dbtype) {
	case DB_BTREE:
	case DB_RECNO:
		if ((ret = __bam_c_refresh(dbc)) != 0)
			goto err;
		break;
	case DB_HASH:
	case DB_QUEUE:
		break;
	case DB_UNKNOWN:
	default:
		ret = __db_unknown_type(dbenv, "DB->cursor", dbp->type);
		goto err;
	}

	/*
	 * The transaction keeps track of how many cursors were opened within
	 * it to catch application errors where the cursor isn't closed when
	 * the transaction is resolved.
	 */
	if (txn != NULL)
		++txn->cursors;

	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	TAILQ_INSERT_TAIL(&dbp->active_queue, dbc, links);
	F_SET(dbc, DBC_ACTIVE);
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	*dbcp = dbc;
	return (0);

err:	if (allocated)
		__os_free(dbenv, dbc);
	return (ret);
}

/*
 * __db_put --
 *	Store a key/data pair.
 *
 * PUBLIC: int __db_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__db_put(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DBC *dbc;
	DBT tdata;
	DB_ENV *dbenv;
	int ret, t_ret;

	dbenv = dbp->dbenv;

	if ((ret = __db_cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, txn, "DB->put", key, data, flags);

	SET_RET_MEM(dbc, dbp);

	/*
	 * See the comment in __db_get().
	 *
	 * Note that the c_get in the DB_NOOVERWRITE case is safe to
	 * do with this flag set;  if it errors in any way other than
	 * DB_NOTFOUND, we're going to close the cursor without doing
	 * anything else, and if it returns DB_NOTFOUND then it's safe
	 * to do a c_put(DB_KEYLAST) even if an access method moved the
	 * cursor, since that's not position-dependent.
	 */
	F_SET(dbc, DBC_TRANSIENT);

	switch (flags) {
	case DB_APPEND:
		/*
		 * If there is an append callback, the value stored in
		 * data->data may be replaced and then freed.  To avoid
		 * passing a freed pointer back to the user, just operate
		 * on a copy of the data DBT.
		 */
		tdata = *data;

		/*
		 * Append isn't a normal put operation;  call the appropriate
		 * access method's append function.
		 */
		switch (dbp->type) {
		case DB_QUEUE:
			if ((ret = __qam_append(dbc, key, &tdata)) != 0)
				goto err;
			break;
		case DB_RECNO:
			if ((ret = __ram_append(dbc, key, &tdata)) != 0)
				goto err;
			break;
		case DB_BTREE:
		case DB_HASH:
		case DB_UNKNOWN:
		default:
			/* The interface should prevent this. */
			DB_ASSERT(
			    dbp->type == DB_QUEUE || dbp->type == DB_RECNO);

			ret = __db_ferr(dbenv, "DB->put", 0);
			goto err;
		}

		/*
		 * Secondary indices:  since we've returned zero from
		 * an append function, we've just put a record, and done
		 * so outside __db_c_put.  We know we're not a secondary--
		 * the interface prevents puts on them--but we may be a
		 * primary.  If so, update our secondary indices
		 * appropriately.
		 */
		DB_ASSERT(!F_ISSET(dbp, DB_AM_SECONDARY));

		if (LIST_FIRST(&dbp->s_secondaries) != NULL)
			ret = __db_append_primary(dbc, key, &tdata);

		/*
		 * The append callback, if one exists, may have allocated
		 * a new tdata.data buffer.  If so, free it.
		 */
		FREE_IF_NEEDED(dbp, &tdata);

		/* No need for a cursor put;  we're done. */
		goto done;
	case DB_NOOVERWRITE:
		flags = 0;
		/*
		 * Set DB_DBT_USERMEM, this might be a threaded application and
		 * the flags checking will catch us.  We don't want the actual
		 * data, so request a partial of length 0.
		 */
		memset(&tdata, 0, sizeof(tdata));
		F_SET(&tdata, DB_DBT_USERMEM | DB_DBT_PARTIAL);

		/*
		 * If we're doing page-level locking, set the read-modify-write
		 * flag, we're going to overwrite immediately.
		 */
		if ((ret = __db_c_get(dbc, key, &tdata,
		    DB_SET | (STD_LOCKING(dbc) ? DB_RMW : 0))) == 0)
			ret = DB_KEYEXIST;
		else if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
			ret = 0;
		break;
	default:
		/* Fall through to normal cursor put. */
		break;
	}

	if (ret == 0)
		ret = __db_c_put(dbc,
		    key, data, flags == 0 ? DB_KEYLAST : flags);

err:
done:	/* Close the cursor. */
	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_del --
 *	Delete the items referenced by a key.
 *
 * PUBLIC: int __db_del __P((DB *, DB_TXN *, DBT *, u_int32_t));
 */
int
__db_del(dbp, txn, key, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	DBC *dbc;
	DBT data, lkey;
	u_int32_t f_init, f_next;
	int ret, t_ret;

	/* Allocate a cursor. */
	if ((ret = __db_cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		goto err;

	DEBUG_LWRITE(dbc, txn, "DB->del", key, NULL, flags);
	COMPQUIET(flags, 0);

	/*
	 * Walk a cursor through the key/data pairs, deleting as we go.  Set
	 * the DB_DBT_USERMEM flag, as this might be a threaded application
	 * and the flags checking will catch us.  We don't actually want the
	 * keys or data, so request a partial of length 0.
	 */
	memset(&lkey, 0, sizeof(lkey));
	F_SET(&lkey, DB_DBT_USERMEM | DB_DBT_PARTIAL);
	memset(&data, 0, sizeof(data));
	F_SET(&data, DB_DBT_USERMEM | DB_DBT_PARTIAL);

	/*
	 * If locking (and we haven't already acquired CDB locks), set the
	 * read-modify-write flag.
	 */
	f_init = DB_SET;
	f_next = DB_NEXT_DUP;
	if (STD_LOCKING(dbc)) {
		f_init |= DB_RMW;
		f_next |= DB_RMW;
	}

	/* Walk through the set of key/data pairs, deleting as we go. */
	if ((ret = __db_c_get(dbc, key, &data, f_init)) != 0)
		goto err;

	/*
	 * Hash permits an optimization in DB->del:  since on-page
	 * duplicates are stored in a single HKEYDATA structure, it's
	 * possible to delete an entire set of them at once, and as
	 * the HKEYDATA has to be rebuilt and re-put each time it
	 * changes, this is much faster than deleting the duplicates
	 * one by one.  Thus, if we're not pointing at an off-page
	 * duplicate set, and we're not using secondary indices (in
	 * which case we'd have to examine the items one by one anyway),
	 * let hash do this "quick delete".
	 *
	 * !!!
	 * Note that this is the only application-executed delete call in
	 * Berkeley DB that does not go through the __db_c_del function.
	 * If anything other than the delete itself (like a secondary index
	 * update) has to happen there in a particular situation, the
	 * conditions here should be modified not to call __ham_quick_delete.
	 * The ordinary AM-independent alternative will work just fine with
	 * a hash;  it'll just be slower.
	 */
	if (dbp->type == DB_HASH)
		if (LIST_FIRST(&dbp->s_secondaries) == NULL &&
		    !F_ISSET(dbp, DB_AM_SECONDARY) &&
		    dbc->internal->opd == NULL) {
			ret = __ham_quick_delete(dbc);
			goto done;
		}

	for (;;) {
		if ((ret = __db_c_del(dbc, 0)) != 0)
			break;
		if ((ret = __db_c_get(dbc, &lkey, &data, f_next)) != 0) {
			if (ret == DB_NOTFOUND)
				ret = 0;
			break;
		}
	}

done:
err:	/* Discard the cursor. */
	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_sync --
 *	Flush the database cache.
 *
 * PUBLIC: int __db_sync __P((DB *));
 */
int
__db_sync(dbp)
	DB *dbp;
{
	int ret, t_ret;

	ret = 0;

	/* If the database was read-only, we're done. */
	if (F_ISSET(dbp, DB_AM_RDONLY))
		return (0);

	/* If it's a Recno tree, write the backing source text file. */
	if (dbp->type == DB_RECNO)
		ret = __ram_writeback(dbp);

	/* If the database was never backed by a database file, we're done. */
	if (F_ISSET(dbp, DB_AM_INMEM))
		return (ret);

	if (dbp->type == DB_QUEUE)
		ret = __qam_sync(dbp);
	else
		/* Flush any dirty pages from the cache to the backing file. */
		if ((t_ret = __memp_fsync(dbp->mpf)) != 0 && ret == 0)
			ret = t_ret;

	return (ret);
}

/*
 * __db_associate --
 *	Associate another database as a secondary index to this one.
 *
 * PUBLIC: int __db_associate __P((DB *, DB_TXN *, DB *,
 * PUBLIC:     int (*)(DB *, const DBT *, const DBT *, DBT *), u_int32_t));
 */
int
__db_associate(dbp, txn, sdbp, callback, flags)
	DB *dbp, *sdbp;
	DB_TXN *txn;
	int (*callback) __P((DB *, const DBT *, const DBT *, DBT *));
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *pdbc, *sdbc;
	DBT skey, key, data;
	int build, ret, t_ret;

	dbenv = dbp->dbenv;
	pdbc = sdbc = NULL;
	ret = 0;

	sdbp->s_callback = callback;
	sdbp->s_primary = dbp;

	sdbp->stored_get = sdbp->get;
	sdbp->get = __db_secondary_get;

	sdbp->stored_close = sdbp->close;
	sdbp->close = __db_secondary_close_pp;

	F_SET(sdbp, DB_AM_SECONDARY);

	/*
	 * Check to see if the secondary is empty--and thus if we should
	 * build it--before we link it in and risk making it show up in
	 * other threads.
	 */
	build = 0;
	if (LF_ISSET(DB_CREATE)) {
		if ((ret = __db_cursor(sdbp, txn, &sdbc, 0)) != 0)
			goto err;

		/*
		 * We don't care about key or data;  we're just doing
		 * an existence check.
		 */
		memset(&key, 0, sizeof(DBT));
		memset(&data, 0, sizeof(DBT));
		F_SET(&key, DB_DBT_PARTIAL | DB_DBT_USERMEM);
		F_SET(&data, DB_DBT_PARTIAL | DB_DBT_USERMEM);
		if ((ret = __db_c_get(sdbc, &key, &data,
		    (STD_LOCKING(sdbc) ? DB_RMW : 0) |
		    DB_FIRST)) == DB_NOTFOUND) {
			build = 1;
			ret = 0;
		}

		/*
		 * Secondary cursors have special refcounting close
		 * methods.  Be careful.
		 */
		if ((t_ret = __db_c_close(sdbc)) != 0 && ret == 0)
			ret = t_ret;

		/* Reset for later error check. */
		sdbc = NULL;

		if (ret != 0)
			goto err;
	}

	/*
	 * Add the secondary to the list on the primary.  Do it here
	 * so that we see any updates that occur while we're walking
	 * the primary.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);

	/* See __db_s_next for an explanation of secondary refcounting. */
	DB_ASSERT(sdbp->s_refcnt == 0);
	sdbp->s_refcnt = 1;
	LIST_INSERT_HEAD(&dbp->s_secondaries, sdbp, s_links);
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	if (build) {
		/*
		 * We loop through the primary, putting each item we
		 * find into the new secondary.
		 *
		 * If we're using CDB, opening these two cursors puts us
		 * in a bit of a locking tangle:  CDB locks are done on the
		 * primary, so that we stay deadlock-free, but that means
		 * that updating the secondary while we have a read cursor
		 * open on the primary will self-block.  To get around this,
		 * we force the primary cursor to use the same locker ID
		 * as the secondary, so they won't conflict.  This should
		 * be harmless even if we're not using CDB.
		 */
		if ((ret = __db_cursor(sdbp, txn, &sdbc,
		    CDB_LOCKING(sdbp->dbenv) ? DB_WRITECURSOR : 0)) != 0)
			goto err;
		if ((ret = __db_cursor_int(dbp,
		    txn, dbp->type, PGNO_INVALID, 0, sdbc->locker, &pdbc)) != 0)
			goto err;

		/* Lock out other threads, now that we have a locker ID. */
		dbp->associate_lid = sdbc->locker;

		memset(&key, 0, sizeof(DBT));
		memset(&data, 0, sizeof(DBT));
		while ((ret = __db_c_get(pdbc, &key, &data, DB_NEXT)) == 0) {
			memset(&skey, 0, sizeof(DBT));
			if ((ret = callback(sdbp, &key, &data, &skey)) != 0) {
				if (ret == DB_DONOTINDEX)
					continue;
				goto err;
			}
			if ((ret = __db_c_put(sdbc,
			    &skey, &key, DB_UPDATE_SECONDARY)) != 0) {
				FREE_IF_NEEDED(sdbp, &skey);
				goto err;
			}

			FREE_IF_NEEDED(sdbp, &skey);
		}
		if (ret == DB_NOTFOUND)
			ret = 0;
	}

err:	if (sdbc != NULL && (t_ret = __db_c_close(sdbc)) != 0 && ret == 0)
		ret = t_ret;

	if (pdbc != NULL && (t_ret = __db_c_close(pdbc)) != 0 && ret == 0)
		ret = t_ret;

	dbp->associate_lid = DB_LOCK_INVALIDID;

	return (ret);
}

/*
 * __db_secondary_get --
 *	This wrapper function for DB->pget() is the DB->get() function
 *	on a database which has been made into a secondary index.
 */
static int
__db_secondary_get(sdbp, txn, skey, data, flags)
	DB *sdbp;
	DB_TXN *txn;
	DBT *skey, *data;
	u_int32_t flags;
{

	DB_ASSERT(F_ISSET(sdbp, DB_AM_SECONDARY));
	return (__db_pget_pp(sdbp, txn, skey, NULL, data, flags));
}

/*
 * __db_secondary_close --
 *	Wrapper function for DB->close() which we use on secondaries to
 *	manage refcounting and make sure we don't close them underneath
 *	a primary that is updating.
 *
 * PUBLIC: int __db_secondary_close __P((DB *, u_int32_t));
 */
int
__db_secondary_close(sdbp, flags)
	DB *sdbp;
	u_int32_t flags;
{
	DB *primary;
	int doclose;

	doclose = 0;
	primary = sdbp->s_primary;

	MUTEX_THREAD_LOCK(primary->dbenv, primary->mutexp);
	/*
	 * Check the refcount--if it was at 1 when we were called, no
	 * thread is currently updating this secondary through the primary,
	 * so it's safe to close it for real.
	 *
	 * If it's not safe to do the close now, we do nothing;  the
	 * database will actually be closed when the refcount is decremented,
	 * which can happen in either __db_s_next or __db_s_done.
	 */
	DB_ASSERT(sdbp->s_refcnt != 0);
	if (--sdbp->s_refcnt == 0) {
		LIST_REMOVE(sdbp, s_links);
		/* We don't want to call close while the mutex is held. */
		doclose = 1;
	}
	MUTEX_THREAD_UNLOCK(primary->dbenv, primary->mutexp);

	/*
	 * sdbp->close is this function;  call the real one explicitly if
	 * need be.
	 */
	return (doclose ? __db_close(sdbp, NULL, flags) : 0);
}

/*
 * __db_append_primary --
 *	Perform the secondary index updates necessary to put(DB_APPEND)
 *	a record to a primary database.
 */
static int
__db_append_primary(dbc, key, data)
	DBC *dbc;
	DBT *key, *data;
{
	DB *dbp, *sdbp;
	DBC *sdbc, *pdbc;
	DBT oldpkey, pkey, pdata, skey;
	int cmp, ret, t_ret;

	dbp = dbc->dbp;
	sdbp = NULL;
	ret = 0;

	/*
	 * Worrying about partial appends seems a little like worrying
	 * about Linear A character encodings.  But we support those
	 * too if your application understands them.
	 */
	pdbc = NULL;
	if (F_ISSET(data, DB_DBT_PARTIAL) || F_ISSET(key, DB_DBT_PARTIAL)) {
		/*
		 * The dbc we were passed is all set to pass things
		 * back to the user;  we can't safely do a call on it.
		 * Dup the cursor, grab the real data item (we don't
		 * care what the key is--we've been passed it directly),
		 * and use that instead of the data DBT we were passed.
		 *
		 * Note that we can get away with this simple get because
		 * an appended item is by definition new, and the
		 * correctly-constructed full data item from this partial
		 * put is on the page waiting for us.
		 */
		if ((ret = __db_c_idup(dbc, &pdbc, DB_POSITION)) != 0)
			return (ret);
		memset(&pkey, 0, sizeof(DBT));
		memset(&pdata, 0, sizeof(DBT));

		if ((ret = __db_c_get(pdbc, &pkey, &pdata, DB_CURRENT)) != 0)
			goto err;

		key = &pkey;
		data = &pdata;
	}

	/*
	 * Loop through the secondary indices, putting a new item in
	 * each that points to the appended item.
	 *
	 * This is much like the loop in "step 3" in __db_c_put, so
	 * I'm not commenting heavily here;  it was unclean to excerpt
	 * just that section into a common function, but the basic
	 * overview is the same here.
	 */
	for (sdbp = __db_s_first(dbp);
	    sdbp != NULL && ret == 0; ret = __db_s_next(&sdbp)) {
		memset(&skey, 0, sizeof(DBT));
		if ((ret = sdbp->s_callback(sdbp, key, data, &skey)) != 0) {
			if (ret == DB_DONOTINDEX)
				continue;
			else
				goto err;
		}

		if ((ret = __db_cursor_int(sdbp, dbc->txn, sdbp->type,
		    PGNO_INVALID, 0, dbc->locker, &sdbc)) != 0) {
			FREE_IF_NEEDED(sdbp, &skey);
			goto err;
		}
		if (CDB_LOCKING(sdbp->dbenv)) {
			DB_ASSERT(sdbc->mylock.off == LOCK_INVALID);
			F_SET(sdbc, DBC_WRITER);
		}

		/*
		 * Since we know we have a new primary key, it can't be a
		 * duplicate duplicate in the secondary.  It can be a
		 * duplicate in a secondary that doesn't support duplicates,
		 * however, so we need to be careful to avoid an overwrite
		 * (which would corrupt our index).
		 */
		if (!F_ISSET(sdbp, DB_AM_DUP)) {
			memset(&oldpkey, 0, sizeof(DBT));
			F_SET(&oldpkey, DB_DBT_MALLOC);
			ret = __db_c_get(sdbc, &skey, &oldpkey,
			    DB_SET | (STD_LOCKING(dbc) ? DB_RMW : 0));
			if (ret == 0) {
				cmp = __bam_defcmp(sdbp, &oldpkey, key);
				/*
				 * XXX
				 * This needs to use the right free function
				 * as soon as this is possible.
				 */
				__os_ufree(sdbp->dbenv,
				    oldpkey.data);
				if (cmp != 0) {
					__db_err(sdbp->dbenv, "%s%s",
			    "Append results in a non-unique secondary key in",
			    " an index not configured to support duplicates");
					ret = EINVAL;
					goto err1;
				}
			} else if (ret != DB_NOTFOUND && ret != DB_KEYEMPTY)
				goto err1;
		}

		ret = __db_c_put(sdbc, &skey, key, DB_UPDATE_SECONDARY);

err1:		FREE_IF_NEEDED(sdbp, &skey);

		if ((t_ret = __db_c_close(sdbc)) != 0 && ret == 0)
			ret = t_ret;

		if (ret != 0)
			goto err;
	}

err:	if (pdbc != NULL && (t_ret = __db_c_close(pdbc)) != 0 && ret == 0)
		ret = t_ret;
	if (sdbp != NULL && (t_ret = __db_s_done(sdbp)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}
