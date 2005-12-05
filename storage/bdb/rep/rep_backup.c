/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_backup.c,v 12.38 2005/11/09 14:17:30 margo Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/qam.h"
#include "dbinc/txn.h"

static int __rep_filedone __P((DB_ENV *, int, REP *, __rep_fileinfo_args *,
    u_int32_t));
static int __rep_find_dbs __P((DB_ENV *, u_int8_t *, size_t *,
    size_t *, u_int32_t *));
static int __rep_get_fileinfo __P((DB_ENV *, const char *,
    const char *, __rep_fileinfo_args *, u_int8_t *, u_int32_t *));
static int __rep_log_setup __P((DB_ENV *, REP *));
static int __rep_mpf_open __P((DB_ENV *, DB_MPOOLFILE **,
    __rep_fileinfo_args *, u_int32_t));
static int __rep_page_gap __P((DB_ENV *, REP *, __rep_fileinfo_args *,
    u_int32_t));
static int __rep_page_sendpages __P((DB_ENV *, int,
    __rep_fileinfo_args *, DB_MPOOLFILE *, DB *));
static int __rep_queue_filedone __P((DB_ENV *, REP *, __rep_fileinfo_args *));
static int __rep_walk_dir __P((DB_ENV *, const char *, u_int8_t *,
    size_t *, size_t *, u_int32_t *));
static int __rep_write_page __P((DB_ENV *, REP *, __rep_fileinfo_args *));

/*
 * __rep_update_req -
 *	Process an update_req and send the file information to the client.
 *
 * PUBLIC: int __rep_update_req __P((DB_ENV *, int));
 */
int
__rep_update_req(dbenv, eid)
	DB_ENV *dbenv;
	int eid;
{
	DBT updbt;
	DB_LOG *dblp;
	DB_LSN lsn;
	size_t filelen, filesz, updlen;
	u_int32_t filecnt;
	u_int8_t *buf, *fp;
	int ret;

	/*
	 * Allocate enough for all currently open files and then some.
	 * Optimize for the common use of having most databases open.
	 * Allocate dbentry_cnt * 2 plus an estimated 60 bytes per
	 * file for the filename/path (or multiplied by 120).
	 *
	 * The data we send looks like this:
	 *	__rep_update_args
	 *	__rep_fileinfo_args
	 *	__rep_fileinfo_args
	 *	...
	 */
	dblp = dbenv->lg_handle;
	filecnt = 0;
	filelen = 0;
	updlen = 0;
	filesz = MEGABYTE;
	if ((ret = __os_calloc(dbenv, 1, filesz, &buf)) != 0)
		return (ret);

	/*
	 * First get our file information.  Get in-memory files first
	 * then get on-disk files.
	 */
	fp = buf + sizeof(__rep_update_args);
	if ((ret = __rep_find_dbs(dbenv, fp, &filesz, &filelen, &filecnt)) != 0)
		goto err;

	/*
	 * Now get our first LSN.  We send the lsn of the first
	 * non-archivable log file.
	 */
	if ((ret = __log_get_stable_lsn(dbenv, &lsn)) != 0)
		goto err;

	/*
	 * Package up the update information.
	 */
	if ((ret = __rep_update_buf(buf, filesz, &updlen, &lsn, filecnt)) != 0)
		goto err;
	/*
	 * We have all the file information now.  Send it to the client.
	 */
	memset(&updbt, 0, sizeof(updbt));
	updbt.data = buf;
	updbt.size = (u_int32_t)(filelen + updlen);
	LOG_SYSTEM_LOCK(dbenv);
	lsn = ((LOG *)dblp->reginfo.primary)->lsn;
	LOG_SYSTEM_UNLOCK(dbenv);
	(void)__rep_send_message(dbenv, eid, REP_UPDATE, &lsn, &updbt, 0,
	    DB_REP_ANYWHERE);

err:
	__os_free(dbenv, buf);
	return (ret);
}

/*
 * __rep_find_dbs -
 *	Walk through all the named files/databases including those in the
 *	environment or data_dirs and those that in named and in-memory.  We
 *	need to	open them, gather the necessary information and then close
 *	them. Then we need to figure out if they're already in the dbentry
 *	array.
 */
static int
__rep_find_dbs(dbenv, fp, fileszp, filelenp, filecntp)
	DB_ENV *dbenv;
	u_int8_t *fp;
	size_t *fileszp, *filelenp;
	u_int32_t *filecntp;
{
	int ret;
	char **ddir;

	ret = 0;
	if (dbenv->db_data_dir == NULL) {
		/*
		 * If we don't have a data dir, we have just the
		 * env home dir.
		 */
		ret = __rep_walk_dir(dbenv, dbenv->db_home, fp,
		    fileszp, filelenp, filecntp);
	} else {
		for (ddir = dbenv->db_data_dir; *ddir != NULL; ++ddir)
			if ((ret = __rep_walk_dir(dbenv, *ddir, fp,
			    fileszp, filelenp, filecntp)) != 0)
				break;
	}

	/* Now, collect any in-memory named databases. */
	if (ret == 0)
		ret = __rep_walk_dir(dbenv,
		    NULL, fp, fileszp, filelenp, filecntp);

	return (ret);
}

/*
 * __rep_walk_dir --
 *
 * This is the routine that walks a directory and fills in the structures
 * that we use to generate messages to the client telling it what files
 * files are available.  If the directory name is NULL, then we should
 * walk the list of in-memory named files.
 */
int
__rep_walk_dir(dbenv, dir, fp, fileszp, filelenp, filecntp)
	DB_ENV *dbenv;
	const char *dir;
	u_int8_t *fp;
	size_t *fileszp, *filelenp;
	u_int32_t *filecntp;
{
	DBT namedbt, uiddbt;
	__rep_fileinfo_args tmpfp;
	size_t len, offset;
	int cnt, i, ret;
	u_int8_t *rfp, uid[DB_FILE_ID_LEN];
	char *file, **names, *subdb;
#ifdef DIAGNOSTIC
	REP *rep;
	DB_MSGBUF mb;
	DB_REP *db_rep;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
#endif
	memset(&namedbt, 0, sizeof(namedbt));
	memset(&uiddbt, 0, sizeof(uiddbt));
	if (dir == NULL) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Walk_dir: Getting info for in-memory named files"));
		if ((ret = __memp_inmemlist(dbenv, &names, &cnt)) != 0)
			return (ret);
	} else {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Walk_dir: Getting info for dir: %s", dir));
		if ((ret = __os_dirlist(dbenv, dir, &names, &cnt)) != 0)
			return (ret);
	}
	rfp = fp;
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "Walk_dir: Dir %s has %d files", dir, cnt));
	for (i = 0; i < cnt; i++) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Walk_dir: File %d name: %s", i, names[i]));
		/*
		 * Skip DB-owned files: ., ..,  __db*, DB_CONFIG, log*
		 */
		if (strcmp(names[i], ".") == 0)
			continue;
		if (strcmp(names[i], "..") == 0)
			continue;
		if (strncmp(names[i], "__db", 4) == 0)
			continue;
		if (strncmp(names[i], "DB_CONFIG", 9) == 0)
			continue;
		if (strncmp(names[i], "log", 3) == 0)
			continue;
		/*
		 * We found a file to process.  Check if we need
		 * to allocate more space.
		 */
		if (dir == NULL) {
			file = NULL;
			subdb = names[i];
		} else {
			file = names[i];
			subdb = NULL;
		}
		if ((ret = __rep_get_fileinfo(dbenv,
		    file, subdb, &tmpfp, uid, filecntp)) != 0) {
			/*
			 * If we find a file that isn't a database, skip it.
			 */
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Walk_dir: File %d %s: returned error %s",
			    i, names[i], db_strerror(ret)));
			ret = 0;
			continue;
		}
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Walk_dir: File %d (of %d) %s: pgsize %lu, max_pgno %lu",
		    tmpfp.filenum, i, names[i],
		    (u_long)tmpfp.pgsize, (u_long)tmpfp.max_pgno));
		namedbt.data = names[i];
		namedbt.size = (u_int32_t)strlen(names[i]) + 1;
		uiddbt.data = uid;
		uiddbt.size = DB_FILE_ID_LEN;
retry:
		ret = __rep_fileinfo_buf(rfp, *fileszp, &len,
		    tmpfp.pgsize, tmpfp.pgno, tmpfp.max_pgno,
		    tmpfp.filenum, tmpfp.id, tmpfp.type,
		    tmpfp.flags, &uiddbt, &namedbt);
		if (ret == ENOMEM) {
			offset = (size_t)(rfp - fp);
			*fileszp *= 2;
			/*
			 * Need to account for update info on both sides
			 * of the allocation.
			 */
			fp -= sizeof(__rep_update_args);
			if ((ret = __os_realloc(dbenv, *fileszp, fp)) != 0)
				break;
			fp += sizeof(__rep_update_args);
			rfp = fp + offset;
			/*
			 * Now that we've reallocated the space, try to
			 * store it again.
			 */
			goto retry;
		}
		rfp += len;
		*filelenp += len;
	}
	__os_dirfree(dbenv, names, cnt);
	return (ret);
}

static int
__rep_get_fileinfo(dbenv, file, subdb, rfp, uid, filecntp)
	DB_ENV *dbenv;
	const char *file, *subdb;
	__rep_fileinfo_args *rfp;
	u_int8_t *uid;
	u_int32_t *filecntp;
{

	DB *dbp, *entdbp;
	DB_LOCK lk;
	DB_LOG *dblp;
	DB_MPOOLFILE *mpf;
	DBC *dbc;
	DBMETA *dbmeta;
	PAGE *pagep;
	int i, ret, t_ret;

	dbp = NULL;
	dbc = NULL;
	pagep = NULL;
	mpf = NULL;
	LOCK_INIT(lk);

	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		goto err;
	if ((ret = __db_open(dbp, NULL, file, subdb, DB_UNKNOWN,
	    DB_RDONLY | (F_ISSET(dbenv, DB_ENV_THREAD) ? DB_THREAD : 0),
	    0, PGNO_BASE_MD)) != 0)
		goto err;

	if ((ret = __db_cursor(dbp, NULL, &dbc, 0)) != 0)
		goto err;
	if ((ret = __db_lget(
	    dbc, 0, dbp->meta_pgno, DB_LOCK_READ, 0, &lk)) != 0)
		goto err;
	if ((ret = __memp_fget(dbp->mpf, &dbp->meta_pgno, 0, &pagep)) != 0)
		goto err;
	/*
	 * We have the meta page.  Set up our information.
	 */
	dbmeta = (DBMETA *)pagep;
	rfp->pgno = 0;
	/*
	 * Queue is a special-case.  We need to set max_pgno to 0 so that
	 * the client can compute the pages from the meta-data.
	 */
	if (dbp->type == DB_QUEUE)
		rfp->max_pgno = 0;
	else
		rfp->max_pgno = dbmeta->last_pgno;
	rfp->pgsize = dbp->pgsize;
	memcpy(uid, dbp->fileid, DB_FILE_ID_LEN);
	rfp->filenum = (*filecntp)++;
	rfp->type = (u_int32_t)dbp->type;
	rfp->flags = dbp->flags;
	rfp->id = DB_LOGFILEID_INVALID;
	ret = __memp_fput(dbp->mpf, pagep, 0);
	pagep = NULL;
	if ((t_ret = __LPUT(dbc, lk)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		goto err;
err:
	if ((t_ret = __LPUT(dbc, lk)) != 0 && ret == 0)
		ret = t_ret;
	if (dbc != NULL && (t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	if (pagep != NULL &&
	    (t_ret = __memp_fput(mpf, pagep, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (dbp != NULL && (t_ret = __db_close(dbp, NULL, 0)) != 0 && ret == 0)
		ret = t_ret;
	/*
	 * We walk the entry table now, after closing the dbp because
	 * otherwise we find the open from this function and the id
	 * is useless in that case.
	 */
	if (ret == 0) {
		LOG_SYSTEM_LOCK(dbenv);
		/*
		 * Walk entry table looking for this uid.
		 * If we find it, save the id.
		 */
		for (dblp = dbenv->lg_handle,
		    i = 0; i < dblp->dbentry_cnt; i++) {
			entdbp = dblp->dbentry[i].dbp;
			if (entdbp == NULL)
				break;
			DB_ASSERT(entdbp->log_filename != NULL);
			if (memcmp(uid,
			    entdbp->log_filename->ufid,
			    DB_FILE_ID_LEN) == 0)
				rfp->id = i;
		}
		LOG_SYSTEM_UNLOCK(dbenv);
	}
	return (ret);
}

/*
 * __rep_page_req
 *	Process a page_req and send the page information to the client.
 *
 * PUBLIC: int __rep_page_req __P((DB_ENV *, int, DBT *));
 */
int
__rep_page_req(dbenv, eid, rec)
	DB_ENV *dbenv;
	int eid;
	DBT *rec;
{
	__rep_fileinfo_args *msgfp;
	DB *dbp;
	DBT msgdbt;
	DB_LOG *dblp;
	DB_MPOOLFILE *mpf;
	DB_REP *db_rep;
	REP *rep;
	int ret, t_ret;
	void *next;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;

	if ((ret = __rep_fileinfo_read(dbenv, rec->data, &next, &msgfp)) != 0)
		return (ret);

	/*
	 * See if we can find it already.  If so we can quickly access its
	 * mpool and process.  Otherwise we have to open the file ourselves.
	 */
	RPRINT(dbenv, rep, (dbenv, &mb, "page_req: file %d page %lu to %lu",
	    msgfp->filenum, (u_long)msgfp->pgno, (u_long)msgfp->max_pgno));
	LOG_SYSTEM_LOCK(dbenv);
	if (msgfp->id >= 0 && dblp->dbentry_cnt > msgfp->id) {
		dbp = dblp->dbentry[msgfp->id].dbp;
		if (dbp != NULL) {
			DB_ASSERT(dbp->log_filename != NULL);
			if (memcmp(msgfp->uid.data, dbp->log_filename->ufid,
			    DB_FILE_ID_LEN) == 0) {
				LOG_SYSTEM_UNLOCK(dbenv);
				RPRINT(dbenv, rep, (dbenv, &mb,
				    "page_req: found %d in dbreg",
				    msgfp->filenum));
				ret = __rep_page_sendpages(dbenv, eid,
				    msgfp, dbp->mpf, dbp);
				goto err;
			}
		}
	}
	LOG_SYSTEM_UNLOCK(dbenv);

	/*
	 * If we get here, we do not have the file open via dbreg.
	 * We need to open the file and then send its pages.
	 * If we cannot open the file, we send REP_FILE_FAIL.
	 */
	RPRINT(dbenv, rep, (dbenv, &mb, "page_req: Open %d via mpf_open",
	    msgfp->filenum));
	if ((ret = __rep_mpf_open(dbenv, &mpf, msgfp, 0)) != 0) {
		memset(&msgdbt, 0, sizeof(msgdbt));
		msgdbt.data = msgfp;
		msgdbt.size = sizeof(*msgfp);
		RPRINT(dbenv, rep, (dbenv, &mb, "page_req: Open %d failed",
		    msgfp->filenum));
		if (F_ISSET(rep, REP_F_MASTER))
			(void)__rep_send_message(dbenv, eid, REP_FILE_FAIL,
			    NULL, &msgdbt, 0, 0);
		else
			ret = DB_NOTFOUND;
		goto err;
	}

	ret = __rep_page_sendpages(dbenv, eid, msgfp, mpf, NULL);
	t_ret = __memp_fclose(mpf, 0);
	if (ret == 0 && t_ret != 0)
		ret = t_ret;
err:
	__os_free(dbenv, msgfp);
	return (ret);
}

static int
__rep_page_sendpages(dbenv, eid, msgfp, mpf, dbp)
	DB_ENV *dbenv;
	int eid;
	__rep_fileinfo_args *msgfp;
	DB_MPOOLFILE *mpf;
	DB *dbp;
{
	DB *qdbp;
	DBT lockdbt, msgdbt, pgdbt;
	DB_LOCK lock;
	DB_LOCK_ILOCK lock_obj;
	DB_LOG *dblp;
	DB_LSN lsn;
	DB_MSGBUF mb;
	DB_REP *db_rep;
	PAGE *pagep;
	REP *rep;
	REP_BULK bulk;
	REP_THROTTLE repth;
	db_pgno_t p;
	uintptr_t bulkoff;
	size_t len, msgsz;
	u_int32_t bulkflags, lockid, use_bulk;
	int opened, ret, t_ret;
	u_int8_t *buf;

#ifndef DIAGNOSTIC
	DB_MSGBUF_INIT(&mb);
#endif
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	lockid = DB_LOCK_INVALIDID;
	opened = 0;
	qdbp = NULL;
	buf = NULL;
	bulk.addr = NULL;
	use_bulk = FLD_ISSET(rep->config, REP_C_BULK);
	if (msgfp->type == (u_int32_t)DB_QUEUE) {
		if (dbp == NULL) {
			if ((ret = db_create(&qdbp, dbenv, 0)) != 0)
				goto err;
			/*
			 * We need to check whether this is in-memory so that
			 * we pass the name correctly as either the file or
			 * the database name.
			 */
			if ((ret = __db_open(qdbp, NULL,
			    FLD_ISSET(msgfp->flags, DB_AM_INMEM) ?
			    NULL : msgfp->info.data,
			    FLD_ISSET(msgfp->flags, DB_AM_INMEM) ?
			    msgfp->info.data : NULL,
			    DB_UNKNOWN,
			    DB_RDONLY | (F_ISSET(dbenv, DB_ENV_THREAD) ?
			    DB_THREAD : 0), 0, PGNO_BASE_MD)) != 0)
				goto err;
			opened = 1;
		} else
			qdbp = dbp;
	}
	msgsz = sizeof(__rep_fileinfo_args) + DB_FILE_ID_LEN + msgfp->pgsize;
	if ((ret = __os_calloc(dbenv, 1, msgsz, &buf)) != 0)
		goto err;
	memset(&msgdbt, 0, sizeof(msgdbt));
	memset(&pgdbt, 0, sizeof(pgdbt));
	RPRINT(dbenv, rep, (dbenv, &mb, "sendpages: file %d page %lu to %lu",
	    msgfp->filenum, (u_long)msgfp->pgno, (u_long)msgfp->max_pgno));
	memset(&repth, 0, sizeof(repth));
	/*
	 * If we're doing bulk transfer, allocate a bulk buffer to put our
	 * pages in.  We still need to initialize the throttle info
	 * because if we encounter a page larger than our entire bulk
	 * buffer, we need to send it as a singleton.
	 *
	 * Use a local var so that we don't need to worry if someone else
	 * turns on/off bulk in the middle of our call here.
	 */
	if (use_bulk && (ret = __rep_bulk_alloc(dbenv, &bulk, eid,
	    &bulkoff, &bulkflags, REP_BULK_PAGE)) != 0)
		goto err;
	REP_SYSTEM_LOCK(dbenv);
	repth.gbytes = rep->gbytes;
	repth.bytes = rep->bytes;
	repth.type = REP_PAGE;
	repth.data_dbt = &msgdbt;
	REP_SYSTEM_UNLOCK(dbenv);

	/*
	 * Set up locking.
	 */
	LOCK_INIT(lock);
	memset(&lock_obj, 0, sizeof(lock_obj));
	if ((ret = __lock_id(dbenv, &lockid, NULL)) != 0)
		goto err;
	memcpy(lock_obj.fileid, mpf->fileid, DB_FILE_ID_LEN);
	lock_obj.type = DB_PAGE_LOCK;

	memset(&lockdbt, 0, sizeof(lockdbt));
	lockdbt.data = &lock_obj;
	lockdbt.size = sizeof(lock_obj);

	for (p = msgfp->pgno; p <= msgfp->max_pgno; p++) {
		/*
		 * We're not waiting for the lock, if we cannot get
		 * the lock for this page, skip it.  The gap
		 * code will rerequest it.
		 */
		lock_obj.pgno = p;
		if ((ret = __lock_get(dbenv, lockid, DB_LOCK_NOWAIT, &lockdbt,
		    DB_LOCK_READ, &lock)) != 0) {
			/*
			 * Continue if we couldn't get the lock.
			 */
			if (ret == DB_LOCK_NOTGRANTED)
				continue;
			/*
			 * Otherwise we have an error.
			 */
			goto err;
		}
		if (msgfp->type == (u_int32_t)DB_QUEUE && p != 0)
#ifdef HAVE_QUEUE
			ret = __qam_fget(qdbp, &p, DB_MPOOL_CREATE, &pagep);
#else
			ret = DB_PAGE_NOTFOUND;
#endif
		else
			ret = __memp_fget(mpf, &p, DB_MPOOL_CREATE, &pagep);
		if (ret == DB_PAGE_NOTFOUND) {
			memset(&pgdbt, 0, sizeof(pgdbt));
			ZERO_LSN(lsn);
			msgfp->pgno = p;
			if (F_ISSET(rep, REP_F_MASTER)) {
				ret = 0;
				RPRINT(dbenv, rep, (dbenv, &mb,
				    "sendpages: PAGE_FAIL on page %lu",
				    (u_long)p));
				(void)__rep_send_message(dbenv, eid,
				    REP_PAGE_FAIL, &lsn, &msgdbt, 0, 0);
			} else
				ret = DB_NOTFOUND;
			goto lockerr;
		} else if (ret != 0)
			goto lockerr;
		else {
			pgdbt.data = pagep;
			pgdbt.size = (u_int32_t)msgfp->pgsize;
		}
		len = 0;
		ret = __rep_fileinfo_buf(buf, msgsz, &len,
		    msgfp->pgsize, p, msgfp->max_pgno,
		    msgfp->filenum, msgfp->id, msgfp->type,
		    msgfp->flags, &msgfp->uid, &pgdbt);
		if (msgfp->type != (u_int32_t)DB_QUEUE || p == 0)
			t_ret = __memp_fput(mpf, pagep, 0);
#ifdef HAVE_QUEUE
		else
			/*
			 * We don't need an #else for HAVE_QUEUE here because if
			 * we're not compiled with queue, then we're guaranteed
			 * to have set REP_PAGE_FAIL above.
			 */
			t_ret = __qam_fput(qdbp, p, pagep, 0);
#endif
		if ((t_ret = __ENV_LPUT(dbenv, lock)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto err;

		DB_ASSERT(len <= msgsz);
		msgdbt.data = buf;
		msgdbt.size = (u_int32_t)len;

		dblp = dbenv->lg_handle;
		LOG_SYSTEM_LOCK(dbenv);
		repth.lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		LOG_SYSTEM_UNLOCK(dbenv);
		/*
		 * If we are configured for bulk, try to send this as a bulk
		 * request.  If not configured, or it is too big for bulk
		 * then just send normally.
		 */
		if (use_bulk)
			ret = __rep_bulk_message(dbenv, &bulk, &repth,
			    &repth.lsn, &msgdbt, 0);
		if (!use_bulk || ret == DB_REP_BULKOVF)
			ret = __rep_send_throttle(dbenv, eid, &repth, 0);
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "sendpages: %lu, lsn [%lu][%lu]", (u_long)p,
		    (u_long)repth.lsn.file, (u_long)repth.lsn.offset));
		/*
		 * If we have REP_PAGE_MORE
		 * we need to break this loop after giving the page back
		 * to mpool.  Otherwise, with REP_PAGE, we keep going.
		 */
		if (ret == 0)
			ret = t_ret;
		if (repth.type == REP_PAGE_MORE || ret != 0)
			break;
	}

	if (0) {
lockerr:	if ((t_ret = __ENV_LPUT(dbenv, lock)) != 0 && ret == 0)
			ret = t_ret;
	}
err:
	/*
	 * We're done, force out whatever remains in the bulk buffer and
	 * free it.
	 */
	if (use_bulk && bulk.addr != NULL &&
	    (t_ret = __rep_bulk_free(dbenv, &bulk, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (opened && (t_ret = __db_close(qdbp, NULL, DB_NOSYNC)) != 0 &&
	    ret == 0)
		ret = t_ret;
	if (buf != NULL)
		__os_free(dbenv, buf);
	if (lockid != DB_LOCK_INVALIDID && (t_ret = __lock_id_free(dbenv,
	    lockid)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_update_setup
 *	Process and setup with this file information.
 *
 * PUBLIC: int __rep_update_setup __P((DB_ENV *, int, REP_CONTROL *, DBT *));
 */
int
__rep_update_setup(dbenv, eid, rp, rec)
	DB_ENV *dbenv;
	int eid;
	REP_CONTROL *rp;
	DBT *rec;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	DBT pagereq_dbt;
	LOG *lp;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	__rep_update_args *rup;
	int ret;
	u_int32_t count, infolen;
	void *next;
#ifdef DIAGNOSTIC
	__rep_fileinfo_args *msgfp;
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	ret = 0;

	REP_SYSTEM_LOCK(dbenv);
	if (!F_ISSET(rep, REP_F_RECOVER_UPDATE)) {
		REP_SYSTEM_UNLOCK(dbenv);
		return (0);
	}
	F_CLR(rep, REP_F_RECOVER_UPDATE);
	/*
	 * We know we're the first to come in here due to the
	 * REP_F_RECOVER_UPDATE flag.
	 */
	F_SET(rep, REP_F_RECOVER_PAGE);
	/*
	 * We do not clear REP_F_READY or rep->in_recovery in this code.
	 * We'll eventually call the normal __rep_verify_match recovery
	 * code and that will clear all the flags and allow others to
	 * proceed.
	 */
	if ((ret = __rep_lockout(dbenv, rep, 1)) != 0)
		goto err;
	/*
	 * We need to update the timestamp and kill any open handles
	 * on this client.  The files are changing completely.
	 */
	infop = dbenv->reginfo;
	renv = infop->primary;
	(void)time(&renv->rep_timestamp);

	REP_SYSTEM_UNLOCK(dbenv);
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	lp->wait_recs = rep->request_gap;
	lp->rcvd_recs = 0;
	ZERO_LSN(lp->ready_lsn);
	ZERO_LSN(lp->waiting_lsn);
	ZERO_LSN(lp->max_wait_lsn);
	ZERO_LSN(lp->max_perm_lsn);
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	if ((ret = __rep_update_read(dbenv, rec->data, &next, &rup)) != 0)
		goto err_nolock;

	/*
	 * We need to empty out any old log records that might be in the
	 * temp database.
	 */
	if ((ret = __db_truncate(db_rep->rep_db, NULL, &count)) != 0)
		goto err_nolock;

	/*
	 * We will remove all logs we have so we need to request
	 * from the master's beginning.
	 */
	REP_SYSTEM_LOCK(dbenv);
	rep->first_lsn = rup->first_lsn;
	rep->last_lsn = rp->lsn;
	rep->nfiles = rup->num_files;
	rep->curfile = 0;
	rep->ready_pg = 0;
	rep->npages = 0;
	rep->waiting_pg = PGNO_INVALID;
	rep->max_wait_pg = PGNO_INVALID;

	__os_free(dbenv, rup);

	RPRINT(dbenv, rep, (dbenv, &mb,
	    "Update setup for %d files.", rep->nfiles));
	RPRINT(dbenv, rep, (dbenv, &mb, "Update setup:  First LSN [%lu][%lu].",
	    (u_long)rep->first_lsn.file, (u_long)rep->first_lsn.offset));
	RPRINT(dbenv, rep, (dbenv, &mb, "Update setup:  Last LSN [%lu][%lu]",
	    (u_long)rep->last_lsn.file, (u_long)rep->last_lsn.offset));

	infolen = rec->size - sizeof(__rep_update_args);
	if ((ret = __os_calloc(dbenv, 1, infolen, &rep->originfo)) != 0)
		goto err;
	memcpy(rep->originfo, next, infolen);
	rep->finfo = rep->originfo;
	if ((ret = __rep_fileinfo_read(dbenv,
	    rep->finfo, &next, &rep->curinfo)) != 0) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Update setup: Fileinfo read: %s", db_strerror(ret)));
		goto errmem1;
	}
	rep->nextinfo = next;

#ifdef DIAGNOSTIC
	msgfp = rep->curinfo;
	DB_ASSERT(msgfp->pgno == 0);
#endif

	/*
	 * We want to create/open our dbp to the database
	 * where we'll keep our page information.
	 */
	if ((ret = __rep_client_dbinit(dbenv, 1, REP_PG)) != 0) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Update setup: Client_dbinit %s", db_strerror(ret)));
		goto errmem;
	}

	/*
	 * We should get file info 'ready to go' to avoid data copies.
	 */
	memset(&pagereq_dbt, 0, sizeof(pagereq_dbt));
	pagereq_dbt.data = rep->finfo;
	pagereq_dbt.size = (u_int32_t)((u_int8_t *)rep->nextinfo -
	    (u_int8_t *)rep->finfo);

	RPRINT(dbenv, rep, (dbenv, &mb,
	    "Update PAGE_REQ file 0: pgsize %lu, maxpg %lu",
	    (u_long)rep->curinfo->pgsize,
	    (u_long)rep->curinfo->max_pgno));
	/*
	 * We set up pagereq_dbt as we went along.  Send it now.
	 */
	(void)__rep_send_message(dbenv, eid, REP_PAGE_REQ,
	    NULL, &pagereq_dbt, 0, DB_REP_ANYWHERE);
	if (0) {
errmem:		__os_free(dbenv, rep->curinfo);
errmem1:	__os_free(dbenv, rep->originfo);
		rep->finfo = NULL;
		rep->curinfo = NULL;
		rep->originfo = NULL;
	}

	if (0) {
err_nolock:	REP_SYSTEM_LOCK(dbenv);
	}

err:	/*
	 * If we get an error, we cannot leave ourselves in the RECOVER_PAGE
	 * state because we have no file information.  That also means undo'ing
	 * the rep_lockout.  We need to move back to the RECOVER_UPDATE stage.
	 */
	if (ret != 0) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Update_setup: Error: Clear PAGE, set UPDATE again. %s",
		    db_strerror(ret)));
		F_CLR(rep, REP_F_RECOVER_PAGE | REP_F_READY);
		rep->in_recovery = 0;
		F_SET(rep, REP_F_RECOVER_UPDATE);
	}
	REP_SYSTEM_UNLOCK(dbenv);
	return (ret);
}

/*
 * __rep_bulk_page
 *	Process a bulk page message.
 *
 * PUBLIC: int __rep_bulk_page __P((DB_ENV *, int, REP_CONTROL *, DBT *));
 */
int
__rep_bulk_page(dbenv, eid, rp, rec)
	DB_ENV *dbenv;
	int eid;
	REP_CONTROL *rp;
	DBT *rec;
{
	DB_REP *db_rep;
	DBT pgrec;
	REP *rep;
	REP_CONTROL tmprp;
	u_int32_t len;
	int ret;
	u_int8_t *p, *ep;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	memset(&pgrec, 0, sizeof(pgrec));
	/*
	 * We're going to be modifying the rp LSN contents so make
	 * our own private copy to play with.  We need to set the
	 * rectype to REP_PAGE because we're calling through __rep_page
	 * to process each page, and lower functions make decisions
	 * based on the rectypes (for throttling/gap processing)
	 */
	memcpy(&tmprp, rp, sizeof(tmprp));
	tmprp.rectype = REP_PAGE;
	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	for (ep = (u_int8_t *)rec->data + rec->size, p = (u_int8_t *)rec->data;
	    p < ep; p += len) {
		/*
		 * First thing in the buffer is the length.  Then the LSN
		 * of this page, then the page info itself.
		 */
		memcpy(&len, p, sizeof(len));
		p += sizeof(len);
		memcpy(&tmprp.lsn, p, sizeof(DB_LSN));
		p += sizeof(DB_LSN);
		pgrec.data = p;
		pgrec.size = len;
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "rep_bulk_page: Processing LSN [%lu][%lu]",
		    (u_long)tmprp.lsn.file, (u_long)tmprp.lsn.offset));
		RPRINT(dbenv, rep, (dbenv, &mb,
    "rep_bulk_page: p %#lx ep %#lx pgrec data %#lx, size %lu (%#lx)",
		    P_TO_ULONG(p), P_TO_ULONG(ep), P_TO_ULONG(pgrec.data),
		    (u_long)pgrec.size, (u_long)pgrec.size));
		/*
		 * Now send the page info DBT to the page processing function.
		 */
		ret = __rep_page(dbenv, eid, &tmprp, &pgrec);
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "rep_bulk_page: rep_page ret %d", ret));

		if (ret != 0)
			break;
	}
	return (ret);
}

/*
 * __rep_page
 *	Process a page message.
 *
 * PUBLIC: int __rep_page __P((DB_ENV *, int, REP_CONTROL *, DBT *));
 */
int
__rep_page(dbenv, eid, rp, rec)
	DB_ENV *dbenv;
	int eid;
	REP_CONTROL *rp;
	DBT *rec;
{

	DB_REP *db_rep;
	DBT key, data;
	REP *rep;
	__rep_fileinfo_args *msgfp;
	db_recno_t recno;
	int ret;
	void *next;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	if (!F_ISSET(rep, REP_F_RECOVER_PAGE)) {
		REP_SYSTEM_UNLOCK(dbenv);
		return (0);
	}
	if ((ret = __rep_fileinfo_read(dbenv, rec->data, &next, &msgfp)) != 0) {
		REP_SYSTEM_UNLOCK(dbenv);
		return (ret);
	}
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "PAGE: Received page %lu from file %d",
	    (u_long)msgfp->pgno, msgfp->filenum));
	/*
	 * Check if this page is from the file we're expecting.
	 * This may be an old or delayed page message.
	 */
	/*
	 * !!!
	 * If we allow dbrename/dbremove on the master while a client
	 * is updating, then we'd have to verify the file's uid here too.
	 */
	if (msgfp->filenum != rep->curfile) {
		RPRINT(dbenv, rep,
		    (dbenv, &mb, "Msg file %d != curfile %d",
		    msgfp->filenum, rep->curfile));
		goto err;
	}
	/*
	 * We want to create/open our dbp to the database
	 * where we'll keep our page information.
	 */
	if ((ret = __rep_client_dbinit(dbenv, 1, REP_PG)) != 0)
		goto err;

	REP_SYSTEM_UNLOCK(dbenv);
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	recno = (db_recno_t)(msgfp->pgno + 1);
	key.data = &recno;
	key.ulen = key.size = sizeof(db_recno_t);
	key.flags = DB_DBT_USERMEM;

	/*
	 * If we already have this page, then we don't want to bother
	 * rewriting it into the file.  Otherwise, any other error
	 * we want to return.
	 */
	ret = __db_put(rep->file_dbp, NULL, &key, &data, DB_NOOVERWRITE);
	if (ret == DB_KEYEXIST) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "PAGE: Received duplicate page %lu from file %d",
		    (u_long)msgfp->pgno, msgfp->filenum));
		rep->stat.st_pg_duplicated++;
		ret = 0;
		goto err_nolock;
	}
	if (ret != 0)
		goto err_nolock;

	RPRINT(dbenv, rep, (dbenv, &mb,
	    "PAGE: Write page %lu into mpool", (u_long)msgfp->pgno));
	REP_SYSTEM_LOCK(dbenv);
	/*
	 * We put the page in the database file itself.
	 */
	ret = __rep_write_page(dbenv, rep, msgfp);
	if (ret != 0) {
		/*
		 * We got an error storing the page, therefore, we need
		 * remove this page marker from the page database too.
		 * !!!
		 * I'm ignoring errors from the delete because we want to
		 * return the original error.  If we cannot write the page
		 * and we cannot delete the item we just put, what should
		 * we do?  Panic the env and return DB_RUNRECOVERY?
		 */
		(void)__db_del(rep->file_dbp, NULL, &key, 0);
		goto err;
	}
	rep->stat.st_pg_records++;
	rep->npages++;

	/*
	 * Now check the LSN on the page and save it if it is later
	 * than the one we have.
	 */
	if (log_compare(&rp->lsn, &rep->last_lsn) > 0)
		rep->last_lsn = rp->lsn;

	/*
	 * We've successfully written the page.  Now we need to see if
	 * we're done with this file.  __rep_filedone will check if we
	 * have all the pages expected and if so, set up for the next
	 * file and send out a page request for the next file's pages.
	 */
	ret = __rep_filedone(dbenv, eid, rep, msgfp, rp->rectype);

err:	REP_SYSTEM_UNLOCK(dbenv);

err_nolock:
	__os_free(dbenv, msgfp);
	return (ret);
}

/*
 * __rep_page_fail
 *	Process a page fail message.
 *
 * PUBLIC: int __rep_page_fail __P((DB_ENV *, int, DBT *));
 */
int
__rep_page_fail(dbenv, eid, rec)
	DB_ENV *dbenv;
	int eid;
	DBT *rec;
{

	DB_REP *db_rep;
	REP *rep;
	__rep_fileinfo_args *msgfp, *rfp;
	int ret;
	void *next;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	if (!F_ISSET(rep, REP_F_RECOVER_PAGE)) {
		REP_SYSTEM_UNLOCK(dbenv);
		return (0);
	}
	if ((ret = __rep_fileinfo_read(dbenv, rec->data, &next, &msgfp)) != 0) {
		REP_SYSTEM_UNLOCK(dbenv);
		return (ret);
	}
	/*
	 * Check if this page is from the file we're expecting.
	 * This may be an old or delayed page message.
	 */
	/*
	 * !!!
	 * If we allow dbrename/dbremove on the master while a client
	 * is updating, then we'd have to verify the file's uid here too.
	 */
	if (msgfp->filenum != rep->curfile) {
		RPRINT(dbenv, rep, (dbenv, &mb, "Msg file %d != curfile %d",
		    msgfp->filenum, rep->curfile));
		REP_SYSTEM_UNLOCK(dbenv);
		return (0);
	}
	rfp = rep->curinfo;
	if (rfp->type != (u_int32_t)DB_QUEUE)
		--rfp->max_pgno;
	else {
		/*
		 * Queue is special.  Pages at the beginning of the queue
		 * may disappear, as well as at the end.  Use msgfp->pgno
		 * to adjust accordingly.
		 */
		RPRINT(dbenv, rep, (dbenv, &mb,
	    "page_fail: BEFORE page %lu failed. ready %lu, max %lu, npages %d",
		    (u_long)msgfp->pgno, (u_long)rep->ready_pg,
		    (u_long)rfp->max_pgno, rep->npages));
		if (msgfp->pgno == rfp->max_pgno)
			--rfp->max_pgno;
		if (msgfp->pgno >= rep->ready_pg) {
			rep->ready_pg = msgfp->pgno + 1;
			rep->npages = rep->ready_pg;
		}
		RPRINT(dbenv, rep, (dbenv, &mb,
	    "page_fail: AFTER page %lu failed. ready %lu, max %lu, npages %d",
		    (u_long)msgfp->pgno, (u_long)rep->ready_pg,
		    (u_long)rfp->max_pgno, rep->npages));
	}
	/*
	 * We've lowered the number of pages expected.  It is possible that
	 * this was the last page we were expecting.  Now we need to see if
	 * we're done with this file.  __rep_filedone will check if we have
	 * all the pages expected and if so, set up for the next file and
	 * send out a page request for the next file's pages.
	 */
	ret = __rep_filedone(dbenv, eid, rep, msgfp, REP_PAGE_FAIL);
	REP_SYSTEM_UNLOCK(dbenv);
	return (ret);
}

/*
 * __rep_write_page -
 *	Write this page into a database.
 */
static int
__rep_write_page(dbenv, rep, msgfp)
	DB_ENV *dbenv;
	REP *rep;
	__rep_fileinfo_args *msgfp;
{
	__rep_fileinfo_args *rfp;
	DB_FH *rfh;
	int ret;
	void *dst;
	char *real_name;

	real_name = NULL;
	rfp = NULL;

	/*
	 * If this is the first page we're putting in this database, we need
	 * to create the mpool file.  Otherwise call memp_fget to create the
	 * page in mpool.  Then copy the data to the page, and memp_fput the
	 * page to give it back to mpool.
	 *
	 * We need to create the file, removing any existing file and associate
	 * the correct file ID with the new one.
	 */
	rfp = rep->curinfo;
	if (rep->file_mpf == NULL) {
		if (!F_ISSET(rfp, DB_AM_INMEM)) {
			if ((ret = __db_appname(dbenv, DB_APP_DATA,
			    rfp->info.data, 0, NULL, &real_name)) != 0)
				goto err;
			/*
			 * Calling memp_nameop will both purge any matching
			 * fileid from mpool and unlink it on disk.
			 */
			if ((ret = __memp_nameop(dbenv,
			    rfp->uid.data, NULL, real_name, NULL, 0)) != 0)
				goto err;
			/*
			 * Create the file on disk.  We'll be putting the data
			 * into the file via mpool.
			 */
			if ((ret = __os_open(dbenv, real_name,
			    DB_OSO_CREATE, dbenv->db_mode, &rfh)) == 0)
				ret = __os_closehandle(dbenv, rfh);
			if (ret != 0)
				goto err;
		}

		if ((ret =
		    __rep_mpf_open(dbenv, &rep->file_mpf, rep->curinfo,
		    F_ISSET(rfp, DB_AM_INMEM) ? DB_CREATE : 0)) != 0)
			goto err;
	}
	/*
	 * Handle queue specially.  If we're a QUEUE database, we need to
	 * use the __qam_fget/put calls.  We need to use rep->queue_dbp for
	 * that.  That dbp is opened after getting the metapage for the
	 * queue database.  Since the meta-page is always in the queue file,
	 * we'll use the normal path for that first page.  After that we
	 * can assume the dbp is opened.
	 */
	if (msgfp->type == (u_int32_t)DB_QUEUE && msgfp->pgno != 0) {
#ifdef HAVE_QUEUE
		if ((ret = __qam_fget(
		    rep->queue_dbp, &msgfp->pgno, DB_MPOOL_CREATE, &dst)) != 0)
			goto err;
#else
		/*
		 * This always returns an error.
		 */
		ret = __db_no_queue_am(dbenv);
		goto err;
#endif
	} else if ((ret = __memp_fget(
		    rep->file_mpf, &msgfp->pgno, DB_MPOOL_CREATE, &dst)) != 0)
			goto err;

	memcpy(dst, msgfp->info.data, msgfp->pgsize);
	if (msgfp->type != (u_int32_t)DB_QUEUE || msgfp->pgno == 0)
		ret = __memp_fput(rep->file_mpf, dst, DB_MPOOL_DIRTY);
#ifdef HAVE_QUEUE
	else
		ret = __qam_fput(rep->queue_dbp, msgfp->pgno, dst,
		    DB_MPOOL_DIRTY);
#endif

err:	if (real_name != NULL)
		 __os_free(dbenv, real_name);
	return (ret);
}

/*
 * __rep_page_gap -
 *	After we've put the page into the database, we need to check if
 *	we have a page gap and whether we need to request pages.
 */
static int
__rep_page_gap(dbenv, rep, msgfp, type)
	DB_ENV *dbenv;
	REP *rep;
	__rep_fileinfo_args *msgfp;
	u_int32_t type;
{
	DB_LOG *dblp;
	DBT data, key;
	LOG *lp;
	__rep_fileinfo_args *rfp;
	db_recno_t recno;
	int ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	ret = 0;

	/*
	 * We've successfully put this page into our file.
	 * Now we need to account for it and re-request new pages
	 * if necessary.
	 */
	/*
	 * We already hold the rep mutex, but we also need the db mutex.
	 * So we need to drop it, acquire both in the right order and
	 * then recheck the state of the world.
	 */
	REP_SYSTEM_UNLOCK(dbenv);
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	REP_SYSTEM_LOCK(dbenv);
	rfp = rep->curinfo;

	/*
	 * Make sure we're still talking about the same file.
	 * If not, we're done here.
	 */
	if (rfp->filenum != msgfp->filenum) {
		ret = DB_REP_PAGEDONE;
		goto err;
	}

	/*
	 * We have 3 possible states:
	 * 1.  We receive a page we already have.
	 *	msg pgno < ready pgno
	 * 2.  We receive a page that is beyond a gap.
	 *	msg pgno > ready pgno
	 * 3.  We receive the page we're expecting.
	 *	msg pgno == ready pgno
	 */
	/*
	 * State 1.  This should not happen because this function
	 * should only be called once per page received because we
	 * check for DB_KEY_EXIST when we save the page information.
	 */
	DB_ASSERT(msgfp->pgno >= rep->ready_pg);

	/*
	 * State 2.  This page is beyond the page we're expecting.
	 * We need to update waiting_pg if this page is less than
	 * (earlier) the current waiting_pg.  There is nothing
	 * to do but see if we need to request.
	 */
	RPRINT(dbenv, rep, (dbenv, &mb,
    "PAGE_GAP: pgno %lu, max_pg %lu ready %lu, waiting %lu max_wait %lu",
	    (u_long)msgfp->pgno, (u_long)rfp->max_pgno, (u_long)rep->ready_pg,
	    (u_long)rep->waiting_pg, (u_long)rep->max_wait_pg));
	if (msgfp->pgno > rep->ready_pg) {
		if (rep->waiting_pg == PGNO_INVALID ||
		    msgfp->pgno < rep->waiting_pg)
			rep->waiting_pg = msgfp->pgno;
	} else {
		/*
		 * We received the page we're expecting.
		 */
		rep->ready_pg++;
		lp->rcvd_recs = 0;
		while (ret == 0 && rep->ready_pg == rep->waiting_pg) {
			/*
			 * If we get here we know we just filled a gap.
			 */
			lp->wait_recs = 0;
			lp->rcvd_recs = 0;
			rep->max_wait_pg = PGNO_INVALID;
			/*
			 * We need to walk the recno database looking for the
			 * next page we need or expect.
			 */
			memset(&key, 0, sizeof(key));
			memset(&data, 0, sizeof(data));
			recno = (db_recno_t)rep->ready_pg;
			key.data = &recno;
			key.ulen = key.size = sizeof(db_recno_t);
			key.flags = DB_DBT_USERMEM;
			ret = __db_get(rep->file_dbp, NULL, &key, &data, 0);
			if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
				break;
			else if (ret != 0)
				goto err;
			rep->ready_pg++;
		}
	}

	/*
	 * If we filled a gap and now have the entire file, there's
	 * nothing to do.  We're done when ready_pg is > max_pgno
	 * because ready_pg is larger than the last page we received.
	 */
	if (rep->ready_pg > rfp->max_pgno)
		goto err;

	/*
	 * Check if we need to ask for more pages.
	 */
	if ((rep->waiting_pg != PGNO_INVALID &&
	    rep->ready_pg != rep->waiting_pg) || type == REP_PAGE_MORE) {
		/*
		 * We got a page but we may still be waiting for more.
		 */
		if (lp->wait_recs == 0) {
			/*
			 * This is a new gap. Initialize the number of
			 * records that we should wait before requesting
			 * that it be resent.  We grab the limits out of
			 * the rep without the mutex.
			 */
			lp->wait_recs = rep->request_gap;
			lp->rcvd_recs = 0;
			rep->max_wait_pg = PGNO_INVALID;
		}
		/*
		 * If we got REP_PAGE_MORE we always want to ask for more.
		 */
		if ((__rep_check_doreq(dbenv, rep) || type == REP_PAGE_MORE) &&
		    ((ret = __rep_pggap_req(dbenv, rep, rfp,
		    (type == REP_PAGE_MORE) ? REP_GAP_FORCE : 0)) != 0))
			goto err;
	} else {
		lp->wait_recs = 0;
		rep->max_wait_pg = PGNO_INVALID;
	}

err:
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	return (ret);
}

/*
 * __rep_init_cleanup -
 *	Clean up internal initialization pieces.
 *
 * PUBLIC: int __rep_init_cleanup __P((DB_ENV *, REP *, int));
 */
int
__rep_init_cleanup(dbenv, rep, force)
	DB_ENV *dbenv;
	REP *rep;
	int force;
{
	int ret, t_ret;

	ret = 0;
	/*
	 * 1.  Close up the file data pointer we used.
	 * 2.  Close/reset the page database.
	 * 3.  Close/reset the queue database if we're forcing a cleanup.
	 * 4.  Free current file info.
	 * 5.  If we have all files or need to force, free original file info.
	 */
	if (rep->file_mpf != NULL) {
		ret = __memp_fclose(rep->file_mpf, 0);
		rep->file_mpf = NULL;
	}
	if (rep->file_dbp != NULL) {
		t_ret = __db_close(rep->file_dbp, NULL, DB_NOSYNC);
		rep->file_dbp = NULL;
		if (t_ret != 0 && ret == 0)
			ret = t_ret;
	}
	if (force && rep->queue_dbp != NULL) {
		t_ret = __db_close(rep->queue_dbp, NULL, DB_NOSYNC);
		rep->queue_dbp = NULL;
		if (t_ret != 0 && ret == 0)
			ret = t_ret;
	}
	if (rep->curinfo != NULL) {
		__os_free(dbenv, rep->curinfo);
		rep->curinfo = NULL;
	}
	if (rep->originfo != NULL &&
	    (force || ++rep->curfile == rep->nfiles)) {
		__os_free(dbenv, rep->originfo);
		rep->originfo = NULL;
	}
	return (ret);
}

/*
 * __rep_filedone -
 *	We need to check if we're done with the current file after
 *	processing the current page.  Stat the database to see if
 *	we have all the pages.  If so, we need to clean up/close
 *	this one, set up for the next one, and ask for its pages,
 *	or if this is the last file, request the log records and
 *	move to the REP_RECOVER_LOG state.
 */
static int
__rep_filedone(dbenv, eid, rep, msgfp, type)
	DB_ENV *dbenv;
	int eid;
	REP *rep;
	__rep_fileinfo_args *msgfp;
	u_int32_t type;
{
	DBT dbt;
	__rep_fileinfo_args *rfp;
	int ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	/*
	 * We've put our page, now we need to do any gap processing
	 * that might be needed to re-request pages.
	 */
	ret = __rep_page_gap(dbenv, rep, msgfp, type);
	/*
	 * The world changed while we were doing gap processing.
	 * We're done here.
	 */
	if (ret == DB_REP_PAGEDONE)
		return (0);

	rfp = rep->curinfo;
	/*
	 * max_pgno is 0-based and npages is 1-based, so we don't have
	 * all the pages until npages is > max_pgno.
	 */
	RPRINT(dbenv, rep, (dbenv, &mb, "FILEDONE: have %lu pages. Need %lu.",
	    (u_long)rep->npages, (u_long)rfp->max_pgno + 1));
	if (rep->npages <= rfp->max_pgno)
		return (0);

	/*
	 * If we're queue and we think we have all the pages for this file,
	 * we need to do special queue processing.  Queue is handled in
	 * several stages.
	 */
	if (rfp->type == (u_int32_t)DB_QUEUE &&
	    ((ret = __rep_queue_filedone(dbenv, rep, rfp)) !=
	    DB_REP_PAGEDONE))
		return (ret);
	/*
	 * We have all the pages for this file.  Clean up.
	 */
	if ((ret = __rep_init_cleanup(dbenv, rep, 0)) != 0)
		goto err;
	if (rep->curfile == rep->nfiles) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "FILEDONE: have %d files.  RECOVER_LOG now", rep->nfiles));
		/*
		 * Move to REP_RECOVER_LOG state.
		 * Request logs.
		 */
		/*
		 * We need to do a sync here so that any later opens
		 * can find the file and file id.  We need to do it
		 * before we clear REP_F_RECOVER_PAGE so that we do not
		 * try to flush the log.
		 */
		if ((ret = __memp_sync(dbenv, NULL)) != 0)
			goto err;
		F_CLR(rep, REP_F_RECOVER_PAGE);
		F_SET(rep, REP_F_RECOVER_LOG);
		memset(&dbt, 0, sizeof(dbt));
		dbt.data = &rep->last_lsn;
		dbt.size = sizeof(rep->last_lsn);
		REP_SYSTEM_UNLOCK(dbenv);
		if ((ret = __rep_log_setup(dbenv, rep)) != 0)
			goto err;
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "FILEDONE: LOG_REQ from LSN [%lu][%lu] to [%lu][%lu]",
		    (u_long)rep->first_lsn.file, (u_long)rep->first_lsn.offset,
		    (u_long)rep->last_lsn.file, (u_long)rep->last_lsn.offset));
		(void)__rep_send_message(dbenv, eid,
		    REP_LOG_REQ, &rep->first_lsn, &dbt, 0, DB_REP_ANYWHERE);
		REP_SYSTEM_LOCK(dbenv);
		return (0);
	}

	/*
	 * 4.  If not, set curinfo to next file and request its pages.
	 */
	rep->finfo = rep->nextinfo;
	if ((ret = __rep_fileinfo_read(dbenv, rep->finfo, &rep->nextinfo,
	    &rep->curinfo)) != 0)
		goto err;
	DB_ASSERT(rep->curinfo->pgno == 0);
	rep->ready_pg = 0;
	rep->npages = 0;
	rep->waiting_pg = PGNO_INVALID;
	rep->max_wait_pg = PGNO_INVALID;
	memset(&dbt, 0, sizeof(dbt));
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "FILEDONE: Next file %d.  Request pages 0 to %lu",
	    rep->curinfo->filenum, (u_long)rep->curinfo->max_pgno));
	dbt.data = rep->finfo;
	dbt.size = (u_int32_t)((u_int8_t *)rep->nextinfo -
	    (u_int8_t *)rep->finfo);
	(void)__rep_send_message(dbenv, eid, REP_PAGE_REQ,
	    NULL, &dbt, 0, DB_REP_ANYWHERE);
err:
	return (ret);
}

/*
 * __rep_mpf_open -
 *	Create and open the mpool file for a database.
 *	Used by both master and client to bring files into mpool.
 */
static int
__rep_mpf_open(dbenv, mpfp, rfp, flags)
	DB_ENV *dbenv;
	DB_MPOOLFILE **mpfp;
	__rep_fileinfo_args *rfp;
	u_int32_t flags;
{
	DB db;
	int ret;

	if ((ret = __memp_fcreate(dbenv, mpfp)) != 0)
		return (ret);

	/*
	 * We need a dbp to pass into to __db_dbenv_mpool.  Set up
	 * only the parts that it needs.
	 */
	db.dbenv = dbenv;
	db.type = (DBTYPE)rfp->type;
	db.pgsize = rfp->pgsize;
	memcpy(db.fileid, rfp->uid.data, DB_FILE_ID_LEN);
	db.flags = rfp->flags;
	/* We need to make sure the dbp isn't marked open. */
	F_CLR(&db, DB_AM_OPEN_CALLED);
	db.mpf = *mpfp;
	if (F_ISSET(&db, DB_AM_INMEM))
		(void)__memp_set_flags(db.mpf, DB_MPOOL_NOFILE, 1);
	if ((ret = __db_dbenv_mpool(&db, rfp->info.data, flags)) != 0) {
		(void)__memp_fclose(*mpfp, 0);
		*mpfp = NULL;
	}
	return (ret);
}

/*
 * __rep_pggap_req -
 *	Request a page gap.  Assumes the caller holds the rep_mutex.
 *
 * PUBLIC: int __rep_pggap_req __P((DB_ENV *, REP *, __rep_fileinfo_args *,
 * PUBLIC:    u_int32_t));
 */
int
__rep_pggap_req(dbenv, rep, reqfp, gapflags)
	DB_ENV *dbenv;
	REP *rep;
	__rep_fileinfo_args *reqfp;
	u_int32_t gapflags;
{
	DBT max_pg_dbt;
	__rep_fileinfo_args *tmpfp;
	size_t len;
	u_int32_t flags;
	int alloc, ret;

	ret = 0;
	alloc = 0;
	/*
	 * There is a window where we have to set REP_RECOVER_PAGE when
	 * we receive the update information to transition from getting
	 * file information to getting page information.  However, that
	 * thread does release and then reacquire mutexes.  So, we might
	 * try re-requesting before the original thread can get curinfo
	 * setup.  If curinfo isn't set up there is nothing to do.
	 */
	if (rep->curinfo == NULL)
		return (0);
	if (reqfp == NULL) {
		if ((ret = __rep_finfo_alloc(dbenv, rep->curinfo, &tmpfp)) != 0)
			return (ret);
		alloc = 1;
	} else
		tmpfp = reqfp;

	/*
	 * If we've never requested this page, then
	 * request everything between it and the first
	 * page we have.  If we have requested this page
	 * then only request this record, not the entire gap.
	 */
	flags = 0;
	memset(&max_pg_dbt, 0, sizeof(max_pg_dbt));
	tmpfp->pgno = rep->ready_pg;
	max_pg_dbt.data = rep->finfo;
	max_pg_dbt.size = (u_int32_t)((u_int8_t *)rep->nextinfo -
	    (u_int8_t *)rep->finfo);
	if (rep->max_wait_pg == PGNO_INVALID ||
	    FLD_ISSET(gapflags, REP_GAP_FORCE | REP_GAP_REREQUEST)) {
		/*
		 * Request the gap - set max to waiting_pg - 1 or if
		 * there is no waiting_pg, just ask for one.
		 */
		if (rep->waiting_pg == PGNO_INVALID) {
			if (FLD_ISSET(gapflags,
			    REP_GAP_FORCE | REP_GAP_REREQUEST))
				rep->max_wait_pg = rep->curinfo->max_pgno;
			else
				rep->max_wait_pg = rep->ready_pg;
		} else
			rep->max_wait_pg = rep->waiting_pg - 1;
		tmpfp->max_pgno = rep->max_wait_pg;
		/*
		 * Gap requests are "new" and can go anywhere.
		 */
		if (FLD_ISSET(gapflags, REP_GAP_REREQUEST))
			flags = DB_REP_REREQUEST;
		else
			flags = DB_REP_ANYWHERE;
	} else {
		/*
		 * Request 1 page - set max to ready_pg.
		 */
		rep->max_wait_pg = rep->ready_pg;
		tmpfp->max_pgno = rep->ready_pg;
		/*
		 * If we're dropping to singletons, this is a rerequest.
		 */
		flags = DB_REP_REREQUEST;
	}
	if (rep->master_id != DB_EID_INVALID) {
		rep->stat.st_pg_requested++;
		/*
		 * We need to request the pages, but we need to get the
		 * new info into rep->finfo.  Assert that the sizes never
		 * change.  The only thing this should do is change
		 * the pgno field.  Everything else remains the same.
		 */
		ret = __rep_fileinfo_buf(rep->finfo, max_pg_dbt.size, &len,
		    tmpfp->pgsize, tmpfp->pgno, tmpfp->max_pgno,
		    tmpfp->filenum, tmpfp->id, tmpfp->type,
		    tmpfp->flags, &tmpfp->uid, &tmpfp->info);
		DB_ASSERT(len == max_pg_dbt.size);
		(void)__rep_send_message(dbenv, rep->master_id,
		    REP_PAGE_REQ, NULL, &max_pg_dbt, 0, flags);
	} else
		(void)__rep_send_message(dbenv, DB_EID_BROADCAST,
		    REP_MASTER_REQ, NULL, NULL, 0, 0);

	if (alloc)
		__os_free(dbenv, tmpfp);
	return (ret);
}

/*
 * __rep_finfo_alloc -
 *	Allocate and initialize a fileinfo structure.
 *
 * PUBLIC: int __rep_finfo_alloc __P((DB_ENV *, __rep_fileinfo_args *,
 * PUBLIC:    __rep_fileinfo_args **));
 */
int
__rep_finfo_alloc(dbenv, rfpsrc, rfpp)
	DB_ENV *dbenv;
	__rep_fileinfo_args *rfpsrc, **rfpp;
{
	__rep_fileinfo_args *rfp;
	size_t size;
	int ret;
	void *uidp, *infop;

	/*
	 * Allocate enough for the structure and the two DBT data areas.
	 */
	size = sizeof(__rep_fileinfo_args) + rfpsrc->uid.size +
	    rfpsrc->info.size;
	if ((ret = __os_malloc(dbenv, size, &rfp)) != 0)
		return (ret);

	/*
	 * Copy the structure itself, and then set the DBT data pointers
	 * to their space and copy the data itself as well.
	 */
	memcpy(rfp, rfpsrc, sizeof(__rep_fileinfo_args));
	uidp = (u_int8_t *)rfp + sizeof(__rep_fileinfo_args);
	rfp->uid.data = uidp;
	memcpy(uidp, rfpsrc->uid.data, rfpsrc->uid.size);

	infop = (u_int8_t *)uidp + rfpsrc->uid.size;
	rfp->info.data = infop;
	memcpy(infop, rfpsrc->info.data, rfpsrc->info.size);
	*rfpp = rfp;
	return (ret);
}

/*
 * __rep_log_setup -
 *	We know our first LSN and need to reset the log subsystem
 *	to get our logs set up for the proper file.
 */
static int
__rep_log_setup(dbenv, rep)
	DB_ENV *dbenv;
	REP *rep;
{
	DB_LOG *dblp;
	DB_LSN lsn;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	LOG *lp;
	u_int32_t fnum, lastfile;
	int ret;
	char *name;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	/*
	 * Forcibly remove *all* existing log files.
	 */
	lastfile = lp->lsn.file;
	for (fnum = 1; fnum <= lastfile; fnum++) {
		if ((ret = __log_name(dblp, fnum, &name, NULL, 0)) != 0)
			goto err;
		(void)__os_unlink(dbenv, name);
		__os_free(dbenv, name);
	}
	/*
	 * Set up the log starting at the file number of the first LSN we
	 * need to get from the master.
	 */
	ret = __log_newfile(dblp, &lsn, rep->first_lsn.file);

	/*
	 * We reset first_lsn to the lp->lsn.  We were given the LSN of
	 * the checkpoint and we now need the LSN for the beginning of
	 * the file, which __log_newfile conveniently set up for us
	 * in lp->lsn.
	 */
	rep->first_lsn = lp->lsn;
	TXN_SYSTEM_LOCK(dbenv);
	ZERO_LSN(region->last_ckp);
	TXN_SYSTEM_UNLOCK(dbenv);
err:
	return (ret);
}

/*
 * __rep_queue_filedone -
 *	Determine if we're really done getting the pages for a queue file.
 *	Queue is handled in several steps.
 *	1.  First we get the meta page only.
 *	2.  We use the meta-page information to figure out first and last
 *	    page numbers (and if queue wraps, first can be > last.
 *	3.  If first < last, we do a REP_PAGE_REQ for all pages.
 *	4.  If first > last, we REP_PAGE_REQ from first -> max page number.
 *	    Then we'll ask for page 1 -> last.
 *
 * This function can return several things:
 *	DB_REP_PAGEDONE - if we're done with this file.
 *	0 - if we're not doen with this file.
 *	error - if we get an error doing some operations.
 *
 * This function will open a dbp handle to the queue file.  This is needed
 * by most of the QAM macros.  We'll open it on the first pass through
 * here and we'll close it whenever we decide we're done.
 */
static int
__rep_queue_filedone(dbenv, rep, rfp)
	DB_ENV *dbenv;
	REP *rep;
	__rep_fileinfo_args *rfp;
{
#ifndef HAVE_QUEUE
	COMPQUIET(rep, NULL);
	COMPQUIET(rfp, NULL);
	return (__db_no_queue_am(dbenv));
#else
	db_pgno_t first, last;
	u_int32_t flags;
	int empty, ret, t_ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	ret = 0;
	if (rep->queue_dbp == NULL) {
		/*
		 * We need to do a sync here so that the open
		 * can find the file and file id.
		 */
		if ((ret = __memp_sync(dbenv, NULL)) != 0)
			goto out;
		if ((ret = db_create(&rep->queue_dbp, dbenv, 0)) != 0)
			goto out;
		flags = DB_NO_AUTO_COMMIT |
		    (F_ISSET(dbenv, DB_ENV_THREAD) ? DB_THREAD : 0);
		/*
		 * We need to check whether this is in-memory so that we pass
		 * the name correctly as either the file or the database name.
		 */
		if ((ret = __db_open(rep->queue_dbp, NULL,
		    FLD_ISSET(rfp->flags, DB_AM_INMEM) ? NULL : rfp->info.data,
		    FLD_ISSET(rfp->flags, DB_AM_INMEM) ? rfp->info.data : NULL,
		    DB_QUEUE, flags, 0, PGNO_BASE_MD)) != 0)
			goto out;
	}
	if ((ret = __queue_pageinfo(rep->queue_dbp,
	    &first, &last, &empty, 0, 0)) != 0)
		goto out;
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "Queue fileinfo: first %lu, last %lu, empty %d",
	    (u_long)first, (u_long)last, empty));
	/*
	 * We can be at the end of 3 possible states.
	 * 1.  We have received the meta-page and now need to get the
	 *     rest of the pages in the database.
	 * 2.  We have received from first -> max_pgno.  We might be done,
	 *     or we might need to ask for wrapped pages.
	 * 3.  We have received all pages in the file.  We're done.
	 */
	if (rfp->max_pgno == 0) {
		/*
		 * We have just received the meta page.  Set up the next
		 * pages to ask for and check if the file is empty.
		 */
		if (empty)
			goto out;
		if (first > last) {
			rfp->max_pgno =
			    QAM_RECNO_PAGE(rep->queue_dbp, UINT32_MAX);
		} else
			rfp->max_pgno = last;
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Queue fileinfo: First req: first %lu, last %lu",
		    (u_long)first, (u_long)rfp->max_pgno));
		goto req;
	} else if (rfp->max_pgno != last) {
		/*
		 * If max_pgno != last that means we're dealing with a
		 * wrapped situation.  Request next batch of pages.
		 * Set npages to 1 because we already have page 0, the
		 * meta-page, now we need pages 1-max_pgno.
		 */
		first = 1;
		rfp->max_pgno = last;
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Queue fileinfo: Wrap req: first %lu, last %lu",
		    (u_long)first, (u_long)last));
req:
		/*
		 * Since we're simulating a "gap" to resend new PAGE_REQ
		 * for this file, we need to set waiting page to last + 1
		 * so that we'll ask for all from ready_pg -> last.
		 */
		rep->npages = first;
		rep->ready_pg = first;
		rep->waiting_pg = rfp->max_pgno + 1;
		rep->max_wait_pg = PGNO_INVALID;
		ret = __rep_pggap_req(dbenv, rep, rfp, 0);
		return (ret);
	}
	/*
	 * max_pgno == last
	 * If we get here, we have all the pages we need.
	 * Close the dbp and return.
	 */
out:
	if (rep->queue_dbp != NULL &&
	    (t_ret = __db_close(rep->queue_dbp, NULL, DB_NOSYNC)) != 0 &&
	    ret == 0)
		ret = t_ret;
	rep->queue_dbp = NULL;
	if (ret == 0)
		ret = DB_REP_PAGEDONE;
	return (ret);
#endif
}
