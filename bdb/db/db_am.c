/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_am.c,v 11.42 2001/01/11 18:19:50 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "btree.h"
#include "hash.h"
#include "qam.h"
#include "lock.h"
#include "mp.h"
#include "txn.h"
#include "db_am.h"
#include "db_ext.h"

/*
 * __db_cursor --
 *	Allocate and return a cursor.
 *
 * PUBLIC: int __db_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
 */
int
__db_cursor(dbp, txn, dbcp, flags)
	DB *dbp;
	DB_TXN *txn;
	DBC **dbcp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *dbc;
	db_lockmode_t mode;
	u_int32_t op;
	int ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->cursor");

	/* Check for invalid flags. */
	if ((ret = __db_cursorchk(dbp, flags, F_ISSET(dbp, DB_AM_RDONLY))) != 0)
		return (ret);

	if ((ret =
	    __db_icursor(dbp, txn, dbp->type, PGNO_INVALID, 0, dbcp)) != 0)
		return (ret);
	dbc = *dbcp;

	/*
	 * If this is CDB, do all the locking in the interface, which is
	 * right here.
	 */
	if (CDB_LOCKING(dbenv)) {
		op = LF_ISSET(DB_OPFLAGS_MASK);
		mode = (op == DB_WRITELOCK) ? DB_LOCK_WRITE :
		    ((op == DB_WRITECURSOR) ? DB_LOCK_IWRITE : DB_LOCK_READ);
		if ((ret = lock_get(dbenv, dbc->locker, 0,
		    &dbc->lock_dbt, mode, &dbc->mylock)) != 0) {
			(void)__db_c_close(dbc);
			return (ret);
		}
		if (op == DB_WRITECURSOR)
			F_SET(dbc, DBC_WRITECURSOR);
		if (op == DB_WRITELOCK)
			F_SET(dbc, DBC_WRITER);
	}

	return (0);
}

/*
 * __db_icursor --
 *	Internal version of __db_cursor.  If dbcp is
 *	non-NULL it is assumed to point to an area to
 *	initialize as a cursor.
 *
 * PUBLIC: int __db_icursor
 * PUBLIC:     __P((DB *, DB_TXN *, DBTYPE, db_pgno_t, int, DBC **));
 */
int
__db_icursor(dbp, txn, dbtype, root, is_opd, dbcp)
	DB *dbp;
	DB_TXN *txn;
	DBTYPE dbtype;
	db_pgno_t root;
	int is_opd;
	DBC **dbcp;
{
	DBC *dbc, *adbc;
	DBC_INTERNAL *cp;
	DB_ENV *dbenv;
	int allocated, ret;

	dbenv = dbp->dbenv;
	allocated = 0;

	/*
	 * Take one from the free list if it's available.  Take only the
	 * right type.  With off page dups we may have different kinds
	 * of cursors on the queue for a single database.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	for (dbc = TAILQ_FIRST(&dbp->free_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links))
		if (dbtype == dbc->dbtype) {
			TAILQ_REMOVE(&dbp->free_queue, dbc, links);
			dbc->flags = 0;
			break;
		}
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	if (dbc == NULL) {
		if ((ret = __os_calloc(dbp->dbenv, 1, sizeof(DBC), &dbc)) != 0)
			return (ret);
		allocated = 1;
		dbc->flags = 0;

		dbc->dbp = dbp;

		/* Set up locking information. */
		if (LOCKING_ON(dbenv)) {
			/*
			 * If we are not threaded, then there is no need to
			 * create new locker ids.  We know that no one else
			 * is running concurrently using this DB, so we can
			 * take a peek at any cursors on the active queue.
			 */
			if (!DB_IS_THREADED(dbp) &&
			    (adbc = TAILQ_FIRST(&dbp->active_queue)) != NULL)
				dbc->lid = adbc->lid;
			else
				if ((ret = lock_id(dbenv, &dbc->lid)) != 0)
					goto err;

			memcpy(dbc->lock.fileid, dbp->fileid, DB_FILE_ID_LEN);
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
		default:
			ret = __db_unknown_type(dbp->dbenv,
			    "__db_icursor", dbtype);
			goto err;
		}

		cp = dbc->internal;
	}

	/* Refresh the DBC structure. */
	dbc->dbtype = dbtype;

	if ((dbc->txn = txn) == NULL)
		dbc->locker = dbc->lid;
	else {
		dbc->locker = txn->txnid;
		txn->cursors++;
	}

	if (is_opd)
		F_SET(dbc, DBC_OPD);
	if (F_ISSET(dbp, DB_AM_RECOVER))
		F_SET(dbc, DBC_RECOVER);

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
	default:
		ret = __db_unknown_type(dbp->dbenv, "__db_icursor", dbp->type);
		goto err;
	}

	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	TAILQ_INSERT_TAIL(&dbp->active_queue, dbc, links);
	F_SET(dbc, DBC_ACTIVE);
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	*dbcp = dbc;
	return (0);

err:	if (allocated)
		__os_free(dbc, sizeof(*dbc));
	return (ret);
}

#ifdef DEBUG
/*
 * __db_cprint --
 *	Display the current cursor list.
 *
 * PUBLIC: int __db_cprint __P((DB *));
 */
int
__db_cprint(dbp)
	DB *dbp;
{
	static const FN fn[] = {
		{ DBC_ACTIVE,		"active" },
		{ DBC_OPD,		"off-page-dup" },
		{ DBC_RECOVER,		"recover" },
		{ DBC_RMW,		"read-modify-write" },
		{ DBC_WRITECURSOR,	"write cursor" },
		{ DBC_WRITEDUP,		"internally dup'ed write cursor" },
		{ DBC_WRITER,		"short-term write cursor" },
		{ 0,			NULL }
	};
	DBC *dbc;
	DBC_INTERNAL *cp;
	char *s;

	MUTEX_THREAD_LOCK(dbp->dbenv, dbp->mutexp);
	for (dbc = TAILQ_FIRST(&dbp->active_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
		switch (dbc->dbtype) {
		case DB_BTREE:
			s = "btree";
			break;
		case DB_HASH:
			s = "hash";
			break;
		case DB_RECNO:
			s = "recno";
			break;
		case DB_QUEUE:
			s = "queue";
			break;
		default:
			DB_ASSERT(0);
			return (1);
		}
		cp = dbc->internal;
		fprintf(stderr, "%s/%#0lx: opd: %#0lx\n",
		    s, P_TO_ULONG(dbc), P_TO_ULONG(cp->opd));
		fprintf(stderr, "\ttxn: %#0lx lid: %lu locker: %lu\n",
		    P_TO_ULONG(dbc->txn),
		    (u_long)dbc->lid, (u_long)dbc->locker);
		fprintf(stderr, "\troot: %lu page/index: %lu/%lu",
		    (u_long)cp->root, (u_long)cp->pgno, (u_long)cp->indx);
		__db_prflags(dbc->flags, fn, stderr);
		fprintf(stderr, "\n");

		if (dbp->type == DB_BTREE)
			__bam_cprint(dbc);
	}
	for (dbc = TAILQ_FIRST(&dbp->free_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links))
		fprintf(stderr, "free: %#0lx ", P_TO_ULONG(dbc));
	fprintf(stderr, "\n");
	MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);

	return (0);
}
#endif /* DEBUG */

/*
 * db_fd --
 *	Return a file descriptor for flock'ing.
 *
 * PUBLIC: int __db_fd __P((DB *, int *));
 */
int
__db_fd(dbp, fdp)
	DB *dbp;
	int *fdp;
{
	DB_FH *fhp;
	int ret;

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->fd");

	/*
	 * XXX
	 * Truly spectacular layering violation.
	 */
	if ((ret = __mp_xxx_fh(dbp->mpf, &fhp)) != 0)
		return (ret);

	if (F_ISSET(fhp, DB_FH_VALID)) {
		*fdp = fhp->fd;
		return (0);
	} else {
		*fdp = -1;
		__db_err(dbp->dbenv, "DB does not have a valid file handle.");
		return (ENOENT);
	}
}

/*
 * __db_get --
 *	Return a key/data pair.
 *
 * PUBLIC: int __db_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__db_get(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DBC *dbc;
	int mode, ret, t_ret;

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->get");

	if ((ret = __db_getchk(dbp, key, data, flags)) != 0)
		return (ret);

	mode = 0;
	if (flags == DB_CONSUME || flags == DB_CONSUME_WAIT)
		mode = DB_WRITELOCK;
	if ((ret = dbp->cursor(dbp, txn, &dbc, mode)) != 0)
		return (ret);

	DEBUG_LREAD(dbc, txn, "__db_get", key, NULL, flags);

	/*
	 * The DBC_TRANSIENT flag indicates that we're just doing a
	 * single operation with this cursor, and that in case of
	 * error we don't need to restore it to its old position--we're
	 * going to close it right away.  Thus, we can perform the get
	 * without duplicating the cursor, saving some cycles in this
	 * common case.
	 */
	F_SET(dbc, DBC_TRANSIENT);

	ret = dbc->c_get(dbc, key, data,
	    flags == 0 || flags == DB_RMW ? flags | DB_SET : flags);

	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

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
	int ret, t_ret;

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->put");

	if ((ret = __db_putchk(dbp, key, data,
	    flags, F_ISSET(dbp, DB_AM_RDONLY),
	    F_ISSET(dbp, DB_AM_DUP) || F_ISSET(key, DB_DBT_DUPOK))) != 0)
		return (ret);

	DB_CHECK_TXN(dbp, txn);

	if ((ret = dbp->cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		return (ret);

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

	DEBUG_LWRITE(dbc, txn, "__db_put", key, data, flags);

	if (flags == DB_NOOVERWRITE) {
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
		if ((ret = dbc->c_get(dbc, key, &tdata,
		    DB_SET | (STD_LOCKING(dbc) ? DB_RMW : 0))) == 0)
			ret = DB_KEYEXIST;
		else if (ret == DB_NOTFOUND)
			ret = 0;
	}
	if (ret == 0)
		ret = dbc->c_put(dbc,
		     key, data, flags == 0 ? DB_KEYLAST : flags);

	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_sync --
 *	Flush the database cache.
 *
 * PUBLIC: int __db_sync __P((DB *, u_int32_t));
 */
int
__db_sync(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	int ret, t_ret;

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->sync");

	if ((ret = __db_syncchk(dbp, flags)) != 0)
		return (ret);

	/* Read-only trees never need to be sync'd. */
	if (F_ISSET(dbp, DB_AM_RDONLY))
		return (0);

	/* If it's a Recno tree, write the backing source text file. */
	if (dbp->type == DB_RECNO)
		ret = __ram_writeback(dbp);

	/* If the tree was never backed by a database file, we're done. */
	if (F_ISSET(dbp, DB_AM_INMEM))
		return (0);

	/* Flush any dirty pages from the cache to the backing file. */
	if ((t_ret = memp_fsync(dbp->mpf)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}
