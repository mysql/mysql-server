/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
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
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db.c,v 11.117 2001/01/11 18:19:50 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "db_swap.h"
#include "btree.h"
#include "db_am.h"
#include "hash.h"
#include "lock.h"
#include "log.h"
#include "mp.h"
#include "qam.h"
#include "common_ext.h"

/* Actions that __db_master_update can take. */
typedef enum { MU_REMOVE, MU_RENAME, MU_OPEN } mu_action;

/* Flag values that __db_file_setup can return. */
#define	DB_FILE_SETUP_CREATE	0x01
#define	DB_FILE_SETUP_ZERO	0x02

static int __db_file_setup __P((DB *,
	       const char *, u_int32_t, int, db_pgno_t, int *));
static int __db_master_update __P((DB *,
	       const char *, u_int32_t,
	       db_pgno_t *, mu_action, const char *, u_int32_t));
static int __db_refresh __P((DB *));
static int __db_remove_callback __P((DB *, void *));
static int __db_set_pgsize __P((DB *, DB_FH *, char *));
static int __db_subdb_remove __P((DB *, const char *, const char *));
static int __db_subdb_rename __P(( DB *,
		const char *, const char *, const char *));
#if     CONFIG_TEST
static void __db_makecopy __P((const char *, const char *));
static int __db_testdocopy __P((DB *, const char *));
static int __qam_testdocopy __P((DB *, const char *));
#endif

/*
 * __db_open --
 *	Main library interface to the DB access methods.
 *
 * PUBLIC: int __db_open __P((DB *,
 * PUBLIC:     const char *, const char *, DBTYPE, u_int32_t, int));
 */
int
__db_open(dbp, name, subdb, type, flags, mode)
	DB *dbp;
	const char *name, *subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	DB_ENV *dbenv;
	DB_LOCK open_lock;
	DB *mdbp;
	db_pgno_t meta_pgno;
	u_int32_t ok_flags;
	int ret, t_ret;

	dbenv = dbp->dbenv;
	mdbp = NULL;

	/* Validate arguments. */
#define	OKFLAGS								\
    (DB_CREATE | DB_EXCL | DB_FCNTL_LOCKING |				\
    DB_NOMMAP | DB_RDONLY | DB_RDWRMASTER | DB_THREAD | DB_TRUNCATE)
	if ((ret = __db_fchk(dbenv, "DB->open", flags, OKFLAGS)) != 0)
		return (ret);
	if (LF_ISSET(DB_EXCL) && !LF_ISSET(DB_CREATE))
		return (__db_ferr(dbenv, "DB->open", 1));
	if (LF_ISSET(DB_RDONLY) && LF_ISSET(DB_CREATE))
		return (__db_ferr(dbenv, "DB->open", 1));
#ifdef	HAVE_VXWORKS
	if (LF_ISSET(DB_TRUNCATE)) {
		__db_err(dbenv, "DB_TRUNCATE unsupported in VxWorks");
		return (__db_eopnotsup(dbenv));
	}
#endif
	switch (type) {
	case DB_UNKNOWN:
		if (LF_ISSET(DB_CREATE|DB_TRUNCATE)) {
			__db_err(dbenv,
	    "%s: DB_UNKNOWN type specified with DB_CREATE or DB_TRUNCATE",
			    name);
			return (EINVAL);
		}
		ok_flags = 0;
		break;
	case DB_BTREE:
		ok_flags = DB_OK_BTREE;
		break;
	case DB_HASH:
		ok_flags = DB_OK_HASH;
		break;
	case DB_QUEUE:
		ok_flags = DB_OK_QUEUE;
		break;
	case DB_RECNO:
		ok_flags = DB_OK_RECNO;
		break;
	default:
		__db_err(dbenv, "unknown type: %lu", (u_long)type);
		return (EINVAL);
	}
	if (ok_flags)
		DB_ILLEGAL_METHOD(dbp, ok_flags);

	/* The environment may have been created, but never opened. */
	if (!F_ISSET(dbenv, DB_ENV_DBLOCAL | DB_ENV_OPEN_CALLED)) {
		__db_err(dbenv, "environment not yet opened");
		return (EINVAL);
	}

	/*
	 * Historically, you could pass in an environment that didn't have a
	 * mpool, and DB would create a private one behind the scenes.  This
	 * no longer works.
	 */
	if (!F_ISSET(dbenv, DB_ENV_DBLOCAL) && !MPOOL_ON(dbenv)) {
		__db_err(dbenv, "environment did not include a memory pool.");
		return (EINVAL);
	}

	/*
	 * You can't specify threads during DB->open if subsystems in the
	 * environment weren't configured with them.
	 */
	if (LF_ISSET(DB_THREAD) &&
	    !F_ISSET(dbenv, DB_ENV_DBLOCAL | DB_ENV_THREAD)) {
		__db_err(dbenv, "environment not created using DB_THREAD");
		return (EINVAL);
	}

	/*
	 * If the environment was configured with threads, the DB handle
	 * must also be free-threaded, so we force the DB_THREAD flag on.
	 * (See SR #2033 for why this is a requirement--recovery needs
	 * to be able to grab a dbp using __db_fileid_to_dbp, and it has
	 * no way of knowing which dbp goes with which thread, so whichever
	 * one it finds has to be usable in any of them.)
	 */
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		LF_SET(DB_THREAD);

	/* DB_TRUNCATE is not transaction recoverable. */
	if (LF_ISSET(DB_TRUNCATE) && TXN_ON(dbenv)) {
		__db_err(dbenv,
	    "DB_TRUNCATE illegal in a transaction protected environment");
		return (EINVAL);
	}

	/* Subdatabase checks. */
	if (subdb != NULL) {
		/* Subdatabases must be created in named files. */
		if (name == NULL) {
			__db_err(dbenv,
		    "multiple databases cannot be created in temporary files");
			return (EINVAL);
		}

		/* QAM can't be done as a subdatabase. */
		if (type == DB_QUEUE) {
			__db_err(dbenv, "Queue databases must be one-per-file");
			return (EINVAL);
		}
	}

	/* Convert any DB->open flags. */
	if (LF_ISSET(DB_RDONLY))
		F_SET(dbp, DB_AM_RDONLY);

	/* Fill in the type. */
	dbp->type = type;

	/*
	 * If we're potentially creating a database, wrap the open inside of
	 * a transaction.
	 */
	if (TXN_ON(dbenv) && LF_ISSET(DB_CREATE))
		if ((ret = __db_metabegin(dbp, &open_lock)) != 0)
			return (ret);

	/*
	 * If we're opening a subdatabase, we have to open (and potentially
	 * create) the main database, and then get (and potentially store)
	 * our base page number in that database.  Then, we can finally open
	 * the subdatabase.
	 */
	if (subdb == NULL)
		meta_pgno = PGNO_BASE_MD;
	else {
		/*
		 * Open the master database, optionally creating or updating
		 * it, and retrieve the metadata page number.
		 */
		if ((ret =
		    __db_master_open(dbp, name, flags, mode, &mdbp)) != 0)
			goto err;

		/* Copy the page size and file id from the master. */
		dbp->pgsize = mdbp->pgsize;
		F_SET(dbp, DB_AM_SUBDB);
		memcpy(dbp->fileid, mdbp->fileid, DB_FILE_ID_LEN);

		if ((ret = __db_master_update(mdbp,
		    subdb, type, &meta_pgno, MU_OPEN, NULL, flags)) != 0)
			goto err;

		/*
		 * Clear the exclusive open and truncation flags, they only
		 * apply to the open of the master database.
		 */
		LF_CLR(DB_EXCL | DB_TRUNCATE);
	}

	ret = __db_dbopen(dbp, name, flags, mode, meta_pgno);

	/*
	 * You can open the database that describes the subdatabases in the
	 * rest of the file read-only.  The content of each key's data is
	 * unspecified and applications should never be adding new records
	 * or updating existing records.  However, during recovery, we need
	 * to open these databases R/W so we can redo/undo changes in them.
	 * Likewise, we need to open master databases read/write during
	 * rename and remove so we can be sure they're fully sync'ed, so
	 * we provide an override flag for the purpose.
	 */
	if (subdb == NULL && !IS_RECOVERING(dbenv) && !LF_ISSET(DB_RDONLY) &&
	    !LF_ISSET(DB_RDWRMASTER) && F_ISSET(dbp, DB_AM_SUBDB)) {
		__db_err(dbenv,
    "files containing multiple databases may only be opened read-only");
		ret = EINVAL;
		goto err;
	}

err:	/*
	 * End any transaction, committing if we were successful, aborting
	 * otherwise.
	 */
	if (TXN_ON(dbenv) && LF_ISSET(DB_CREATE))
		if ((t_ret = __db_metaend(dbp,
		    &open_lock, ret == 0, NULL, NULL)) != 0 && ret == 0)
			ret = t_ret;

	/* If we were successful, don't discard the file on close. */
	if (ret == 0)
		F_CLR(dbp, DB_AM_DISCARD);

	/* If we were unsuccessful, destroy the DB handle. */
	if (ret != 0) {
		/* In recovery we set log_fileid early. */
		if (IS_RECOVERING(dbenv))
			dbp->log_fileid = DB_LOGFILEID_INVALID;
		__db_refresh(dbp);
	}

	if (mdbp != NULL) {
		/* If we were successful, don't discard the file on close. */
		if (ret == 0)
			F_CLR(mdbp, DB_AM_DISCARD);
		if ((t_ret = mdbp->close(mdbp, 0)) != 0 && ret == 0)
			ret = t_ret;
	}

	return (ret);
}

/*
 * __db_dbopen --
 *	Open a database.
 * PUBLIC: int __db_dbopen __P((DB *, const char *, u_int32_t, int, db_pgno_t));
 */
int
__db_dbopen(dbp, name, flags, mode, meta_pgno)
	DB *dbp;
	const char *name;
	u_int32_t flags;
	int mode;
	db_pgno_t meta_pgno;
{
	DB_ENV *dbenv;
	int ret, retinfo;

	dbenv = dbp->dbenv;

	/* Set up the underlying file. */
	if ((ret = __db_file_setup(dbp,
	    name, flags, mode, meta_pgno, &retinfo)) != 0)
		return (ret);

	/*
	 * If we created the file, set the truncate flag for the mpool.  This
	 * isn't for anything we've done, it's protection against stupid user
	 * tricks: if the user deleted a file behind Berkeley DB's back, we
	 * may still have pages in the mpool that match the file's "unique" ID.
	 */
	if (retinfo & DB_FILE_SETUP_CREATE)
		flags |= DB_TRUNCATE;

	/* Set up the underlying environment. */
	if ((ret = __db_dbenv_setup(dbp, name, flags)) != 0)
		return (ret);

	/*
	 * Do access method specific initialization.
	 *
	 * !!!
	 * Set the open flag.  (The underlying access method open functions
	 * may want to do things like acquire cursors, so the open flag has
	 * to be set before calling them.)
	 */
	F_SET(dbp, DB_OPEN_CALLED);

	if (retinfo & DB_FILE_SETUP_ZERO)
		return (0);

	switch (dbp->type) {
	case DB_BTREE:
		ret = __bam_open(dbp, name, meta_pgno, flags);
		break;
	case DB_HASH:
		ret = __ham_open(dbp, name, meta_pgno, flags);
		break;
	case DB_RECNO:
		ret = __ram_open(dbp, name, meta_pgno, flags);
		break;
	case DB_QUEUE:
		ret = __qam_open(dbp, name, meta_pgno, mode, flags);
		break;
	case DB_UNKNOWN:
		return (__db_unknown_type(dbp->dbenv,
		     "__db_dbopen", dbp->type));
		break;
	}
	return (ret);
}

/*
 * __db_master_open --
 *	Open up a handle on a master database.
 *
 * PUBLIC: int __db_master_open __P((DB *,
 * PUBLIC:     const char *, u_int32_t, int, DB **));
 */
int
__db_master_open(subdbp, name, flags, mode, dbpp)
	DB *subdbp;
	const char *name;
	u_int32_t flags;
	int mode;
	DB **dbpp;
{
	DB *dbp;
	int ret;

	/* Open up a handle on the main database. */
	if ((ret = db_create(&dbp, subdbp->dbenv, 0)) != 0)
		return (ret);

	/*
	 * It's always a btree.
	 * Run in the transaction we've created.
	 * Set the pagesize in case we're creating a new database.
	 * Flag that we're creating a database with subdatabases.
	 */
	dbp->type = DB_BTREE;
	dbp->open_txn = subdbp->open_txn;
	dbp->pgsize = subdbp->pgsize;
	F_SET(dbp, DB_AM_SUBDB);

	if ((ret = __db_dbopen(dbp, name, flags, mode, PGNO_BASE_MD)) != 0) {
		if (!F_ISSET(dbp, DB_AM_DISCARD))
			dbp->close(dbp, 0);
		return (ret);
	}

	*dbpp = dbp;
	return (0);
}

/*
 * __db_master_update --
 *	Add/Remove a subdatabase from a master database.
 */
static int
__db_master_update(mdbp, subdb, type, meta_pgnop, action, newname, flags)
	DB *mdbp;
	const char *subdb;
	u_int32_t type;
	db_pgno_t *meta_pgnop;		/* may be NULL on MU_RENAME */
	mu_action action;
	const char *newname;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *dbc, *ndbc;
	DBT key, data, ndata;
	PAGE *p;
	db_pgno_t t_pgno;
	int modify, ret, t_ret;

	dbenv = mdbp->dbenv;
	dbc = ndbc = NULL;
	p = NULL;

	/* Might we modify the master database?  If so, we'll need to lock. */
	modify = (action != MU_OPEN || LF_ISSET(DB_CREATE)) ? 1 : 0;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/*
	 * Open up a cursor.  If this is CDB and we're creating the database,
	 * make it an update cursor.
	 */
	if ((ret = mdbp->cursor(mdbp, mdbp->open_txn, &dbc,
	    (CDB_LOCKING(dbenv) && modify) ? DB_WRITECURSOR : 0)) != 0)
		goto err;

	/*
	 * Try to point the cursor at the record.
	 *
	 * If we're removing or potentially creating an entry, lock the page
	 * with DB_RMW.
	 *
	 * !!!
	 * We don't include the name's nul termination in the database.
	 */
	key.data = (char *)subdb;
	key.size = strlen(subdb);
	/* In the rename case, we do multiple cursor ops, so MALLOC is safer. */
	F_SET(&data, DB_DBT_MALLOC);
	ret = dbc->c_get(dbc, &key, &data,
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
		if ((ret = dbc->c_del(dbc, 0)) != 0)
			goto err;

		/*
		 * We're handling actual data, not on-page meta-data,
		 * so it hasn't been converted to/from opposite
		 * endian architectures.  Do it explicitly, now.
		 */
		memcpy(meta_pgnop, data.data, sizeof(db_pgno_t));
		DB_NTOHL(meta_pgnop);
		if ((ret = memp_fget(mdbp->mpf, meta_pgnop, 0, &p)) != 0)
			goto err;

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
		if ((ret = mdbp->cursor(mdbp, mdbp->open_txn, &ndbc, 0)) != 0)
			goto err;
		DB_ASSERT(newname != NULL);
		key.data = (void *) newname;
		key.size = strlen(newname);

		/*
		 * We don't actually care what the meta page of the potentially-
		 * overwritten DB is;  we just care about existence.
		 */
		memset(&ndata, 0, sizeof(ndata));
		F_SET(&ndata, DB_DBT_USERMEM | DB_DBT_PARTIAL);

		if ((ret = ndbc->c_get(ndbc, &key, &ndata, DB_SET)) == 0) {
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
		if ((ret = ndbc->c_put(ndbc, &key, &data, DB_KEYFIRST)) != 0)
			goto err;
		if ((ret = dbc->c_del(dbc, 0)) != 0) {
			/*
			 * If the delete fails, try to delete the record
			 * we just put, in case we're not txn-protected.
			 */
			(void)ndbc->c_del(ndbc, 0);
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
			memcpy(meta_pgnop, data.data, sizeof(db_pgno_t));
			DB_NTOHL(meta_pgnop);
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

		if ((ret = __db_new(dbc,
		    type == DB_HASH ? P_HASHMETA : P_BTREEMETA, &p)) != 0)
			goto err;
		*meta_pgnop = PGNO(p);

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
		if ((ret = dbc->c_put(dbc, &key, &ndata, DB_KEYLAST)) != 0)
			goto err;
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
			    memp_fput(mdbp->mpf, p, DB_MPOOL_DIRTY)) != 0)
				ret = t_ret;
			/*
			 * Since we cannot close this file until after
			 * transaction commit, we need to sync the dirty
			 * pages, because we'll read these directly from
			 * disk to open.
			 */
			if ((t_ret = mdbp->sync(mdbp, 0)) != 0 && ret == 0)
				ret = t_ret;
		} else
			(void)__db_free(dbc, p);
	}

	/* Discard the cursor(s) and data. */
	if (data.data != NULL)
		__os_free(data.data, data.size);
	if (dbc != NULL && (t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	if (ndbc != NULL && (t_ret = ndbc->c_close(ndbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_dbenv_setup --
 *	Set up the underlying environment during a db_open.
 *
 * PUBLIC: int __db_dbenv_setup __P((DB *, const char *, u_int32_t));
 */
int
__db_dbenv_setup(dbp, name, flags)
	DB *dbp;
	const char *name;
	u_int32_t flags;
{
	DB *ldbp;
	DB_ENV *dbenv;
	DBT pgcookie;
	DB_MPOOL_FINFO finfo;
	DB_PGINFO pginfo;
	int ret;
	u_int32_t maxid;

	dbenv = dbp->dbenv;

	/* If we don't yet have an environment, it's time to create it. */
	if (!F_ISSET(dbenv, DB_ENV_OPEN_CALLED)) {
		/* Make sure we have at least DB_MINCACHE pages in our cache. */
		if (dbenv->mp_gbytes == 0 &&
		    dbenv->mp_bytes < dbp->pgsize * DB_MINPAGECACHE &&
		    (ret = dbenv->set_cachesize(
		    dbenv, 0, dbp->pgsize * DB_MINPAGECACHE, 0)) != 0)
			return (ret);

		if ((ret = dbenv->open(dbenv, NULL, DB_CREATE |
		    DB_INIT_MPOOL | DB_PRIVATE | LF_ISSET(DB_THREAD), 0)) != 0)
			return (ret);
	}

	/* Register DB's pgin/pgout functions. */
	if ((ret =
	    memp_register(dbenv, DB_FTYPE_SET, __db_pgin, __db_pgout)) != 0)
		return (ret);

	/*
	 * Open a backing file in the memory pool.
	 *
	 * If we need to pre- or post-process a file's pages on I/O, set the
	 * file type.  If it's a hash file, always call the pgin and pgout
	 * routines.  This means that hash files can never be mapped into
	 * process memory.  If it's a btree file and requires swapping, we
	 * need to page the file in and out.  This has to be right -- we can't
	 * mmap files that are being paged in and out.
	 */
	memset(&finfo, 0, sizeof(finfo));
	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		finfo.ftype =
		    F_ISSET(dbp, DB_AM_SWAP) ? DB_FTYPE_SET : DB_FTYPE_NOTSET;
		finfo.clear_len = DB_PAGE_DB_LEN;
		break;
	case DB_HASH:
		finfo.ftype = DB_FTYPE_SET;
		finfo.clear_len = DB_PAGE_DB_LEN;
		break;
	case DB_QUEUE:
		finfo.ftype =
		    F_ISSET(dbp, DB_AM_SWAP) ? DB_FTYPE_SET : DB_FTYPE_NOTSET;
		finfo.clear_len = DB_PAGE_QUEUE_LEN;
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
			finfo.ftype = DB_FTYPE_NOTSET;
			finfo.clear_len = DB_PAGE_DB_LEN;
			break;
		}
		return (__db_unknown_type(dbp->dbenv,
		     "__db_dbenv_setup", dbp->type));
	}
	finfo.pgcookie = &pgcookie;
	finfo.fileid = dbp->fileid;
	finfo.lsn_offset = 0;

	pginfo.db_pagesize = dbp->pgsize;
	pginfo.needswap = F_ISSET(dbp, DB_AM_SWAP);
	pgcookie.data = &pginfo;
	pgcookie.size = sizeof(DB_PGINFO);

	if ((ret = memp_fopen(dbenv, name,
	    LF_ISSET(DB_RDONLY | DB_NOMMAP | DB_ODDFILESIZE | DB_TRUNCATE),
	    0, dbp->pgsize, &finfo, &dbp->mpf)) != 0)
		return (ret);

	/*
	 * We may need a per-thread mutex.  Allocate it from the environment
	 * region, there's supposed to be extra space there for that purpose.
	 */
	if (LF_ISSET(DB_THREAD)) {
		if ((ret = __db_mutex_alloc(
		    dbenv, dbenv->reginfo, (MUTEX **)&dbp->mutexp)) != 0)
			return (ret);
		if ((ret = __db_mutex_init(
		    dbenv, dbp->mutexp, 0, MUTEX_THREAD)) != 0) {
			__db_mutex_free(dbenv, dbenv->reginfo, dbp->mutexp);
			return (ret);
		}
	}

	/* Get a log file id. */
	if (LOGGING_ON(dbenv) && !IS_RECOVERING(dbenv) &&
#if !defined(DEBUG_ROP)
	    !F_ISSET(dbp, DB_AM_RDONLY) &&
#endif
	    (ret = log_register(dbenv, dbp, name)) != 0)
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
	    ldbp != NULL; ldbp = LIST_NEXT(dbp, dblistlinks)) {
		if (name != NULL &&
		    memcmp(ldbp->fileid, dbp->fileid, DB_FILE_ID_LEN) == 0 &&
		    ldbp->meta_pgno == dbp->meta_pgno)
			break;
		if (ldbp->adj_fileid > maxid)
			maxid = ldbp->adj_fileid;
	}

	/*
	 * If ldbp is NULL, we didn't find a match, or we weren't
	 * really looking because name is NULL.  Assign the dbp an
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
 * __db_file_setup --
 *	Setup the file or in-memory data.
 *	Read the database metadata and resolve it with our arguments.
 */
static int
__db_file_setup(dbp, name, flags, mode, meta_pgno, retflags)
	DB *dbp;
	const char *name;
	u_int32_t flags;
	int mode;
	db_pgno_t meta_pgno;
	int *retflags;
{
	DB *mdb;
	DBT namedbt;
	DB_ENV *dbenv;
	DB_FH *fhp, fh;
	DB_LSN lsn;
	DB_TXN *txn;
	size_t nr;
	u_int32_t magic, oflags;
	int ret, retry_cnt, t_ret;
	char *real_name, mbuf[DBMETASIZE];

#define	IS_SUBDB_SETUP	(meta_pgno != PGNO_BASE_MD)

	dbenv = dbp->dbenv;
	dbp->meta_pgno = meta_pgno;
	txn = NULL;
	*retflags = 0;

	/*
	 * If we open a file handle and our caller is doing fcntl(2) locking,
	 * we can't close it because that would discard the caller's lock.
	 * Save it until we close the DB handle.
	 */
	if (LF_ISSET(DB_FCNTL_LOCKING)) {
		if ((ret = __os_malloc(dbenv, sizeof(*fhp), NULL, &fhp)) != 0)
			return (ret);
	} else
		fhp = &fh;
	memset(fhp, 0, sizeof(*fhp));

	/*
	 * If the file is in-memory, set up is simple.  Otherwise, do the
	 * hard work of opening and reading the file.
	 *
	 * If we have a file name, try and read the first page, figure out
	 * what type of file it is, and initialize everything we can based
	 * on that file's meta-data page.
	 *
	 * !!!
	 * There's a reason we don't push this code down into the buffer cache.
	 * The problem is that there's no information external to the file that
	 * we can use as a unique ID.  UNIX has dev/inode pairs, but they are
	 * not necessarily unique after reboot, if the file was mounted via NFS.
	 * Windows has similar problems, as the FAT filesystem doesn't maintain
	 * dev/inode numbers across reboot.  So, we must get something from the
	 * file we can use to ensure that, even after a reboot, the file we're
	 * joining in the cache is the right file for us to join.  The solution
	 * we use is to maintain a file ID that's stored in the database, and
	 * that's why we have to open and read the file before calling into the
	 * buffer cache.
	 *
	 * The secondary reason is that there's additional information that
	 * we want to have before instantiating a file in the buffer cache:
	 * the page size, file type (btree/hash), if swapping is required,
	 * and flags (DB_RDONLY, DB_CREATE, DB_TRUNCATE).  We could handle
	 * needing this information by allowing it to be set for a file in
	 * the buffer cache even after the file has been opened, and, of
	 * course, supporting the ability to flush a file from the cache as
	 * necessary, e.g., if we guessed wrongly about the page size.  Given
	 * that we have to read the file anyway to get the file ID, we might
	 * as well get the rest, too.
	 *
	 * Get the real file name.
	 */
	if (name == NULL) {
		F_SET(dbp, DB_AM_INMEM);

		if (dbp->type == DB_UNKNOWN) {
			__db_err(dbenv,
			    "DBTYPE of unknown without existing file");
			return (EINVAL);
		}
		real_name = NULL;

		/* Set the page size if we don't have one yet. */
		if (dbp->pgsize == 0)
			dbp->pgsize = DB_DEF_IOSIZE;

		/*
		 * If the file is a temporary file and we're doing locking,
		 * then we have to create a unique file ID.  We can't use our
		 * normal dev/inode pair (or whatever this OS uses in place of
		 * dev/inode pairs) because no backing file will be created
		 * until the mpool cache is filled forcing the buffers to disk.
		 * Grab a random locker ID to use as a file ID.  The created
		 * ID must never match a potential real file ID -- we know it
		 * won't because real file IDs contain a time stamp after the
		 * dev/inode pair, and we're simply storing a 4-byte value.
		 *
		 * !!!
		 * Store the locker in the file id structure -- we can get it
		 * from there as necessary, and it saves having two copies.
		 */
		if (LOCKING_ON(dbenv) &&
		    (ret = lock_id(dbenv, (u_int32_t *)dbp->fileid)) != 0)
			return (ret);

		return (0);
	}

	/* Get the real backing file name. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, NULL, name, 0, NULL, &real_name)) != 0)
		return (ret);

	/*
	 * Open the backing file.  We need to make sure that multiple processes
	 * attempting to create the file at the same time are properly ordered
	 * so that only one of them creates the "unique" file ID, so we open it
	 * O_EXCL and O_CREAT so two simultaneous attempts to create the region
	 * will return failure in one of the attempts.  If we're the one that
	 * fails, simply retry without the O_CREAT flag, which will require the
	 * meta-data page exist.
	 */

	/* Fill in the default file mode. */
	if (mode == 0)
		mode = __db_omode("rwrw--");

	oflags = 0;
	if (LF_ISSET(DB_RDONLY))
		oflags |= DB_OSO_RDONLY;
	if (LF_ISSET(DB_TRUNCATE))
		oflags |= DB_OSO_TRUNC;

	retry_cnt = 0;
open_retry:
	*retflags = 0;
	ret = 0;
	if (!IS_SUBDB_SETUP && LF_ISSET(DB_CREATE)) {
		if (dbp->open_txn != NULL) {
			/*
			 * Start a child transaction to wrap this individual
			 * create.
			 */
			if ((ret =
			    txn_begin(dbenv, dbp->open_txn, &txn, 0)) != 0)
				goto err_msg;

			memset(&namedbt, 0, sizeof(namedbt));
			namedbt.data = (char *)name;
			namedbt.size = strlen(name) + 1;
			if ((ret = __crdel_fileopen_log(dbenv, txn,
			    &lsn, DB_FLUSH, &namedbt, mode)) != 0)
				goto err_msg;
		}
		DB_TEST_RECOVERY(dbp, DB_TEST_PREOPEN, ret, name);
		if ((ret = __os_open(dbenv, real_name,
		    oflags | DB_OSO_CREATE | DB_OSO_EXCL, mode, fhp)) == 0) {
			DB_TEST_RECOVERY(dbp, DB_TEST_POSTOPEN, ret, name);

			/* Commit the file create. */
			if (dbp->open_txn != NULL) {
				if ((ret = txn_commit(txn, DB_TXN_SYNC)) != 0)
					goto err_msg;
				txn = NULL;
			}

			/*
			 * We created the file.  This means that if we later
			 * fail, we need to delete the file and if we're going
			 * to do that, we need to trash any pages in the
			 * memory pool.  Since we only know here that we
			 * created the file, we're going to set the flag here
			 * and clear it later if we commit successfully.
			 */
			F_SET(dbp, DB_AM_DISCARD);
			*retflags |= DB_FILE_SETUP_CREATE;
		} else {
			/*
			 * Abort the file create.  If the abort fails, report
			 * the error returned by txn_abort(), rather than the
			 * open error, for no particular reason.
			 */
			if (dbp->open_txn != NULL) {
				if ((t_ret = txn_abort(txn)) != 0) {
					ret = t_ret;
					goto err_msg;
				}
				txn = NULL;
			}

			/*
			 * If we were not doing an exclusive open, try again
			 * without the create flag.
			 */
			if (ret == EEXIST && !LF_ISSET(DB_EXCL)) {
				LF_CLR(DB_CREATE);
				DB_TEST_RECOVERY(dbp,
				    DB_TEST_POSTOPEN, ret, name);
				goto open_retry;
			}
		}
	} else
		ret = __os_open(dbenv, real_name, oflags, mode, fhp);

	/*
	 * Be quiet if we couldn't open the file because it didn't exist
	 * or we did not have permission,
	 * the customers don't like those messages appearing in the logs.
	 * Otherwise, complain loudly.
	 */
	if (ret != 0) {
		if (ret == EACCES || ret == ENOENT)
			goto err;
		goto err_msg;
	}

	/* Set the page size if we don't have one yet. */
	if (dbp->pgsize == 0) {
		if (IS_SUBDB_SETUP) {
			if ((ret = __db_master_open(dbp,
			    name, flags, mode, &mdb)) != 0)
				goto err;
			dbp->pgsize = mdb->pgsize;
			(void)mdb->close(mdb, 0);
		} else if ((ret = __db_set_pgsize(dbp, fhp, real_name)) != 0)
			goto err;
	}

	/*
	 * Seek to the metadata offset; if it's a master database open or a
	 * database without subdatabases, we're seeking to 0, but that's OK.
	 */
	if ((ret = __os_seek(dbenv, fhp,
	    dbp->pgsize, meta_pgno, 0, 0, DB_OS_SEEK_SET)) != 0)
		goto err_msg;

	/*
	 * Read the metadata page.  We read DBMETASIZE bytes, which is larger
	 * than any access method's metadata page and smaller than any disk
	 * sector.
	 */
	if ((ret = __os_read(dbenv, fhp, mbuf, sizeof(mbuf), &nr)) != 0)
		goto err_msg;

	if (nr == sizeof(mbuf)) {
		/*
		 * Figure out what access method we're dealing with, and then
		 * call access method specific code to check error conditions
		 * based on conflicts between the found file and application
		 * arguments.  A found file overrides some user information --
		 * we don't consider it an error, for example, if the user set
		 * an expected byte order and the found file doesn't match it.
		 */
		F_CLR(dbp, DB_AM_SWAP);
		magic = ((DBMETA *)mbuf)->magic;

swap_retry:	switch (magic) {
		case DB_BTREEMAGIC:
			if ((ret =
			    __bam_metachk(dbp, name, (BTMETA *)mbuf)) != 0)
				goto err;
			break;
		case DB_HASHMAGIC:
			if ((ret =
			    __ham_metachk(dbp, name, (HMETA *)mbuf)) != 0)
				goto err;
			break;
		case DB_QAMMAGIC:
			if ((ret =
			    __qam_metachk(dbp, name, (QMETA *)mbuf)) != 0)
				goto err;
			break;
		case 0:
			/*
			 * There are two ways we can get a 0 magic number.
			 * If we're creating a subdatabase, then the magic
			 * number will be 0.  We allocate a page as part of
			 * finding out what the base page number will be for
			 * the new subdatabase, but it's not initialized in
			 * any way.
			 *
			 * The second case happens if we are in recovery
			 * and we are going to recreate a database, it's
			 * possible that it's page was created (on systems
			 * where pages must be created explicitly to avoid
			 * holes in files) but is still 0.
			 */
			if (IS_SUBDB_SETUP) {		/* Case 1 */
				if ((IS_RECOVERING(dbenv)
				    && F_ISSET((DB_LOG *)
				    dbenv->lg_handle, DBLOG_FORCE_OPEN))
				    || ((DBMETA *)mbuf)->pgno != PGNO_INVALID)
					goto empty;

				ret = EINVAL;
				goto err;
			}
							/* Case 2 */
			if (IS_RECOVERING(dbenv)) {
				*retflags |= DB_FILE_SETUP_ZERO;
				goto empty;
			}
			goto bad_format;
		default:
			if (F_ISSET(dbp, DB_AM_SWAP))
				goto bad_format;

			M_32_SWAP(magic);
			F_SET(dbp, DB_AM_SWAP);
			goto swap_retry;
		}
	} else {
		/*
		 * Only newly created files are permitted to fail magic
		 * number tests.
		 */
		if (nr != 0 || (!IS_RECOVERING(dbenv) && IS_SUBDB_SETUP))
			goto bad_format;

		/* Let the caller know that we had a 0-length file. */
		if (!LF_ISSET(DB_CREATE | DB_TRUNCATE))
			*retflags |= DB_FILE_SETUP_ZERO;

		/*
		 * The only way we can reach here with the DB_CREATE flag set
		 * is if we created the file.  If that's not the case, then
		 * either (a) someone else created the file but has not yet
		 * written out the metadata page, or (b) we truncated the file
		 * (DB_TRUNCATE) leaving it zero-length.  In the case of (a),
		 * we want to sleep and give the file creator time to write
		 * the metadata page.  In the case of (b), we want to continue.
		 *
		 * !!!
		 * There's a race in the case of two processes opening the file
		 * with the DB_TRUNCATE flag set at roughly the same time, and
		 * they could theoretically hurt each other.  Sure hope that's
		 * unlikely.
		 */
		if (!LF_ISSET(DB_CREATE | DB_TRUNCATE) &&
		    !IS_RECOVERING(dbenv)) {
			if (retry_cnt++ < 3) {
				__os_sleep(dbenv, 1, 0);
				goto open_retry;
			}
bad_format:		if (!IS_RECOVERING(dbenv))
				__db_err(dbenv,
				    "%s: unexpected file type or format", name);
			ret = EINVAL;
			goto err;
		}

		DB_ASSERT (dbp->type != DB_UNKNOWN);

empty:		/*
		 * The file is empty, and that's OK.  If it's not a subdatabase,
		 * though, we do need to generate a unique file ID for it.  The
		 * unique file ID includes a timestamp so that we can't collide
		 * with any other files, even when the file IDs (dev/inode pair)
		 * are reused.
		 */
		if (!IS_SUBDB_SETUP) {
			if (*retflags & DB_FILE_SETUP_ZERO)
				memset(dbp->fileid, 0, DB_FILE_ID_LEN);
			else if ((ret = __os_fileid(dbenv,
			    real_name, 1, dbp->fileid)) != 0)
				goto err_msg;
		}
	}

	if (0) {
err_msg:	__db_err(dbenv, "%s: %s", name, db_strerror(ret));
	}

	/*
	 * Abort any running transaction -- it can only exist if something
	 * went wrong.
	 */
err:
DB_TEST_RECOVERY_LABEL

	/*
	 * If we opened a file handle and our caller is doing fcntl(2) locking,
	 * then we can't close it because that would discard the caller's lock.
	 * Otherwise, close the handle.
	 */
	if (F_ISSET(fhp, DB_FH_VALID)) {
		if (ret == 0 && LF_ISSET(DB_FCNTL_LOCKING))
			dbp->saved_open_fhp = fhp;
		else
			if ((t_ret = __os_closehandle(fhp)) != 0 && ret == 0)
				ret = t_ret;
	}

	/*
	 * This must be done after the file is closed, since
	 * txn_abort() may remove the file, and an open file
	 * cannot be removed on a Windows platforms.
	 */
	if (txn != NULL)
		(void)txn_abort(txn);

	if (real_name != NULL)
		__os_freestr(real_name);

	return (ret);
}

/*
 * __db_set_pgsize --
 *	Set the page size based on file information.
 */
static int
__db_set_pgsize(dbp, fhp, name)
	DB *dbp;
	DB_FH *fhp;
	char *name;
{
	DB_ENV *dbenv;
	u_int32_t iopsize;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * Use the filesystem's optimum I/O size as the pagesize if a pagesize
	 * not specified.  Some filesystems have 64K as their optimum I/O size,
	 * but as that results in fairly large default caches, we limit the
	 * default pagesize to 16K.
	 */
	if ((ret = __os_ioinfo(dbenv, name, fhp, NULL, NULL, &iopsize)) != 0) {
		__db_err(dbenv, "%s: %s", name, db_strerror(ret));
		return (ret);
	}
	if (iopsize < 512)
		iopsize = 512;
	if (iopsize > 16 * 1024)
		iopsize = 16 * 1024;

	/*
	 * Sheer paranoia, but we don't want anything that's not a power-of-2
	 * (we rely on that for alignment of various types on the pages), and
	 * we want a multiple of the sector size as well.
	 */
	OS_ROUNDOFF(iopsize, 512);

	dbp->pgsize = iopsize;
	F_SET(dbp, DB_AM_PGDEF);

	return (0);
}

/*
 * __db_close --
 *	DB destructor.
 *
 * PUBLIC: int __db_close __P((DB *, u_int32_t));
 */
int
__db_close(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *dbc;
	int ret, t_ret;

	ret = 0;

	dbenv = dbp->dbenv;
	PANIC_CHECK(dbenv);

	/* Validate arguments. */
	if ((ret = __db_closechk(dbp, flags)) != 0)
		goto err;

	/* If never opened, or not currently open, it's easy. */
	if (!F_ISSET(dbp, DB_OPEN_CALLED))
		goto never_opened;

	/* Sync the underlying access method. */
	if (!LF_ISSET(DB_NOSYNC) && !F_ISSET(dbp, DB_AM_DISCARD) &&
	    (t_ret = dbp->sync(dbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Go through the active cursors and call the cursor recycle routine,
	 * which resolves pending operations and moves the cursors onto the
	 * free list.  Then, walk the free list and call the cursor destroy
	 * routine.
	 */
	while ((dbc = TAILQ_FIRST(&dbp->active_queue)) != NULL)
		if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
			ret = t_ret;
	while ((dbc = TAILQ_FIRST(&dbp->free_queue)) != NULL)
		if ((t_ret = __db_c_destroy(dbc)) != 0 && ret == 0)
			ret = t_ret;

	/*
	 * Close any outstanding join cursors.  Join cursors destroy
	 * themselves on close and have no separate destroy routine.
	 */
	while ((dbc = TAILQ_FIRST(&dbp->join_queue)) != NULL)
		if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
			ret = t_ret;

	/* Remove this DB handle from the DB_ENV's dblist. */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	LIST_REMOVE(dbp, dblistlinks);
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	/* Sync the memory pool. */
	if (!LF_ISSET(DB_NOSYNC) && !F_ISSET(dbp, DB_AM_DISCARD) &&
	    (t_ret = memp_fsync(dbp->mpf)) != 0 &&
	    t_ret != DB_INCOMPLETE && ret == 0)
		ret = t_ret;

	/* Close any handle we've been holding since the open.  */
	if (dbp->saved_open_fhp != NULL &&
	    F_ISSET(dbp->saved_open_fhp, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(dbp->saved_open_fhp)) != 0 && ret == 0)
		ret = t_ret;

never_opened:
	/*
	 * Call the access specific close function.
	 *
	 * !!!
	 * Because of where the function is called in the close process,
	 * these routines can't do anything that would dirty pages or
	 * otherwise affect closing down the database.
	 */
	if ((t_ret = __ham_db_close(dbp)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __bam_db_close(dbp)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __qam_db_close(dbp)) != 0 && ret == 0)
		ret = t_ret;

err:
	/* Refresh the structure and close any local environment. */
	if ((t_ret = __db_refresh(dbp)) != 0 && ret == 0)
		ret = t_ret;
	if (F_ISSET(dbenv, DB_ENV_DBLOCAL) &&
	    --dbenv->dblocal_ref == 0 &&
	    (t_ret = dbenv->close(dbenv, 0)) != 0 && ret == 0)
		ret = t_ret;

	memset(dbp, CLEAR_BYTE, sizeof(*dbp));
	__os_free(dbp, sizeof(*dbp));

	return (ret);
}

/*
 * __db_refresh --
 *	Refresh the DB structure, releasing any allocated resources.
 */
static int
__db_refresh(dbp)
	DB *dbp;
{
	DB_ENV *dbenv;
	DBC *dbc;
	int ret, t_ret;

	ret = 0;

	dbenv = dbp->dbenv;

	/*
	 * Go through the active cursors and call the cursor recycle routine,
	 * which resolves pending operations and moves the cursors onto the
	 * free list.  Then, walk the free list and call the cursor destroy
	 * routine.
	 */
	while ((dbc = TAILQ_FIRST(&dbp->active_queue)) != NULL)
		if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
			ret = t_ret;
	while ((dbc = TAILQ_FIRST(&dbp->free_queue)) != NULL)
		if ((t_ret = __db_c_destroy(dbc)) != 0 && ret == 0)
			ret = t_ret;

	dbp->type = 0;

	/* Close the memory pool file handle. */
	if (dbp->mpf != NULL) {
		if (F_ISSET(dbp, DB_AM_DISCARD))
			(void)__memp_fremove(dbp->mpf);
		if ((t_ret = memp_fclose(dbp->mpf)) != 0 && ret == 0)
			ret = t_ret;
		dbp->mpf = NULL;
	}

	/* Discard the thread mutex. */
	if (dbp->mutexp != NULL) {
		__db_mutex_free(dbenv, dbenv->reginfo, dbp->mutexp);
		dbp->mutexp = NULL;
	}

	/* Discard the log file id. */
	if (!IS_RECOVERING(dbenv)
	    && dbp->log_fileid != DB_LOGFILEID_INVALID)
		(void)log_unregister(dbenv, dbp);

	F_CLR(dbp, DB_AM_DISCARD);
	F_CLR(dbp, DB_AM_INMEM);
	F_CLR(dbp, DB_AM_RDONLY);
	F_CLR(dbp, DB_AM_SWAP);
	F_CLR(dbp, DB_DBM_ERROR);
	F_CLR(dbp, DB_OPEN_CALLED);

	return (ret);
}

/*
 * __db_remove
 *	Remove method for DB.
 *
 * PUBLIC: int __db_remove __P((DB *, const char *, const char *, u_int32_t));
 */
int
__db_remove(dbp, name, subdb, flags)
	DB *dbp;
	const char *name, *subdb;
	u_int32_t flags;
{
	DBT namedbt;
	DB_ENV *dbenv;
	DB_LOCK remove_lock;
	DB_LSN newlsn;
	int ret, t_ret, (*callback_func) __P((DB *, void *));
	char *backup, *real_back, *real_name;
	void *cookie;

	dbenv = dbp->dbenv;
	ret = 0;
	backup = real_back = real_name = NULL;

	PANIC_CHECK(dbenv);
	/*
	 * Cannot use DB_ILLEGAL_AFTER_OPEN here because that returns
	 * and we cannot return, but must deal with the error and destroy
	 * the handle anyway.
	 */
	if (F_ISSET(dbp, DB_OPEN_CALLED)) {
		ret = __db_mi_open(dbp->dbenv, "remove", 1);
		goto err_close;
	}

	/* Validate arguments. */
	if ((ret = __db_removechk(dbp, flags)) != 0)
		goto err_close;

	/*
	 * Subdatabases.
	 */
	if (subdb != NULL) {
		/* Subdatabases must be created in named files. */
		if (name == NULL) {
			__db_err(dbenv,
		    "multiple databases cannot be created in temporary files");
			goto err_close;
		}
		return (__db_subdb_remove(dbp, name, subdb));
	}

	if ((ret = dbp->open(dbp,
	    name, NULL, DB_UNKNOWN, DB_RDWRMASTER, 0)) != 0)
		goto err_close;

	if (LOGGING_ON(dbenv) && (ret = __log_file_lock(dbp)) != 0)
		goto err_close;

	if ((ret = dbp->sync(dbp, 0)) != 0)
		goto err_close;

	/* Start the transaction and log the delete. */
	if (TXN_ON(dbenv) && (ret = __db_metabegin(dbp, &remove_lock)) != 0)
		goto err_close;

	if (LOGGING_ON(dbenv)) {
		memset(&namedbt, 0, sizeof(namedbt));
		namedbt.data = (char *)name;
		namedbt.size = strlen(name) + 1;

		if ((ret = __crdel_delete_log(dbenv,
		    dbp->open_txn, &newlsn, DB_FLUSH,
		    dbp->log_fileid, &namedbt)) != 0) {
			__db_err(dbenv,
			    "%s: %s", name, db_strerror(ret));
			goto err;
		}
	}

	/* Find the real name of the file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, NULL, name, 0, NULL, &real_name)) != 0)
		goto err;

	/*
	 * XXX
	 * We don't bother to open the file and call __memp_fremove on the mpf.
	 * There is a potential race here.  It is at least possible that, if
	 * the unique filesystem ID (dev/inode pair on UNIX) is reallocated
	 * within a second (the granularity of the fileID timestamp), a new
	 * file open will get the same fileID as the file being "removed".
	 * We may actually want to open the file and call __memp_fremove on
	 * the mpf to get around this.
	 */

	/* Create name for backup file. */
	if (TXN_ON(dbenv)) {
		if ((ret =
		    __db_backup_name(dbenv, name, &backup, &newlsn)) != 0)
			goto err;
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, backup, 0, NULL, &real_back)) != 0)
			goto err;
	}

	callback_func = __db_remove_callback;
	cookie = real_back;
	DB_TEST_RECOVERY(dbp, DB_TEST_PRERENAME, ret, name);
	if (dbp->db_am_remove != NULL &&
	    (ret = dbp->db_am_remove(dbp,
	    name, subdb, &newlsn, &callback_func, &cookie)) != 0)
		goto err;
	/*
	 * On Windows, the underlying file must be closed to perform a remove.
	 * Nothing later in __db_remove requires that it be open, and the
	 * dbp->close closes it anyway, so we just close it early.
	 */
	(void)__memp_fremove(dbp->mpf);
	if ((ret = memp_fclose(dbp->mpf)) != 0)
		goto err;
	dbp->mpf = NULL;

	if (TXN_ON(dbenv))
		ret = __os_rename(dbenv, real_name, real_back);
	else
		ret = __os_unlink(dbenv, real_name);

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTRENAME, ret, name);

err:
DB_TEST_RECOVERY_LABEL
	/*
	 * End the transaction, committing the transaction if we were
	 * successful, aborting otherwise.
	 */
	if (dbp->open_txn != NULL && (t_ret = __db_metaend(dbp, &remove_lock,
	   ret == 0, callback_func, cookie)) != 0 && ret == 0)
		ret = t_ret;

	/* FALLTHROUGH */

err_close:
	if (real_back != NULL)
		__os_freestr(real_back);
	if (real_name != NULL)
		__os_freestr(real_name);
	if (backup != NULL)
		__os_freestr(backup);

	/* We no longer have an mpool, so syncing would be disastrous. */
	if ((t_ret = dbp->close(dbp, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_subdb_remove --
 *	Remove a subdatabase.
 */
static int
__db_subdb_remove(dbp, name, subdb)
	DB *dbp;
	const char *name, *subdb;
{
	DB *mdbp;
	DBC *dbc;
	DB_ENV *dbenv;
	DB_LOCK remove_lock;
	db_pgno_t meta_pgno;
	int ret, t_ret;

	mdbp = NULL;
	dbc = NULL;
	dbenv = dbp->dbenv;

	/* Start the transaction. */
	if (TXN_ON(dbenv) && (ret = __db_metabegin(dbp, &remove_lock)) != 0)
		goto err_close;

	/*
	 * Open the subdatabase.  We can use the user's DB handle for this
	 * purpose, I think.
	 */
	if ((ret = __db_open(dbp, name, subdb, DB_UNKNOWN, 0, 0)) != 0)
		goto err;

	/* Free up the pages in the subdatabase. */
	switch (dbp->type) {
		case DB_BTREE:
		case DB_RECNO:
			if ((ret = __bam_reclaim(dbp, dbp->open_txn)) != 0)
				goto err;
			break;
		case DB_HASH:
			if ((ret = __ham_reclaim(dbp, dbp->open_txn)) != 0)
				goto err;
			break;
		default:
			ret = __db_unknown_type(dbp->dbenv,
			     "__db_subdb_remove", dbp->type);
			goto err;
	}

	/*
	 * Remove the entry from the main database and free the subdatabase
	 * metadata page.
	 */
	if ((ret = __db_master_open(dbp, name, 0, 0, &mdbp)) != 0)
		goto err;

	if ((ret = __db_master_update(mdbp,
	     subdb, dbp->type, &meta_pgno, MU_REMOVE, NULL, 0)) != 0)
		goto err;

err:	/*
	 * End the transaction, committing the transaction if we were
	 * successful, aborting otherwise.
	 */
	if (dbp->open_txn != NULL && (t_ret = __db_metaend(dbp,
	    &remove_lock, ret == 0, NULL, NULL)) != 0 && ret == 0)
		ret = t_ret;

err_close:
	/*
	 * Close the user's DB handle -- do this LAST to avoid smashing the
	 * the transaction information.
	 */
	if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (mdbp != NULL && (t_ret = mdbp->close(mdbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_rename
 *	Rename method for DB.
 *
 * PUBLIC: int __db_rename __P((DB *,
 * PUBLIC:     const char *, const char *, const char *, u_int32_t));
 */
int
__db_rename(dbp, filename, subdb, newname, flags)
	DB *dbp;
	const char *filename, *subdb, *newname;
	u_int32_t flags;
{
	DBT namedbt, newnamedbt;
	DB_ENV *dbenv;
	DB_LOCK remove_lock;
	DB_LSN newlsn;
	char *real_name, *real_newname;
	int ret, t_ret;

	dbenv = dbp->dbenv;
	ret = 0;
	real_name = real_newname = NULL;

	PANIC_CHECK(dbenv);
	/*
	 * Cannot use DB_ILLEGAL_AFTER_OPEN here because that returns
	 * and we cannot return, but must deal with the error and destroy
	 * the handle anyway.
	 */
	if (F_ISSET(dbp, DB_OPEN_CALLED)) {
		ret = __db_mi_open(dbp->dbenv, "rename", 1);
		goto err_close;
	}

	/* Validate arguments -- has same rules as remove. */
	if ((ret = __db_removechk(dbp, flags)) != 0)
		goto err_close;

	/*
	 * Subdatabases.
	 */
	if (subdb != NULL) {
		if (filename == NULL) {
			__db_err(dbenv,
		    "multiple databases cannot be created in temporary files");
			goto err_close;
		}
		return (__db_subdb_rename(dbp, filename, subdb, newname));
	}

	if ((ret = dbp->open(dbp,
	    filename, NULL, DB_UNKNOWN, DB_RDWRMASTER, 0)) != 0)
		goto err_close;

	if (LOGGING_ON(dbenv) && (ret = __log_file_lock(dbp)) != 0)
		goto err_close;

	if ((ret = dbp->sync(dbp, 0)) != 0)
		goto err_close;

	/* Start the transaction and log the rename. */
	if (TXN_ON(dbenv) && (ret = __db_metabegin(dbp, &remove_lock)) != 0)
		goto err_close;

	if (LOGGING_ON(dbenv)) {
		memset(&namedbt, 0, sizeof(namedbt));
		namedbt.data = (char *)filename;
		namedbt.size = strlen(filename) + 1;

		memset(&newnamedbt, 0, sizeof(namedbt));
		newnamedbt.data = (char *)newname;
		newnamedbt.size = strlen(newname) + 1;

		if ((ret = __crdel_rename_log(dbenv, dbp->open_txn,
		    &newlsn, 0, dbp->log_fileid, &namedbt, &newnamedbt)) != 0) {
			__db_err(dbenv, "%s: %s", filename, db_strerror(ret));
			goto err;
		}

		if ((ret = __log_filelist_update(dbenv, dbp,
		    dbp->log_fileid, newname, NULL)) != 0)
			goto err;
	}

	/* Find the real name of the file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, NULL, filename, 0, NULL, &real_name)) != 0)
		goto err;

	/* Find the real newname of the file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, NULL, newname, 0, NULL, &real_newname)) != 0)
		goto err;

	/*
	 * It is an error to rename a file over one that already exists,
	 * as that wouldn't be transaction-safe.
	 */
	if (__os_exists(real_newname, NULL) == 0) {
		ret = EEXIST;
		__db_err(dbenv, "rename: file %s exists", real_newname);
		goto err;
	}

	DB_TEST_RECOVERY(dbp, DB_TEST_PRERENAME, ret, filename);
	if (dbp->db_am_rename != NULL &&
	    (ret = dbp->db_am_rename(dbp, filename, subdb, newname)) != 0)
		goto err;
	/*
	 * We have to flush the cache for a couple of reasons.  First, the
	 * underlying MPOOLFILE maintains a "name" that unrelated processes
	 * can use to open the file in order to flush pages, and that name
	 * is about to be wrong.  Second, on Windows the unique file ID is
	 * generated from the file's name, not other file information as is
	 * the case on UNIX, and so a subsequent open of the old file name
	 * could conceivably result in a matching "unique" file ID.
	 */
	if ((ret = __memp_fremove(dbp->mpf)) != 0)
		goto err;

	/*
	 * On Windows, the underlying file must be closed to perform a rename.
	 * Nothing later in __db_rename requires that it be open, and the call
	 * to dbp->close closes it anyway, so we just close it early.
	 */
	if ((ret = memp_fclose(dbp->mpf)) != 0)
		goto err;
	dbp->mpf = NULL;

	ret = __os_rename(dbenv, real_name, real_newname);
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTRENAME, ret, newname);

DB_TEST_RECOVERY_LABEL
err:	if (dbp->open_txn != NULL && (t_ret = __db_metaend(dbp,
	    &remove_lock, ret == 0, NULL, NULL)) != 0 && ret == 0)
		ret = t_ret;

err_close:
	/* We no longer have an mpool, so syncing would be disastrous. */
	dbp->close(dbp, DB_NOSYNC);
	if (real_name != NULL)
		__os_freestr(real_name);
	if (real_newname != NULL)
		__os_freestr(real_newname);

	return (ret);
}

/*
 * __db_subdb_rename --
 *	Rename a subdatabase.
 */
static int
__db_subdb_rename(dbp, name, subdb, newname)
	DB *dbp;
	const char *name, *subdb, *newname;
{
	DB *mdbp;
	DBC *dbc;
	DB_ENV *dbenv;
	DB_LOCK remove_lock;
	int ret, t_ret;

	mdbp = NULL;
	dbc = NULL;
	dbenv = dbp->dbenv;

	/* Start the transaction. */
	if (TXN_ON(dbenv) && (ret = __db_metabegin(dbp, &remove_lock)) != 0)
		goto err_close;

	/*
	 * Open the subdatabase.  We can use the user's DB handle for this
	 * purpose, I think.
	 */
	if ((ret = __db_open(dbp, name, subdb, DB_UNKNOWN, 0, 0)) != 0)
		goto err;

	/*
	 * Rename the entry in the main database.
	 */
	if ((ret = __db_master_open(dbp, name, 0, 0, &mdbp)) != 0)
		goto err;

	if ((ret = __db_master_update(mdbp,
	     subdb, dbp->type, NULL, MU_RENAME, newname, 0)) != 0)
		goto err;

err:	/*
	 * End the transaction, committing the transaction if we were
	 * successful, aborting otherwise.
	 */
	if (dbp->open_txn != NULL && (t_ret = __db_metaend(dbp,
	    &remove_lock, ret == 0, NULL, NULL)) != 0 && ret == 0)
		ret = t_ret;

err_close:
	/*
	 * Close the user's DB handle -- do this LAST to avoid smashing the
	 * the transaction information.
	 */
	if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (mdbp != NULL && (t_ret = mdbp->close(mdbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_metabegin --
 *
 * Begin a meta-data operation.  This involves doing any required locking,
 * potentially beginning a transaction and then telling the caller if you
 * did or did not begin the transaction.
 *
 * The writing flag indicates if the caller is actually allowing creates
 * or doing deletes (i.e., if the caller is opening and not creating, then
 * we don't need to do any of this).
 * PUBLIC: int __db_metabegin __P((DB *, DB_LOCK *));
 */
int
__db_metabegin(dbp, lockp)
	DB *dbp;
	DB_LOCK *lockp;
{
	DB_ENV *dbenv;
	DBT dbplock;
	u_int32_t locker, lockval;
	int ret;

	dbenv = dbp->dbenv;

	lockp->off = LOCK_INVALID;

	/*
	 * There is no single place where we can know that we are or are not
	 * going to be creating any files and/or subdatabases, so we will
	 * always begin a tranasaction when we start creating one.  If we later
	 * discover that this was unnecessary, we will abort the transaction.
	 * Recovery is written so that if we log a file create, but then
	 * discover that we didn't have to do it, we recover correctly.  The
	 * file recovery design document has details.
	 *
	 * We need to single thread all create and delete operations, so if we
	 * are running with locking, we must obtain a lock. We use lock_id to
	 * generate a unique locker id and use a handcrafted DBT as the object
	 * on which we are locking.
	 */
	if (LOCKING_ON(dbenv)) {
		if ((ret = lock_id(dbenv, &locker)) != 0)
			return (ret);
		lockval = 0;
		dbplock.data = &lockval;
		dbplock.size = sizeof(lockval);
		if ((ret = lock_get(dbenv,
		    locker, 0, &dbplock, DB_LOCK_WRITE, lockp)) != 0)
			return (ret);
	}

	return (txn_begin(dbenv, NULL, &dbp->open_txn, 0));
}

/*
 * __db_metaend --
 *	End a meta-data operation.
 * PUBLIC: int __db_metaend __P((DB *,
 * PUBLIC:       DB_LOCK *, int, int (*)(DB *, void *), void *));
 */
int
__db_metaend(dbp, lockp, commit, callback, cookie)
	DB *dbp;
	DB_LOCK *lockp;
	int commit, (*callback) __P((DB *, void *));
	void *cookie;
{
	DB_ENV *dbenv;
	int ret, t_ret;

	ret = 0;
	dbenv = dbp->dbenv;

	/* End the transaction. */
	if (commit) {
		if ((ret = txn_commit(dbp->open_txn, DB_TXN_SYNC)) == 0) {
			/*
			 * Unlink any underlying file, we've committed the
			 * transaction.
			 */
			if (callback != NULL)
				ret = callback(dbp, cookie);
		}
	} else if ((t_ret = txn_abort(dbp->open_txn)) && ret == 0)
		ret = t_ret;

	/* Release our lock. */
	if (lockp->off != LOCK_INVALID &&
	    (t_ret = lock_put(dbenv, lockp)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_log_page
 *	Log a meta-data or root page during a create operation.
 *
 * PUBLIC: int __db_log_page __P((DB *,
 * PUBLIC:     const char *, DB_LSN *, db_pgno_t, PAGE *));
 */
int
__db_log_page(dbp, name, lsn, pgno, page)
	DB *dbp;
	const char *name;
	DB_LSN *lsn;
	db_pgno_t pgno;
	PAGE *page;
{
	DBT name_dbt, page_dbt;
	DB_LSN new_lsn;
	int ret;

	if (dbp->open_txn == NULL)
		return (0);

	memset(&page_dbt, 0, sizeof(page_dbt));
	page_dbt.size = dbp->pgsize;
	page_dbt.data = page;
	if (pgno == PGNO_BASE_MD) {
		/*
		 * !!!
		 * Make sure that we properly handle a null name.  The old
		 * Tcl sent us pathnames of the form ""; it may be the case
		 * that the new Tcl doesn't do that, so we can get rid of
		 * the second check here.
		 */
		memset(&name_dbt, 0, sizeof(name_dbt));
		name_dbt.data = (char *)name;
		if (name == NULL || *name == '\0')
			name_dbt.size = 0;
		else
			name_dbt.size = strlen(name) + 1;

		ret = __crdel_metapage_log(dbp->dbenv,
		    dbp->open_txn, &new_lsn, DB_FLUSH,
		    dbp->log_fileid, &name_dbt, pgno, &page_dbt);
	} else
		ret = __crdel_metasub_log(dbp->dbenv, dbp->open_txn,
		    &new_lsn, 0, dbp->log_fileid, pgno, &page_dbt, lsn);

	if (ret == 0)
		page->lsn = new_lsn;
	return (ret);
}

/*
 * __db_backup_name
 *	Create the backup file name for a given file.
 *
 * PUBLIC: int __db_backup_name __P((DB_ENV *,
 * PUBLIC:     const char *, char **, DB_LSN *));
 */
#undef	BACKUP_PREFIX
#define	BACKUP_PREFIX	"__db."

#undef	MAX_LSN_TO_TEXT
#define	MAX_LSN_TO_TEXT	21
int
__db_backup_name(dbenv, name, backup, lsn)
	DB_ENV *dbenv;
	const char *name;
	char **backup;
	DB_LSN *lsn;
{
	size_t len;
	int plen, ret;
	char *p, *retp;

	len = strlen(name) + strlen(BACKUP_PREFIX) + MAX_LSN_TO_TEXT + 1;

	if ((ret = __os_malloc(dbenv, len, NULL, &retp)) != 0)
		return (ret);

	/*
	 * Create the name.  Backup file names are of the form:
	 *
	 *	__db.name.0x[lsn-file].0x[lsn-offset]
	 *
	 * which guarantees uniqueness.
	 *
	 * However, name may contain an env-relative path in it.
	 * In that case, put the __db. after the last portion of
	 * the pathname.
	 */
	if ((p = __db_rpath(name)) == NULL)
		snprintf(retp, len,
		    "%s%s.0x%x0x%x", BACKUP_PREFIX, name,
		    lsn->file, lsn->offset);
	else {
		plen = p - name + 1;
		p++;
		snprintf(retp, len,
		    "%.*s%s%s.0x%x0x%x", plen, name, BACKUP_PREFIX, p,
		    lsn->file, lsn->offset);
	}

	*backup = retp;
	return (0);
}

/*
 * __db_remove_callback --
 *	Callback function -- on file remove commit, it unlinks the backing
 *	file.
 */
static int
__db_remove_callback(dbp, cookie)
	DB *dbp;
	void *cookie;
{
	return (__os_unlink(dbp->dbenv, cookie));
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

#if	CONFIG_TEST
/*
 * __db_testcopy
 *	Create a copy of all backup files and our "main" DB.
 *
 * PUBLIC: int __db_testcopy __P((DB *, const char *));
 */
int
__db_testcopy(dbp, name)
	DB *dbp;
	const char *name;
{
	if (dbp->type == DB_QUEUE)
		return (__qam_testdocopy(dbp, name));
	else
		return (__db_testdocopy(dbp, name));
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
	if ((ret = __db_testdocopy(dbp, name)) != 0)
		return (ret);
	if (dbp->mpf != NULL &&
	    (ret = __qam_gen_filelist(dbp, &filelist)) != 0)
		return (ret);

	if (filelist == NULL)
		return (0);
	dir = ((QUEUE *)dbp->q_internal)->dir;
	for (fp = filelist; fp->mpf != NULL; fp++) {
		snprintf(buf, sizeof(buf), QUEUE_EXTENT, dir, name, fp->id);
		if ((ret = __db_testdocopy(dbp, buf)) != 0)
			return (ret);
	}

	__os_free(filelist, 0);
	return (0);
}

/*
 * __db_testdocopy
 *	Create a copy of all backup files and our "main" DB.
 *
 */
static int
__db_testdocopy(dbp, name)
	DB *dbp;
	const char *name;
{
	size_t len;
	int dircnt, i, ret;
	char **namesp, *backup, *copy, *dir, *p, *real_name;
	real_name = NULL;
	/* Get the real backing file name. */
	if ((ret = __db_appname(dbp->dbenv,
	    DB_APP_DATA, NULL, name, 0, NULL, &real_name)) != 0)
		return (ret);

	copy = backup = NULL;
	namesp = NULL;

	/*
	 * Maximum size of file, including adding a ".afterop".
	 */
	len = strlen(real_name) + strlen(BACKUP_PREFIX) + MAX_LSN_TO_TEXT + 9;

	if ((ret = __os_malloc(dbp->dbenv, len, NULL, &copy)) != 0)
		goto out;

	if ((ret = __os_malloc(dbp->dbenv, len, NULL, &backup)) != 0)
		goto out;

	/*
	 * First copy the file itself.
	 */
	snprintf(copy, len, "%s.afterop", real_name);
	__db_makecopy(real_name, copy);

	if ((ret = __os_strdup(dbp->dbenv, real_name, &dir)) != 0)
		goto out;
	__os_freestr(real_name);
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
	ret = __os_dirlist(dbp->dbenv, dir, &namesp, &dircnt);
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
	__os_freestr(dir);
	if (ret != 0)
		goto out;
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
			if ((ret = __db_appname(dbp->dbenv, DB_APP_DATA,
			    NULL, namesp[i], 0, NULL, &real_name)) != 0)
				goto out;

			/*
			 * This should not happen.  Check that old
			 * .afterop files aren't around.
			 * If so, just move on.
			 */
			if (strstr(real_name, ".afterop") != NULL) {
				__os_freestr(real_name);
				real_name = NULL;
				continue;
			}
			snprintf(copy, len, "%s.afterop", real_name);
			__db_makecopy(real_name, copy);
			__os_freestr(real_name);
			real_name = NULL;
		}
	}
out:
	if (backup != NULL)
		__os_freestr(backup);
	if (copy != NULL)
		__os_freestr(copy);
	if (namesp != NULL)
		__os_dirfree(namesp, dircnt);
	if (real_name != NULL)
		__os_freestr(real_name);
	return (ret);
}

static void
__db_makecopy(src, dest)
	const char *src, *dest;
{
	DB_FH rfh, wfh;
	size_t rcnt, wcnt;
	char *buf;

	memset(&rfh, 0, sizeof(rfh));
	memset(&wfh, 0, sizeof(wfh));

	if (__os_malloc(NULL, 1024, NULL, &buf) != 0)
		return;

	if (__os_open(NULL,
	    src, DB_OSO_RDONLY, __db_omode("rw----"), &rfh) != 0)
		goto err;
	if (__os_open(NULL, dest,
	    DB_OSO_CREATE | DB_OSO_TRUNC, __db_omode("rw----"), &wfh) != 0)
		goto err;

	for (;;)
		if (__os_read(NULL, &rfh, buf, 1024, &rcnt) < 0 || rcnt == 0 ||
		    __os_write(NULL, &wfh, buf, rcnt, &wcnt) < 0 || wcnt != rcnt)
			break;

err:	__os_free(buf, 1024);
	if (F_ISSET(&rfh, DB_FH_VALID))
		__os_closehandle(&rfh);
	if (F_ISSET(&wfh, DB_FH_VALID))
		__os_closehandle(&wfh);
}
#endif
