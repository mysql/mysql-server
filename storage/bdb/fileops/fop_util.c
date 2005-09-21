/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: fop_util.c,v 1.104 2004/09/24 00:43:18 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/fop.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __fop_set_pgsize __P((DB *, DB_FH *, const char *));

/*
 * Acquire the environment meta-data lock.  The parameters are the
 * environment (ENV), the locker id to use in acquiring the lock (ID)
 * and a pointer to a DB_LOCK.
 *
 * !!!
 * Turn off locking for Critical Path.  The application must do its own
 * synchronization of open/create.  Two threads creating and opening a
 * file at the same time may have unpredictable results.
 */
#ifdef CRITICALPATH_10266
#define	GET_ENVLOCK(ENV, ID, L) (0)
#else
#define	GET_ENVLOCK(ENV, ID, L) do {					\
	DBT __dbt;							\
	u_int32_t __lockval;						\
									\
	if (LOCKING_ON((ENV))) {					\
		__lockval = 1;						\
		__dbt.data = &__lockval;				\
		__dbt.size = sizeof(__lockval);				\
		if ((ret = __lock_get((ENV), (ID),			\
		    0, &__dbt, DB_LOCK_WRITE, (L))) != 0)		\
			goto err;					\
	}								\
} while (0)
#endif

/*
 * If we open a file handle and our caller is doing fcntl(2) locking,
 * we can't close the handle because that would discard the caller's
 * lock. Save it until we close or refresh the DB handle.
 */
#define	CLOSE_HANDLE(D, F) {						\
	if ((F) != NULL) {						\
		if (LF_ISSET(DB_FCNTL_LOCKING))				\
			(D)->saved_open_fhp = (F);			\
		else if ((t_ret =					\
		    __os_closehandle((D)->dbenv, (F))) != 0) {		\
			if (ret == 0)					\
				ret = t_ret;				\
			goto err;					\
		}							\
		(F) = NULL;						\
	}								\
}

/*
 * __fop_lock_handle --
 *
 * Get the handle lock for a database.  If the envlock is specified, do this
 * as a lock_vec call that releases the environment lock before acquiring the
 * handle lock.
 *
 * PUBLIC: int __fop_lock_handle __P((DB_ENV *,
 * PUBLIC:     DB *, u_int32_t, db_lockmode_t, DB_LOCK *, u_int32_t));
 *
 */
int
__fop_lock_handle(dbenv, dbp, locker, mode, elockp, flags)
	DB_ENV *dbenv;
	DB *dbp;
	u_int32_t locker;
	db_lockmode_t mode;
	DB_LOCK *elockp;
	u_int32_t flags;
{
	DBT fileobj;
	DB_LOCKREQ reqs[2], *ereq;
	DB_LOCK_ILOCK lock_desc;
	int ret;

	if (!LOCKING_ON(dbenv) ||
	    F_ISSET(dbp, DB_AM_COMPENSATE | DB_AM_RECOVER))
		return (0);

	/*
	 * If we are in recovery, the only locking we should be
	 * doing is on the global environment.
	 */
	if (IS_RECOVERING(dbenv))
		return (elockp == NULL ? 0 : __ENV_LPUT(dbenv, *elockp, 0));

	memcpy(lock_desc.fileid, dbp->fileid, DB_FILE_ID_LEN);
	lock_desc.pgno = dbp->meta_pgno;
	lock_desc.type = DB_HANDLE_LOCK;

	memset(&fileobj, 0, sizeof(fileobj));
	fileobj.data = &lock_desc;
	fileobj.size = sizeof(lock_desc);
	DB_TEST_SUBLOCKS(dbenv, flags);
	if (elockp == NULL)
		ret = __lock_get(dbenv, locker,
		    flags, &fileobj, mode, &dbp->handle_lock);
	else {
		reqs[0].op = DB_LOCK_PUT;
		reqs[0].lock = *elockp;
		reqs[1].op = DB_LOCK_GET;
		reqs[1].mode = mode;
		reqs[1].obj = &fileobj;
		reqs[1].timeout = 0;
		if ((ret = __lock_vec(dbenv,
		    locker, flags, reqs, 2, &ereq)) == 0) {
			dbp->handle_lock = reqs[1].lock;
			LOCK_INIT(*elockp);
		} else if (ereq != reqs)
			LOCK_INIT(*elockp);
	}

	dbp->cur_lid = locker;
	return (ret);
}

/*
 * __fop_file_setup --
 *
 * Perform all the needed checking and locking to open up or create a
 * file.
 *
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
 * buffer cache or obtaining a lock (we use this unique fileid to lock
 * as well as to identify like files in the cache).
 *
 * There are a couple of idiosyncrasies that this code must support, in
 * particular, DB_TRUNCATE and DB_FCNTL_LOCKING.  First, we disallow
 * DB_TRUNCATE in the presence of transactions, since opening a file with
 * O_TRUNC will result in data being lost in an unrecoverable fashion.
 * We also disallow DB_TRUNCATE if locking is enabled, because even in
 * the presence of locking, we cannot avoid race conditions, so allowing
 * DB_TRUNCATE with locking would be misleading.  See SR [#7345] for more
 * details.
 *
 * However, if you are running with neither locking nor transactions, then
 * you can specify DB_TRUNCATE, and if you do so, we will truncate the file
 * regardless of its contents.
 *
 * FCNTL locking introduces another set of complications.  First, the only
 * reason we support the DB_FCNTL_LOCKING flag is for historic compatibility
 * with programs like Sendmail and Postfix.  In these cases, the caller may
 * already have a lock on the file; we need to make sure that any file handles
 * we open remain open, because if we were to close them, the lock held by the
 * caller would go away.  Furthermore, Sendmail and/or Postfix need the ability
 * to create databases in empty files.  So, when you're doing FCNTL locking,
 * it's reasonable that you are trying to create a database into a 0-length
 * file and we allow it, while under normal conditions, we do not create
 * databases if the files already exist and are not Berkeley DB files.
 *
 * PUBLIC: int __fop_file_setup __P((DB *,
 * PUBLIC:     DB_TXN *, const char *, int, u_int32_t, u_int32_t *));
 */
int
__fop_file_setup(dbp, txn, name, mode, flags, retidp)
	DB *dbp;
	DB_TXN *txn;
	const char *name;
	int mode;
	u_int32_t flags, *retidp;
{
	DB_ENV *dbenv;
	DB_FH *fhp;
	DB_LOCK elock;
	DB_TXN *stxn;
	size_t len;
	u_int32_t dflags, locker, oflags;
	u_int8_t mbuf[DBMETASIZE];
	int created_locker, ret, retries, t_ret, tmp_created, truncating;
	char *real_name, *real_tmpname, *tmpname;

	DB_ASSERT(name != NULL);

	*retidp = TXN_INVALID;

	dbenv = dbp->dbenv;
	fhp = NULL;
	LOCK_INIT(elock);
	stxn = NULL;
	created_locker = tmp_created = truncating = 0;
	real_name = real_tmpname = tmpname = NULL;
	dflags = F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0;

	/*
	 * Get a lockerid for this handle.  There are paths through queue
	 * rename and remove where this dbp already has a locker, so make
	 * sure we don't clobber it and conflict.
	 */
	if (LOCKING_ON(dbenv) &&
	    !F_ISSET(dbp, DB_AM_COMPENSATE) &&
	    !F_ISSET(dbp, DB_AM_RECOVER) &&
	    dbp->lid == DB_LOCK_INVALIDID) {
		if ((ret = __lock_id(dbenv, &dbp->lid)) != 0)
			goto err;
		created_locker = 1;
	}
	LOCK_INIT(dbp->handle_lock);

	locker = txn == NULL ? dbp->lid : txn->txnid;

	/* Get the real backing file name. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		goto err;

	/* Fill in the default file mode. */
	if (mode == 0)
		mode = __db_omode("rwrw--");

	oflags = 0;
	if (LF_ISSET(DB_RDONLY))
		oflags |= DB_OSO_RDONLY;
	if (LF_ISSET(DB_TRUNCATE))
		oflags |= DB_OSO_TRUNC;
	retries = 0;
retry:
	/*
	 * If we cannot create the file, only retry a few times.  We
	 * think we might be in a race with another create, but it could
	 * be that the backup filename exists (that is, is left over from
	 * a previous crash).
	 */
	if (++retries > DB_RETRY) {
		__db_err(dbenv, "__fop_file_setup:  Retry limit (%d) exceeded",
		    DB_RETRY);
		goto err;
	}
	if (!F_ISSET(dbp, DB_AM_COMPENSATE) && !F_ISSET(dbp, DB_AM_RECOVER))
		GET_ENVLOCK(dbenv, locker, &elock);
	if ((ret = __os_exists(real_name, NULL)) == 0) {
		/*
		 * If the file exists, there are 5 possible cases:
		 * 1. DB_EXCL was specified so this is an error, unless
		 *	this is a file left around after a rename and we
		 *	are in the same transaction.  This gets decomposed
		 *	into several subcases, because we check for various
		 *	errors before we know we're in rename.
		 * 2. We are truncating, and it doesn't matter what kind
		 *	of file it is, we should open/create it.
		 * 3. It is 0-length, we are not doing transactions (i.e.,
		 *      we are sendmail), we should open/create into it.
		 * 4. Is it a Berkeley DB file and we should simply open it.
		 * 5. It is not a BDB file and we should return an error.
		 */

		/* We have to open the file. */
reopen:		if ((ret = __os_open(dbenv, real_name, oflags, 0, &fhp)) != 0)
			goto err;

		/* Case 2: DB_TRUNCATE: we must do the creation in place. */
		if (LF_ISSET(DB_TRUNCATE)) {
			if (LF_ISSET(DB_EXCL)) {
				/* Case 1a: DB_EXCL and DB_TRUNCATE. */
				ret = EEXIST;
				goto err;
			}
			tmpname = (char *)name;
			goto creat2;
		}

		/* Cases 1,3-5: we need to read the meta-data page. */
		ret = __fop_read_meta(dbenv, real_name, mbuf, sizeof(mbuf), fhp,
		    LF_ISSET(DB_FCNTL_LOCKING) && txn == NULL ? 1 : 0, &len);

		/* Case 3: 0-length, no txns. */
		if (ret != 0 && len == 0 && txn == NULL) {
			if (LF_ISSET(DB_EXCL)) {
				/* Case 1b: DB_EXCL and 0-lenth file exists. */
				ret = EEXIST;
				goto err;
			}
			tmpname = (char *)name;
			goto creat2;
		}

		/* Case 5: Invalid file. */
		if (ret != 0)
			goto err;

		/* Case 4: This is a valid file. */
		if ((ret = __db_meta_setup(dbenv,
		    dbp, real_name, (DBMETA *)mbuf, flags, 1)) != 0)
			goto err;

		/* Now, get our handle lock. */
		if ((ret = __fop_lock_handle(dbenv,
		    dbp, locker, DB_LOCK_READ, NULL, DB_LOCK_NOWAIT)) == 0) {
			if ((ret = __ENV_LPUT(dbenv, elock, 0)) != 0)
				goto err;
		} else if (ret != DB_LOCK_NOTGRANTED ||
		    (txn != NULL && F_ISSET(txn, TXN_NOWAIT)))
			goto err;
		else {
			/*
			 * We were unable to acquire the handle lock without
			 * blocking.  The fact that we are blocking might mean
			 * that someone else is trying to delete the file.
			 * Since some platforms cannot delete files while they
			 * are open (Windows), we are going to have to close
			 * the file.  This would be a problem if we were doing
			 * FCNTL locking, because our closing the handle would
			 * release the FCNTL locks.  Fortunately, if we are
			 * doing FCNTL locking, then we should never fail to
			 * acquire our handle lock, so we should never get here.
			 * We assert it here to make sure we aren't destroying
			 * any application level FCNTL semantics.
			 */
			DB_ASSERT(!LF_ISSET(DB_FCNTL_LOCKING));
			if ((ret = __os_closehandle(dbenv, fhp)) != 0)
				goto err;
			fhp = NULL;
			ret = __fop_lock_handle(dbenv,
			    dbp, locker, DB_LOCK_READ, &elock, 0);
			if (ret == DB_LOCK_NOTEXIST)
				goto retry;
			if (ret != 0)
				goto err;
			/*
			 * XXX
			 * I need to convince myself that I don't need to
			 * re-read the metadata page here.  If you do need
			 * to re-read it you'd better decrypt it too...
			 */
			if ((ret =
			    __os_open(dbenv, real_name, 0, 0, &fhp)) != 0)
				goto err;
		}

		/* If we got here, then we now have the handle lock. */

		/*
		 * Check for a file in the midst of a rename.  If we find that
		 * the file is in the midst of a rename, it must be the case
		 * that it is in our current transaction (else we would still
		 * be blocking), so we can continue along and create a new file
		 * with the same name.  In that case, we have to close the file
		 * handle because we reuse it below.
		 */
		if (F_ISSET(dbp, DB_AM_IN_RENAME)) {
			if (LF_ISSET(DB_CREATE)) {
				if ((ret = __os_closehandle(dbenv, fhp)) != 0)
					goto err;
				goto create;
			} else {
				ret = ENOENT;
				goto err;
			}
		}

		/*
		 * Now, case 1: check for DB_EXCL, because the file that exists
		 * is not in the middle of a rename, so we have an error.  This
		 * is a weird case, but we need to make sure that we don't
		 * continue to hold the handle lock, since technically, we
		 * should not have been allowed to open it.
		 */
		if (LF_ISSET(DB_EXCL)) {
			ret = __ENV_LPUT(dbenv, dbp->handle_lock, 0);
			LOCK_INIT(dbp->handle_lock);
			if (ret == 0)
				ret = EEXIST;
			goto err;
		}
		goto done;
	}

	/* File does not exist. */
	if (!LF_ISSET(DB_CREATE))
		goto err;
	ret = 0;

	/*
	 * We need to create file, which means that we need to set up the file,
	 * the fileid and the locks.  Then we need to call the appropriate
	 * routines to create meta-data pages.
	 */
	if ((ret = __ENV_LPUT(dbenv, elock, 0)) != 0)
		goto err;

create:	if (txn != NULL && IS_REP_CLIENT(dbenv)) {
		__db_err(dbenv,
		    "Transactional create on replication client disallowed");
		ret = EINVAL;
		goto err;
	}
	if ((ret = __db_backup_name(dbenv, name, txn, &tmpname)) != 0)
		goto err;
	if (TXN_ON(dbenv) && txn != NULL &&
	    (ret = __txn_begin(dbenv, txn, &stxn, 0)) != 0)
		goto err;
	if ((ret = __fop_create(dbenv,
	    stxn, &fhp, tmpname, DB_APP_DATA, mode, dflags)) != 0) {
		/*
		 * If we don't have transactions there is a race on
		 * creating the temp file.
		 */
		if (!TXN_ON(dbenv) && ret == EEXIST) {
			__os_free(dbenv, tmpname);
			tmpname = NULL;
			__os_yield(dbenv, 1);
			goto retry;
		}
		goto err;
	}
	tmp_created = 1;

creat2:	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, tmpname, 0, NULL, &real_tmpname)) != 0)
		goto err;

	/* Set the pagesize if it isn't yet set. */
	if (dbp->pgsize == 0 &&
	    (ret = __fop_set_pgsize(dbp, fhp, real_tmpname)) != 0)
		goto errmsg;

	/* Construct a file_id. */
	if ((ret = __os_fileid(dbenv, real_tmpname, 1, dbp->fileid)) != 0)
		goto errmsg;

	if ((ret = __db_new_file(dbp, stxn, fhp, tmpname)) != 0)
		goto err;

	/*
	 * We need to close the handle here on platforms where remove and
	 * rename fail if a handle is open (including Windows).
	 */
	CLOSE_HANDLE(dbp, fhp);

	/*
	 * Now move the file into place unless we are creating in place (because
	 * we created a database in a file that started out 0-length).
	 */
	if (!F_ISSET(dbp, DB_AM_COMPENSATE) && !F_ISSET(dbp, DB_AM_RECOVER))
		GET_ENVLOCK(dbenv, locker, &elock);

	if (F_ISSET(dbp, DB_AM_IN_RENAME)) {
		F_CLR(dbp, DB_AM_IN_RENAME);
		__txn_remrem(dbenv, txn, real_name);
	} else if (name == tmpname) {
		/* We created it in place. */
	} else if (__os_exists(real_name, NULL) == 0) {
		/*
		 * Someone managed to create the file; remove our temp
		 * and try to open the file that now exists.
		 */
		(void)__fop_remove(dbenv,
		    NULL, dbp->fileid, tmpname, DB_APP_DATA, dflags);
		(void)__ENV_LPUT(dbenv, dbp->handle_lock, 0);
		LOCK_INIT(dbp->handle_lock);

		if (stxn != NULL) {
			ret = __txn_abort(stxn);
			stxn = NULL;
		}
		if (ret != 0)
			goto err;
		goto reopen;
	}

	if ((ret = __fop_lock_handle(dbenv,
	    dbp, locker, DB_LOCK_WRITE, &elock, NOWAIT_FLAG(txn))) != 0)
		goto err;
	if (tmpname != name && (ret = __fop_rename(dbenv,
	    stxn, tmpname, name, dbp->fileid, DB_APP_DATA, dflags)) != 0)
		goto err;

	if (stxn != NULL) {
		*retidp = stxn->txnid;
		ret = __txn_commit(stxn, 0);
		stxn = NULL;
	} else
		*retidp = TXN_INVALID;

	if (ret != 0)
		goto err;

	F_SET(dbp, DB_AM_CREATED);

	if (0) {
errmsg:		__db_err(dbenv, "%s: %s", name, db_strerror(ret));

err:		CLOSE_HANDLE(dbp, fhp);
		if (stxn != NULL)
			(void)__txn_abort(stxn);
		if (tmp_created && txn == NULL)
			(void)__fop_remove(dbenv,
			    NULL, NULL, tmpname, DB_APP_DATA, dflags);
		if (txn == NULL)
			(void)__ENV_LPUT(dbenv, dbp->handle_lock, 0);
		(void)__ENV_LPUT(dbenv, elock, 0);
		if (created_locker) {
			(void)__lock_id_free(dbenv, dbp->lid);
			dbp->lid = DB_LOCK_INVALIDID;
		}
	}

done:	/*
	 * There are cases where real_name and tmpname take on the
	 * exact same string, so we need to make sure that we do not
	 * free twice.
	 */
	if (!truncating && tmpname != NULL && tmpname != name)
		__os_free(dbenv, tmpname);
	if (real_name != NULL)
		__os_free(dbenv, real_name);
	if (real_tmpname != NULL)
		__os_free(dbenv, real_tmpname);
	CLOSE_HANDLE(dbp, fhp);

	return (ret);
}

/*
 * __fop_set_pgsize --
 *	Set the page size based on file information.
 */
static int
__fop_set_pgsize(dbp, fhp, name)
	DB *dbp;
	DB_FH *fhp;
	const char *name;
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
	 * we want a multiple of the sector size as well.  If the value
	 * we got out of __os_ioinfo looks bad, use a default instead.
	 */
	if (!IS_VALID_PAGESIZE(iopsize))
		iopsize = DB_DEF_IOSIZE;

	dbp->pgsize = iopsize;
	F_SET(dbp, DB_AM_PGDEF);

	return (0);
}

/*
 * __fop_subdb_setup --
 *
 * Subdb setup is significantly simpler than file setup.  In terms of
 * locking, for the duration of the operation/transaction, the locks on
 * the meta-data page will suffice to protect us from simultaneous operations
 * on the sub-database.  Before we complete the operation though, we'll get a
 * handle lock on the subdatabase so that on one else can try to remove it
 * while we've got it open.  We use an object that looks like the meta-data
 * page lock with a different type (DB_HANDLE_LOCK) for the long-term handle.
 * locks.
 *
 * PUBLIC: int __fop_subdb_setup __P((DB *, DB_TXN *,
 * PUBLIC:     const char *, const char *, int, u_int32_t));
 */
int
__fop_subdb_setup(dbp, txn, mname, name, mode, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *mname, *name;
	int mode;
	u_int32_t flags;
{
	DB *mdbp;
	DB_ENV *dbenv;
	db_lockmode_t lkmode;
	int ret, t_ret;

	mdbp = NULL;
	dbenv = dbp->dbenv;

	if ((ret = __db_master_open(dbp, txn, mname, flags, mode, &mdbp)) != 0)
		return (ret);
	/*
	 * If we created this file, then we need to set the DISCARD flag so
	 * that if we fail in the middle of this routine, we discard from the
	 * mpool any pages that we just created.
	 */
	if (F_ISSET(mdbp, DB_AM_CREATED))
		F_SET(mdbp, DB_AM_DISCARD);

	/*
	 * We are going to close this instance of the master, so we can
	 * steal its handle instead of reopening a handle on the database.
	 */
	if (LF_ISSET(DB_FCNTL_LOCKING)) {
		dbp->saved_open_fhp = mdbp->saved_open_fhp;
		mdbp->saved_open_fhp = NULL;
	}

	/* Copy the pagesize and set the sub-database flag. */
	dbp->pgsize = mdbp->pgsize;
	F_SET(dbp, DB_AM_SUBDB);

	if (name != NULL && (ret = __db_master_update(mdbp, dbp, txn,
	    name, dbp->type, MU_OPEN, NULL, flags)) != 0)
		goto err;

	/*
	 * Hijack the master's locker ID as well, so that our locks don't
	 * conflict with the master's.  Since we're closing the master,
	 * that lid would just have been freed anyway.  Once we've gotten
	 * the locker id, we need to acquire the handle lock for this
	 * subdatabase.
	 */
	dbp->lid = mdbp->lid;
	mdbp->lid = DB_LOCK_INVALIDID;

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTLOG, ret, mname);

	/*
	 * We copy our fileid from our master so that we all open
	 * the same file in mpool.  We'll use the meta-pgno to lock
	 * so that we end up with different handle locks.
	 */

	memcpy(dbp->fileid, mdbp->fileid, DB_FILE_ID_LEN);
	lkmode = F_ISSET(dbp, DB_AM_CREATED) || LF_ISSET(DB_WRITEOPEN) ?
	    DB_LOCK_WRITE : DB_LOCK_READ;
	if ((ret = __fop_lock_handle(dbenv, dbp,
	    txn == NULL ? dbp->lid : txn->txnid, lkmode, NULL,
	    NOWAIT_FLAG(txn))) != 0)
		goto err;

	if ((ret = __db_init_subdb(mdbp, dbp, name, txn)) != 0) {
		/*
		 * If there was no transaction and we created this database,
		 * then we need to undo the update of the master database.
		 */
		if (F_ISSET(dbp, DB_AM_CREATED) && txn != NULL)
			(void)__db_master_update(mdbp, dbp, txn,
			    name, dbp->type, MU_REMOVE, NULL, 0);
		F_CLR(dbp, DB_AM_CREATED);
		goto err;
	}

	/*
	 * XXX
	 * This should have been done at the top of this routine.  The problem
	 * is that __db_init_subdb() uses "standard" routines to process the
	 * meta-data page and set information in the DB handle based on it.
	 * Those routines have to deal with swapped pages and will normally set
	 * the DB_AM_SWAP flag.  However, we use the master's metadata page and
	 * that has already been swapped, so they get the is-swapped test wrong.
	 */
	F_CLR(dbp, DB_AM_SWAP);
	F_SET(dbp, F_ISSET(mdbp, DB_AM_SWAP));

	/*
	 * In the file create case, these happen in separate places so we have
	 * two different tests.  They end up in the same place for subdbs, but
	 * for compatibility with file testing, we put them both here anyway.
	 */
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTLOGMETA, ret, mname);
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTSYNC, ret, mname);

	/*
	 * File exists and we have the appropriate locks; we should now
	 * process a normal open.
	 */
	if (F_ISSET(mdbp, DB_AM_CREATED)) {
		F_SET(dbp, DB_AM_CREATED_MSTR);
		F_CLR(mdbp, DB_AM_DISCARD);
	}

	if (0) {
err:
DB_TEST_RECOVERY_LABEL
		if (txn == NULL)
			(void)__ENV_LPUT(dbenv, dbp->handle_lock, 0);
	}

	/*
	 * The master's handle lock is under the control of the
	 * subdb (it acquired the master's locker).  We want to
	 * keep the master's handle lock so that no one can remove
	 * the file while the subdb is open.  If we register the
	 * trade event and then invalidate the copy of the lock
	 * in the master's handle, that will accomplish this.  However,
	 * before we register this event, we'd better remove any
	 * events that we've already registered for the master.
	 */
	if (!F_ISSET(dbp, DB_AM_RECOVER) && txn != NULL) {
		/* Unregister old master events. */
		 __txn_remlock(dbenv,
		    txn, &mdbp->handle_lock, DB_LOCK_INVALIDID);

		/* Now register the new event. */
		if ((t_ret = __txn_lockevent(dbenv, txn, dbp,
		    &mdbp->handle_lock, dbp->lid == DB_LOCK_INVALIDID ?
		    mdbp->lid : dbp->lid)) != 0 && ret == 0)
			ret = t_ret;
	}
	LOCK_INIT(mdbp->handle_lock);

	/*
	 * If the master was created, we need to sync so that the metadata
	 * page is correct on disk for recovery, since it isn't read through
	 * mpool.  If we're opening a subdb in an existing file, we can skip
	 * the sync.
	 */
	if ((t_ret =__db_close(mdbp, txn,
	    F_ISSET(dbp, DB_AM_CREATED_MSTR) ? 0 : DB_NOSYNC)) != 0 &&
	    ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __fop_remove_setup --
 *	Open handle appropriately and lock for removal of a database file.
 *
 * PUBLIC: int __fop_remove_setup __P((DB *,
 * PUBLIC:      DB_TXN *, const char *, u_int32_t));
 */
int
__fop_remove_setup(dbp, txn, name, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *name;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_FH *fhp;
	DB_LOCK elock;
	u_int32_t refcnt;
	u_int8_t mbuf[DBMETASIZE];
	int ret;

	COMPQUIET(flags, 0);
	dbenv = dbp->dbenv;
	PANIC_CHECK(dbenv);
	LOCK_INIT(elock);
	fhp = NULL;

	/* Create locker if necessary. */
retry:	if (LOCKING_ON(dbenv)) {
		if (txn != NULL)
			dbp->lid = txn->txnid;
		else if (dbp->lid == DB_LOCK_INVALIDID) {
			if ((ret = __lock_id(dbenv, &dbp->lid)) != 0)
				goto err;
		}
	}

	/*
	 * We are about to open a file handle and then possibly close it.
	 * We cannot close handles if we are doing FCNTL locking.  However,
	 * there is no way to pass the FCNTL flag into this routine via the
	 * user API.  The only way we can get in here and be doing FCNTL
	 * locking is if we are trying to clean up an open that was called
	 * with FCNTL locking.  In that case, the save_fhp should already be
	 * set.  So, we use that field to tell us if we need to make sure
	 * that we shouldn't close the handle.
	 */
	fhp = dbp->saved_open_fhp;
	DB_ASSERT(LF_ISSET(DB_FCNTL_LOCKING) || fhp == NULL);

	/*
	 * Lock environment to protect file open.  That will enable us to
	 * read the meta-data page and get the fileid so that we can lock
	 * the handle.
	 */
	GET_ENVLOCK(dbenv, dbp->lid, &elock);
	if (fhp == NULL &&
	    (ret = __os_open(dbenv, name, DB_OSO_RDONLY, 0, &fhp)) != 0)
		goto err;
	if ((ret = __fop_read_meta(dbenv,
	    name, mbuf, sizeof(mbuf), fhp, 0, NULL)) != 0)
		goto err;

	if ((ret =
	    __db_meta_setup(dbenv, dbp, name, (DBMETA *)mbuf, flags, 1)) != 0)
		goto err;

	/*
	 * Now, get the handle lock.  We first try with NOWAIT, because if
	 * we have to wait, we're going to have to close the file and reopen
	 * it, so that if there is someone else removing it, our open doesn't
	 * prevent that.
	 */
	if ((ret = __fop_lock_handle(dbenv,
	    dbp, dbp->lid, DB_LOCK_WRITE, NULL, DB_LOCK_NOWAIT)) != 0) {
		/*
		 * Close the file, block on the lock, clean up the dbp, and
		 * then start all over again.
		 */
		if (!LF_ISSET(DB_FCNTL_LOCKING)) {
			(void)__os_closehandle(dbenv, fhp);
			fhp = NULL;
		}
		if (ret == DB_LOCK_NOTEXIST) {
			if ((ret = __ENV_LPUT(dbenv, elock, 0)) != 0)
				goto err;
		} else if (ret != DB_LOCK_NOTGRANTED ||
		    (txn != NULL && F_ISSET(txn, TXN_NOWAIT)))
			goto err;
		else if ((ret = __fop_lock_handle(dbenv,
		    dbp, dbp->lid, DB_LOCK_WRITE, &elock, 0)) != 0 &&
		    ret != DB_LOCK_NOTEXIST)
			goto err;

		if (txn != NULL)
			dbp->lid = DB_LOCK_INVALIDID;
		(void)__db_refresh(dbp, txn, DB_NOSYNC, NULL);
		goto retry;
	} else if ((ret = __ENV_LPUT(dbenv, elock, 0)) != 0)
		goto err;

	/* Check if the file is already open. */
	if ((ret = __memp_get_refcnt(dbenv, dbp->fileid, &refcnt)) != 0)
		goto err;
	/*
	 * Now, error check.  If the file is already open (refcnt != 0), then
	 * we must have it open (since we got the lock) and we need to panic,
	 * because this is a self deadlock and the application has a bug.
	 * If the file isn't open, but it's in the midst of a rename then
	 * this file doesn't really exist.
	 */
	if (refcnt != 0) {
		__db_err(dbenv,
"Attempting to remove file open in current transaction causing self-deadlock");
		ret = __db_panic(dbenv, DB_LOCK_DEADLOCK);
	} else if (F_ISSET(dbp, DB_AM_IN_RENAME))
		ret = ENOENT;

	if (0) {
err:		(void)__ENV_LPUT(dbenv, elock, 0);
	}
	if (fhp != NULL && !LF_ISSET(DB_FCNTL_LOCKING))
		(void)__os_closehandle(dbenv, fhp);
	/*
	 * If we are going to proceed with the removal, then we need to make
	 * sure that we don't leave any pages around in the mpool.
	 */
	if (ret == 0)
		F_SET(dbp, DB_AM_DISCARD);
	return (ret);
}

/*
 * __fop_read_meta --
 *	Read the meta-data page from a file and return it in buf.
 *
 * PUBLIC: int __fop_read_meta __P((DB_ENV *, const char *,
 * PUBLIC:     u_int8_t *, size_t, DB_FH *, int, size_t *));
 */
int
__fop_read_meta(dbenv, name, buf, size, fhp, errok, nbytesp)
	DB_ENV *dbenv;
	const char *name;
	u_int8_t *buf;
	size_t size;
	DB_FH *fhp;
	int errok;
	size_t *nbytesp;
{
	size_t nr;
	int ret;

	/*
	 * Our caller wants to know the number of bytes read, even if we
	 * return an error.
	 */
	if (nbytesp != NULL)
		*nbytesp = 0;

	nr = 0;
	ret = __os_read(dbenv, fhp, buf, size, &nr);
	if (nbytesp != NULL)
		*nbytesp = nr;

	if (ret != 0) {
		if (!errok)
			__db_err(dbenv, "%s: %s", name, db_strerror(ret));
		goto err;
	}

	if (nr != size) {
		if (!errok)
			__db_err(dbenv,
			    "%s: unexpected file type or format", name);
		ret = EINVAL;
	}

err:
	return (ret);
}

/*
 * __fop_dummy --
 *	This implements the creation and name swapping of dummy files that
 * we use for remove and rename (remove is simply a rename with a delayed
 * remove).
 *
 * PUBLIC: int __fop_dummy __P((DB *,
 * PUBLIC:     DB_TXN *, const char *, const char *, u_int32_t));
 */
int
__fop_dummy(dbp, txn, old, new, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *old, *new;
	u_int32_t flags;
{
	DB *tmpdbp, *t2dbp;
	DB_ENV *dbenv;
	DB_FH *fhp;
	DB_LOCK elock;
	DB_LSN lsn;
	DBT fiddbt, namedbt, tmpdbt;
	DB_TXN *stxn;
	char *back;
	char *realback, *realnew, *realold;
	int ret, t_ret;
	size_t len;
	u_int8_t mbuf[DBMETASIZE];
	u_int32_t dflags, locker, stxnid;

	dbenv = dbp->dbenv;
	LOCK_INIT(elock);
	realback = NULL;
	realnew = NULL;
	realold = NULL;
	back = NULL;
	stxn = NULL;
	tmpdbp = t2dbp = NULL;
	fhp = NULL;
	dflags = F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0;

	DB_ASSERT(txn != NULL);
	locker = txn->txnid;

	/* Begin sub transaction to encapsulate the rename. */
	if (TXN_ON(dbenv) && (ret = __txn_begin(dbenv, txn, &stxn, 0)) != 0)
		goto err;

	/* We need to create a dummy file as a place holder. */
	if ((ret = __db_backup_name(dbenv, new, stxn, &back)) != 0)
		goto err;
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, back, flags, NULL, &realback)) != 0)
		goto err;
	if ((ret = __fop_create(dbenv,
	    stxn, NULL, back, DB_APP_DATA, 0, dflags)) != 0)
		goto err;

	memset(mbuf, 0, sizeof(mbuf));
	if ((ret =
	    __os_fileid(dbenv, realback, 1, ((DBMETA *)mbuf)->uid)) != 0)
		goto err;
	((DBMETA *)mbuf)->magic = DB_RENAMEMAGIC;
	if ((ret = __fop_write(dbenv, stxn, back,
	    DB_APP_DATA, NULL, 0, 0, 0, mbuf, DBMETASIZE, 1, dflags)) != 0)
		goto err;

	/* Create a dummy dbp handle. */
	if ((ret = db_create(&tmpdbp, dbenv, 0)) != 0)
		goto err;
	memcpy(tmpdbp->fileid, ((DBMETA *)mbuf)->uid, DB_FILE_ID_LEN);

	/* Now, lock the name space while we initialize this file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, new, 0, NULL, &realnew)) != 0)
		goto err;
	GET_ENVLOCK(dbenv, locker, &elock);
	if (__os_exists(realnew, NULL) == 0) {
		/*
		 * It is possible that the only reason this file exists is
		 * because we've done a previous rename of it and we have
		 * left a placeholder here.  We need to check for that case
		 * and allow this rename to succeed if that's the case.
		 */
		if ((ret = db_create(&t2dbp, dbenv, 0)) != 0)
			goto err;
		if ((ret = __os_open(dbenv, realnew, 0, 0, &fhp)) != 0)
			goto err;
		if ((ret = __fop_read_meta(dbenv,
		    realnew, mbuf, sizeof(mbuf), fhp, 0, &len)) != 0 ||
		    (ret = __db_meta_setup(dbenv,
		    t2dbp, realnew, (DBMETA *)mbuf, 0, 1)) != 0) {
			ret = EEXIST;
			goto err;
		}

		/*
		 * Now, try to acquire the handle lock.  If it's from our txn,
		 * then we'll get the lock.  If it's not, then someone else has
		 * it locked,  and we need to report this as an error.  If we
		 * know we can get the lock, we can immediately release it,
		 * which we need to do since this is a temporary handle.
		 */
		if ((ret = __fop_lock_handle(dbenv,
		    t2dbp, locker, DB_LOCK_WRITE, NULL, DB_LOCK_NOWAIT)) != 0)
			ret = EEXIST;
		else {
			(void)__lock_put(dbenv, &t2dbp->handle_lock, 0);
			if (!F_ISSET(t2dbp, DB_AM_IN_RENAME))
				ret = EEXIST;
		}
		if ((t_ret = __os_closehandle(dbenv, fhp)) != 0 && ret == 0)
			ret = t_ret;
		fhp = NULL;
		if (ret != 0)
			goto err;
	}

	/*
	 * While we have the namespace locked, do the renames and then
	 * swap for the handle lock.
	 */
	if ((ret = __fop_rename(dbenv,
	    stxn, old, new, dbp->fileid, DB_APP_DATA, dflags)) != 0)
		goto err;
	if ((ret = __fop_rename(dbenv,
	    stxn, back, old, tmpdbp->fileid, DB_APP_DATA, dflags)) != 0)
		goto err;
	if ((ret = __fop_lock_handle(dbenv,
	    tmpdbp, locker, DB_LOCK_WRITE, &elock, NOWAIT_FLAG(txn))) != 0)
		goto err;

	/*
	 * We just acquired a transactional lock on the tmp handle.
	 * We need to null out the tmp handle's lock so that it
	 * doesn't create problems for us in the close path.
	 */
	LOCK_INIT(tmpdbp->handle_lock);

	if (stxn != NULL) {
		/* Commit the child. */
		stxnid = stxn->txnid;
		ret = __txn_commit(stxn, 0);
		stxn = NULL;

		/* Now log the child information in the parent. */
		memset(&fiddbt, 0, sizeof(fiddbt));
		memset(&tmpdbt, 0, sizeof(fiddbt));
		memset(&namedbt, 0, sizeof(namedbt));
		fiddbt.data = dbp->fileid;
		fiddbt.size = DB_FILE_ID_LEN;
		tmpdbt.data = tmpdbp->fileid;
		tmpdbt.size = DB_FILE_ID_LEN;
		namedbt.data = (void *)old;
		namedbt.size = (u_int32_t)strlen(old) + 1;
		if ((t_ret =
		    __fop_file_remove_log(dbenv, txn, &lsn, 0, &fiddbt,
		    &tmpdbt, &namedbt, DB_APP_DATA, stxnid)) != 0 && ret == 0)
			ret = t_ret;
	}

	/* This is a delayed delete of the dummy file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, old, flags, NULL, &realold)) != 0)
		goto err;
	if ((ret = __txn_remevent(dbenv, txn, realold, NULL)) != 0)
		goto err;

err:	(void)__ENV_LPUT(dbenv, elock, 0);
	if (stxn != NULL)
		(void)__txn_abort(stxn);
	if (tmpdbp != NULL &&
	    (t_ret = __db_close(tmpdbp, NULL, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (t2dbp != NULL &&
	    (t_ret = __db_close(t2dbp, NULL, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);
	if (realold != NULL)
		__os_free(dbenv, realold);
	if (realnew != NULL)
		__os_free(dbenv, realnew);
	if (realback != NULL)
		__os_free(dbenv, realback);
	if (back != NULL)
		__os_free(dbenv, back);
	return (ret);
}

/*
 * __fop_dbrename --
 *	Do the appropriate file locking and file system operations
 * to effect a dbrename in the absence of transactions (__fop_dummy
 * and the subsequent calls in __db_rename do the work for the
 * transactional case).
 *
 * PUBLIC: int __fop_dbrename __P((DB *, const char *, const char *));
 */
int
__fop_dbrename(dbp, old, new)
	DB *dbp;
	const char *old, *new;
{
	DB_ENV *dbenv;
	DB_LOCK elock;
	char *real_new, *real_old;
	int ret, t_ret;

	dbenv = dbp->dbenv;
	real_new = NULL;
	real_old = NULL;
	LOCK_INIT(elock);

	/* Find the real newname of the file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, new, 0, NULL, &real_new)) != 0)
		goto err;

	/*
	 * It is an error to rename a file over one that already exists,
	 * as that wouldn't be transaction-safe.
	 */
	GET_ENVLOCK(dbenv, dbp->lid, &elock);
	if (__os_exists(real_new, NULL) == 0) {
		ret = EEXIST;
		__db_err(dbenv, "rename: file %s exists", real_new);
		goto err;
	}

	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, old, 0, NULL, &real_old)) != 0)
		goto err;

	ret = __memp_nameop(dbenv, dbp->fileid, new, real_old, real_new);

err:	if ((t_ret = __ENV_LPUT(dbenv, elock, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (real_old != NULL)
		__os_free(dbenv, real_old);
	if (real_new != NULL)
		__os_free(dbenv, real_new);
	return (ret);
}
