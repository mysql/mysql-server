/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_open.c,v 11.215 2002/08/15 15:27:52 bostic Exp $";
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
#include "dbinc/db_swap.h"
#include "dbinc/btree.h"
#include "dbinc/crypto.h"
#include "dbinc/hmac.h"
#include "dbinc/fop.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/qam.h"
#include "dbinc/txn.h"

static int __db_openchk __P((DB *,
    DB_TXN *, const char *, const char *, DBTYPE, u_int32_t));

/*
 * __db_open --
 *	Main library interface to the DB access methods.
 *
 * PUBLIC: int __db_open __P((DB *, DB_TXN *,
 * PUBLIC:     const char *, const char *, DBTYPE, u_int32_t, int));
 */
int
__db_open(dbp, txn, name, subdb, type, flags, mode)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	DB_ENV *dbenv;
	int remove_master, remove_me, ret, t_ret, txn_local;

	dbenv = dbp->dbenv;
	remove_me = remove_master = txn_local = 0;

	PANIC_CHECK(dbenv);

	if ((ret = __db_openchk(dbp, txn, name, subdb, type, flags)) != 0)
		return (ret);

	/*
	 * Create local transaction as necessary, check for consistent
	 * transaction usage.
	 */
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto(dbp, &txn)) != 0)
			return (ret);
		txn_local = 1;
	} else
		if (txn != NULL && !TXN_ON(dbenv))
			return (__db_not_txn_env(dbenv));

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

	/* Convert any DB->open flags. */
	if (LF_ISSET(DB_RDONLY))
		F_SET(dbp, DB_AM_RDONLY);
	if (LF_ISSET(DB_DIRTY_READ))
		F_SET(dbp, DB_AM_DIRTY);

	/* Fill in the type. */
	dbp->type = type;

	/*
	 * If we're opening a subdatabase, we have to open (and potentially
	 * create) the main database, and then get (and potentially store)
	 * our base page number in that database.  Then, we can finally open
	 * the subdatabase.
	 */
	if ((ret = __db_dbopen(
	    dbp, txn, name, subdb, flags, mode, PGNO_BASE_MD)) != 0)
		goto err;

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

err:	/* If we were successful, don't discard the file on close. */
	if (ret == 0)
		/* If we were successful, don't discard the file on close. */
		F_CLR(dbp, DB_AM_DISCARD | DB_AM_CREATED | DB_AM_CREATED_MSTR);
	else {
		/*
		 * If we are not transactional, we need to remove the
		 * databases/subdatabases.  If we are transactional, then
		 * the abort of the child transaction should take care of
		 * cleaning them up.
		 */
		remove_me = txn == NULL && F_ISSET(dbp, DB_AM_CREATED);
		remove_master = txn == NULL && F_ISSET(dbp, DB_AM_CREATED_MSTR);

		/*
		 * If we had an error, it may have happened before or after
		 * we actually logged the open.  If it happened before, then
		 * abort won't know anything about it and won't close or
		 * refresh the dbp, so we need to do it explicitly.
		 */
		(void)__db_refresh(dbp, txn, DB_NOSYNC);
	}

	/* Remove anyone we created. */
	if (remove_master || (subdb == NULL && remove_me))
		/* Remove file. */
		(void)dbenv->dbremove(dbenv, txn, name, NULL, 0);
	else if (remove_me)
		/* Remove subdatabase. */
		(void)dbenv->dbremove(dbenv, txn, name, subdb, 0);

	/* Commit for DB_AUTO_COMMIT. */
	if (txn_local) {
		if (ret == 0)
			ret = txn->commit(txn, 0);
		else
			if ((t_ret = txn->abort(txn)) != 0)
				ret = __db_panic(dbenv, t_ret);
	}

	return (ret);
}

/*
 * __db_dbopen --
 *	Open a database.  This routine  gets called in three different ways.
 * 1. It can be called to open a file/database.  In this case, subdb will
 *    be NULL and meta_pgno will be PGNO_BASE_MD.
 * 2. It can be called to open a subdatabase during normal operation.  In
 *    this case, name and subname will both be non-NULL and meta_pgno will
 *    be PGNO_BAS_MD (also PGNO_INVALID).
 * 3. It can be called during recovery to open a subdatabase in which case
 *    name will be non-NULL, subname mqy be NULL and meta-pgno will be
 *    a valid pgno (i.e., not PGNO_BASE_MD).
 *
 * PUBLIC: int __db_dbopen __P((DB *, DB_TXN *,
 * PUBLIC:     const char *, const char *, u_int32_t, int, db_pgno_t));
 */
int
__db_dbopen(dbp, txn, name, subdb, flags, mode, meta_pgno)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	u_int32_t flags;
	int mode;
	db_pgno_t meta_pgno;
{
	DB_ENV *dbenv;
	int ret;
	u_int32_t id;

	dbenv = dbp->dbenv;
	id = TXN_INVALID;
	if (txn != NULL)
		F_SET(dbp, DB_AM_TXN);

	DB_TEST_RECOVERY(dbp, DB_TEST_PREOPEN, ret, name);
	/*
	 * If name is NULL, it's always a create, so make sure that we
	 * have a type specified.  It would be nice if this checking
	 * were done in __db_open where most of the interface checking
	 * is done, but this interface (__db_dbopen) is used by the
	 * recovery and limbo system, so we need to safeguard this
	 * interface as well.
	 */
	if (name == NULL) {
		F_SET(dbp, DB_AM_INMEM);

		if (dbp->type == DB_UNKNOWN) {
			__db_err(dbenv,
			    "DBTYPE of unknown without existing file");
			return (EINVAL);
		}

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
		if (LOCKING_ON(dbenv) && (ret = dbenv->lock_id(dbenv,
		    (u_int32_t *)dbp->fileid)) != 0)
			return (ret);
	} else if (subdb == NULL && meta_pgno == PGNO_BASE_MD) {
		/* Open/create the underlying file.  Acquire locks. */
		if ((ret =
		    __fop_file_setup(dbp, txn, name, mode, flags, &id)) != 0)
			return (ret);
	} else {
		if ((ret = __fop_subdb_setup(dbp,
		    txn, name, subdb, mode, flags)) != 0)
			return (ret);
		meta_pgno = dbp->meta_pgno;
	}

	/*
	 * If we created the file, set the truncate flag for the mpool.  This
	 * isn't for anything we've done, it's protection against stupid user
	 * tricks: if the user deleted a file behind Berkeley DB's back, we
	 * may still have pages in the mpool that match the file's "unique" ID.
	 *
	 * Note that if we're opening a subdatabase, we don't want to set
	 * the TRUNCATE flag even if we just created the file--we already
	 * opened and updated the master using access method interfaces,
	 * so we don't want to get rid of any pages that are in the mpool.
	 * If we created the file when we opened the master, we already hit
	 * this check in a non-subdb context then.
	 */
	if (subdb == NULL && F_ISSET(dbp, DB_AM_CREATED))
		LF_SET(DB_TRUNCATE);

	/* Set up the underlying environment. */
	if ((ret = __db_dbenv_setup(dbp, txn, name, id, flags)) != 0)
		return (ret);

	/*
	 * Set the open flag.  We use it to mean that the dbp has gone
	 * through mpf setup, including dbreg_register.  Also, below,
	 * the underlying access method open functions may want to do
	 * things like acquire cursors, so the open flag has to be set
	 * before calling them.
	 */
	F_SET(dbp, DB_AM_OPEN_CALLED);

	/*
	 * For unnamed files, we need to actually create the file now
	 * that the mpool is open.
	 */
	if (name == NULL && (ret = __db_new_file(dbp, txn, NULL, NULL)) != 0)
		return (ret);

	switch (dbp->type) {
	case DB_BTREE:
		ret = __bam_open(dbp, txn, name, meta_pgno, flags);
		break;
	case DB_HASH:
		ret = __ham_open(dbp, txn, name, meta_pgno, flags);
		break;
	case DB_RECNO:
		ret = __ram_open(dbp, txn, name, meta_pgno, flags);
		break;
	case DB_QUEUE:
		ret = __qam_open(dbp, txn, name, meta_pgno, mode, flags);
		break;
	case DB_UNKNOWN:
		return (__db_unknown_type(dbenv, "__db_dbopen", dbp->type));
	}
	if (ret != 0)
		goto err;

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTOPEN, ret, name);

	/*
	 * Unnamed files don't need handle locks, so we only have to check
	 * for a handle lock downgrade or lockevent in the case of named
	 * files.
	 */
	if (!F_ISSET(dbp, DB_AM_RECOVER) &&
	    name != NULL && LOCK_ISSET(dbp->handle_lock)) {
		if (txn != NULL) {
			ret = __txn_lockevent(dbenv,
			    txn, dbp, &dbp->handle_lock, dbp->lid);
		} else if (LOCKING_ON(dbenv))
			/* Trade write handle lock for read handle lock. */
			ret = __lock_downgrade(dbenv,
			    &dbp->handle_lock, DB_LOCK_READ, 0);
	}
DB_TEST_RECOVERY_LABEL
err:
	return (ret);
}

/*
 * __db_new_file --
 *	Create a new database file.
 *
 * PUBLIC: int __db_new_file __P((DB *, DB_TXN *, DB_FH *, const char *));
 */
int
__db_new_file(dbp, txn, fhp, name)
	DB *dbp;
	DB_TXN *txn;
	DB_FH *fhp;
	const char *name;
{
	int ret;

	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		ret = __bam_new_file(dbp, txn, fhp, name);
		break;
	case DB_HASH:
		ret = __ham_new_file(dbp, txn, fhp, name);
		break;
	case DB_QUEUE:
		ret = __qam_new_file(dbp, txn, fhp, name);
		break;
	default:
		__db_err(dbp->dbenv,
		    "%s: Invalid type %d specified", name, dbp->type);
		ret = EINVAL;
		break;
	}

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTLOGMETA, ret, name);
	/* Sync the file in preparation for moving it into place. */
	if (ret == 0 && fhp != NULL)
		ret = __os_fsync(dbp->dbenv, fhp);

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTSYNC, ret, name);

DB_TEST_RECOVERY_LABEL
	return (ret);
}

/*
 * __db_init_subdb --
 *	Initialize the dbp for a subdb.
 *
 * PUBLIC: int __db_init_subdb __P((DB *, DB *, const char *, DB_TXN *));
 */
int
__db_init_subdb(mdbp, dbp, name, txn)
	DB *mdbp, *dbp;
	const char *name;
	DB_TXN *txn;
{
	DBMETA *meta;
	DB_MPOOLFILE *mpf;
	int ret, t_ret;

	ret = 0;
	if (!F_ISSET(dbp, DB_AM_CREATED)) {
		/* Subdb exists; read meta-data page and initialize. */
		mpf = mdbp->mpf;
		if  ((ret = mpf->get(mpf, &dbp->meta_pgno, 0, &meta)) != 0)
			goto err;
		ret = __db_meta_setup(mdbp->dbenv, dbp, name, meta, 0, 0);
		if ((t_ret = mpf->put(mpf, meta, 0)) != 0 && ret == 0)
			ret = t_ret;
		/*
		 * If __db_meta_setup found that the meta-page hadn't
		 * been written out during recovery, we can just return.
		 */
		if (ret == ENOENT)
			ret = 0;
		goto err;
	}

	/* Handle the create case here. */
	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		ret = __bam_new_subdb(mdbp, dbp, txn);
		break;
	case DB_HASH:
		ret = __ham_new_subdb(mdbp, dbp, txn);
		break;
	case DB_QUEUE:
		ret = EINVAL;
		break;
	default:
		__db_err(dbp->dbenv,
		    "Invalid subdatabase type %d specified", dbp->type);
		return (EINVAL);
	}

err:	return (ret);
}

/*
 * __db_chk_meta --
 *	Take a buffer containing a meta-data page and check it for a checksum
 *	(and verify the checksum if necessary) and possibly decrypt it.
 *
 *	Return 0 on success, >0 (errno) on error, -1 on checksum mismatch.
 *
 * PUBLIC: int __db_chk_meta __P((DB_ENV *, DB *, DBMETA *, int));
 */
int
__db_chk_meta(dbenv, dbp, meta, do_metachk)
	DB_ENV *dbenv;
	DB *dbp;
	DBMETA *meta;
	int do_metachk;
{
	int is_hmac, ret;
	u_int8_t *chksum;

	ret = 0;

	if (FLD_ISSET(meta->metaflags, DBMETA_CHKSUM)) {
		if (dbp != NULL)
			F_SET(dbp, DB_AM_CHKSUM);

		is_hmac = meta->encrypt_alg == 0 ? 0 : 1;
		chksum = ((BTMETA *)meta)->chksum;
		if (do_metachk && ((ret = __db_check_chksum(dbenv,
		    (DB_CIPHER *)dbenv->crypto_handle, chksum, meta,
		    DBMETASIZE, is_hmac)) != 0))
			return (ret);
	}

#ifdef HAVE_CRYPTO
	ret = __crypto_decrypt_meta(dbenv, dbp, (u_int8_t *)meta, do_metachk);
#endif
	return (ret);
}

/*
 * __db_meta_setup --
 *
 * Take a buffer containing a meta-data page and figure out if it's
 * valid, and if so, initialize the dbp from the meta-data page.
 *
 * PUBLIC: int __db_meta_setup __P((DB_ENV *,
 * PUBLIC:     DB *, const char *, DBMETA *, u_int32_t, int));
 */
int
__db_meta_setup(dbenv, dbp, name, meta, oflags, do_metachk)
	DB_ENV *dbenv;
	DB *dbp;
	const char *name;
	DBMETA *meta;
	u_int32_t oflags;
	int do_metachk;
{
	u_int32_t flags, magic;
	int ret;

	ret = 0;

	/*
	 * Figure out what access method we're dealing with, and then
	 * call access method specific code to check error conditions
	 * based on conflicts between the found file and application
	 * arguments.  A found file overrides some user information --
	 * we don't consider it an error, for example, if the user set
	 * an expected byte order and the found file doesn't match it.
	 */
	F_CLR(dbp, DB_AM_SWAP);
	magic = meta->magic;

swap_retry:
	switch (magic) {
	case DB_BTREEMAGIC:
	case DB_HASHMAGIC:
	case DB_QAMMAGIC:
	case DB_RENAMEMAGIC:
		break;
	case 0:
		/*
		 * The only time this should be 0 is if we're in the
		 * midst of opening a subdb during recovery and that
		 * subdatabase had its meta-data page allocated, but
		 * not yet initialized.
		 */
		if (F_ISSET(dbp, DB_AM_SUBDB) && ((IS_RECOVERING(dbenv) &&
		    F_ISSET((DB_LOG *) dbenv->lg_handle, DBLOG_FORCE_OPEN)) ||
		    meta->pgno != PGNO_INVALID))
			return (ENOENT);

		goto bad_format;
	default:
		if (F_ISSET(dbp, DB_AM_SWAP))
			goto bad_format;

		M_32_SWAP(magic);
		F_SET(dbp, DB_AM_SWAP);
		goto swap_retry;
	}

	/*
	 * We can only check the meta page if we are sure we have a meta page.
	 * If it is random data, then this check can fail.  So only now can we
	 * checksum and decrypt.  Don't distinguish between configuration and
	 * checksum match errors here, because we haven't opened the database
	 * and even a checksum error isn't a reason to panic the environment.
	 */
	if ((ret = __db_chk_meta(dbenv, dbp, meta, do_metachk)) != 0) {
		if (ret == -1)
			__db_err(dbenv,
			    "%s: metadata page checksum error", name);
		goto bad_format;
	}

	switch (magic) {
	case DB_BTREEMAGIC:
		flags = meta->flags;
		if (F_ISSET(dbp, DB_AM_SWAP))
			M_32_SWAP(flags);
		if (LF_ISSET(BTM_RECNO))
			dbp->type = DB_RECNO;
		else
			dbp->type = DB_BTREE;
		if ((oflags & DB_TRUNCATE) == 0 && (ret =
		    __bam_metachk(dbp, name, (BTMETA *)meta)) != 0)
			return (ret);
		break;
	case DB_HASHMAGIC:
		dbp->type = DB_HASH;
		if ((oflags & DB_TRUNCATE) == 0 && (ret =
		    __ham_metachk(dbp, name, (HMETA *)meta)) != 0)
			return (ret);
		break;
	case DB_QAMMAGIC:
		dbp->type = DB_QUEUE;
		if ((oflags & DB_TRUNCATE) == 0 && (ret =
		    __qam_metachk(dbp, name, (QMETA *)meta)) != 0)
			return (ret);
		break;
	case DB_RENAMEMAGIC:
		F_SET(dbp, DB_AM_IN_RENAME);
		break;
	}
	return (0);

bad_format:
	__db_err(dbenv, "%s: unexpected file type or format", name);
	return (ret == 0 ? EINVAL : ret);
}

/*
 * __db_openchk --
 *	Interface error checking for open calls.
 */
static int
__db_openchk(dbp, txn, name, subdb, type, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	DBTYPE type;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;
	u_int32_t ok_flags;

	dbenv = dbp->dbenv;

	/* Validate arguments. */
#define	OKFLAGS								\
    (DB_AUTO_COMMIT | DB_CREATE | DB_DIRTY_READ | DB_EXCL |		\
     DB_FCNTL_LOCKING | DB_NOMMAP | DB_RDONLY | DB_RDWRMASTER |		\
     DB_THREAD | DB_TRUNCATE | DB_WRITEOPEN)
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
		__db_err(dbenv, "environment did not include a memory pool");
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

	/* DB_TRUNCATE is not transaction recoverable. */
	if (LF_ISSET(DB_TRUNCATE) && txn != NULL) {
		__db_err(dbenv,
		    "DB_TRUNCATE illegal with transaction specified");
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

		/* Truncate is a physical file operation */
		if (LF_ISSET(DB_TRUNCATE)) {
			__db_err(dbenv,
			    "DB_TRUNCATE illegal with multiple databases");
			return (EINVAL);
		}

		/* QAM can't be done as a subdatabase. */
		if (type == DB_QUEUE) {
			__db_err(dbenv, "Queue databases must be one-per-file");
			return (EINVAL);
		}
	}

	return (0);
}
