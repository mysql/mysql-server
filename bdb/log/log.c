/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log.c,v 11.42 2001/01/15 16:42:37 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "log.h"
#include "db_dispatch.h"
#include "txn.h"
#include "txn_auto.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int __log_init __P((DB_ENV *, DB_LOG *));
static int __log_recover __P((DB_LOG *));

/*
 * __log_open --
 *	Internal version of log_open: only called from DB_ENV->open.
 *
 * PUBLIC: int __log_open __P((DB_ENV *));
 */
int
__log_open(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	LOG *lp;
	int ret;
	u_int8_t *readbufp;

	readbufp = NULL;

	/* Create/initialize the DB_LOG structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_LOG), &dblp)) != 0)
		return (ret);
	if ((ret = __os_calloc(dbenv, 1, dbenv->lg_bsize, &readbufp)) != 0)
		goto err;
	ZERO_LSN(dblp->c_lsn);
	dblp->dbenv = dbenv;

	/* Join/create the log region. */
	dblp->reginfo.type = REGION_TYPE_LOG;
	dblp->reginfo.id = INVALID_REGION_ID;
	dblp->reginfo.mode = dbenv->db_mode;
	dblp->reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&dblp->reginfo, REGION_CREATE_OK);
	if ((ret = __db_r_attach(
	    dbenv, &dblp->reginfo, LG_BASE_REGION_SIZE + dbenv->lg_bsize)) != 0)
		goto err;

	dblp->readbufp = readbufp;

	/* If we created the region, initialize it. */
	if (F_ISSET(&dblp->reginfo, REGION_CREATE) &&
	    (ret = __log_init(dbenv, dblp)) != 0)
		goto err;

	/* Set the local addresses. */
	lp = dblp->reginfo.primary =
	    R_ADDR(&dblp->reginfo, dblp->reginfo.rp->primary);
	dblp->bufp = R_ADDR(&dblp->reginfo, lp->buffer_off);

	/*
	 * If the region is threaded, then we have to lock both the handles
	 * and the region, and we need to allocate a mutex for that purpose.
	 */
	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		if ((ret = __db_mutex_alloc(
		    dbenv, &dblp->reginfo, &dblp->mutexp)) != 0)
			goto err;
		if ((ret = __db_mutex_init(
		    dbenv, dblp->mutexp, 0, MUTEX_THREAD)) != 0)
			goto err;
	}

	R_UNLOCK(dbenv, &dblp->reginfo);

	dblp->r_file = 0;
	dblp->r_off = 0;
	dblp->r_size = 0;
	dbenv->lg_handle = dblp;
	return (0);

err:	if (dblp->reginfo.addr != NULL) {
		if (F_ISSET(&dblp->reginfo, REGION_CREATE))
			ret = __db_panic(dbenv, ret);
		R_UNLOCK(dbenv, &dblp->reginfo);
		(void)__db_r_detach(dbenv, &dblp->reginfo, 0);
	}

	if (readbufp != NULL)
		__os_free(readbufp, dbenv->lg_bsize);
	if (dblp->mutexp != NULL)
		__db_mutex_free(dbenv, &dblp->reginfo, dblp->mutexp);
	__os_free(dblp, sizeof(*dblp));
	return (ret);
}

/*
 * __log_init --
 *	Initialize a log region in shared memory.
 */
static int
__log_init(dbenv, dblp)
	DB_ENV *dbenv;
	DB_LOG *dblp;
{
	LOG *region;
	int ret;
	void *p;

	if ((ret = __db_shalloc(dblp->reginfo.addr,
	    sizeof(*region), 0, &dblp->reginfo.primary)) != 0)
		goto mem_err;
	dblp->reginfo.rp->primary =
	    R_OFFSET(&dblp->reginfo, dblp->reginfo.primary);
	region = dblp->reginfo.primary;
	memset(region, 0, sizeof(*region));

	region->persist.lg_max = dbenv->lg_max;
	region->persist.magic = DB_LOGMAGIC;
	region->persist.version = DB_LOGVERSION;
	region->persist.mode = dbenv->db_mode;
	SH_TAILQ_INIT(&region->fq);

	/* Initialize LOG LSNs. */
	region->lsn.file = 1;
	region->lsn.offset = 0;

	/* Initialize the buffer. */
	if ((ret =
	    __db_shalloc(dblp->reginfo.addr, dbenv->lg_bsize, 0, &p)) != 0) {
mem_err:	__db_err(dbenv, "Unable to allocate memory for the log buffer");
		return (ret);
	}
	region->buffer_size = dbenv->lg_bsize;
	region->buffer_off = R_OFFSET(&dblp->reginfo, p);

	/* Try and recover any previous log files before releasing the lock. */
	return (__log_recover(dblp));
}

/*
 * __log_recover --
 *	Recover a log.
 */
static int
__log_recover(dblp)
	DB_LOG *dblp;
{
	DBT dbt;
	DB_LSN lsn;
	LOG *lp;
	int cnt, found_checkpoint, ret;
	u_int32_t chk;
	logfile_validity status;

	lp = dblp->reginfo.primary;

	/*
	 * Find a log file.  If none exist, we simply return, leaving
	 * everything initialized to a new log.
	 */
	if ((ret = __log_find(dblp, 0, &cnt, &status)) != 0)
		return (ret);
	if (cnt == 0)
		return (0);

	/*
	 * If the last file is an old version, readable or no, start a new
	 * file.  Don't bother finding checkpoints;  if we didn't take a
	 * checkpoint right before upgrading, the user screwed up anyway.
	 */
	if (status == DB_LV_OLD_READABLE || status == DB_LV_OLD_UNREADABLE) {
		lp->lsn.file = lp->s_lsn.file = cnt + 1;
		lp->lsn.offset = lp->s_lsn.offset = 0;
		goto skipsearch;
	}
	DB_ASSERT(status == DB_LV_NORMAL);

	/*
	 * We have the last useful log file and we've loaded any persistent
	 * information.  Set the end point of the log past the end of the last
	 * file. Read the last file, looking for the last checkpoint and
	 * the log's end.
	 */
	lp->lsn.file = cnt + 1;
	lp->lsn.offset = 0;
	lsn.file = cnt;
	lsn.offset = 0;

	/* Set the cursor.  Shouldn't fail;  leave error messages on. */
	memset(&dbt, 0, sizeof(dbt));
	if ((ret = __log_get(dblp, &lsn, &dbt, DB_SET, 0)) != 0)
		return (ret);

	/*
	 * Read to the end of the file, saving checkpoints.  This will fail
	 * at some point, so turn off error messages.
	 */
	found_checkpoint = 0;
	while (__log_get(dblp, &lsn, &dbt, DB_NEXT, 1) == 0) {
		if (dbt.size < sizeof(u_int32_t))
			continue;
		memcpy(&chk, dbt.data, sizeof(u_int32_t));
		if (chk == DB_txn_ckp) {
			lp->chkpt_lsn = lsn;
			found_checkpoint = 1;
		}
	}

	/*
	 * We now know where the end of the log is.  Set the first LSN that
	 * we want to return to an application and the LSN of the last known
	 * record on disk.
	 */
	lp->lsn = lsn;
	lp->s_lsn = lsn;
	lp->lsn.offset += dblp->c_len;
	lp->s_lsn.offset += dblp->c_len;

	/* Set up the current buffer information, too. */
	lp->len = dblp->c_len;
	lp->b_off = 0;
	lp->w_off = lp->lsn.offset;

	/*
	 * It's possible that we didn't find a checkpoint because there wasn't
	 * one in the last log file.  Start searching.
	 */
	if (!found_checkpoint && cnt > 1) {
		lsn.file = cnt;
		lsn.offset = 0;

		/* Set the cursor.  Shouldn't fail, leave error messages on. */
		if ((ret = __log_get(dblp, &lsn, &dbt, DB_SET, 0)) != 0)
			return (ret);

		/*
		 * Read to the end of the file, saving checkpoints.  Again,
		 * this can fail if there are no checkpoints in any log file,
		 * so turn error messages off.
		 */
		while (__log_get(dblp, &lsn, &dbt, DB_PREV, 1) == 0) {
			if (dbt.size < sizeof(u_int32_t))
				continue;
			memcpy(&chk, dbt.data, sizeof(u_int32_t));
			if (chk == DB_txn_ckp) {
				lp->chkpt_lsn = lsn;
				found_checkpoint = 1;
				break;
			}
		}
	}

	/* If we never find a checkpoint, that's okay, just 0 it out. */
	if (!found_checkpoint)
skipsearch:	ZERO_LSN(lp->chkpt_lsn);

	/*
	 * Reset the cursor lsn to the beginning of the log, so that an
	 * initial call to DB_NEXT does the right thing.
	 */
	ZERO_LSN(dblp->c_lsn);

	if (FLD_ISSET(dblp->dbenv->verbose, DB_VERB_RECOVERY))
		__db_err(dblp->dbenv,
		    "Finding last valid log LSN: file: %lu offset %lu",
		    (u_long)lp->lsn.file, (u_long)lp->lsn.offset);

	return (0);
}

/*
 * __log_find --
 *	Try to find a log file.  If find_first is set, valp will contain
 * the number of the first readable log file, else it will contain the number
 * of the last log file (which may be too old to read).
 *
 * PUBLIC: int __log_find __P((DB_LOG *, int, int *, logfile_validity *));
 */
int
__log_find(dblp, find_first, valp, statusp)
	DB_LOG *dblp;
	int find_first, *valp;
	logfile_validity *statusp;
{
	logfile_validity logval_status, status;
	u_int32_t clv, logval;
	int cnt, fcnt, ret;
	const char *dir;
	char **names, *p, *q, savech;

	logval_status = status = DB_LV_NONEXISTENT;

	/* Return a value of 0 as the log file number on failure. */
	*valp = 0;

	/* Find the directory name. */
	if ((ret = __log_name(dblp, 1, &p, NULL, 0)) != 0)
		return (ret);
	if ((q = __db_rpath(p)) == NULL) {
		COMPQUIET(savech, 0);
		dir = PATH_DOT;
	} else {
		savech = *q;
		*q = '\0';
		dir = p;
	}

	/* Get the list of file names. */
	ret = __os_dirlist(dblp->dbenv, dir, &names, &fcnt);

	/*
	 * !!!
	 * We overwrote a byte in the string with a nul.  Restore the string
	 * so that the diagnostic checks in the memory allocation code work
	 * and any error messages display the right file name.
	 */
	if (q != NULL)
		*q = savech;

	if (ret != 0) {
		__db_err(dblp->dbenv, "%s: %s", dir, db_strerror(ret));
		__os_freestr(p);
		return (ret);
	}

	/* Search for a valid log file name. */
	for (cnt = fcnt, clv = logval = 0; --cnt >= 0;) {
		if (strncmp(names[cnt], LFPREFIX, sizeof(LFPREFIX) - 1) != 0)
			continue;

		/*
		 * Use atol, not atoi; if an "int" is 16-bits, the largest
		 * log file name won't fit.
		 */
		clv = atol(names[cnt] + (sizeof(LFPREFIX) - 1));
		if (find_first) {
			if (logval != 0 && clv > logval)
				continue;
		} else
			if (logval != 0 && clv < logval)
				continue;

		/*
		 * Take note of whether the log file logval is
		 * an old version or incompletely initialized.
		 */
		if ((ret = __log_valid(dblp, clv, 1, &status)) != 0)
			goto err;
		switch (status) {
		case DB_LV_INCOMPLETE:
			/*
			 * It's acceptable for the last log file to
			 * have been incompletely initialized--it's possible
			 * to create a log file but not write anything to it,
			 * and recovery needs to gracefully handle this.
			 *
			 * Just ignore it;  we don't want to return this
			 * as a valid log file.
			 */
			break;
		case DB_LV_NONEXISTENT:
			/* Should never happen. */
			DB_ASSERT(0);
			break;
		case DB_LV_NORMAL:
		case DB_LV_OLD_READABLE:
			logval = clv;
			logval_status = status;
			break;
		case DB_LV_OLD_UNREADABLE:
			/*
			 * Continue;  we want the oldest valid log,
			 * and clv is too old to be useful.  We don't
			 * want it to supplant logval if we're looking for
			 * the oldest valid log, but we do want to return
			 * it if it's the last log file--we want the very
			 * last file number, so that our caller can
			 * start a new file after it.
			 *
			 * The code here assumes that there will never
			 * be a too-old log that's preceded by a log
			 * of the current version, but in order to
			 * attain that state of affairs the user
			 * would have had to really seriously screw
			 * up;  I think we can safely assume this won't
			 * happen.
			 */
			if (!find_first) {
				logval = clv;
				logval_status = status;
			}
			break;
		}
	}

	*valp = logval;

err:	__os_dirfree(names, fcnt);
	__os_freestr(p);
	*statusp = logval_status;

	return (ret);
}

/*
 * log_valid --
 *	Validate a log file.  Returns an error code in the event of
 *	a fatal flaw in a the specified log file;  returns success with
 *	a code indicating the currentness and completeness of the specified
 *	log file if it is not unexpectedly flawed (that is, if it's perfectly
 *	normal, if it's zero-length, or if it's an old version).
 *
 * PUBLIC: int __log_valid __P((DB_LOG *, u_int32_t, int, logfile_validity *));
 */
int
__log_valid(dblp, number, set_persist, statusp)
	DB_LOG *dblp;
	u_int32_t number;
	int set_persist;
	logfile_validity *statusp;
{
	DB_FH fh;
	LOG *region;
	LOGP persist;
	char *fname;
	int ret;
	logfile_validity status;
	size_t nw;

	status = DB_LV_NORMAL;

	/* Try to open the log file. */
	if ((ret = __log_name(dblp,
	    number, &fname, &fh, DB_OSO_RDONLY | DB_OSO_SEQ)) != 0) {
		__os_freestr(fname);
		return (ret);
	}

	/* Try to read the header. */
	if ((ret =
	    __os_seek(dblp->dbenv,
	    &fh, 0, 0, sizeof(HDR), 0, DB_OS_SEEK_SET)) != 0 ||
	    (ret =
	    __os_read(dblp->dbenv, &fh, &persist, sizeof(LOGP), &nw)) != 0 ||
	    nw != sizeof(LOGP)) {
		if (ret == 0)
			status = DB_LV_INCOMPLETE;
		else
			/*
			 * The error was a fatal read error, not just an
			 * incompletely initialized log file.
			 */
			__db_err(dblp->dbenv, "Ignoring log file: %s: %s",
			    fname, db_strerror(ret));

		(void)__os_closehandle(&fh);
		goto err;
	}
	(void)__os_closehandle(&fh);

	/* Validate the header. */
	if (persist.magic != DB_LOGMAGIC) {
		__db_err(dblp->dbenv,
		    "Ignoring log file: %s: magic number %lx, not %lx",
		    fname, (u_long)persist.magic, (u_long)DB_LOGMAGIC);
		ret = EINVAL;
		goto err;
	}

	/*
	 * Set our status code to indicate whether the log file
	 * belongs to an unreadable or readable old version;  leave it
	 * alone if and only if the log file version is the current one.
	 */
	if (persist.version > DB_LOGVERSION) {
		/* This is a fatal error--the log file is newer than DB. */
		__db_err(dblp->dbenv,
		    "Ignoring log file: %s: unsupported log version %lu",
		    fname, (u_long)persist.version);
		ret = EINVAL;
		goto err;
	} else if (persist.version < DB_LOGOLDVER) {
		status = DB_LV_OLD_UNREADABLE;
		/*
		 * We don't want to set persistent info based on an
		 * unreadable region, so jump to "err".
		 */
		goto err;
	} else if (persist.version < DB_LOGVERSION)
		status = DB_LV_OLD_READABLE;

	/*
	 * If the log is thus far readable and we're doing system
	 * initialization, set the region's persistent information
	 * based on the headers.
	 */
	if (set_persist) {
		region = dblp->reginfo.primary;
		region->persist.lg_max = persist.lg_max;
		region->persist.mode = persist.mode;
	}

err:	__os_freestr(fname);
	*statusp = status;
	return (ret);
}

/*
 * __log_close --
 *	Internal version of log_close: only called from dbenv_refresh.
 *
 * PUBLIC: int __log_close __P((DB_ENV *));
 */
int
__log_close(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	int ret, t_ret;

	ret = 0;
	dblp = dbenv->lg_handle;

	/* We may have opened files as part of XA; if so, close them. */
	F_SET(dblp, DBLOG_RECOVER);
	__log_close_files(dbenv);

	/* Discard the per-thread lock. */
	if (dblp->mutexp != NULL)
		__db_mutex_free(dbenv, &dblp->reginfo, dblp->mutexp);

	/* Detach from the region. */
	ret = __db_r_detach(dbenv, &dblp->reginfo, 0);

	/* Close open files, release allocated memory. */
	if (F_ISSET(&dblp->lfh, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(&dblp->lfh)) != 0 && ret == 0)
		ret = t_ret;
	if (dblp->c_dbt.data != NULL)
		__os_free(dblp->c_dbt.data, dblp->c_dbt.ulen);
	if (F_ISSET(&dblp->c_fh, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(&dblp->c_fh)) != 0 && ret == 0)
		ret = t_ret;
	if (dblp->dbentry != NULL)
		__os_free(dblp->dbentry,
		    (dblp->dbentry_cnt * sizeof(DB_ENTRY)));
	if (dblp->readbufp != NULL)
		__os_free(dblp->readbufp, dbenv->lg_bsize);

	__os_free(dblp, sizeof(*dblp));

	dbenv->lg_handle = NULL;
	return (ret);
}

/*
 * log_stat --
 *	Return LOG statistics.
 */
int
log_stat(dbenv, statp, db_malloc)
	DB_ENV *dbenv;
	DB_LOG_STAT **statp;
	void *(*db_malloc) __P((size_t));
{
	DB_LOG *dblp;
	DB_LOG_STAT *stats;
	LOG *region;
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_log_stat(dbenv, statp, db_malloc));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lg_handle, DB_INIT_LOG);

	*statp = NULL;

	dblp = dbenv->lg_handle;
	region = dblp->reginfo.primary;

	if ((ret = __os_malloc(dbenv,
	    sizeof(DB_LOG_STAT), db_malloc, &stats)) != 0)
		return (ret);

	/* Copy out the global statistics. */
	R_LOCK(dbenv, &dblp->reginfo);
	*stats = region->stat;

	stats->st_magic = region->persist.magic;
	stats->st_version = region->persist.version;
	stats->st_mode = region->persist.mode;
	stats->st_lg_bsize = region->buffer_size;
	stats->st_lg_max = region->persist.lg_max;

	stats->st_region_wait = dblp->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = dblp->reginfo.rp->mutex.mutex_set_nowait;
	stats->st_regsize = dblp->reginfo.rp->size;

	stats->st_cur_file = region->lsn.file;
	stats->st_cur_offset = region->lsn.offset;

	R_UNLOCK(dbenv, &dblp->reginfo);

	*statp = stats;
	return (0);
}

/*
 * __log_lastckp --
 *	Return the current chkpt_lsn, so that we can store it in
 *	the transaction region and keep the chain of checkpoints
 *	unbroken across environment recreates.
 *
 * PUBLIC: int __log_lastckp __P((DB_ENV *, DB_LSN *));
 */
int
__log_lastckp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	LOG *lp;

	lp = (LOG *)(((DB_LOG *)dbenv->lg_handle)->reginfo.primary);

	*lsnp = lp->chkpt_lsn;
	return (0);
}
