/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: fop_util.c,v 1.52 2002/09/10 02:41:42 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/fop.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __fop_set_pgsize __P((DB *, DB_FH *, const char *));

/*
 * Acquire the environment meta-data lock.  The parameters are the
 * environment (ENV), the locker id to use in acquiring the lock (ID)
 * and a pointer to a DB_LOCK.
 */
#define	GET_ENVLOCK(ENV, ID, L) do {					\
	DBT __dbt;							\
	u_int32_t __lockval;						\
									\
	if (LOCKING_ON((ENV))) {					\
		__lockval = 0;						\
		__dbt.data = &__lockval;				\
		__dbt.size = sizeof(__lockval);				\
		if ((ret = (ENV)->lock_get((ENV), (ID),			\
		    0, &__dbt, DB_LOCK_WRITE, (L))) != 0)		\
			goto err;					\
	}								\
} while (0)

#define	REL_ENVLOCK(ENV, L)						\
	(!LOCK_ISSET(*(L)) ? 0 : (ENV)->lock_put((ENV), (L)))

/*
 * If our caller is doing fcntl(2) locking, then we can't close it
 * because that would discard the caller's lock.  Otherwise, close
 * the handle.
 */
#define	CLOSE_HANDLE(D, F) {						\
	if (F_ISSET((F), DB_FH_VALID)) {				\
		if (LF_ISSET(DB_FCNTL_LOCKING))				\
			(D)->saved_open_fhp = (F);			\
		else if ((ret = __os_closehandle((D)->dbenv,(F))) != 0)	\
			goto err;					\
	}								\
}

/*
 * __fop_lock_handle --
 *
 * Get the handle lock for a database.  If the envlock is specified,
 * do this as a lock_vec call that releases the enviroment lock before
 * acquiring the handle lock.
 *
 * PUBLIC: int __fop_lock_handle __P((DB_ENV *,
 * PUBLIC:     DB *, u_int32_t, db_lockmode_t, DB_LOCK *, u_int32_t));
 *
 */
int
__fop_lock_handle(dbenv, dbp, locker, mode, elock, flags)
	DB_ENV *dbenv;
	DB *dbp;
	u_int32_t locker;
	db_lockmode_t mode;
	DB_LOCK *elock;
	u_int32_t flags;
{
	DBT fileobj;
	DB_LOCKREQ reqs[2], *ereq;
	DB_LOCK_ILOCK lock_desc;
	int ret;

	if (!LOCKING_ON(dbenv) || F_ISSET(dbp, DB_AM_COMPENSATE))
		return (0);

	/*
	 * If we are in recovery, the only locking we should be
	 * doing is on the global environment.
	 */
	if (IS_RECOVERING(dbenv)) {
		if (elock != NULL)
			REL_ENVLOCK(dbenv, elock);
		return (0);
	}

	memcpy(&lock_desc.fileid, &dbp->fileid, DB_FILE_ID_LEN);
	lock_desc.pgno = dbp->meta_pgno;
	lock_desc.type = DB_HANDLE_LOCK;

	memset(&fileobj, 0, sizeof(fileobj));
	fileobj.data = &lock_desc;
	fileobj.size = sizeof(lock_desc);
	DB_TEST_SUBLOCKS(dbenv, flags);
	if (elock == NULL)
		ret = dbenv->lock_get(dbenv, locker,
		    flags, &fileobj, mode, &dbp->handle_lock);
	else {
		reqs[0].op = DB_LOCK_PUT;
		reqs[0].lock = *elock;
		reqs[1].op = DB_LOCK_GET;
		reqs[1].mode = mode;
		reqs[1].obj = &fileobj;
		reqs[1].timeout = 0;
		if ((ret = __lock_vec(dbenv,
		    locker, flags, reqs, 2, &ereq)) == 0) {
			dbp->handle_lock = reqs[1].lock;
			LOCK_INIT(*elock);
		} else if (ereq != reqs)
			LOCK_INIT(*elock);
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
	DB_FH fh, *fhp;
	DB_LOCK elock, tmp_lock;
	DB_TXN *stxn;
	db_lockmode_t lmode;
	u_int32_t locker, oflags;
	u_int8_t mbuf[DBMETASIZE];
	int created_fhp, created_locker, ret, tmp_created, truncating;
	char *real_name, *real_tmpname, *tmpname;

	DB_ASSERT(name != NULL);

	*retidp = TXN_INVALID;

	dbenv = dbp->dbenv;
	LOCK_INIT(elock);
	LOCK_INIT(tmp_lock);
	stxn = NULL;
	created_fhp = created_locker = 0;
	real_name = real_tmpname = tmpname = NULL;
	tmp_created = truncating = 0;

	/*
	 * If we open a file handle and our caller is doing fcntl(2) locking,
	 * we can't close it because that would discard the caller's lock.
	 * Save it until we close the DB handle.
	 */
	if (LF_ISSET(DB_FCNTL_LOCKING)) {
		if ((ret = __os_malloc(dbenv, sizeof(*fhp), &fhp)) != 0)
			return (ret);
		created_fhp = 1;
	} else
		fhp = &fh;
	memset(fhp, 0, sizeof(*fhp));

	/*
	 * Get a lockerid for this handle.  There are paths through queue
	 * rename and remove where this dbp already has a locker, so make
	 * sure we don't clobber it and conflict.
	 */
	if (LOCKING_ON(dbenv) &&
	    !F_ISSET(dbp, DB_AM_COMPENSATE) && dbp->lid == DB_LOCK_INVALIDID) {
		if ((ret = __lock_id(dbenv, &dbp->lid)) != 0)
			goto err;
		created_locker = 1;
	}

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

retry:	if (!F_ISSET(dbp, DB_AM_COMPENSATE))
		GET_ENVLOCK(dbenv, locker, &elock);
	if ((ret = __os_exists(real_name, NULL)) == 0) {
		if (LF_ISSET(DB_EXCL)) {
			ret = EEXIST;
			goto err;
		}
reopen:		if ((ret = __fop_read_meta(dbenv, real_name,
		    mbuf, sizeof(mbuf), fhp, 0, oflags)) != 0)
			goto err;

		if ((ret = __db_meta_setup(dbenv,
		    dbp, real_name, (DBMETA *)mbuf, flags, 1)) != 0)
			goto err;

		/* Now, get our handle lock. */
		lmode = LF_ISSET(DB_TRUNCATE) ? DB_LOCK_WRITE : DB_LOCK_READ;
		if ((ret = __fop_lock_handle(dbenv,
		    dbp, locker, lmode, NULL, DB_LOCK_NOWAIT)) == 0) {
			if ((ret = REL_ENVLOCK(dbenv, &elock)) != 0)
				goto err;
		} else {
			/* Someone else has file locked; need to wait. */
			if ((ret = __os_closehandle(dbenv, fhp)) != 0)
				goto err;
			ret = __fop_lock_handle(dbenv,
			    dbp, locker, lmode, &elock, 0);
			if (ret == DB_LOCK_NOTEXIST)
				goto retry;
			if (ret != 0)
				goto err;
			/*
			 * XXX I need to convince myself that I don't need
			 * to re-read the metadata page here.
			 * XXX If you do need to re-read it you'd better
			 * decrypt it too...
			 */
			if ((ret = __os_open(dbenv, real_name, 0, 0, fhp)) != 0)
				goto err;
		}

		/*
		 * Check for a truncate which needs to leap over to the
		 * create case.
		 */
		if (LF_ISSET(DB_TRUNCATE)) {
			/*
			 * Sadly, we need to close and reopen the handle
			 * in order to do the actual truncate.  We couldn't
			 * do the truncate on the initial open because we
			 * needed to read the old file-id in order to lock.
			 */
			if ((ret = __os_closehandle(dbenv, fhp)) != 0)
				goto err;
			if ((ret = __os_open(dbenv,
			    real_name, DB_OSO_TRUNC, 0, fhp)) != 0)
				goto err;
			/*
			 * This is not-transactional, so we'll do the
			 * open/create in-place.
			 */
			tmp_lock = dbp->handle_lock;
			truncating = 1;
			tmpname = (char *)name;
			goto creat2;
		}

		/*
		 * Check for a file in the midst of a rename
		 */
		if (F_ISSET(dbp, DB_AM_IN_RENAME)) {
			if (LF_ISSET(DB_CREATE)) {
				F_CLR(dbp, DB_AM_IN_RENAME);
				goto create;
			} else {
				ret = ENOENT;
				goto err;
			}
		}

		CLOSE_HANDLE(dbp, fhp);
		goto done;
	}

	/* File does not exist. */
	if (!LF_ISSET(DB_CREATE))
		goto err;
	ret = 0;

	/*
	 * Need to create file; we need to set up the file,
	 * the fileid and the locks.  Then we need to call
	 * the appropriate routines to create meta-data pages.
	 */
	if ((ret = REL_ENVLOCK(dbenv, &elock)) != 0)
		goto err;

create:	if ((ret = __db_backup_name(dbenv, name, txn, &tmpname)) != 0)
		goto err;
	if (TXN_ON(dbenv) && txn != NULL &&
	    (ret = dbenv->txn_begin(dbenv, txn, &stxn, 0)) != 0)
		goto err;
	if ((ret = __fop_create(dbenv,
	    stxn, fhp, tmpname, DB_APP_DATA, mode)) != 0)
		goto err;
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
	CLOSE_HANDLE(dbp, fhp);

	/* Now move the file into place. */
	if (!F_ISSET(dbp, DB_AM_COMPENSATE))
		GET_ENVLOCK(dbenv, locker, &elock);
	if (!truncating && __os_exists(real_name, NULL) == 0) {
		/*
		 * Someone managed to create the file; remove our temp
		 * and try to open the file that now exists.
		 */
		(void)__fop_remove(dbenv,
		    NULL, dbp->fileid, tmpname, DB_APP_DATA);
		if (LOCKING_ON(dbenv))
			dbenv->lock_put(dbenv, &dbp->handle_lock);
		LOCK_INIT(dbp->handle_lock);

		/* If we have a saved handle; close it. */
		if (LF_ISSET(DB_FCNTL_LOCKING))
			(void)__os_closehandle(dbenv, fhp);
		if (stxn != NULL) {
			ret = stxn->abort(stxn);
			stxn = NULL;
		}
		if (ret != 0)
			goto err;
		goto reopen;
	}

	/* We've successfully created, move the file into place. */
	if ((ret = __fop_lock_handle(dbenv,
	    dbp, locker, DB_LOCK_WRITE, &elock, 0)) != 0)
		goto err;
	if (!truncating && (ret = __fop_rename(dbenv,
	    stxn, tmpname, name, dbp->fileid, DB_APP_DATA)) != 0)
		goto err;

	/* If this was a truncate; release lock on the old file. */
	if (LOCK_ISSET(tmp_lock) && (ret = __lock_put(dbenv, &tmp_lock)) != 0)
		goto err;

	if (stxn != NULL) {
		*retidp = stxn->txnid;
		ret = stxn->commit(stxn, 0);
		stxn = NULL;
	} else
		*retidp = TXN_INVALID;

	if (ret != 0)
		goto err;

	F_SET(dbp, DB_AM_CREATED);

	if (0) {
errmsg:		__db_err(dbenv, "%s: %s", name, db_strerror(ret));

err:		if (stxn != NULL)
			(void)stxn->abort(stxn);
		if (tmp_created && txn == NULL)
			(void)__fop_remove(dbenv,
			    NULL, NULL, tmpname, DB_APP_DATA);
		if (F_ISSET(fhp, DB_FH_VALID))
			(void)__os_closehandle(dbenv, fhp);
		if (LOCK_ISSET(tmp_lock))
			__lock_put(dbenv, &tmp_lock);
		if (LOCK_ISSET(dbp->handle_lock) && txn == NULL)
			__lock_put(dbenv, &dbp->handle_lock);
		if (LOCK_ISSET(elock))
			(void)REL_ENVLOCK(dbenv, &elock);
		if (created_locker) {
			(void)__lock_id_free(dbenv, dbp->lid);
			dbp->lid = DB_LOCK_INVALIDID;
		}
		if (created_fhp)
			__os_free(dbenv, fhp);
	}

done:	if (!truncating && tmpname != NULL)
		__os_free(dbenv, tmpname);
	if (real_name != NULL)
		__os_free(dbenv, real_name);
	if (real_tmpname != NULL)
		__os_free(dbenv, real_tmpname);

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
	int do_remove, ret;

	mdbp = NULL;
	dbenv = dbp->dbenv;

	if ((ret = __db_master_open(dbp, txn, mname, flags, mode, &mdbp)) != 0)
		return (ret);

	/*
	 * We are going to close this instance of the master, so we can
	 * steal its handle instead of reopening a handle on the database.
	 */
	if (LF_ISSET(DB_FCNTL_LOCKING)) {
		dbp->saved_open_fhp = mdbp->saved_open_fhp;
		mdbp->saved_open_fhp = NULL;
	}

	/* Now copy the pagesize. */
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
	if ((ret = __fop_lock_handle(dbenv, dbp,
	    txn == NULL ? dbp->lid : txn->txnid,
	    F_ISSET(dbp, DB_AM_CREATED) || LF_ISSET(DB_WRITEOPEN) ?
	    DB_LOCK_WRITE : DB_LOCK_READ, NULL, 0)) != 0)
		goto err;

	if ((ret = __db_init_subdb(mdbp, dbp, name, txn)) != 0)
		goto err;

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

	/*
	 * The master's handle lock is under the control of the
	 * subdb (it acquired the master's locker.  We want to
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
		if ((ret = __txn_lockevent(dbenv,
		    txn, dbp, &mdbp->handle_lock, dbp->lid)) != 0)
			goto err;
	}
	LOCK_INIT(mdbp->handle_lock);
	return (__db_close_i(mdbp, txn, 0));

err:
DB_TEST_RECOVERY_LABEL
	if (LOCK_ISSET(dbp->handle_lock) && txn == NULL)
		__lock_put(dbenv, &dbp->handle_lock);

	/* If we created the master file then we need to remove it.  */
	if (mdbp != NULL) {
		do_remove = F_ISSET(mdbp, DB_AM_CREATED) ? 1 : 0;
		if (do_remove)
			F_SET(mdbp, DB_AM_DISCARD);
		(void)__db_close_i(mdbp, txn, 0);
		if (do_remove) {
			(void)db_create(&mdbp, dbp->dbenv, 0);
			(void)__db_remove_i(mdbp, txn, mname, NULL);
		}
	}
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
	DB_LOCK elock;
	u_int8_t mbuf[DBMETASIZE];
	int ret;

	COMPQUIET(flags, 0);
	dbenv = dbp->dbenv;
	PANIC_CHECK(dbenv);
	LOCK_INIT(elock);

	/* Create locker if necessary. */
	if (LOCKING_ON(dbenv)) {
		if (txn != NULL)
			dbp->lid = txn->txnid;
		else if (dbp->lid == DB_LOCK_INVALIDID) {
			if ((ret = __lock_id(dbenv, &dbp->lid)) != 0)
				goto err;
		}
	}

	/*
	 * Lock environment to protect file open.  That will enable us to
	 * read the meta-data page and get the fileid so that we can lock
	 * the handle.
	 */
	GET_ENVLOCK(dbenv, dbp->lid, &elock);
	if ((ret = __fop_read_meta(dbenv,
	    name, mbuf, sizeof(mbuf), NULL, 0, 0)) != 0)
		goto err;

	if ((ret =
	    __db_meta_setup(dbenv, dbp, name, (DBMETA *)mbuf, flags, 1)) != 0)
		goto err;

	/* Now, release the environment and get the handle lock. */
	if ((ret = __fop_lock_handle(dbenv,
	    dbp, dbp->lid, DB_LOCK_WRITE, &elock, 0)) != 0)
		goto err;

	return (0);

err:	(void)REL_ENVLOCK(dbenv, &elock);
	return (ret);
}

/*
 * __fop_read_meta --
 *	Read the meta-data page from a file and return it in buf.  The
 * open file handle is returned in fhp.
 *
 * PUBLIC: int __fop_read_meta __P((DB_ENV *,
 * PUBLIC:     const char *, u_int8_t *, size_t, DB_FH *, int, u_int32_t));
 */
int
__fop_read_meta(dbenv, name, buf, size, fhp, errok, flags)
	DB_ENV *dbenv;
	const char *name;
	u_int8_t *buf;
	size_t size;
	DB_FH *fhp;
	int errok;
	u_int32_t flags;
{
	DB_FH fh, *lfhp;
	size_t nr;
	int ret;

	lfhp = fhp == NULL ? &fh : fhp;
	memset(lfhp, 0, sizeof(*fhp));
	if ((ret = __os_open(dbenv, name, flags, 0, lfhp)) != 0)
		goto err;
	if ((ret = __os_read(dbenv, lfhp, buf, size, &nr)) != 0) {
		if (!errok)
			__db_err(dbenv, "%s: %s", name, db_strerror(ret));
		goto err;
	}

	if (nr != size) {
		if (!errok)
			__db_err(dbenv,
			    "%s: unexpected file type or format", name);
		ret = EINVAL;
		goto err;
	}

err:	/*
	 * On error, we always close the handle.  If there is no error,
	 * then we only return the handle if the user didn't pass us
	 * a handle into which to return it.  If fhp is valid, then
	 * lfhp is the same as fhp.
	 */
	if (F_ISSET(lfhp, DB_FH_VALID) && (ret != 0 || fhp == NULL))
		__os_closehandle(dbenv, lfhp);
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
	DB *tmpdbp;
	DB_ENV *dbenv;
	DB_LOCK elock;
	DB_LSN lsn;
	DBT fiddbt, namedbt, tmpdbt;
	DB_TXN *stxn;
	char *back;
	char *realback, *realnew, *realold;
	int ret, t_ret;
	u_int8_t mbuf[DBMETASIZE];
	u_int32_t locker, stxnid;

	dbenv = dbp->dbenv;
	LOCK_INIT(elock);
	realback = NULL;
	realnew = NULL;
	realold = NULL;
	back = NULL;
	stxn = NULL;
	tmpdbp = NULL;

	DB_ASSERT(txn != NULL);
	locker = txn->txnid;

	/* Begin sub transaction to encapsulate the rename. */
	if (TXN_ON(dbenv) &&
	    (ret = dbenv->txn_begin(dbenv, txn, &stxn, 0)) != 0)
		goto err;

	/* We need to create a dummy file as a place holder. */
	if ((ret = __db_backup_name(dbenv, new, stxn, &back)) != 0)
		goto err;
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, back, flags, NULL, &realback)) != 0)
		goto err;
	if ((ret = __fop_create(dbenv, stxn, NULL, back, DB_APP_DATA, 0)) != 0)
		goto err;

	memset(mbuf, 0, sizeof(mbuf));
	if ((ret =
	    __os_fileid(dbenv, realback, 1, ((DBMETA *)mbuf)->uid)) != 0)
		goto err;
	((DBMETA *)mbuf)->magic = DB_RENAMEMAGIC;
	if ((ret = __fop_write(dbenv,
	    stxn, back, DB_APP_DATA, NULL, 0, mbuf, DBMETASIZE, 1)) != 0)
		goto err;

	/* Create a dummy dbp handle. */
	if ((ret = db_create(&tmpdbp, dbenv, 0)) != 0)
		goto err;
	memcpy(&tmpdbp->fileid, ((DBMETA *)mbuf)->uid, DB_FILE_ID_LEN);

	/* Now, lock the name space while we initialize this file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, new, 0, NULL, &realnew)) != 0)
		goto err;
	GET_ENVLOCK(dbenv, locker, &elock);
	if (__os_exists(realnew, NULL) == 0) {
		ret = EEXIST;
		goto err;
	}

	/*
	 * While we have the namespace locked, do the renames and then
	 * swap for the handle lock.
	 */
	if ((ret = __fop_rename(dbenv,
	    stxn, old, new, dbp->fileid, DB_APP_DATA)) != 0)
		goto err;
	if ((ret = __fop_rename(dbenv,
	    stxn, back, old, tmpdbp->fileid, DB_APP_DATA)) != 0)
		goto err;
	if ((ret = __fop_lock_handle(dbenv,
	    tmpdbp, locker, DB_LOCK_WRITE, &elock, 0)) != 0)
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
		ret = stxn->commit(stxn, 0);
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

err:	(void)REL_ENVLOCK(dbenv, &elock);
	if (stxn != NULL)
		(void)stxn->abort(stxn);
	if (tmpdbp != NULL &&
	    (t_ret = __db_close_i(tmpdbp, NULL, 0)) != 0 && ret == 0)
		ret = t_ret;
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
	int ret, tret;

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

	ret = dbenv->memp_nameop(dbenv, dbp->fileid, new, real_old, real_new);

err:	if ((tret = REL_ENVLOCK(dbenv, &elock)) != 0 && ret == 0)
		ret = tret;
	if (real_old != NULL)
		__os_free(dbenv, real_old);
	if (real_new != NULL)
		__os_free(dbenv, real_new);
	return (ret);
}
