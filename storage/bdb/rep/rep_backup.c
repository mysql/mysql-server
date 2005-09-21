/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_backup.c,v 1.33 2004/10/29 18:08:09 bostic Exp $
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
static int __rep_files_data __P((DB_ENV *, u_int8_t *, size_t *,
    size_t *, int *));
static int __rep_files_inmem __P((DB_ENV *, u_int8_t *, size_t *,
    size_t *, int *));
static int __rep_get_fileinfo __P((DB_ENV *, const char *,
    __rep_fileinfo_args *, u_int8_t *, int *));
static int __rep_log_setup __P((DB_ENV *, REP *));
static int __rep_mpf_open __P((DB_ENV *, DB_MPOOLFILE **,
    __rep_fileinfo_args *));
static int __rep_page_gap __P((DB_ENV *, REP *, __rep_fileinfo_args *,
    u_int32_t));
static int __rep_page_sendpages __P((DB_ENV *, int,
    __rep_fileinfo_args *, DB_MPOOLFILE *, DB *));
static int __rep_queue_filedone __P((DB_ENV *, REP *, __rep_fileinfo_args *));
static int __rep_walk_dir __P((DB_ENV *, const char *, u_int8_t *,
    size_t *, size_t *, int *));
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
	DB_LOGC *logc;
	DB_LSN lsn;
	DBT data_dbt;
	size_t filelen, filesz, updlen;
	u_int8_t *buf, *fp;
	int filecnt, ret, t_ret;

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
	if ((ret = __rep_files_inmem(dbenv, fp, &filesz, &filelen,
	    &filecnt)) != 0)
		goto err;
	if ((ret = __rep_files_data(dbenv, fp, &filesz, &filelen,
	    &filecnt)) != 0)
		goto err;

	/*
	 * Now get our first LSN.
	 */
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		goto err;
	memset(&data_dbt, 0, sizeof(data_dbt));
	ret = __log_c_get(logc, &lsn, &data_dbt, DB_FIRST);
	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
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
	R_LOCK(dbenv, &dblp->reginfo);
	lsn = ((LOG *)dblp->reginfo.primary)->lsn;
	R_UNLOCK(dbenv, &dblp->reginfo);
	(void)__rep_send_message(dbenv, eid, REP_UPDATE, &lsn, &updbt, 0);

err:
	__os_free(dbenv, buf);
	return (ret);
}

/*
 * __rep_files_data -
 *	Walk through all the files in the env's data_dirs.  We need to
 *	open them, gather the necessary information and then close them.
 *	Then we need to figure out if they're already in the dbentry array.
 */
static int
__rep_files_data(dbenv, fp, fileszp, filelenp, filecntp)
	DB_ENV *dbenv;
	u_int8_t *fp;
	size_t *fileszp, *filelenp;
	int *filecntp;
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
	return (ret);
}

static int
__rep_walk_dir(dbenv, dir, fp, fileszp, filelenp, filecntp)
	DB_ENV *dbenv;
	const char *dir;
	u_int8_t *fp;
	size_t *fileszp, *filelenp;
	int *filecntp;
{
	DBT namedbt, uiddbt;
	__rep_fileinfo_args tmpfp;
	size_t len, offset;
	int cnt, i, ret;
	u_int8_t *rfp, uid[DB_FILE_ID_LEN];
	char **names;
#ifdef DIAGNOSTIC
	REP *rep;
	DB_MSGBUF mb;
	DB_REP *db_rep;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
#endif
	memset(&namedbt, 0, sizeof(namedbt));
	memset(&uiddbt, 0, sizeof(uiddbt));
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "Walk_dir: Getting info for dir: %s", dir));
	if ((ret = __os_dirlist(dbenv, dir, &names, &cnt)) != 0)
		return (ret);
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
		if ((ret = __rep_get_fileinfo(dbenv, names[i], &tmpfp, uid,
		    filecntp)) != 0) {
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
__rep_get_fileinfo(dbenv, file, rfp, uid, filecntp)
	DB_ENV *dbenv;
	const char *file;
	__rep_fileinfo_args *rfp;
	u_int8_t *uid;
	int *filecntp;
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

	dblp = dbenv->lg_handle;
	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		goto err;
	if ((ret = __db_open(dbp, NULL, file, NULL, DB_UNKNOWN,
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
	rfp->type = dbp->type;
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
		MUTEX_THREAD_LOCK(dbenv, dblp->mutexp);
		/*
		 * Walk entry table looking for this uid.
		 * If we find it, save the id.
		 */
		for (i = 0; i < dblp->dbentry_cnt; i++) {
			entdbp = dblp->dbentry[i].dbp;
			if (entdbp == NULL)
				break;
			DB_ASSERT(entdbp->log_filename != NULL);
			if (memcmp(uid,
			    entdbp->log_filename->ufid,
			    DB_FILE_ID_LEN) == 0)
				rfp->id = i;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);
	}
	return (ret);
}

/*
 * __rep_files_inmem -
 *	Gather all the information about in-memory files.
 */
static int
__rep_files_inmem(dbenv, fp, fileszp, filelenp, filecntp)
	DB_ENV *dbenv;
	u_int8_t *fp;
	size_t *fileszp, *filelenp;
	int *filecntp;
{

	int ret;

	COMPQUIET(dbenv, NULL);
	COMPQUIET(fp, NULL);
	COMPQUIET(fileszp, NULL);
	COMPQUIET(filelenp, NULL);
	COMPQUIET(filecntp, NULL);
	ret = 0;
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
	DB *dbp;
	DBT msgdbt;
	DB_LOG *dblp;
	DB_MPOOLFILE *mpf;
	__rep_fileinfo_args *msgfp;
	int ret, t_ret;
	void *next;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
	DB_REP *db_rep;
	REP *rep;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
#endif
	dblp = dbenv->lg_handle;
	if ((ret = __rep_fileinfo_read(dbenv, rec->data, &next, &msgfp)) != 0)
		return (ret);
	/*
	 * See if we can find it already.  If so we can quickly
	 * access its mpool and process.  Otherwise we have to
	 * open the file ourselves.
	 */
	RPRINT(dbenv, rep, (dbenv, &mb, "page_req: file %d page %lu to %lu",
	    msgfp->filenum, (u_long)msgfp->pgno, (u_long)msgfp->max_pgno));
	MUTEX_THREAD_LOCK(dbenv, dblp->mutexp);
	if (msgfp->id >= 0 && dblp->dbentry_cnt > msgfp->id) {
		dbp = dblp->dbentry[msgfp->id].dbp;
		if (dbp != NULL) {
			DB_ASSERT(dbp->log_filename != NULL);
			if (memcmp(msgfp->uid.data, dbp->log_filename->ufid,
			    DB_FILE_ID_LEN) == 0) {
				MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);
				RPRINT(dbenv, rep, (dbenv, &mb,
				    "page_req: found %d in dbreg",
				    msgfp->filenum));
				ret = __rep_page_sendpages(dbenv, eid,
				    msgfp, dbp->mpf, dbp);
				goto err;
			}
		}
	}
	MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);

	/*
	 * If we get here, we do not have the file open via dbreg.
	 * We need to open the file and then send its pages.
	 * If we cannot open the file, we send REP_FILE_FAIL.
	 */
	RPRINT(dbenv, rep, (dbenv, &mb, "page_req: Open %d via mpf_open",
	    msgfp->filenum));
	if ((ret = __rep_mpf_open(dbenv, &mpf, msgfp)) != 0) {
		memset(&msgdbt, 0, sizeof(msgdbt));
		msgdbt.data = msgfp;
		msgdbt.size = sizeof(*msgfp);
		RPRINT(dbenv, rep, (dbenv, &mb, "page_req: Open %d failed",
		    msgfp->filenum));
		(void)__rep_send_message(dbenv, eid, REP_FILE_FAIL,
		    NULL, &msgdbt, 0);
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
	DBT msgdbt, pgdbt;
	DB_LOG *dblp;
	DB_LSN lsn;
	DB_MSGBUF mb;
	DB_REP *db_rep;
	PAGE *pagep;
	REP *rep;
	db_pgno_t p;
	size_t len, msgsz;
	u_int32_t bytes, gbytes, type;
	int check_limit, opened, ret, t_ret;
	u_int8_t *buf;

#ifndef DIAGNOSTIC
	DB_MSGBUF_INIT(&mb);
#endif
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	opened = 0;
	gbytes = bytes = 0;
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	gbytes = rep->gbytes;
	bytes = rep->bytes;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	check_limit = gbytes != 0 || bytes != 0;
	qdbp = NULL;
	buf = NULL;
	if (msgfp->type == DB_QUEUE) {
		if (dbp == NULL) {
			if ((ret = db_create(&qdbp, dbenv, 0)) != 0)
				goto err;
			if ((ret = __db_open(qdbp, NULL, msgfp->info.data,
			    NULL, DB_UNKNOWN,
			    DB_RDONLY | (F_ISSET(dbenv, DB_ENV_THREAD) ?
			    DB_THREAD : 0), 0, PGNO_BASE_MD)) != 0)
				goto err;
			opened = 1;
		} else
			qdbp = dbp;
	}
	msgsz = sizeof(__rep_fileinfo_args) + DB_FILE_ID_LEN + msgfp->pgsize;
	if ((ret = __os_calloc(dbenv, 1, msgsz, &buf)) != 0)
		return (ret);
	memset(&msgdbt, 0, sizeof(msgdbt));
	memset(&pgdbt, 0, sizeof(pgdbt));
	RPRINT(dbenv, rep, (dbenv, &mb, "sendpages: file %d page %lu to %lu",
	    msgfp->filenum, (u_long)msgfp->pgno, (u_long)msgfp->max_pgno));
	for (p = msgfp->pgno; p <= msgfp->max_pgno; p++) {
		if (msgfp->type == DB_QUEUE && p != 0)
#ifdef HAVE_QUEUE
			ret = __qam_fget(qdbp, &p, DB_MPOOL_CREATE, &pagep);
#else
			ret = DB_PAGE_NOTFOUND;
#endif
		else
			ret = __memp_fget(mpf, &p, DB_MPOOL_CREATE, &pagep);
		type = REP_PAGE;
		if (ret == DB_PAGE_NOTFOUND) {
			memset(&pgdbt, 0, sizeof(pgdbt));
			ret = 0;
			ZERO_LSN(lsn);
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "sendpages: PAGE_FAIL on page %lu", (u_long)p));
			type = REP_PAGE_FAIL;
			msgfp->pgno = p;
			goto send;
		} else if (ret != 0)
			goto err;
		else {
			pgdbt.data = pagep;
			pgdbt.size = (u_int32_t)msgfp->pgsize;
		}
		len = 0;
		ret = __rep_fileinfo_buf(buf, msgsz, &len,
		    msgfp->pgsize, p, msgfp->max_pgno,
		    msgfp->filenum, msgfp->id, msgfp->type,
		    msgfp->flags, &msgfp->uid, &pgdbt);
		if (ret != 0)
			goto err;

		DB_ASSERT(len <= msgsz);
		msgdbt.data = buf;
		msgdbt.size = (u_int32_t)len;

		dblp = dbenv->lg_handle;
		R_LOCK(dbenv, &dblp->reginfo);
		lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		if (check_limit) {
			/*
			 * msgdbt.size is only the size of the page and
			 * other information we're sending.  It doesn't
			 * count the size of the control structure.  Factor
			 * that in as well so we're not off by a lot if
			 * pages are small.
			 */
			while (bytes < msgdbt.size + sizeof(REP_CONTROL)) {
				if (gbytes > 0) {
					bytes += GIGABYTE;
					--gbytes;
					continue;
				}
				/*
				 * We don't hold the rep mutex, and may
				 * miscount.
				 */
				rep->stat.st_nthrottles++;
				type = REP_PAGE_MORE;
				goto send;
			}
			bytes -= (msgdbt.size + sizeof(REP_CONTROL));
		}
send:
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "sendpages: %s %lu, lsn [%lu][%lu]",
		    (type == REP_PAGE ? "PAGE" :
		    (type == REP_PAGE_MORE ? "PAGE_MORE" : "PAGE_FAIL")),
		    (u_long)p, (u_long)lsn.file, (u_long)lsn.offset));
		(void)__rep_send_message(dbenv, eid, type, &lsn, &msgdbt, 0);
		/*
		 * If we have REP_PAGE_FAIL we need to break before trying
		 * to give the page back to mpool.  If we have REP_PAGE_MORE
		 * we need to break this loop after giving the page back
		 * to mpool.  Otherwise, with REP_PAGE, we keep going.
		 */
		if (type == REP_PAGE_FAIL)
			break;
		if (msgfp->type != DB_QUEUE || p == 0)
			ret = __memp_fput(mpf, pagep, 0);
#ifdef HAVE_QUEUE
		else
			/*
			 * We don't need an #else for HAVE_QUEUE here because if
			 * we're not compiled with queue, then we're guaranteed
			 * to have set REP_PAGE_FAIL above.
			 */
			ret = __qam_fput(qdbp, p, pagep, 0);
#endif
		if (type == REP_PAGE_MORE)
			break;
	}
err:
	if (opened && (t_ret = __db_close(qdbp, NULL, DB_NOSYNC)) != 0 &&
	    ret == 0)
		ret = t_ret;
	if (buf != NULL)
		__os_free(dbenv, buf);
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
	DB_LSN lsn;
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

	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (!F_ISSET(rep, REP_F_RECOVER_UPDATE)) {
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		return (0);
	}
	F_CLR(rep, REP_F_RECOVER_UPDATE);
	/*
	 * We know we're the first to come in here due to the
	 * REP_F_RECOVER_UPDATE flag.  REP_F_READY should not be set.
	 */
	DB_ASSERT(!F_ISSET(rep, REP_F_READY));
	F_SET(rep, REP_F_RECOVER_PAGE);
	/*
	 * We do not clear REP_F_READY or rep->in_recovery in this code.
	 * We'll eventually call the normal __rep_verify_match recovery
	 * code and that will clear all the flags and allow others to
	 * proceed.
	 */
	__rep_lockout(dbenv, db_rep, rep, 1);
	/*
	 * We need to update the timestamp and kill any open handles
	 * on this client.  The files are changing completely.
	 */
	infop = dbenv->reginfo;
	renv = infop->primary;
	(void)time(&renv->rep_timestamp);

	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	lp->wait_recs = rep->request_gap;
	lp->rcvd_recs = 0;
	ZERO_LSN(lp->ready_lsn);
	ZERO_LSN(lp->waiting_lsn);
	ZERO_LSN(lp->max_wait_lsn);
	ZERO_LSN(lp->max_perm_lsn);
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
	if ((ret = __rep_update_read(dbenv, rec->data, &next, &rup)) != 0)
		goto err;
	R_LOCK(dbenv, &dblp->reginfo);
	lsn = lp->lsn;
	R_UNLOCK(dbenv, &dblp->reginfo);

	/*
	 * We need to empty out any old log records that might be in the
	 * temp database.
	 */
	if ((ret = __db_truncate(db_rep->rep_db, NULL, &count)) != 0)
		goto err;

	/*
	 * If our log is before the master's beginning of log,
	 * we need to request from the master's beginning.
	 * If we have some log, we need the earlier of the
	 * master's last checkpoint LSN or our current LSN.
	 */
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (log_compare(&lsn, &rup->first_lsn) < 0)
		rep->first_lsn = rup->first_lsn;
	else
		rep->first_lsn = lsn;
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
	    NULL, &pagereq_dbt, 0);
	if (0) {
errmem:		__os_free(dbenv, rep->curinfo);
errmem1:	__os_free(dbenv, rep->originfo);
		rep->finfo = NULL;
		rep->curinfo = NULL;
		rep->originfo = NULL;
	}
err:
	/*
	 * If we get an error, we cannot leave ourselves in the
	 * RECOVER_PAGE state because we have no file information.
	 * That also means undo'ing the rep_lockout.
	 * We need to move back to the RECOVER_UPDATE stage.
	 */
	if (ret != 0) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Update_setup: Error: Clear PAGE, set UPDATE again. %s",
		    db_strerror(ret)));
		F_CLR(rep, REP_F_RECOVER_PAGE | REP_F_READY);
		rep->in_recovery = 0;
		F_SET(rep, REP_F_RECOVER_UPDATE);
	}
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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

	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (!F_ISSET(rep, REP_F_RECOVER_PAGE)) {
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		return (0);
	}
	if ((ret = __rep_fileinfo_read(dbenv, rec->data, &next, &msgfp)) != 0) {
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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

	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
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

err:
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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

	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (!F_ISSET(rep, REP_F_RECOVER_PAGE)) {
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		return (0);
	}
	if ((ret = __rep_fileinfo_read(dbenv, rec->data, &next, &msgfp)) != 0) {
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		return (0);
	}
	rfp = rep->curinfo;
	if (rfp->type != DB_QUEUE)
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
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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

	/*
	 * If this is the first page we're putting in this database, we need
	 * to create the mpool file.  Otherwise call memp_fget to create the
	 * page in mpool.  Then copy the data to the page, and memp_fput the
	 * page to give it back to mpool.
	 *
	 * We need to create the file, removing any existing file and associate
	 * the correct file ID with the new one.
	 */
	if (rep->file_mpf == NULL) {
		rfp = rep->curinfo;

		if (!F_ISSET(rfp, DB_AM_INMEM)) {
			if ((ret = __db_appname(dbenv, DB_APP_DATA,
			    rfp->info.data, 0, NULL, &real_name)) != 0)
				goto err;
			/*
			 * Calling memp_nameop will both purge any matching
			 * fileid from mpool and unlink it on disk.
			 */
			if ((ret = __memp_nameop(dbenv,
			    rfp->uid.data, NULL, real_name, NULL)) != 0)
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
		    __rep_mpf_open(dbenv, &rep->file_mpf, rep->curinfo)) != 0)
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
	if (msgfp->type == DB_QUEUE && msgfp->pgno != 0) {
#ifdef HAVE_QUEUE
		if ((ret = __qam_fget(
		    rep->queue_dbp, &msgfp->pgno, DB_MPOOL_CREATE, &dst)) != 0)
#else
		if ((ret = __db_no_queue_am(dbenv)) != 0)
#endif
			goto err;
	} else if ((ret = __memp_fget(
		    rep->file_mpf, &msgfp->pgno, DB_MPOOL_CREATE, &dst)) != 0)
			goto err;

	memcpy(dst, msgfp->info.data, msgfp->pgsize);
	if (msgfp->type != DB_QUEUE || msgfp->pgno == 0)
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
	DB_REP *db_rep;
	DBT data, key;
	LOG *lp;
	__rep_fileinfo_args *rfp;
	db_recno_t recno;
	int ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
	ret = 0;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

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
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
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
		    type == REP_PAGE_MORE)) != 0))
			goto err;
	} else {
		lp->wait_recs = 0;
		rep->max_wait_pg = PGNO_INVALID;
	}

err:
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
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
	DB_REP *db_rep;
	__rep_fileinfo_args *rfp;
	int ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
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
	if (rfp->type == DB_QUEUE &&
	    ((ret = __rep_queue_filedone(dbenv, rep, rfp)) !=
	    DB_REP_PAGEDONE))
		return (ret);
	/*
	 * We have all the pages for this file.  We need to:
	 * 1.  Close up the file data pointer we used.
	 * 2.  Close/reset the page database.
	 * 3.  Check if we have all file data.  If so, request logs.
	 * 4.  If not, set curfile to next file and request its pages.
	 */
	/*
	 * 1.  Close up the file data pointer we used.
	 */
	if (rep->file_mpf != NULL) {
		ret = __memp_fclose(rep->file_mpf, 0);
		rep->file_mpf = NULL;
		if (ret != 0)
			goto err;
	}

	/*
	 * 2.  Close/reset the page database.
	 */
	ret = __db_close(rep->file_dbp, NULL, DB_NOSYNC);
	rep->file_dbp = NULL;
	if (ret != 0)
		goto err;

	/*
	 * 3.  Check if we have all file data.  If so, request logs.
	 */
	__os_free(dbenv, rep->curinfo);
	if (++rep->curfile == rep->nfiles) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "FILEDONE: have %d files.  RECOVER_LOG now", rep->nfiles));
		/*
		 * Move to REP_RECOVER_LOG state.
		 * Request logs.
		 */
		__os_free(dbenv, rep->originfo);
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
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "FILEDONE: LOG_REQ from LSN [%lu][%lu] to [%lu][%lu]",
		    (u_long)rep->first_lsn.file, (u_long)rep->first_lsn.offset,
		    (u_long)rep->last_lsn.file, (u_long)rep->last_lsn.offset));
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		if ((ret = __rep_log_setup(dbenv, rep)) != 0)
			goto err;
		(void)__rep_send_message(dbenv, eid,
		    REP_LOG_REQ, &rep->first_lsn, &dbt, 0);
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
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
	    NULL, &dbt, 0);
err:
	return (ret);
}

/*
 * __rep_mpf_open -
 *	Create and open the mpool file for a database.
 *	Used by both master and client to bring files into mpool.
 */
static int
__rep_mpf_open(dbenv, mpfp, rfp)
	DB_ENV *dbenv;
	DB_MPOOLFILE **mpfp;
	__rep_fileinfo_args *rfp;
{
	DB db;
	int ret;

	if ((ret = __memp_fcreate(dbenv, mpfp)) != 0)
		return (ret);

	/*
	 * We need a dbp to pass into to __db_dbenv_mpool.  Set up
	 * only the parts that it needs.
	 */
	db.type = rfp->type;
	db.pgsize = (u_int32_t)rfp->pgsize;
	memcpy(db.fileid, rfp->uid.data, DB_FILE_ID_LEN);
	db.flags = rfp->flags;
	db.mpf = *mpfp;
	db.dbenv = dbenv;
	if ((ret = __db_dbenv_mpool(&db, rfp->info.data, 0)) != 0) {
		(void)__memp_fclose(*mpfp, 0);
		*mpfp = NULL;
	}
	return (ret);
}

/*
 * __rep_pggap_req -
 *	Request a page gap.  Assumes the caller holds the rep_mutexp.
 *
 * PUBLIC: int __rep_pggap_req __P((DB_ENV *, REP *, __rep_fileinfo_args *,
 * PUBLIC:    int));
 */
int
__rep_pggap_req(dbenv, rep, reqfp, moregap)
	DB_ENV *dbenv;
	REP *rep;
	__rep_fileinfo_args *reqfp;
	int moregap;
{
	DBT max_pg_dbt;
	__rep_fileinfo_args *tmpfp;
	size_t len;
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
	memset(&max_pg_dbt, 0, sizeof(max_pg_dbt));
	tmpfp->pgno = rep->ready_pg;
	max_pg_dbt.data = rep->finfo;
	max_pg_dbt.size = (u_int32_t)((u_int8_t *)rep->nextinfo -
	    (u_int8_t *)rep->finfo);
	if (rep->max_wait_pg == PGNO_INVALID || moregap) {
		/*
		 * Request the gap - set max to waiting_pg - 1 or if
		 * there is no waiting_pg, just ask for one.
		 */
		if (rep->waiting_pg == PGNO_INVALID) {
			if (moregap)
				rep->max_wait_pg = rep->curinfo->max_pgno;
			else
				rep->max_wait_pg = rep->ready_pg;
		} else
			rep->max_wait_pg = rep->waiting_pg - 1;
		tmpfp->max_pgno = rep->max_wait_pg;
	} else {
		/*
		 * Request 1 page - set max to ready_pg.
		 */
		rep->max_wait_pg = rep->ready_pg;
		tmpfp->max_pgno = rep->ready_pg;
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
		    REP_PAGE_REQ, NULL, &max_pg_dbt, 0);
	} else
		(void)__rep_send_message(dbenv, DB_EID_BROADCAST,
		    REP_MASTER_REQ, NULL, NULL, 0);

	if (alloc)
		__os_free(dbenv, tmpfp);
	return (ret);
}

/*
 * __rep_loggap_req -
 *	Request a log gap.  Assumes the caller holds the db_mutexp.
 *
 * PUBLIC: void __rep_loggap_req __P((DB_ENV *, REP *, DB_LSN *, int));
 */
void
__rep_loggap_req(dbenv, rep, lsnp, moregap)
	DB_ENV *dbenv;
	REP *rep;
	DB_LSN *lsnp;
	int moregap;
{
	DB_LOG *dblp;
	DBT max_lsn_dbt, *max_lsn_dbtp;
	DB_LSN next_lsn;
	LOG *lp;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	R_LOCK(dbenv, &dblp->reginfo);
	next_lsn = lp->lsn;
	R_UNLOCK(dbenv, &dblp->reginfo);

	if (moregap ||
	    (lsnp != NULL &&
	    (log_compare(lsnp, &lp->max_wait_lsn) == 0 ||
	    IS_ZERO_LSN(lp->max_wait_lsn)))) {
		/*
		 * We need to ask for the gap.  Either we never asked
		 * for records before, or we asked for a single record
		 * and received it.
		 */
		lp->max_wait_lsn = lp->waiting_lsn;
		memset(&max_lsn_dbt, 0, sizeof(max_lsn_dbt));
		max_lsn_dbt.data = &lp->waiting_lsn;
		max_lsn_dbt.size = sizeof(lp->waiting_lsn);
		max_lsn_dbtp = &max_lsn_dbt;
	} else {
		max_lsn_dbtp = NULL;
		lp->max_wait_lsn = next_lsn;
	}
	if (rep->master_id != DB_EID_INVALID) {
		rep->stat.st_log_requested++;
		(void)__rep_send_message(dbenv, rep->master_id,
		    REP_LOG_REQ, &next_lsn, max_lsn_dbtp, 0);
	} else
		(void)__rep_send_message(dbenv, DB_EID_BROADCAST,
		    REP_MASTER_REQ, NULL, NULL, 0);

	return;
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
	size_t size;
	int ret;

	size = sizeof(__rep_fileinfo_args) + rfpsrc->uid.size +
	    rfpsrc->info.size;
	if ((ret = __os_malloc(dbenv, size, rfpp)) != 0)
		return (ret);

	memcpy(*rfpp, rfpsrc, size);
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
	u_int32_t fnum;
	int ret;
	char *name;

	dblp = dbenv->lg_handle;
	/*
	 * Set up the log starting at the file number of the first LSN we
	 * need to get from the master.
	 */
	if ((ret = __log_newfile(dblp, &lsn, rep->first_lsn.file)) == 0) {
		/*
		 * We do know we want to start this client's log at
		 * log file 'first_lsn.file'.  So we want to forcibly
		 * remove any log files earlier than that number.
		 * We don't know what might be in any earlier log files
		 * so we cannot just use __log_autoremove.
		 */
		for (fnum = 1; fnum < rep->first_lsn.file; fnum++) {
			if ((ret = __log_name(dblp, fnum, &name, NULL, 0)) != 0)
				goto err;
			(void)__os_unlink(dbenv, name);
			__os_free(dbenv, name);
		}
	}
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
		if ((ret = db_create(&rep->queue_dbp, dbenv,
		    DB_REP_CREATE)) != 0)
			goto out;
		flags = DB_NO_AUTO_COMMIT |
		    (F_ISSET(dbenv, DB_ENV_THREAD) ? DB_THREAD : 0);
		if ((ret = __db_open(rep->queue_dbp, NULL, rfp->info.data,
		    NULL, DB_QUEUE, flags, 0, PGNO_BASE_MD)) != 0)
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
