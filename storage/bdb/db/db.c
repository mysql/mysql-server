/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: db.c,v 11.300 2004/10/26 17:38:41 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_swap.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/qam.h"
#include "dbinc/txn.h"

static int __db_disassociate __P((DB *));

#ifdef CONFIG_TEST
static void __db_makecopy __P((DB_ENV *, const char *, const char *));
static int  __db_testdocopy __P((DB_ENV *, const char *));
static int  __qam_testdocopy __P((DB *, const char *));
#endif

/*
 * DB.C --
 *	This file contains the utility functions for the DBP layer.
 */

/*
 * __db_master_open --
 *	Open up a handle on a master database.
 *
 * PUBLIC: int __db_master_open __P((DB *,
 * PUBLIC:     DB_TXN *, const char *, u_int32_t, int, DB **));
 */
int
__db_master_open(subdbp, txn, name, flags, mode, dbpp)
	DB *subdbp;
	DB_TXN *txn;
	const char *name;
	u_int32_t flags;
	int mode;
	DB **dbpp;
{
	DB *dbp;
	int ret;

	*dbpp = NULL;

	/* Open up a handle on the main database. */
	if ((ret = db_create(&dbp, subdbp->dbenv, 0)) != 0)
		return (ret);

	/*
	 * It's always a btree.
	 * Run in the transaction we've created.
	 * Set the pagesize in case we're creating a new database.
	 * Flag that we're creating a database with subdatabases.
	 */
	dbp->pgsize = subdbp->pgsize;
	F_SET(dbp, DB_AM_SUBDB);
	F_SET(dbp, F_ISSET(subdbp,
	    DB_AM_RECOVER | DB_AM_SWAP |
	    DB_AM_ENCRYPT | DB_AM_CHKSUM | DB_AM_NOT_DURABLE));

	/*
	 * If there was a subdb specified, then we only want to apply
	 * DB_EXCL to the subdb, not the actual file.  We only got here
	 * because there was a subdb specified.
	 */
	LF_CLR(DB_EXCL);
	LF_SET(DB_RDWRMASTER);
	if ((ret = __db_open(dbp,
	    txn, name, NULL, DB_BTREE, flags, mode, PGNO_BASE_MD)) != 0)
		goto err;

	/*
	 * Verify that pagesize is the same on both.  The items in dbp were now
	 * initialized from the meta page.  The items in dbp were set in
	 * __db_dbopen when we either read or created the master file.  Other
	 * items such as checksum and encryption are checked when we read the
	 * meta-page.  So we do not check those here.  However, if the
	 * meta-page caused checksumming to be turned on and it wasn't already,
	 * set it here.
	 */
	if (F_ISSET(dbp, DB_AM_CHKSUM))
		F_SET(subdbp, DB_AM_CHKSUM);
	if (subdbp->pgsize != 0 && dbp->pgsize != subdbp->pgsize) {
		ret = EINVAL;
		__db_err(dbp->dbenv,
		    "Different pagesize specified on existent file");
		goto err;
	}
err:
	if (ret != 0 && !F_ISSET(dbp, DB_AM_DISCARD))
		(void)__db_close(dbp, txn, 0);
	else
		*dbpp = dbp;
	return (ret);
}

/*
 * __db_master_update --
 *	Add/Open/Remove a subdatabase from a master database.
 *
 * PUBLIC: int __db_master_update __P((DB *, DB *, DB_TXN *, const char *,
 * PUBLIC:     DBTYPE, mu_action, const char *, u_int32_t));
 */
int
__db_master_update(mdbp, sdbp, txn, subdb, type, action, newname, flags)
	DB *mdbp, *sdbp;
	DB_TXN *txn;
	const char *subdb;
	DBTYPE type;
	mu_action action;
	const char *newname;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *dbc, *ndbc;
	DBT key, data, ndata;
	PAGE *p, *r;
	db_pgno_t t_pgno;
	int modify, ret, t_ret;

	dbenv = mdbp->dbenv;
	dbc = ndbc = NULL;
	p = NULL;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Might we modify the master database?  If so, we'll need to lock. */
	modify = (action != MU_OPEN || LF_ISSET(DB_CREATE)) ? 1 : 0;

	/*
	 * Open up a cursor.  If this is CDB and we're creating the database,
	 * make it an update cursor.
	 */
	if ((ret = __db_cursor(mdbp, txn, &dbc,
	    (CDB_LOCKING(dbenv) && modify) ? DB_WRITECURSOR : 0)) != 0)
		goto err;

	/*
	 * Point the cursor at the record.
	 *
	 * If we're removing or potentially creating an entry, lock the page
	 * with DB_RMW.
	 *
	 * We do multiple cursor operations with the cursor in some cases and
	 * subsequently access the data DBT information.  Set DB_DBT_MALLOC so
	 * we don't risk modification of the data between our uses of it.
	 *
	 * !!!
	 * We don't include the name's nul termination in the database.
	 */
	key.data = (void *)subdb;
	key.size = (u_int32_t)strlen(subdb);
	F_SET(&data, DB_DBT_MALLOC);

	ret = __db_c_get(dbc, &key, &data,
	    DB_SET | ((STD_LOCKING(dbc) && modify) ? DB_RMW : 0));

	/*
	 * What we do next--whether or not we found a record for the
	 * specified subdatabase--depends on what the specified action is.
	 * Handle ret appropriately as the first statement of each case.
	 */
	switch (action) {
	case MU_REMOVE:
		/*
		 * We should have found something if we're removing it.  Note
		 * that in the common case where the DB we're asking to remove
		 * doesn't exist, we won't get this far;  __db_subdb_remove
		 * will already have returned an error from __db_open.
		 */
		if (ret != 0)
			goto err;

		/*
		 * Delete the subdatabase entry first;  if this fails,
		 * we don't want to touch the actual subdb pages.
		 */
		if ((ret = __db_c_del(dbc, 0)) != 0)
			goto err;

		/*
		 * We're handling actual data, not on-page meta-data,
		 * so it hasn't been converted to/from opposite
		 * endian architectures.  Do it explicitly, now.
		 */
		memcpy(&sdbp->meta_pgno, data.data, sizeof(db_pgno_t));
		DB_NTOHL(&sdbp->meta_pgno);
		if ((ret =
		    __memp_fget(mdbp->mpf, &sdbp->meta_pgno, 0, &p)) != 0)
			goto err;

		/* Free the root on the master db. */
		if (TYPE(p) == P_BTREEMETA) {
			if ((ret = __memp_fget(mdbp->mpf,
			     &((BTMETA *)p)->root, 0, &r)) != 0)
				goto err;

			/* Free and put the page. */
			if ((ret = __db_free(dbc, r)) != 0) {
				r = NULL;
				goto err;
			}
		}
		/* Free and put the page. */
		if ((ret = __db_free(dbc, p)) != 0) {
			p = NULL;
			goto err;
		}
		p = NULL;
		break;
	case MU_RENAME:
		/* We should have found something if we're renaming it. */
		if (ret != 0)
			goto err;

		/*
		 * Before we rename, we need to make sure we're not
		 * overwriting another subdatabase, or else this operation
		 * won't be undoable.  Open a second cursor and check
		 * for the existence of newname;  it shouldn't appear under
		 * us since we hold the metadata lock.
		 */
		if ((ret = __db_cursor(mdbp, txn, &ndbc, 0)) != 0)
			goto err;
		key.data = (void *)newname;
		key.size = (u_int32_t)strlen(newname);

		/*
		 * We don't actually care what the meta page of the potentially-
		 * overwritten DB is;  we just care about existence.
		 */
		memset(&ndata, 0, sizeof(ndata));
		F_SET(&ndata, DB_DBT_USERMEM | DB_DBT_PARTIAL);

		if ((ret = __db_c_get(ndbc, &key, &ndata, DB_SET)) == 0) {
			/* A subdb called newname exists.  Bail. */
			ret = EEXIST;
			__db_err(dbenv, "rename: database %s exists", newname);
			goto err;
		} else if (ret != DB_NOTFOUND)
			goto err;

		/*
		 * Now do the put first;  we don't want to lose our
		 * sole reference to the subdb.  Use the second cursor
		 * so that the first one continues to point to the old record.
		 */
		if ((ret = __db_c_put(ndbc, &key, &data, DB_KEYFIRST)) != 0)
			goto err;
		if ((ret = __db_c_del(dbc, 0)) != 0) {
			/*
			 * If the delete fails, try to delete the record
			 * we just put, in case we're not txn-protected.
			 */
			(void)__db_c_del(ndbc, 0);
			goto err;
		}

		break;
	case MU_OPEN:
		/*
		 * Get the subdatabase information.  If it already exists,
		 * copy out the page number and we're done.
		 */
		switch (ret) {
		case 0:
			if (LF_ISSET(DB_CREATE) && LF_ISSET(DB_EXCL)) {
				ret = EEXIST;
				goto err;
			}
			memcpy(&sdbp->meta_pgno, data.data, sizeof(db_pgno_t));
			DB_NTOHL(&sdbp->meta_pgno);
			goto done;
		case DB_NOTFOUND:
			if (LF_ISSET(DB_CREATE))
				break;
			/*
			 * No db_err, it is reasonable to remove a
			 * nonexistent db.
			 */
			ret = ENOENT;
			goto err;
		default:
			goto err;
		}

		/* Create a subdatabase. */
		if ((ret = __db_new(dbc,
		    type == DB_HASH ? P_HASHMETA : P_BTREEMETA, &p)) != 0)
			goto err;
		sdbp->meta_pgno = PGNO(p);

		/*
		 * XXX
		 * We're handling actual data, not on-page meta-data, so it
		 * hasn't been converted to/from opposite endian architectures.
		 * Do it explicitly, now.
		 */
		t_pgno = PGNO(p);
		DB_HTONL(&t_pgno);
		memset(&ndata, 0, sizeof(ndata));
		ndata.data = &t_pgno;
		ndata.size = sizeof(db_pgno_t);
		if ((ret = __db_c_put(dbc, &key, &ndata, DB_KEYLAST)) != 0)
			goto err;
		F_SET(sdbp, DB_AM_CREATED);
		break;
	}

err:
done:	/*
	 * If we allocated a page: if we're successful, mark the page dirty
	 * and return it to the cache, otherwise, discard/free it.
	 */
	if (p != NULL) {
		if (ret == 0) {
			if ((t_ret =
			    __memp_fput(mdbp->mpf, p, DB_MPOOL_DIRTY)) != 0)
				ret = t_ret;
		} else
			(void)__memp_fput(mdbp->mpf, p, 0);
	}

	/* Discard the cursor(s) and data. */
	if (data.data != NULL)
		__os_ufree(dbenv, data.data);
	if (dbc != NULL && (t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	if (ndbc != NULL && (t_ret = __db_c_close(ndbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_dbenv_setup --
 *	Set up the underlying environment during a db_open.
 *
 * PUBLIC: int __db_dbenv_setup __P((DB *,
 * PUBLIC:     DB_TXN *, const char *, u_int32_t, u_int32_t));
 */
int
__db_dbenv_setup(dbp, txn, fname, id, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *fname;
	u_int32_t id, flags;
{
	DB *ldbp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	u_int32_t maxid;
	int ret;

	dbenv = dbp->dbenv;

	/* If we don't yet have an environment, it's time to create it. */
	if (!F_ISSET(dbenv, DB_ENV_OPEN_CALLED)) {
		/* Make sure we have at least DB_MINCACHE pages in our cache. */
		if (dbenv->mp_gbytes == 0 &&
		    dbenv->mp_bytes < dbp->pgsize * DB_MINPAGECACHE &&
		    (ret = __memp_set_cachesize(
		    dbenv, 0, dbp->pgsize * DB_MINPAGECACHE, 0)) != 0)
			return (ret);

		if ((ret = __dbenv_open(dbenv, NULL, DB_CREATE |
		    DB_INIT_MPOOL | DB_PRIVATE | LF_ISSET(DB_THREAD), 0)) != 0)
			return (ret);
	}

	/* Join the underlying cache. */
	if ((ret = __db_dbenv_mpool(dbp, fname, flags)) != 0)
		return (ret);

	/*
	 * We may need a per-thread mutex.  Allocate it from the mpool
	 * region, there's supposed to be extra space there for that purpose.
	 */
	if (LF_ISSET(DB_THREAD)) {
		dbmp = dbenv->mp_handle;
		if ((ret = __db_mutex_setup(dbenv, dbmp->reginfo, &dbp->mutexp,
		    MUTEX_ALLOC | MUTEX_THREAD)) != 0)
			return (ret);
	}

	/*
	 * Set up a bookkeeping entry for this database in the log region,
	 * if such a region exists.  Note that even if we're in recovery
	 * or a replication client, where we won't log registries, we'll
	 * still need an FNAME struct, so LOGGING_ON is the correct macro.
	 */
	if (LOGGING_ON(dbenv) &&
	    (ret = __dbreg_setup(dbp, fname, id)) != 0)
		return (ret);

	/*
	 * If we're actively logging and our caller isn't a recovery function
	 * that already did so, assign this dbp a log fileid.
	 */
	if (DBENV_LOGGING(dbenv) && !F_ISSET(dbp, DB_AM_RECOVER) &&
#if !defined(DEBUG_ROP)
	    !F_ISSET(dbp, DB_AM_RDONLY) &&
#endif
	    (ret = __dbreg_new_id(dbp, txn)) != 0)
		return (ret);

	/*
	 * Insert ourselves into the DB_ENV's dblist.  We allocate a
	 * unique ID to each {fileid, meta page number} pair, and to
	 * each temporary file (since they all have a zero fileid).
	 * This ID gives us something to use to tell which DB handles
	 * go with which databases in all the cursor adjustment
	 * routines, where we don't want to do a lot of ugly and
	 * expensive memcmps.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (maxid = 0, ldbp = LIST_FIRST(&dbenv->dblist);
	    ldbp != NULL; ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		if (fname != NULL &&
		    memcmp(ldbp->fileid, dbp->fileid, DB_FILE_ID_LEN) == 0 &&
		    ldbp->meta_pgno == dbp->meta_pgno)
			break;
		if (ldbp->adj_fileid > maxid)
			maxid = ldbp->adj_fileid;
	}

	/*
	 * If ldbp is NULL, we didn't find a match, or we weren't
	 * really looking because fname is NULL.  Assign the dbp an
	 * adj_fileid one higher than the largest we found, and
	 * insert it at the head of the master dbp list.
	 *
	 * If ldbp is not NULL, it is a match for our dbp.  Give dbp
	 * the same ID that ldbp has, and add it after ldbp so they're
	 * together in the list.
	 */
	if (ldbp == NULL) {
		dbp->adj_fileid = maxid + 1;
		LIST_INSERT_HEAD(&dbenv->dblist, dbp, dblistlinks);
	} else {
		dbp->adj_fileid = ldbp->adj_fileid;
		LIST_INSERT_AFTER(ldbp, dbp, dblistlinks);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	return (0);
}

/*
 * __db_dbenv_mpool --
 *	Set up the underlying environment cache during a db_open.
 *
 * PUBLIC: int __db_dbenv_mpool __P((DB *, const char *, u_int32_t));
 */
int
__db_dbenv_mpool(dbp, fname, flags)
	DB *dbp;
	const char *fname;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBT pgcookie;
	DB_MPOOLFILE *mpf;
	DB_PGINFO pginfo;
	u_int32_t clear_len;
	int ftype, ret;

	COMPQUIET(mpf, NULL);

	dbenv = dbp->dbenv;

	/*
	 * If we need to pre- or post-process a file's pages on I/O, set the
	 * file type.  If it's a hash file, always call the pgin and pgout
	 * routines.  This means that hash files can never be mapped into
	 * process memory.  If it's a btree file and requires swapping, we
	 * need to page the file in and out.  This has to be right -- we can't
	 * mmap files that are being paged in and out.
	 */
	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		ftype = F_ISSET(dbp, DB_AM_SWAP | DB_AM_ENCRYPT | DB_AM_CHKSUM)
		    ? DB_FTYPE_SET : DB_FTYPE_NOTSET;
		clear_len = CRYPTO_ON(dbenv) ? dbp->pgsize : DB_PAGE_DB_LEN;
		break;
	case DB_HASH:
		ftype = DB_FTYPE_SET;
		clear_len = CRYPTO_ON(dbenv) ? dbp->pgsize : DB_PAGE_DB_LEN;
		break;
	case DB_QUEUE:
		ftype = F_ISSET(dbp,
		    DB_AM_SWAP | DB_AM_ENCRYPT | DB_AM_CHKSUM) ?
		    DB_FTYPE_SET : DB_FTYPE_NOTSET;
		clear_len = CRYPTO_ON(dbenv) ? dbp->pgsize : DB_PAGE_QUEUE_LEN;
		break;
	case DB_UNKNOWN:
		/*
		 * If we're running in the verifier, our database might
		 * be corrupt and we might not know its type--but we may
		 * still want to be able to verify and salvage.
		 *
		 * If we can't identify the type, it's not going to be safe
		 * to call __db_pgin--we pretty much have to give up all
		 * hope of salvaging cross-endianness.  Proceed anyway;
		 * at worst, the database will just appear more corrupt
		 * than it actually is, but at best, we may be able
		 * to salvage some data even with no metadata page.
		 */
		if (F_ISSET(dbp, DB_AM_VERIFYING)) {
			ftype = DB_FTYPE_NOTSET;
			clear_len = DB_PAGE_DB_LEN;
			break;
		}
		/* FALLTHROUGH */
	default:
		return (__db_unknown_type(dbenv, "DB->open", dbp->type));
	}

	mpf = dbp->mpf;

	(void)__memp_set_clear_len(mpf, clear_len);
	(void)__memp_set_fileid(mpf, dbp->fileid);
	(void)__memp_set_ftype(mpf, ftype);
	(void)__memp_set_lsn_offset(mpf, 0);

	pginfo.db_pagesize = dbp->pgsize;
	pginfo.flags =
	    F_ISSET(dbp, (DB_AM_CHKSUM | DB_AM_ENCRYPT | DB_AM_SWAP));
	pginfo.type = dbp->type;
	pgcookie.data = &pginfo;
	pgcookie.size = sizeof(DB_PGINFO);
	(void)__memp_set_pgcookie(mpf, &pgcookie);

	if ((ret = __memp_fopen(mpf, NULL, fname,
	    LF_ISSET(DB_RDONLY | DB_NOMMAP |
	    DB_ODDFILESIZE | DB_TRUNCATE) |
	    (F_ISSET(dbenv, DB_ENV_DIRECT_DB) ? DB_DIRECT : 0) |
	    (F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_TXN_NOT_DURABLE : 0),
	    0, dbp->pgsize)) != 0)
		return (ret);

	return (0);
}

/*
 * __db_close --
 *	DB->close method.
 *
 * PUBLIC: int __db_close __P((DB *, DB_TXN *, u_int32_t));
 */
int
__db_close(dbp, txn, flags)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int db_ref, deferred_close, ret, t_ret;

	dbenv = dbp->dbenv;
	deferred_close = ret = 0;

	/*
	 * Validate arguments, but as a DB handle destructor, we can't fail.
	 *
	 * Check for consistent transaction usage -- ignore errors.  Only
	 * internal callers specify transactions, so it's a serious problem
	 * if we get error messages.
	 */
	if (txn != NULL)
		(void)__db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 0);

	/* Refresh the structure and close any underlying resources. */
	ret = __db_refresh(dbp, txn, flags, &deferred_close);

	/*
	 * If we've deferred the close because the logging of the close failed,
	 * return our failure right away without destroying the handle.
	 */
	if (deferred_close)
		return (ret);

	/* !!!
	 * This code has an apparent race between the moment we read and
	 * decrement dbenv->db_ref and the moment we check whether it's 0.
	 * However, if the environment is DBLOCAL, the user shouldn't have a
	 * reference to the dbenv handle anyway;  the only way we can get
	 * multiple dbps sharing a local dbenv is if we open them internally
	 * during something like a subdatabase open.  If any such thing is
	 * going on while the user is closing the original dbp with a local
	 * dbenv, someone's already badly screwed up, so there's no reason
	 * to bother engineering around this possibility.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	db_ref = --dbenv->db_ref;
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);
	if (F_ISSET(dbenv, DB_ENV_DBLOCAL) && db_ref == 0 &&
	    (t_ret = __dbenv_close(dbenv, 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Free the database handle. */
	memset(dbp, CLEAR_BYTE, sizeof(*dbp));
	__os_free(dbenv, dbp);

	return (ret);
}

/*
 * __db_refresh --
 *	Refresh the DB structure, releasing any allocated resources.
 * This does most of the work of closing files now because refresh
 * is what is used during abort processing (since we can't destroy
 * the actual handle) and during abort processing, we may have a
 * fully opened handle.
 *
 * PUBLIC: int __db_refresh __P((DB *, DB_TXN *, u_int32_t, int *));
 */
int
__db_refresh(dbp, txn, flags, deferred_closep)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t flags;
	int *deferred_closep;
{
	DB *sdbp;
	DBC *dbc;
	DB_ENV *dbenv;
	DB_LOCKREQ lreq;
	DB_MPOOL *dbmp;
	int resync, ret, t_ret;

	ret = 0;

	dbenv = dbp->dbenv;

	/* If never opened, or not currently open, it's easy. */
	if (!F_ISSET(dbp, DB_AM_OPEN_CALLED))
		goto never_opened;

	/*
	 * If we have any secondary indices, disassociate them from us.
	 * We don't bother with the mutex here;  it only protects some
	 * of the ops that will make us core-dump mid-close anyway, and
	 * if you're trying to do something with a secondary *while* you're
	 * closing the primary, you deserve what you get.  The disassociation
	 * is mostly done just so we can close primaries and secondaries in
	 * any order--but within one thread of control.
	 */
	for (sdbp = LIST_FIRST(&dbp->s_secondaries);
	    sdbp != NULL; sdbp = LIST_NEXT(sdbp, s_links)) {
		LIST_REMOVE(sdbp, s_links);
		if ((t_ret = __db_disassociate(sdbp)) != 0 && ret == 0)
			ret = t_ret;
	}

	/*
	 * Sync the underlying access method.  Do before closing the cursors
	 * because DB->sync allocates cursors in order to write Recno backing
	 * source text files.
	 *
	 * Sync is slow on some systems, notably Solaris filesystems where the
	 * entire buffer cache is searched.  If we're in recovery, don't flush
	 * the file, it's not necessary.
	 */
	if (!LF_ISSET(DB_NOSYNC) &&
	    !F_ISSET(dbp, DB_AM_DISCARD | DB_AM_RECOVER) &&
	    (t_ret = __db_sync(dbp)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Go through the active cursors and call the cursor recycle routine,
	 * which resolves pending operations and moves the cursors onto the
	 * free list.  Then, walk the free list and call the cursor destroy
	 * routine.  Note that any failure on a close is considered "really
	 * bad" and we just break out of the loop and force forward.
	 */
	resync = TAILQ_FIRST(&dbp->active_queue) == NULL ? 0 : 1;
	while ((dbc = TAILQ_FIRST(&dbp->active_queue)) != NULL)
		if ((t_ret = __db_c_close(dbc)) != 0) {
			if (ret == 0)
				ret = t_ret;
			break;
		}

	while ((dbc = TAILQ_FIRST(&dbp->free_queue)) != NULL)
		if ((t_ret = __db_c_destroy(dbc)) != 0) {
			if (ret == 0)
				ret = t_ret;
			break;
		}

	/*
	 * Close any outstanding join cursors.  Join cursors destroy themselves
	 * on close and have no separate destroy routine.  We don't have to set
	 * the resync flag here, because join cursors aren't write cursors.
	 */
	while ((dbc = TAILQ_FIRST(&dbp->join_queue)) != NULL)
		if ((t_ret = __db_join_close(dbc)) != 0) {
			if (ret == 0)
				ret = t_ret;
			break;
		}

	/*
	 * Sync the memory pool, even though we've already called DB->sync,
	 * because closing cursors can dirty pages by deleting items they
	 * referenced.
	 *
	 * Sync is slow on some systems, notably Solaris filesystems where the
	 * entire buffer cache is searched.  If we're in recovery, don't flush
	 * the file, it's not necessary.
	 */
	if (resync && !LF_ISSET(DB_NOSYNC) &&
	    !F_ISSET(dbp, DB_AM_DISCARD | DB_AM_RECOVER) &&
	    (t_ret = __memp_fsync(dbp->mpf)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * At this point, we haven't done anything to render the DB
	 * handle unusable, at least by a transaction abort.  Take the
	 * opportunity now to log the file close.  If this log fails
	 * and we're in a transaction, we have to bail out of the attempted
	 * close; we'll need a dbp in order to successfully abort the
	 * transaction, and we can't conjure a new one up because we haven't
	 * gotten out the dbreg_register record that represents the close.
	 * In this case, we put off actually closing the dbp until we've
	 * performed the abort.
	 */
	if (LOGGING_ON(dbp->dbenv)) {
		/*
		 * Discard the log file id, if any.  We want to log the close
		 * if and only if this is not a recovery dbp.
		 */
		if (F_ISSET(dbp, DB_AM_RECOVER))
			t_ret = __dbreg_revoke_id(dbp, 0, DB_LOGFILEID_INVALID);
		else {
			if ((t_ret = __dbreg_close_id(dbp,
			    txn, DBREG_CLOSE)) != 0 && txn != NULL) {
				/*
				 * We're in a txn and the attempt to log the
				 * close failed;  let the txn subsystem know
				 * that we need to destroy this dbp once we're
				 * done with the abort, then bail from the
				 * close.
				 *
				 * Note that if the attempt to put off the
				 * close -also- fails--which it won't unless
				 * we're out of heap memory--we're really
				 * screwed.  Panic.
				 */
				if ((ret =
				    __txn_closeevent(dbenv, txn, dbp)) != 0)
					return (__db_panic(dbenv, ret));
				if (deferred_closep != NULL)
					*deferred_closep = 1;
				return (t_ret);
			}
		}

		if (ret == 0)
			ret = t_ret;

		/* Discard the log FNAME. */
		if ((t_ret = __dbreg_teardown(dbp)) != 0 && ret == 0)
			ret = t_ret;
	}

	/* Close any handle we've been holding since the open.  */
	if (dbp->saved_open_fhp != NULL &&
	    (t_ret = __os_closehandle(dbenv, dbp->saved_open_fhp)) != 0 &&
	    ret == 0)
		ret = t_ret;

never_opened:
	/*
	 * Remove this DB handle from the DB_ENV's dblist, if it's been added.
	 *
	 * Close our reference to the underlying cache while locked, we don't
	 * want to race with a thread searching for our underlying cache link
	 * while opening a DB handle.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	if (dbp->dblistlinks.le_prev != NULL) {
		LIST_REMOVE(dbp, dblistlinks);
		dbp->dblistlinks.le_prev = NULL;
	}

	/* Close the memory pool file handle. */
	if (dbp->mpf != NULL) {
		if ((t_ret = __memp_fclose(dbp->mpf,
		    F_ISSET(dbp, DB_AM_DISCARD) ? DB_MPOOL_DISCARD : 0)) != 0 &&
		    ret == 0)
			ret = t_ret;
		dbp->mpf = NULL;
	}

	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	/*
	 * Call the access specific close function.
	 *
	 * We do this here rather than in __db_close as we need to do this when
	 * aborting an open so that file descriptors are closed and abort of
	 * renames can succeed on platforms that lock open files (such as
	 * Windows).  In particular, we need to ensure that all the extents
	 * associated with a queue are closed so that queue renames can be
	 * aborted.
	 *
	 * It is also important that we do this before releasing the handle
	 * lock, because dbremove and dbrename assume that once they have the
	 * handle lock, it is safe to modify the underlying file(s).
	 *
	 * !!!
	 * Because of where these functions are called in the DB handle close
	 * process, these routines can't do anything that would dirty pages or
	 * otherwise affect closing down the database.  Specifically, we can't
	 * abort and recover any of the information they control.
	 */
	if ((t_ret = __bam_db_close(dbp)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __ham_db_close(dbp)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __qam_db_close(dbp, dbp->flags)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * !!!
	 * At this point, the access-method specific information has been
	 * freed.  From now on, we can use the dbp, but not touch any
	 * access-method specific data.
	 */

	if (dbp->lid != DB_LOCK_INVALIDID) {
		/* We may have pending trade operations on this dbp. */
		if (txn != NULL)
			__txn_remlock(dbenv, txn, &dbp->handle_lock, dbp->lid);

		/* We may be holding the handle lock; release it. */
		lreq.op = DB_LOCK_PUT_ALL;
		lreq.obj = NULL;
		if ((t_ret = __lock_vec(dbenv,
		    dbp->lid, 0, &lreq, 1, NULL)) != 0 && ret == 0)
			ret = t_ret;

		if ((t_ret = __lock_id_free(dbenv, dbp->lid)) != 0 && ret == 0)
			ret = t_ret;
		dbp->lid = DB_LOCK_INVALIDID;
		LOCK_INIT(dbp->handle_lock);
	}

	/* Discard the locker ID allocated as the fileid. */
	if (F_ISSET(dbp, DB_AM_INMEM) && LOCKING_ON(dbenv) &&
	    (t_ret = __lock_id_free(dbenv, *(u_int32_t *)dbp->fileid)) != 0 &&
	    ret == 0)
		ret = t_ret;

	dbp->type = DB_UNKNOWN;

	/* Discard the thread mutex. */
	if (dbp->mutexp != NULL) {
		dbmp = dbenv->mp_handle;
		__db_mutex_free(dbenv, dbmp->reginfo, dbp->mutexp);
		dbp->mutexp = NULL;
	}

	/* Discard any memory allocated for the file and database names. */
	if (dbp->fname != NULL) {
		__os_free(dbp->dbenv, dbp->fname);
		dbp->fname = NULL;
	}
	if (dbp->dname != NULL) {
		__os_free(dbp->dbenv, dbp->dname);
		dbp->dname = NULL;
	}

	/* Discard any memory used to store returned data. */
	if (dbp->my_rskey.data != NULL)
		__os_free(dbp->dbenv, dbp->my_rskey.data);
	if (dbp->my_rkey.data != NULL)
		__os_free(dbp->dbenv, dbp->my_rkey.data);
	if (dbp->my_rdata.data != NULL)
		__os_free(dbp->dbenv, dbp->my_rdata.data);

	/* For safety's sake;  we may refresh twice. */
	memset(&dbp->my_rskey, 0, sizeof(DBT));
	memset(&dbp->my_rkey, 0, sizeof(DBT));
	memset(&dbp->my_rdata, 0, sizeof(DBT));

	/* Clear out fields that normally get set during open. */
	memset(dbp->fileid, 0, sizeof(dbp->fileid));
	dbp->adj_fileid = 0;
	dbp->meta_pgno = 0;
	dbp->cur_lid = DB_LOCK_INVALIDID;
	dbp->associate_lid = DB_LOCK_INVALIDID;
	dbp->cl_id = 0;
	dbp->open_flags = 0;

	/*
	 * If we are being refreshed with a txn specified, then we need
	 * to make sure that we clear out the lock handle field, because
	 * releasing all the locks for this transaction will release this
	 * lock and we don't want close to stumble upon this handle and
	 * try to close it.
	 */
	if (txn != NULL)
		LOCK_INIT(dbp->handle_lock);

	/* Reset flags to whatever the user configured. */
	dbp->flags = dbp->orig_flags;

	return (ret);
}

/*
 * __db_log_page
 *	Log a meta-data or root page during a subdatabase create operation.
 *
 * PUBLIC: int __db_log_page __P((DB *, DB_TXN *, DB_LSN *, db_pgno_t, PAGE *));
 */
int
__db_log_page(dbp, txn, lsn, pgno, page)
	DB *dbp;
	DB_TXN *txn;
	DB_LSN *lsn;
	db_pgno_t pgno;
	PAGE *page;
{
	DBT page_dbt;
	DB_LSN new_lsn;
	int ret;

	if (!LOGGING_ON(dbp->dbenv) || txn == NULL)
		return (0);

	memset(&page_dbt, 0, sizeof(page_dbt));
	page_dbt.size = dbp->pgsize;
	page_dbt.data = page;

	ret = __crdel_metasub_log(dbp, txn, &new_lsn, 0, pgno, &page_dbt, lsn);

	if (ret == 0)
		page->lsn = new_lsn;
	return (ret);
}

/*
 * __db_backup_name
 *	Create the backup file name for a given file.
 *
 * PUBLIC: int __db_backup_name __P((DB_ENV *,
 * PUBLIC:     const char *, DB_TXN *, char **));
 */
#undef	BACKUP_PREFIX
#define	BACKUP_PREFIX	"__db."

#undef	MAX_LSN_TO_TEXT
#define	MAX_LSN_TO_TEXT	17

int
__db_backup_name(dbenv, name, txn, backup)
	DB_ENV *dbenv;
	const char *name;
	DB_TXN *txn;
	char **backup;
{
	DB_LSN lsn;
	size_t len;
	int ret;
	char *p, *retp;

	/*
	 * Part of the name may be a full path, so we need to make sure that
	 * we allocate enough space for it, even in the case where we don't
	 * use the entire filename for the backup name.
	 */
	len = strlen(name) + strlen(BACKUP_PREFIX) + MAX_LSN_TO_TEXT;
	if ((ret = __os_malloc(dbenv, len, &retp)) != 0)
		return (ret);

	/*
	 * Create the name.  Backup file names are in one of two forms:
	 *
	 *	In a transactional env: __db.LSN(8).LSN(8)
	 * and
	 *	in a non-transactional env: __db.FILENAME
	 *
	 * If the transaction doesn't have a current LSN, we write a dummy
	 * log record to force it, so we ensure all tmp names are unique.
	 *
	 * In addition, the name passed may contain an env-relative path.
	 * In that case, put the __db. in the right place (in the last
	 * component of the pathname).
	 *
	 * There are four cases here:
	 *	1. simple path w/out transaction
	 *	2. simple path + transaction
	 *	3. multi-component path w/out transaction
	 *	4. multi-component path + transaction
	 */
	p = __db_rpath(name);
	if (txn == NULL)
		if (p == NULL)				/* Case 1. */
			snprintf(retp, len, "%s%s", BACKUP_PREFIX, name);
		else					/* Case 3. */
			snprintf(retp, len, "%.*s%s%s",
			    (int)(p - name) + 1, name, BACKUP_PREFIX, p + 1);
	else {
		if (IS_ZERO_LSN(txn->last_lsn)) {
			/*
			 * Write dummy log record.   The two choices for dummy
			 * log records are __db_noop_log and __db_debug_log;
			 * unfortunately __db_noop_log requires a valid dbp,
			 * and we aren't guaranteed to be able to pass one in
			 * here.
			 */
			if ((ret = __db_debug_log(dbenv,
			    txn, &lsn, 0, NULL, 0, NULL, NULL, 0)) != 0) {
				__os_free(dbenv, retp);
				return (ret);
			}
		} else
			lsn = txn->last_lsn;

		if (p == NULL)				/* Case 2. */
			snprintf(retp, len,
			    "%s%x.%x", BACKUP_PREFIX, lsn.file, lsn.offset);
		else					/* Case 4. */
			snprintf(retp, len, "%.*s%x.%x",
			    (int)(p - name) + 1, name, lsn.file, lsn.offset);
	}

	*backup = retp;
	return (0);
}

/*
 * __dblist_get --
 *	Get the first element of dbenv->dblist with
 *	dbp->adj_fileid matching adjid.
 *
 * PUBLIC: DB *__dblist_get __P((DB_ENV *, u_int32_t));
 */
DB *
__dblist_get(dbenv, adjid)
	DB_ENV *dbenv;
	u_int32_t adjid;
{
	DB *dbp;

	for (dbp = LIST_FIRST(&dbenv->dblist);
	    dbp != NULL && dbp->adj_fileid != adjid;
	    dbp = LIST_NEXT(dbp, dblistlinks))
		;

	return (dbp);
}

/*
 * __db_disassociate --
 *	Destroy the association between a given secondary and its primary.
 */
static int
__db_disassociate(sdbp)
	DB *sdbp;
{
	DBC *dbc;
	int ret, t_ret;

	ret = 0;

	sdbp->s_callback = NULL;
	sdbp->s_primary = NULL;
	sdbp->get = sdbp->stored_get;
	sdbp->close = sdbp->stored_close;

	/*
	 * Complain, but proceed, if we have any active cursors.  (We're in
	 * the middle of a close, so there's really no turning back.)
	 */
	if (sdbp->s_refcnt != 1 ||
	    TAILQ_FIRST(&sdbp->active_queue) != NULL ||
	    TAILQ_FIRST(&sdbp->join_queue) != NULL) {
		__db_err(sdbp->dbenv,
    "Closing a primary DB while a secondary DB has active cursors is unsafe");
		ret = EINVAL;
	}
	sdbp->s_refcnt = 0;

	while ((dbc = TAILQ_FIRST(&sdbp->free_queue)) != NULL)
		if ((t_ret = __db_c_destroy(dbc)) != 0 && ret == 0)
			ret = t_ret;

	F_CLR(sdbp, DB_AM_SECONDARY);
	return (ret);
}

#ifdef CONFIG_TEST
/*
 * __db_testcopy
 *	Create a copy of all backup files and our "main" DB.
 *
 * PUBLIC: #ifdef CONFIG_TEST
 * PUBLIC: int __db_testcopy __P((DB_ENV *, DB *, const char *));
 * PUBLIC: #endif
 */
int
__db_testcopy(dbenv, dbp, name)
	DB_ENV *dbenv;
	DB *dbp;
	const char *name;
{
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *mpf;

	DB_ASSERT(dbp != NULL || name != NULL);

	if (name == NULL) {
		dbmp = dbenv->mp_handle;
		mpf = dbp->mpf;
		name = R_ADDR(dbmp->reginfo, mpf->mfp->path_off);
	}

	if (dbp != NULL && dbp->type == DB_QUEUE)
		return (__qam_testdocopy(dbp, name));
	else
		return (__db_testdocopy(dbenv, name));
}

static int
__qam_testdocopy(dbp, name)
	DB *dbp;
	const char *name;
{
	QUEUE_FILELIST *filelist, *fp;
	char buf[256], *dir;
	int ret;

	filelist = NULL;
	if ((ret = __db_testdocopy(dbp->dbenv, name)) != 0)
		return (ret);
	if (dbp->mpf != NULL &&
	    (ret = __qam_gen_filelist(dbp, &filelist)) != 0)
		return (ret);

	if (filelist == NULL)
		return (0);
	dir = ((QUEUE *)dbp->q_internal)->dir;
	for (fp = filelist; fp->mpf != NULL; fp++) {
		snprintf(buf, sizeof(buf),
		    QUEUE_EXTENT, dir, PATH_SEPARATOR[0], name, fp->id);
		if ((ret = __db_testdocopy(dbp->dbenv, buf)) != 0)
			return (ret);
	}

	__os_free(dbp->dbenv, filelist);
	return (0);
}

/*
 * __db_testdocopy
 *	Create a copy of all backup files and our "main" DB.
 *
 */
static int
__db_testdocopy(dbenv, name)
	DB_ENV *dbenv;
	const char *name;
{
	size_t len;
	int dircnt, i, ret;
	char *backup, *copy, *dir, **namesp, *p, *real_name;

	dircnt = 0;
	copy = backup = NULL;
	namesp = NULL;

	/* Get the real backing file name. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		return (ret);

	/*
	 * Maximum size of file, including adding a ".afterop".
	 */
	len = strlen(real_name) + strlen(BACKUP_PREFIX) + MAX_LSN_TO_TEXT + 9;

	if ((ret = __os_malloc(dbenv, len, &copy)) != 0)
		goto err;

	if ((ret = __os_malloc(dbenv, len, &backup)) != 0)
		goto err;

	/*
	 * First copy the file itself.
	 */
	snprintf(copy, len, "%s.afterop", real_name);
	__db_makecopy(dbenv, real_name, copy);

	if ((ret = __os_strdup(dbenv, real_name, &dir)) != 0)
		goto err;
	__os_free(dbenv, real_name);
	real_name = NULL;

	/*
	 * Create the name.  Backup file names are of the form:
	 *
	 *	__db.name.0x[lsn-file].0x[lsn-offset]
	 *
	 * which guarantees uniqueness.  We want to look for the
	 * backup name, followed by a '.0x' (so that if they have
	 * files named, say, 'a' and 'abc' we won't match 'abc' when
	 * looking for 'a'.
	 */
	snprintf(backup, len, "%s%s.0x", BACKUP_PREFIX, name);

	/*
	 * We need the directory path to do the __os_dirlist.
	 */
	p = __db_rpath(dir);
	if (p != NULL)
		*p = '\0';
	ret = __os_dirlist(dbenv, dir, &namesp, &dircnt);
#if DIAGNOSTIC
	/*
	 * XXX
	 * To get the memory guard code to work because it uses strlen and we
	 * just moved the end of the string somewhere sooner.  This causes the
	 * guard code to fail because it looks at one byte past the end of the
	 * string.
	 */
	*p = '/';
#endif
	__os_free(dbenv, dir);
	if (ret != 0)
		goto err;
	for (i = 0; i < dircnt; i++) {
		/*
		 * Need to check if it is a backup file for this.
		 * No idea what namesp[i] may be or how long, so
		 * must use strncmp and not memcmp.  We don't want
		 * to use strcmp either because we are only matching
		 * the first part of the real file's name.  We don't
		 * know its LSN's.
		 */
		if (strncmp(namesp[i], backup, strlen(backup)) == 0) {
			if ((ret = __db_appname(dbenv, DB_APP_DATA,
			    namesp[i], 0, NULL, &real_name)) != 0)
				goto err;

			/*
			 * This should not happen.  Check that old
			 * .afterop files aren't around.
			 * If so, just move on.
			 */
			if (strstr(real_name, ".afterop") != NULL) {
				__os_free(dbenv, real_name);
				real_name = NULL;
				continue;
			}
			snprintf(copy, len, "%s.afterop", real_name);
			__db_makecopy(dbenv, real_name, copy);
			__os_free(dbenv, real_name);
			real_name = NULL;
		}
	}

err:	if (backup != NULL)
		__os_free(dbenv, backup);
	if (copy != NULL)
		__os_free(dbenv, copy);
	if (namesp != NULL)
		__os_dirfree(dbenv, namesp, dircnt);
	if (real_name != NULL)
		__os_free(dbenv, real_name);
	return (ret);
}

static void
__db_makecopy(dbenv, src, dest)
	DB_ENV *dbenv;
	const char *src, *dest;
{
	DB_FH *rfhp, *wfhp;
	size_t rcnt, wcnt;
	char *buf;

	rfhp = wfhp = NULL;

	if (__os_malloc(dbenv, 1024, &buf) != 0)
		return;

	if (__os_open(dbenv,
	    src, DB_OSO_RDONLY, __db_omode("rw----"), &rfhp) != 0)
		goto err;
	if (__os_open(dbenv, dest,
	    DB_OSO_CREATE | DB_OSO_TRUNC, __db_omode("rw----"), &wfhp) != 0)
		goto err;

	for (;;)
		if (__os_read(dbenv, rfhp, buf, 1024, &rcnt) < 0 || rcnt == 0 ||
		    __os_write(dbenv, wfhp, buf, rcnt, &wcnt) < 0)
			break;

err:	if (buf != NULL)
		__os_free(dbenv, buf);
	if (rfhp != NULL)
		(void)__os_closehandle(dbenv, rfhp);
	if (wfhp != NULL)
		(void)__os_closehandle(dbenv, wfhp);
}
#endif
