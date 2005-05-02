/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log.c,v 11.111 2002/08/16 00:27:44 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/hmac.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __log_init __P((DB_ENV *, DB_LOG *));
static int __log_recover __P((DB_LOG *));
static size_t __log_region_size __P((DB_ENV *));
static int __log_zero __P((DB_ENV *, DB_LSN *, DB_LSN *));

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

	/* Create/initialize the DB_LOG structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_LOG), &dblp)) != 0)
		return (ret);
	dblp->dbenv = dbenv;

	/* Join/create the log region. */
	dblp->reginfo.type = REGION_TYPE_LOG;
	dblp->reginfo.id = INVALID_REGION_ID;
	dblp->reginfo.mode = dbenv->db_mode;
	dblp->reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&dblp->reginfo, REGION_CREATE_OK);
	if ((ret = __db_r_attach(
	    dbenv, &dblp->reginfo, __log_region_size(dbenv))) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&dblp->reginfo, REGION_CREATE))
		if ((ret = __log_init(dbenv, dblp)) != 0)
			goto err;

	/* Set the local addresses. */
	lp = dblp->reginfo.primary =
	    R_ADDR(&dblp->reginfo, dblp->reginfo.rp->primary);

	/*
	 * If the region is threaded, then we have to lock both the handles
	 * and the region, and we need to allocate a mutex for that purpose.
	 */
	if (F_ISSET(dbenv, DB_ENV_THREAD) &&
	    (ret = __db_mutex_setup(dbenv, &dblp->reginfo, &dblp->mutexp,
	    MUTEX_ALLOC | MUTEX_NO_RLOCK)) != 0)
		goto err;

	/* Initialize the rest of the structure. */
	dblp->bufp = R_ADDR(&dblp->reginfo, lp->buffer_off);

	/*
	 * Set the handle -- we may be about to run recovery, which allocates
	 * log cursors.  Log cursors require logging be already configured,
	 * and the handle being set is what demonstrates that.
	 *
	 * If we created the region, run recovery.  If that fails, make sure
	 * we reset the log handle before cleaning up, otherwise we will try
	 * and clean up again in the mainline DB_ENV initialization code.
	 */
	dbenv->lg_handle = dblp;

	if (F_ISSET(&dblp->reginfo, REGION_CREATE)) {
		if ((ret = __log_recover(dblp)) != 0) {
			dbenv->lg_handle = NULL;
			goto err;
		}

		/*
		 * We first take the log file size from the environment, if
		 * specified.  If that wasn't set, recovery may have set it
		 * from the persistent information in a log file header.  If
		 * that didn't set it either, we default.
		 */
		if (lp->log_size == 0)
			lp->log_size = lp->log_nsize = LG_MAX_DEFAULT;
	} else {
		/*
		 * A process joining the region may have reset the log file
		 * size, too.  If so, it only affects the next log file we
		 * create.
		 */
		 if (dbenv->lg_size != 0)
			lp->log_nsize = dbenv->lg_size;
	}

	R_UNLOCK(dbenv, &dblp->reginfo);
	return (0);

err:	if (dblp->reginfo.addr != NULL) {
		if (F_ISSET(&dblp->reginfo, REGION_CREATE))
			ret = __db_panic(dbenv, ret);
		R_UNLOCK(dbenv, &dblp->reginfo);
		(void)__db_r_detach(dbenv, &dblp->reginfo, 0);
	}

	if (dblp->mutexp != NULL)
		__db_mutex_free(dbenv, &dblp->reginfo, dblp->mutexp);

	__os_free(dbenv, dblp);

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
	DB_MUTEX *flush_mutexp;
	LOG *region;
	int ret;
	void *p;
#ifdef  HAVE_MUTEX_SYSTEM_RESOURCES
	u_int8_t *addr;
#endif

	if ((ret = __db_shalloc(dblp->reginfo.addr,
	    sizeof(*region), 0, &dblp->reginfo.primary)) != 0)
		goto mem_err;
	dblp->reginfo.rp->primary =
	    R_OFFSET(&dblp->reginfo, dblp->reginfo.primary);
	region = dblp->reginfo.primary;
	memset(region, 0, sizeof(*region));

	region->fid_max = 0;
	SH_TAILQ_INIT(&region->fq);
	region->free_fid_stack = INVALID_ROFF;
	region->free_fids = region->free_fids_alloced = 0;

	/* Initialize LOG LSNs. */
	INIT_LSN(region->lsn);
	INIT_LSN(region->ready_lsn);
	INIT_LSN(region->t_lsn);

	/*
	 * It's possible to be waiting for an LSN of [1][0], if a replication
	 * client gets the first log record out of order.  An LSN of [0][0]
	 * signifies that we're not waiting.
	 */
	ZERO_LSN(region->waiting_lsn);

	/*
	 * Log makes note of the fact that it ran into a checkpoint on
	 * startup if it did so, as a recovery optimization.  A zero
	 * LSN signifies that it hasn't found one [yet].
	 */
	ZERO_LSN(region->cached_ckp_lsn);

#ifdef  HAVE_MUTEX_SYSTEM_RESOURCES
	/* Allocate room for the log maintenance info and initialize it. */
	if ((ret = __db_shalloc(dblp->reginfo.addr,
	    sizeof(REGMAINT) + LG_MAINT_SIZE, 0, &addr)) != 0)
		goto mem_err;
	__db_maintinit(&dblp->reginfo, addr, LG_MAINT_SIZE);
	region->maint_off = R_OFFSET(&dblp->reginfo, addr);
#endif

	if ((ret = __db_mutex_setup(dbenv, &dblp->reginfo, &region->fq_mutex,
	    MUTEX_NO_RLOCK)) != 0)
		return (ret);

	/*
	 * We must create a place for the flush mutex separately; mutexes have
	 * to be aligned to MUTEX_ALIGN, and the only way to guarantee that is
	 * to make sure they're at the beginning of a shalloc'ed chunk.
	 */
	if ((ret = __db_shalloc(dblp->reginfo.addr,
	    sizeof(DB_MUTEX), MUTEX_ALIGN, &flush_mutexp)) != 0)
		goto mem_err;
	if ((ret = __db_mutex_setup(dbenv, &dblp->reginfo, flush_mutexp,
	    MUTEX_NO_RLOCK)) != 0)
		return (ret);
	region->flush_mutex_off = R_OFFSET(&dblp->reginfo, flush_mutexp);

	/* Initialize the buffer. */
	if ((ret =
	    __db_shalloc(dblp->reginfo.addr, dbenv->lg_bsize, 0, &p)) != 0) {
mem_err:	__db_err(dbenv, "Unable to allocate memory for the log buffer");
		return (ret);
	}
	region->buffer_size = dbenv->lg_bsize;
	region->buffer_off = R_OFFSET(&dblp->reginfo, p);
	region->log_size = region->log_nsize = dbenv->lg_size;

	/* Initialize the commit Queue. */
	SH_TAILQ_INIT(&region->free_commits);
	SH_TAILQ_INIT(&region->commits);
	region->ncommit = 0;

	/*
	 * Fill in the log's persistent header.  Don't fill in the log file
	 * sizes, as they may change at any time and so have to be filled in
	 * as each log file is created.
	 */
	region->persist.magic = DB_LOGMAGIC;
	region->persist.version = DB_LOGVERSION;
	region->persist.mode = (u_int32_t)dbenv->db_mode;

	return (0);
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
	DB_ENV *dbenv;
	DB_LOGC *logc;
	DB_LSN lsn;
	LOG *lp;
	u_int32_t cnt, rectype;
	int ret;
	logfile_validity status;

	logc = NULL;
	dbenv = dblp->dbenv;
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
	 * file.  Don't bother finding the end of the last log file;
	 * we assume that it's valid in its entirety, since the user
	 * should have shut down cleanly or run recovery before upgrading.
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

	/*
	 * Allocate a cursor and set it to the first record.  This shouldn't
	 * fail, leave error messages on.
	 */
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		return (ret);
	F_SET(logc, DB_LOG_LOCKED);
	memset(&dbt, 0, sizeof(dbt));
	if ((ret = logc->get(logc, &lsn, &dbt, DB_SET)) != 0)
		goto err;

	/*
	 * Read to the end of the file.  This may fail at some point, so
	 * turn off error messages.
	 */
	F_SET(logc, DB_LOG_SILENT_ERR);
	while (logc->get(logc, &lsn, &dbt, DB_NEXT) == 0) {
		if (dbt.size < sizeof(u_int32_t))
			continue;
		memcpy(&rectype, dbt.data, sizeof(u_int32_t));
		if (rectype == DB___txn_ckp)
			/*
			 * If we happen to run into a checkpoint, cache its
			 * LSN so that the transaction system doesn't have
			 * to walk this log file again looking for it.
			 */
			lp->cached_ckp_lsn = lsn;
	}
	F_CLR(logc, DB_LOG_SILENT_ERR);

	/*
	 * We now know where the end of the log is.  Set the first LSN that
	 * we want to return to an application and the LSN of the last known
	 * record on disk.
	 */
	lp->lsn = lsn;
	lp->s_lsn = lsn;
	lp->lsn.offset += logc->c_len;
	lp->s_lsn.offset += logc->c_len;

	/* Set up the current buffer information, too. */
	lp->len = logc->c_len;
	lp->b_off = 0;
	lp->w_off = lp->lsn.offset;

skipsearch:
	if (FLD_ISSET(dbenv->verbose, DB_VERB_RECOVERY))
		__db_err(dbenv,
		    "Finding last valid log LSN: file: %lu offset %lu",
		    (u_long)lp->lsn.file, (u_long)lp->lsn.offset);

err:	if (logc != NULL)
		(void)logc->close(logc, 0);

	return (ret);
}

/*
 * __log_find --
 *	Try to find a log file.  If find_first is set, valp will contain
 * the number of the first readable log file, else it will contain the number
 * of the last log file (which may be too old to read).
 *
 * PUBLIC: int __log_find __P((DB_LOG *, int, u_int32_t *, logfile_validity *));
 */
int
__log_find(dblp, find_first, valp, statusp)
	DB_LOG *dblp;
	int find_first;
	u_int32_t *valp;
	logfile_validity *statusp;
{
	DB_ENV *dbenv;
	logfile_validity logval_status, status;
	u_int32_t clv, logval;
	int cnt, fcnt, ret;
	const char *dir;
	char *c, **names, *p, *q, savech;

	dbenv = dblp->dbenv;
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
	ret = __os_dirlist(dbenv, dir, &names, &fcnt);

	/*
	 * !!!
	 * We overwrote a byte in the string with a nul.  Restore the string
	 * so that the diagnostic checks in the memory allocation code work
	 * and any error messages display the right file name.
	 */
	if (q != NULL)
		*q = savech;

	if (ret != 0) {
		__db_err(dbenv, "%s: %s", dir, db_strerror(ret));
		__os_free(dbenv, p);
		return (ret);
	}

	/* Search for a valid log file name. */
	for (cnt = fcnt, clv = logval = 0; --cnt >= 0;) {
		if (strncmp(names[cnt], LFPREFIX, sizeof(LFPREFIX) - 1) != 0)
			continue;

		/*
		 * Names of the form log\.[0-9]* are reserved for DB.  Other
		 * names sharing LFPREFIX, such as "log.db", are legal.
		 */
		for (c = names[cnt] + sizeof(LFPREFIX) - 1; *c != '\0'; c++)
			if (!isdigit((int)*c))
				break;
		if (*c != '\0')
			continue;

		/*
		 * Use atol, not atoi; if an "int" is 16-bits, the largest
		 * log file name won't fit.
		 */
		clv = atol(names[cnt] + (sizeof(LFPREFIX) - 1));

		/*
		 * If searching for the first log file, we want to return the
		 * oldest log file we can read, or, if no readable log files
		 * exist, the newest log file we can't read (the crossover
		 * point between the old and new versions of the log file).
		 *
		 * If we're searching for the last log file, we want to return
		 * the newest log file, period.
		 *
		 * Readable log files should never preceede unreadable log
		 * files, that would mean the admin seriously screwed up.
		 */
		if (find_first) {
			if (logval != 0 &&
			    status != DB_LV_OLD_UNREADABLE && clv > logval)
				continue;
		} else
			if (logval != 0 && clv < logval)
				continue;

		if ((ret = __log_valid(dblp, clv, 1, &status)) != 0) {
			__db_err(dbenv, "Invalid log file: %s: %s",
			    names[cnt], db_strerror(ret));
			goto err;
		}
		switch (status) {
		case DB_LV_NONEXISTENT:
			/* __log_valid never returns DB_LV_NONEXISTENT. */
			DB_ASSERT(0);
			break;
		case DB_LV_INCOMPLETE:
			/*
			 * The last log file may not have been initialized --
			 * it's possible to create a log file but not write
			 * anything to it.  If performing recovery (that is,
			 * if find_first isn't set), ignore the file, it's
			 * not interesting.  If we're searching for the first
			 * log record, return the file (assuming we don't find
			 * something better), as the "real" first log record
			 * is likely to be in the log buffer, and we want to
			 * set the file LSN for our return.
			 */
			if (find_first)
				goto found;
			break;
		case DB_LV_OLD_UNREADABLE:
			/*
			 * If we're searching for the first log file, then we
			 * only want this file if we don't yet have a file or
			 * already have an unreadable file and this one is
			 * newer than that one.  If we're searching for the
			 * last log file, we always want this file because we
			 * wouldn't be here if it wasn't newer than our current
			 * choice.
			 */
			if (!find_first || logval == 0 ||
			    (status == DB_LV_OLD_UNREADABLE && clv > logval))
				goto found;
			break;
		case DB_LV_NORMAL:
		case DB_LV_OLD_READABLE:
found:			logval = clv;
			logval_status = status;
			break;
		}
	}

	*valp = logval;

err:	__os_dirfree(dbenv, names, fcnt);
	__os_free(dbenv, p);
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
	DB_CIPHER *db_cipher;
	DB_ENV *dbenv;
	DB_FH fh;
	HDR *hdr;
	LOG *region;
	LOGP *persist;
	logfile_validity status;
	size_t hdrsize, nw, recsize;
	int is_hmac, need_free, ret;
	u_int8_t *tmp;
	char *fname;

	dbenv = dblp->dbenv;
	db_cipher = dbenv->crypto_handle;
	persist = NULL;
	status = DB_LV_NORMAL;

	/* Try to open the log file. */
	if ((ret = __log_name(dblp,
	    number, &fname, &fh, DB_OSO_RDONLY | DB_OSO_SEQ)) != 0) {
		__os_free(dbenv, fname);
		return (ret);
	}

	need_free = 0;
	hdrsize = HDR_NORMAL_SZ;
	is_hmac = 0;
	recsize = sizeof(LOGP);
	if (CRYPTO_ON(dbenv)) {
		hdrsize = HDR_CRYPTO_SZ;
		recsize = sizeof(LOGP);
		recsize += db_cipher->adj_size(recsize);
		is_hmac = 1;
	}
	if ((ret = __os_calloc(dbenv, 1, recsize + hdrsize, &tmp)) != 0)
		return (ret);
	need_free = 1;
	hdr = (HDR *)tmp;
	persist = (LOGP *)(tmp + hdrsize);
	/* Try to read the header. */
	if ((ret = __os_read(dbenv, &fh, tmp, recsize + hdrsize, &nw)) != 0 ||
	    nw != recsize + hdrsize) {
		if (ret == 0)
			status = DB_LV_INCOMPLETE;
		else
			/*
			 * The error was a fatal read error, not just an
			 * incompletely initialized log file.
			 */
			__db_err(dbenv, "Ignoring log file: %s: %s",
			    fname, db_strerror(ret));

		(void)__os_closehandle(dbenv, &fh);
		goto err;
	}
	(void)__os_closehandle(dbenv, &fh);

	/*
	 * Now we have to validate the persistent record.  We have
	 * several scenarios we have to deal with:
	 *
	 * 1.  User has crypto turned on:
	 *	- They're reading an old, unencrypted log file
	 *	  .  We will fail the record size match check below.
	 *	- They're reading a current, unencrypted log file
	 *	  .  We will fail the record size match check below.
	 *	- They're reading an old, encrypted log file [NOT YET]
	 *	  .  After decryption we'll fail the version check.  [NOT YET]
	 *	- They're reading a current, encrypted log file
	 *	  .  We should proceed as usual.
	 * 2.  User has crypto turned off:
	 *	- They're reading an old, unencrypted log file
	 *	  .  We will fail the version check.
	 *	- They're reading a current, unencrypted log file
	 *	  .  We should proceed as usual.
	 *	- They're reading an old, encrypted log file [NOT YET]
	 *	  .  We'll fail the magic number check (it is encrypted).
	 *	- They're reading a current, encrypted log file
	 *	  .  We'll fail the magic number check (it is encrypted).
	 */
	if (CRYPTO_ON(dbenv)) {
		/*
		 * If we are trying to decrypt an unencrypted log
		 * we can only detect that by having an unreasonable
		 * data length for our persistent data.
		 */
		if ((hdr->len - hdrsize) != sizeof(LOGP)) {
			__db_err(dbenv, "log record size mismatch");
			goto err;
		}
		/* Check the checksum and decrypt. */
		if ((ret = __db_check_chksum(dbenv, db_cipher, &hdr->chksum[0],
		    (u_int8_t *)persist, hdr->len - hdrsize, is_hmac)) != 0) {
			__db_err(dbenv, "log record checksum mismatch");
			goto err;
		}
		if ((ret = db_cipher->decrypt(dbenv, db_cipher->data,
		    &hdr->iv[0], (u_int8_t *)persist, hdr->len - hdrsize)) != 0)
			goto err;
	}

	/* Validate the header. */
	if (persist->magic != DB_LOGMAGIC) {
		__db_err(dbenv,
		    "Ignoring log file: %s: magic number %lx, not %lx",
		    fname, (u_long)persist->magic, (u_long)DB_LOGMAGIC);
		ret = EINVAL;
		goto err;
	}

	/*
	 * Set our status code to indicate whether the log file
	 * belongs to an unreadable or readable old version;  leave it
	 * alone if and only if the log file version is the current one.
	 */
	if (persist->version > DB_LOGVERSION) {
		/* This is a fatal error--the log file is newer than DB. */
		__db_err(dbenv,
		    "Ignoring log file: %s: unsupported log version %lu",
		    fname, (u_long)persist->version);
		ret = EINVAL;
		goto err;
	} else if (persist->version < DB_LOGOLDVER) {
		status = DB_LV_OLD_UNREADABLE;
		/*
		 * We don't want to set persistent info based on an
		 * unreadable region, so jump to "err".
		 */
		goto err;
	} else if (persist->version < DB_LOGVERSION)
		status = DB_LV_OLD_READABLE;

	/*
	 * Only if we have a current log do we verify the checksum.
	 * We could not check the checksum before checking the magic
	 * and version because old log hdrs have the length and checksum
	 * in a different location.
	 */
	if (!CRYPTO_ON(dbenv) && ((ret = __db_check_chksum(dbenv,
	    db_cipher, &hdr->chksum[0], (u_int8_t *)persist,
	    hdr->len - hdrsize, is_hmac)) != 0)) {
		__db_err(dbenv, "log record checksum mismatch");
		goto err;
	}

	/*
	 * If the log is readable so far and we're doing system initialization,
	 * set the region's persistent information based on the headers.
	 *
	 * Always set the current log file size.  Only set the next log file's
	 * size if the application hasn't set it already.
	 *
	 * XXX
	 * Always use the persistent header's mode, regardless of what was set
	 * in the current environment.  We've always done it this way, but it's
	 * probably a bug -- I can't think of a way not-changing the mode would
	 * be a problem, though.
	 */
	if (set_persist) {
		region = dblp->reginfo.primary;
		region->log_size = persist->log_size;
		if (region->log_nsize == 0)
			region->log_nsize = persist->log_size;
		region->persist.mode = persist->mode;
	}

err:	__os_free(dbenv, fname);
	if (need_free)
		__os_free(dbenv, tmp);
	*statusp = status;
	return (ret);
}

/*
 * __log_dbenv_refresh --
 *	Clean up after the log system on a close or failed open.  Called only
 * from __dbenv_refresh.  (Formerly called __log_close.)
 *
 * PUBLIC: int __log_dbenv_refresh __P((DB_ENV *));
 */
int
__log_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	int ret, t_ret;

	dblp = dbenv->lg_handle;

	/* We may have opened files as part of XA; if so, close them. */
	F_SET(dblp, DBLOG_RECOVER);
	ret = __dbreg_close_files(dbenv);

	/* Discard the per-thread lock. */
	if (dblp->mutexp != NULL)
		__db_mutex_free(dbenv, &dblp->reginfo, dblp->mutexp);

	/* Detach from the region. */
	if ((t_ret =
	    __db_r_detach(dbenv, &dblp->reginfo, 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Close open files, release allocated memory. */
	if (F_ISSET(&dblp->lfh, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(dbenv, &dblp->lfh)) != 0 && ret == 0)
		ret = t_ret;
	if (dblp->dbentry != NULL)
		__os_free(dbenv, dblp->dbentry);

	__os_free(dbenv, dblp);

	dbenv->lg_handle = NULL;
	return (ret);
}

/*
 * __log_stat --
 *	Return log statistics.
 *
 * PUBLIC: int __log_stat __P((DB_ENV *, DB_LOG_STAT **, u_int32_t));
 */
int
__log_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOG_STAT **statp;
	u_int32_t flags;
{
	DB_LOG *dblp;
	DB_LOG_STAT *stats;
	LOG *region;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_stat", DB_INIT_LOG);

	*statp = NULL;
	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->log_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	dblp = dbenv->lg_handle;
	region = dblp->reginfo.primary;

	if ((ret = __os_umalloc(dbenv, sizeof(DB_LOG_STAT), &stats)) != 0)
		return (ret);

	/* Copy out the global statistics. */
	R_LOCK(dbenv, &dblp->reginfo);
	*stats = region->stat;
	if (LF_ISSET(DB_STAT_CLEAR))
		memset(&region->stat, 0, sizeof(region->stat));

	stats->st_magic = region->persist.magic;
	stats->st_version = region->persist.version;
	stats->st_mode = region->persist.mode;
	stats->st_lg_bsize = region->buffer_size;
	stats->st_lg_size = region->log_nsize;

	stats->st_region_wait = dblp->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = dblp->reginfo.rp->mutex.mutex_set_nowait;
	if (LF_ISSET(DB_STAT_CLEAR)) {
		dblp->reginfo.rp->mutex.mutex_set_wait = 0;
		dblp->reginfo.rp->mutex.mutex_set_nowait = 0;
	}
	stats->st_regsize = dblp->reginfo.rp->size;

	stats->st_cur_file = region->lsn.file;
	stats->st_cur_offset = region->lsn.offset;
	stats->st_disk_file = region->s_lsn.file;
	stats->st_disk_offset = region->s_lsn.offset;

	R_UNLOCK(dbenv, &dblp->reginfo);

	*statp = stats;
	return (0);
}

/*
 * __log_get_cached_ckp_lsn --
 *	Retrieve any last checkpoint LSN that we may have found on startup.
 *
 * PUBLIC: void __log_get_cached_ckp_lsn __P((DB_ENV *, DB_LSN *));
 */
void
__log_get_cached_ckp_lsn(dbenv, ckp_lsnp)
	DB_ENV *dbenv;
	DB_LSN *ckp_lsnp;
{
	DB_LOG *dblp;
	LOG *lp;

	dblp = (DB_LOG *)dbenv->lg_handle;
	lp = (LOG *)dblp->reginfo.primary;

	R_LOCK(dbenv, &dblp->reginfo);
	*ckp_lsnp = lp->cached_ckp_lsn;
	R_UNLOCK(dbenv, &dblp->reginfo);
}

/*
 * __log_region_size --
 *	Return the amount of space needed for the log region.
 *	Make the region large enough to hold txn_max transaction
 *	detail structures  plus some space to hold thread handles
 *	and the beginning of the shalloc region and anything we
 *	need for mutex system resource recording.
 */
static size_t
__log_region_size(dbenv)
	DB_ENV *dbenv;
{
	size_t s;

	s = dbenv->lg_regionmax + dbenv->lg_bsize;
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		s += sizeof(REGMAINT) + LG_MAINT_SIZE;
#endif
	return (s);
}

/*
 * __log_region_destroy
 *	Destroy any region maintenance info.
 *
 * PUBLIC: void __log_region_destroy __P((DB_ENV *, REGINFO *));
 */
void
__log_region_destroy(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	__db_shlocks_destroy(infop, (REGMAINT *)R_ADDR(infop,
	    ((LOG *)R_ADDR(infop, infop->rp->primary))->maint_off));

	COMPQUIET(dbenv, NULL);
	COMPQUIET(infop, NULL);
}

/*
 * __log_vtruncate
 *	This is a virtual truncate.  We set up the log indicators to
 * make everyone believe that the given record is the last one in the
 * log.  Returns with the next valid LSN (i.e., the LSN of the next
 * record to be written). This is used in replication to discard records
 * in the log file that do not agree with the master.
 *
 * PUBLIC: int __log_vtruncate __P((DB_ENV *, DB_LSN *, DB_LSN *));
 */
int
__log_vtruncate(dbenv, lsn, ckplsn)
	DB_ENV *dbenv;
	DB_LSN *lsn, *ckplsn;
{
	DBT log_dbt;
	DB_FH fh;
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN end_lsn;
	LOG *lp;
	u_int32_t bytes, c_len;
	int fn, ret, t_ret;
	char *fname;

	/* Need to find out the length of this soon-to-be-last record. */
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		return (ret);
	memset(&log_dbt, 0, sizeof(log_dbt));
	ret = logc->get(logc, lsn, &log_dbt, DB_SET);
	c_len = logc->c_len;
	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		return (ret);

	/* Now do the truncate. */
	dblp = (DB_LOG *)dbenv->lg_handle;
	lp = (LOG *)dblp->reginfo.primary;

	R_LOCK(dbenv, &dblp->reginfo);
	end_lsn = lp->lsn;
	lp->lsn = *lsn;
	lp->len = c_len;
	lp->lsn.offset += lp->len;

	/*
	 * I am going to assume that the number of bytes written since
	 * the last checkpoint doesn't exceed a 32-bit number.
	 */
	DB_ASSERT(lp->lsn.file >= ckplsn->file);
	bytes = 0;
	if (ckplsn->file != lp->lsn.file) {
		bytes = lp->log_size - ckplsn->offset;
		if (lp->lsn.file > ckplsn->file + 1)
			bytes += lp->log_size *
			    (lp->lsn.file - ckplsn->file - 1);
		bytes += lp->lsn.offset;
	} else
		bytes = lp->lsn.offset - ckplsn->offset;

	lp->stat.st_wc_mbytes += bytes / MEGABYTE;
	lp->stat.st_wc_bytes += bytes % MEGABYTE;

	/*
	 * If the saved lsn is greater than our new end of log, reset it
	 * to our current end of log.
	 */
	if (log_compare(&lp->s_lsn, lsn) > 0)
		lp->s_lsn = lp->lsn;

	/*
	 * If the new end of log is in the middle of the buffer,
	 * don't change the w_off or f_lsn.  If the new end is
	 * before the w_off then reset w_off and f_lsn to the new
	 * end of log.
	 */
	if (lp->w_off >= lp->lsn.offset) {
		lp->f_lsn = lp->lsn;
		lp->w_off = lp->lsn.offset;
		lp->b_off = 0;
	} else
		lp->b_off = lp->lsn.offset - lp->w_off;

	ZERO_LSN(lp->waiting_lsn);
	lp->ready_lsn = lp->lsn;
	lp->wait_recs = 0;
	lp->rcvd_recs = 0;

	/* Now throw away any extra log files that we have around. */
	for (fn = lp->lsn.file + 1;; fn++) {
		if (__log_name(dblp, fn, &fname, &fh, DB_OSO_RDONLY) != 0) {
			__os_free(dbenv, fname);
			break;
		}
		(void)__os_closehandle(dbenv, &fh);
		ret = __os_unlink(dbenv, fname);
		__os_free(dbenv, fname);
		if (ret != 0)
			goto err;
	}

	/* Truncate the log to the new point. */
	if ((ret = __log_zero(dbenv, &lp->lsn, &end_lsn)) != 0)
		goto err;

err:	R_UNLOCK(dbenv, &dblp->reginfo);
	return (ret);
}

/*
 * __log_is_outdated --
 *	Used by the replication system to identify if a client's logs
 * are too old.  The log represented by dbenv is compared to the file
 * number passed in fnum.  If the log file fnum does not exist and is
 * lower-numbered than the current logs, the we return *outdatedp non
 * zero, else we return it 0.
 *
 * PUBLIC: int __log_is_outdated __P((DB_ENV *dbenv,
 * PUBLIC:     u_int32_t fnum, int *outdatedp));
 */
int
__log_is_outdated(dbenv, fnum, outdatedp)
	DB_ENV *dbenv;
	u_int32_t fnum;
	int *outdatedp;
{
	DB_LOG *dblp;
	LOG *lp;
	char *name;
	int ret;
	u_int32_t cfile;

	dblp = dbenv->lg_handle;
	*outdatedp = 0;

	if ((ret = __log_name(dblp, fnum, &name, NULL, 0)) != 0)
		return (ret);

	/* If the file exists, we're just fine. */
	if (__os_exists(name, NULL) == 0)
		goto out;

	/*
	 * It didn't exist, decide if the file number is too big or
	 * too little.  If it's too little, then we need to indicate
	 * that the LSN is outdated.
	 */
	R_LOCK(dbenv, &dblp->reginfo);
	lp = (LOG *)dblp->reginfo.primary;
	cfile = lp->lsn.file;
	R_UNLOCK(dbenv, &dblp->reginfo);

	if (cfile > fnum)
		*outdatedp = 1;
out:	__os_free(dbenv, name);
	return (ret);
}

/*
 * __log_zero --
 *	Zero out the tail of a log after a truncate.
 */
static int
__log_zero(dbenv, from_lsn, to_lsn)
	DB_ENV *dbenv;
	DB_LSN *from_lsn, *to_lsn;
{
	char *lname;
	DB_LOG *dblp;
	LOG *lp;
	int ret;
	size_t nbytes, len, nw;
	u_int8_t buf[4096];
	u_int32_t mbytes, bytes;

	dblp = dbenv->lg_handle;
	lp = (LOG *)dblp->reginfo.primary;
	lname = NULL;

	if (dblp->lfname != lp->lsn.file) {
		if (F_ISSET(&dblp->lfh, DB_FH_VALID))
			(void)__os_closehandle(dbenv, &dblp->lfh);
		dblp->lfname = lp->lsn.file;
	}

	if (from_lsn->file != to_lsn->file) {
		/* We removed some log files; have to 0 to end of file. */
		if (!F_ISSET(&dblp->lfh, DB_FH_VALID) && (ret =
		    __log_name(dblp, dblp->lfname, &lname, &dblp->lfh, 0)) != 0)
			return (ret);
		if ((ret = __os_ioinfo(dbenv,
		    NULL, &dblp->lfh, &mbytes, &bytes, NULL)) != 0)
			goto err;
		len = mbytes * MEGABYTE + bytes - from_lsn->offset;
	} else if (to_lsn->offset <= from_lsn->offset)
		return (0);
	else
		len = to_lsn->offset = from_lsn->offset;

	memset(buf, 0, sizeof(buf));

	/* Initialize the write position. */
	if (!F_ISSET(&dblp->lfh, DB_FH_VALID) &&
	    (ret = __log_name(dblp, dblp->lfname, &lname, &dblp->lfh, 0)) != 0)
		goto err;

	if ((ret = __os_seek(dbenv,
	    &dblp->lfh, 0, 0, from_lsn->offset, 0, DB_OS_SEEK_SET)) != 0)
		return (ret);

	while (len > 0) {
		nbytes = len > sizeof(buf) ? sizeof(buf) : len;
		if ((ret =
		    __os_write(dbenv, &dblp->lfh, buf, nbytes, &nw)) != 0)
			return (ret);
		len -= nbytes;
	}
err:	if (lname != NULL)
		__os_free(dbenv, lname);

	return (0);
}
