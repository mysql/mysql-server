/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_open.c,v 11.31 2000/12/20 17:59:29 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "db_swap.h"
#include "db_am.h"
#include "lock.h"
#include "qam.h"

/*
 * __qam_open
 *
 * PUBLIC: int __qam_open __P((DB *, const char *, db_pgno_t, int, u_int32_t));
 */
int
__qam_open(dbp, name, base_pgno, mode, flags)
	DB *dbp;
	const char *name;
	db_pgno_t base_pgno;
	int mode;
	u_int32_t flags;
{
	QUEUE *t;
	DBC *dbc;
	DB_LOCK metalock;
	DB_LSN orig_lsn;
	QMETA *qmeta;
	int locked;
	int ret, t_ret;

	ret = 0;
	locked = 0;
	t = dbp->q_internal;

	if (name == NULL && t->page_ext != 0) {
		__db_err(dbp->dbenv,
	"Extent size may not be specified for in-memory queue database.");
		return (EINVAL);
	}
	/* Initialize the remaining fields/methods of the DB. */
	dbp->del = __qam_delete;
	dbp->put = __qam_put;
	dbp->stat = __qam_stat;
	dbp->sync = __qam_sync;
	dbp->db_am_remove = __qam_remove;
	dbp->db_am_rename = __qam_rename;

	metalock.off = LOCK_INVALID;

	/*
	 * Get a cursor.  If DB_CREATE is specified, we may be creating
	 * pages, and to do that safely in CDB we need a write cursor.
	 * In STD_LOCKING mode, we'll synchronize using the meta page
	 * lock instead.
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
	    dbp->mpf, &base_pgno, DB_MPOOL_CREATE, (PAGE **)&qmeta)) != 0)
		goto err;

	/*
	 * If the magic number is correct, we're not creating the tree.
	 * Correct any fields that may not be right.  Note, all of the
	 * local flags were set by DB->open.
	 */
again:	if (qmeta->dbmeta.magic == DB_QAMMAGIC) {
		t->re_pad = qmeta->re_pad;
		t->re_len = qmeta->re_len;
		t->rec_page = qmeta->rec_page;
		t->page_ext = qmeta->page_ext;

		(void)memp_fput(dbp->mpf, (PAGE *)qmeta, 0);
		goto done;
	}

	/* If we're doing CDB; we now have to get the write lock. */
	if (CDB_LOCKING(dbp->dbenv)) {
		DB_ASSERT(LF_ISSET(DB_CREATE));
		if ((ret = lock_get(dbp->dbenv, dbc->locker, DB_LOCK_UPGRADE,
		    &dbc->lock_dbt, DB_LOCK_WRITE, &dbc->mylock)) != 0)
			goto err;
	}

	/*
	 * If we are doing locking, relase the read lock
	 * and get a write lock.  We want to avoid deadlock.
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
	orig_lsn = qmeta->dbmeta.lsn;
	memset(qmeta, 0, sizeof(QMETA));
	ZERO_LSN(qmeta->dbmeta.lsn);
	qmeta->dbmeta.pgno = base_pgno;
	qmeta->dbmeta.magic = DB_QAMMAGIC;
	qmeta->dbmeta.version = DB_QAMVERSION;
	qmeta->dbmeta.pagesize = dbp->pgsize;
	qmeta->dbmeta.type = P_QAMMETA;
	qmeta->re_pad = t->re_pad;
	qmeta->re_len = t->re_len;
	qmeta->rec_page = CALC_QAM_RECNO_PER_PAGE(dbp);
	qmeta->cur_recno = 1;
	qmeta->first_recno = 1;
	qmeta->page_ext = t->page_ext;
	t->rec_page = qmeta->rec_page;
	memcpy(qmeta->dbmeta.uid, dbp->fileid, DB_FILE_ID_LEN);

	/* Verify that we can fit at least one record per page. */
	if (QAM_RECNO_PER_PAGE(dbp) < 1) {
		__db_err(dbp->dbenv,
		    "Record size of %lu too large for page size of %lu",
		    (u_long)t->re_len, (u_long)dbp->pgsize);
		(void)memp_fput(dbp->mpf, (PAGE *)qmeta, 0);
		ret = EINVAL;
		goto err;
	}

	if ((ret = __db_log_page(dbp,
	    name, &orig_lsn, base_pgno, (PAGE *)qmeta)) != 0)
		goto err;

	/* Release the metadata page. */
	if ((ret = memp_fput(dbp->mpf, (PAGE *)qmeta, DB_MPOOL_DIRTY)) != 0)
		goto err;
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTLOG, ret, name);

	/*
	 * Flush the metadata page to disk.
	 *
	 * !!!
	 * It's not useful to return not-yet-flushed here -- convert it to
	 * an error.
	 */
	if ((ret = memp_fsync(dbp->mpf)) == DB_INCOMPLETE) {
		__db_err(dbp->dbenv, "Flush of metapage failed");
		ret = EINVAL;
	}
	DB_TEST_RECOVERY(dbp, DB_TEST_POSTSYNC, ret, name);

done:	t->q_meta = base_pgno;
	t->q_root = base_pgno + 1;

	/* Setup information needed to open extents. */
	if (t->page_ext != 0) {
		t->finfo.pgcookie = &t->pgcookie;
		t->finfo.fileid = NULL;
		t->finfo.lsn_offset = 0;

		t->pginfo.db_pagesize = dbp->pgsize;
		t->pginfo.needswap = F_ISSET(dbp, DB_AM_SWAP);
		t->pgcookie.data = &t->pginfo;
		t->pgcookie.size = sizeof(DB_PGINFO);

		if ((ret = __os_strdup(dbp->dbenv, name,  &t->path)) != 0)
			goto err;
		t->dir = t->path;
		if ((t->name = __db_rpath(t->path)) == NULL) {
			t->name = t->path;
			t->dir = PATH_DOT;
		} else
			*t->name++ = '\0';

		if (mode == 0)
			mode = __db_omode("rwrw--");
		t->mode = mode;
	}

err:
DB_TEST_RECOVERY_LABEL
	/* Don't hold the meta page long term. */
	(void)__LPUT(dbc, metalock);

	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __qam_metachk --
 *
 * PUBLIC: int __qam_metachk __P((DB *, const char *, QMETA *));
 */
int
__qam_metachk(dbp, name, qmeta)
	DB *dbp;
	const char *name;
	QMETA *qmeta;
{
	DB_ENV *dbenv;
	u_int32_t vers;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * At this point, all we know is that the magic number is for a Queue.
	 * Check the version, the database may be out of date.
	 */
	vers = qmeta->dbmeta.version;
	if (F_ISSET(dbp, DB_AM_SWAP))
		M_32_SWAP(vers);
	switch (vers) {
	case 1:
	case 2:
		__db_err(dbenv,
		    "%s: queue version %lu requires a version upgrade",
		    name, (u_long)vers);
		return (DB_OLD_VERSION);
	case 3:
		break;
	default:
		__db_err(dbenv,
		    "%s: unsupported qam version: %lu", name, (u_long)vers);
		return (EINVAL);
	}

	/* Swap the page if we need to. */
	if (F_ISSET(dbp, DB_AM_SWAP) && (ret = __qam_mswap((PAGE *)qmeta)) != 0)
		return (ret);

	/* Check the type. */
	if (dbp->type != DB_QUEUE && dbp->type != DB_UNKNOWN)
		return (EINVAL);
	dbp->type = DB_QUEUE;
	DB_ILLEGAL_METHOD(dbp, DB_OK_QUEUE);

	/* Set the page size. */
	dbp->pgsize = qmeta->dbmeta.pagesize;

	/* Copy the file's ID. */
	memcpy(dbp->fileid, qmeta->dbmeta.uid, DB_FILE_ID_LEN);

	return (0);
}
