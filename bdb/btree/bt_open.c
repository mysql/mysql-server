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
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
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
static const char revid[] = "$Id: bt_open.c,v 11.42 2000/11/30 00:58:28 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <limits.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_swap.h"
#include "btree.h"
#include "db_shash.h"
#include "lock.h"
#include "log.h"
#include "mp.h"

/*
 * __bam_open --
 *	Open a btree.
 *
 * PUBLIC: int __bam_open __P((DB *, const char *, db_pgno_t, u_int32_t));
 */
int
__bam_open(dbp, name, base_pgno, flags)
	DB *dbp;
	const char *name;
	db_pgno_t base_pgno;
	u_int32_t flags;
{
	BTREE *t;

	t = dbp->bt_internal;

	/* Initialize the remaining fields/methods of the DB. */
	dbp->del = __bam_delete;
	dbp->key_range = __bam_key_range;
	dbp->stat = __bam_stat;

	/*
	 * We don't permit the user to specify a prefix routine if they didn't
	 * also specify a comparison routine, they can't know enough about our
	 * comparison routine to get it right.
	 */
	if (t->bt_compare == __bam_defcmp && t->bt_prefix != __bam_defpfx) {
		__db_err(dbp->dbenv,
"prefix comparison may not be specified for default comparison routine");
		return (EINVAL);
	}

	/*
	 * Verify that the bt_minkey value specified won't cause the
	 * calculation of ovflsize to underflow [#2406] for this pagesize.
	 */
	if (B_MINKEY_TO_OVFLSIZE(t->bt_minkey, dbp->pgsize) >
	    B_MINKEY_TO_OVFLSIZE(DEFMINKEYPAGE, dbp->pgsize)) {
		__db_err(dbp->dbenv,
		    "bt_minkey value of %lu too high for page size of %lu",
		    (u_long)t->bt_minkey, (u_long)dbp->pgsize);
		return (EINVAL);
	}

	/* Start up the tree. */
	return (__bam_read_root(dbp, name, base_pgno, flags));
}

/*
 * __bam_metachk --
 *
 * PUBLIC: int __bam_metachk __P((DB *, const char *, BTMETA *));
 */
int
__bam_metachk(dbp, name, btm)
	DB *dbp;
	const char *name;
	BTMETA *btm;
{
	DB_ENV *dbenv;
	u_int32_t vers;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * At this point, all we know is that the magic number is for a Btree.
	 * Check the version, the database may be out of date.
	 */
	vers = btm->dbmeta.version;
	if (F_ISSET(dbp, DB_AM_SWAP))
		M_32_SWAP(vers);
	switch (vers) {
	case 6:
	case 7:
		__db_err(dbenv,
		    "%s: btree version %lu requires a version upgrade",
		    name, (u_long)vers);
		return (DB_OLD_VERSION);
	case 8:
		break;
	default:
		__db_err(dbenv,
		    "%s: unsupported btree version: %lu", name, (u_long)vers);
		return (EINVAL);
	}

	/* Swap the page if we need to. */
	if (F_ISSET(dbp, DB_AM_SWAP) && (ret = __bam_mswap((PAGE *)btm)) != 0)
		return (ret);

	/*
	 * Check application info against metadata info, and set info, flags,
	 * and type based on metadata info.
	 */
	if ((ret =
	    __db_fchk(dbenv, "DB->open", btm->dbmeta.flags, BTM_MASK)) != 0)
		return (ret);

	if (F_ISSET(&btm->dbmeta, BTM_RECNO)) {
		if (dbp->type == DB_BTREE)
			goto wrong_type;
		dbp->type = DB_RECNO;
		DB_ILLEGAL_METHOD(dbp, DB_OK_RECNO);
	} else {
		if (dbp->type == DB_RECNO)
			goto wrong_type;
		dbp->type = DB_BTREE;
		DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);
	}

	if (F_ISSET(&btm->dbmeta, BTM_DUP))
		F_SET(dbp, DB_AM_DUP);
	else
		if (F_ISSET(dbp, DB_AM_DUP)) {
			__db_err(dbenv,
		"%s: DB_DUP specified to open method but not set in database",
			    name);
			return (EINVAL);
		}

	if (F_ISSET(&btm->dbmeta, BTM_RECNUM)) {
		if (dbp->type != DB_BTREE)
			goto wrong_type;
		F_SET(dbp, DB_BT_RECNUM);

		if ((ret = __db_fcchk(dbenv,
		    "DB->open", dbp->flags, DB_AM_DUP, DB_BT_RECNUM)) != 0)
			return (ret);
	} else
		if (F_ISSET(dbp, DB_BT_RECNUM)) {
			__db_err(dbenv,
	    "%s: DB_RECNUM specified to open method but not set in database",
			    name);
			return (EINVAL);
		}

	if (F_ISSET(&btm->dbmeta, BTM_FIXEDLEN)) {
		if (dbp->type != DB_RECNO)
			goto wrong_type;
		F_SET(dbp, DB_RE_FIXEDLEN);
	} else
		if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
			__db_err(dbenv,
	"%s: DB_FIXEDLEN specified to open method but not set in database",
			    name);
			return (EINVAL);
		}

	if (F_ISSET(&btm->dbmeta, BTM_RENUMBER)) {
		if (dbp->type != DB_RECNO)
			goto wrong_type;
		F_SET(dbp, DB_RE_RENUMBER);
	} else
		if (F_ISSET(dbp, DB_RE_RENUMBER)) {
			__db_err(dbenv,
	    "%s: DB_RENUMBER specified to open method but not set in database",
			    name);
			return (EINVAL);
		}

	if (F_ISSET(&btm->dbmeta, BTM_SUBDB))
		F_SET(dbp, DB_AM_SUBDB);
	else
		if (F_ISSET(dbp, DB_AM_SUBDB)) {
			__db_err(dbenv,
	    "%s: multiple databases specified but not supported by file",
			    name);
			return (EINVAL);
		}

	if (F_ISSET(&btm->dbmeta, BTM_DUPSORT)) {
		if (dbp->dup_compare == NULL)
			dbp->dup_compare = __bam_defcmp;
		F_SET(dbp, DB_AM_DUPSORT);
	} else
		if (dbp->dup_compare != NULL) {
			__db_err(dbenv,
		"%s: duplicate sort specified but not supported in database",
			    name);
			return (EINVAL);
		}

	/* Set the page size. */
	dbp->pgsize = btm->dbmeta.pagesize;

	/* Copy the file's ID. */
	memcpy(dbp->fileid, btm->dbmeta.uid, DB_FILE_ID_LEN);

	return (0);

wrong_type:
	if (dbp->type == DB_BTREE)
		__db_err(dbenv,
		    "open method type is Btree, database type is Recno");
	else
		__db_err(dbenv,
		    "open method type is Recno, database type is Btree");
	return (EINVAL);
}

/*
 * __bam_read_root --
 *	Check (and optionally create) a tree.
 *
 * PUBLIC: int __bam_read_root __P((DB *, const char *, db_pgno_t, u_int32_t));
 */
int
__bam_read_root(dbp, name, base_pgno, flags)
	DB *dbp;
	const char *name;
	db_pgno_t base_pgno;
	u_int32_t flags;
{
	BTMETA *meta;
	BTREE *t;
	DBC *dbc;
	DB_LSN orig_lsn;
	DB_LOCK metalock;
	PAGE *root;
	int locked, ret, t_ret;

	ret = 0;
	t = dbp->bt_internal;
	meta = NULL;
	root = NULL;
	locked = 0;

	/*
	 * Get a cursor.  If DB_CREATE is specified, we may be creating
	 * the root page, and to do that safely in CDB we need a write
	 * cursor.  In STD_LOCKING mode, we'll synchronize using the
	 * meta page lock instead.
	 */
	if ((ret = dbp->cursor(dbp, dbp->open_txn,
	    &dbc, LF_ISSET(DB_CREATE) && CDB_LOCKING(dbp->dbenv) ?
	    DB_WRITECURSOR : 0)) != 0)
		return (ret);

	/* Get, and optionally create the metadata page. */
	if ((ret =
	    __db_lget(dbc, 0, base_pgno, DB_LOCK_READ, 0, &metalock)) != 0)
		goto err;
	if ((ret = memp_fget(
	    dbp->mpf, &base_pgno, DB_MPOOL_CREATE, (PAGE **)&meta)) != 0)
		goto err;

	/*
	 * If the magic number is correct, we're not creating the tree.
	 * Correct any fields that may not be right.  Note, all of the
	 * local flags were set by DB->open.
	 */
again:	if (meta->dbmeta.magic != 0) {
		t->bt_maxkey = meta->maxkey;
		t->bt_minkey = meta->minkey;
		t->re_pad = meta->re_pad;
		t->re_len = meta->re_len;

		t->bt_meta = base_pgno;
		t->bt_root = meta->root;

		(void)memp_fput(dbp->mpf, meta, 0);
		meta = NULL;
		goto done;
	}

	/* In recovery if it's not there it will be created elsewhere.*/
	if (IS_RECOVERING(dbp->dbenv))
		goto done;

	/* If we're doing CDB; we now have to get the write lock. */
	if (CDB_LOCKING(dbp->dbenv)) {
		/*
		 * We'd better have DB_CREATE set if we're actually doing
		 * the create.
		 */
		DB_ASSERT(LF_ISSET(DB_CREATE));
		if ((ret = lock_get(dbp->dbenv, dbc->locker, DB_LOCK_UPGRADE,
		    &dbc->lock_dbt, DB_LOCK_WRITE, &dbc->mylock)) != 0)
			goto err;
	}

	/*
	 * If we are doing locking, relase the read lock and get a write lock.
	 * We want to avoid deadlock.
	 */
	if (locked == 0 && STD_LOCKING(dbc)) {
		if ((ret = __LPUT(dbc, metalock)) != 0)
			goto err;
		if ((ret = __db_lget(dbc,
		     0, base_pgno, DB_LOCK_WRITE, 0, &metalock)) != 0)
			goto err;
		locked = 1;
		goto again;
	}

	/* Initialize the tree structure metadata information. */
	orig_lsn = meta->dbmeta.lsn;
	memset(meta, 0, sizeof(BTMETA));
	meta->dbmeta.lsn = orig_lsn;
	meta->dbmeta.pgno = base_pgno;
	meta->dbmeta.magic = DB_BTREEMAGIC;
	meta->dbmeta.version = DB_BTREEVERSION;
	meta->dbmeta.pagesize = dbp->pgsize;
	meta->dbmeta.type = P_BTREEMETA;
	meta->dbmeta.free = PGNO_INVALID;
	if (F_ISSET(dbp, DB_AM_DUP))
		F_SET(&meta->dbmeta, BTM_DUP);
	if (F_ISSET(dbp, DB_RE_FIXEDLEN))
		F_SET(&meta->dbmeta, BTM_FIXEDLEN);
	if (F_ISSET(dbp, DB_BT_RECNUM))
		F_SET(&meta->dbmeta, BTM_RECNUM);
	if (F_ISSET(dbp, DB_RE_RENUMBER))
		F_SET(&meta->dbmeta, BTM_RENUMBER);
	if (F_ISSET(dbp, DB_AM_SUBDB))
		F_SET(&meta->dbmeta, BTM_SUBDB);
	if (dbp->dup_compare != NULL)
		F_SET(&meta->dbmeta, BTM_DUPSORT);
	if (dbp->type == DB_RECNO)
		F_SET(&meta->dbmeta, BTM_RECNO);
	memcpy(meta->dbmeta.uid, dbp->fileid, DB_FILE_ID_LEN);

	meta->maxkey = t->bt_maxkey;
	meta->minkey = t->bt_minkey;
	meta->re_len = t->re_len;
	meta->re_pad = t->re_pad;

	/* If necessary, log the meta-data and root page creates.  */
	if ((ret = __db_log_page(dbp,
	    name, &orig_lsn, base_pgno, (PAGE *)meta)) != 0)
		goto err;

	/* Create and initialize a root page. */
	if ((ret = __db_new(dbc,
	    dbp->type == DB_RECNO ? P_LRECNO : P_LBTREE, &root)) != 0)
		goto err;
	root->level = LEAFLEVEL;

	if (dbp->open_txn != NULL && (ret = __bam_root_log(dbp->dbenv,
	    dbp->open_txn, &meta->dbmeta.lsn, 0, dbp->log_fileid,
	    meta->dbmeta.pgno, root->pgno, &meta->dbmeta.lsn)) != 0)
		goto err;

	meta->root = root->pgno;

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTLOGMETA, ret, name);
	if ((ret = __db_log_page(dbp,
	    name, &root->lsn, root->pgno, root)) != 0)
		goto err;
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTLOG, ret, name);

	t->bt_meta = base_pgno;
	t->bt_root = root->pgno;

	/* Release the metadata and root pages. */
	if ((ret = memp_fput(dbp->mpf, meta, DB_MPOOL_DIRTY)) != 0)
		goto err;
	meta = NULL;
	if ((ret = memp_fput(dbp->mpf, root, DB_MPOOL_DIRTY)) != 0)
		goto err;
	root = NULL;

	/*
	 * Flush the metadata and root pages to disk.
	 *
	 * !!!
	 * It's not useful to return not-yet-flushed here -- convert it to
	 * an error.
	 */
	if ((ret = memp_fsync(dbp->mpf)) == DB_INCOMPLETE) {
		__db_err(dbp->dbenv, "Metapage flush failed");
		ret = EINVAL;
	}
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTSYNC, ret, name);

done:	/*
	 * !!!
	 * We already did an insert and so the last-page-inserted has been
	 * set.  I'm not sure where the *right* place to clear this value
	 * is, it's not intuitively obvious that it belongs here.
	 */
	t->bt_lpgno = PGNO_INVALID;

err:
DB_TEST_RECOVERY_LABEL
	/* Put any remaining pages back. */
	if (meta != NULL)
		if ((t_ret = memp_fput(dbp->mpf, meta, 0)) != 0 &&
		    ret == 0)
			ret = t_ret;
	if (root != NULL)
		if ((t_ret = memp_fput(dbp->mpf, root, 0)) != 0 &&
		    ret == 0)
			ret = t_ret;

	/* We can release the metapage lock when we are done. */
	if ((t_ret = __LPUT(dbc, metalock)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}
