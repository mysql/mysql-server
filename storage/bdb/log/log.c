/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: log.c,v 11.161 2004/10/15 16:59:42 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/hmac.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int	__log_init __P((DB_ENV *, DB_LOG *));
static int	__log_recover __P((DB_LOG *));
static size_t	__log_region_size __P((DB_ENV *));
static int	__log_zero __P((DB_ENV *, DB_LSN *, DB_LSN *));

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

	/* Set the default buffer size, if not otherwise configured. */
	if (dbenv->lg_bsize == 0)
		dbenv->lg_bsize = F_ISSET(dbenv, DB_ENV_LOG_INMEMORY) ?
		    LG_BSIZE_INMEM : LG_BSIZE_DEFAULT;

	/* Join/create the log region. */
	dblp->reginfo.dbenv = dbenv;
	dblp->reginfo.type = REGION_TYPE_LOG;
	dblp->reginfo.id = INVALID_REGION_ID;
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
		/*
		 * We first take the log file size from the environment, if
		 * specified.  If that wasn't set, default it.  Regardless,
		 * recovery may set it from the persistent information in a
		 * log file header.
		 */
		if (lp->log_size == 0)
			lp->log_size = F_ISSET(dbenv, DB_ENV_LOG_INMEMORY) ?
			    LG_MAX_INMEM : LG_MAX_DEFAULT;

		if ((ret = __log_recover(dblp)) != 0)
			goto err;

		/*
		 * If the next log file size hasn't been set yet, default it
		 * to the current log file size.
		 */
		if (lp->log_nsize == 0)
			lp->log_nsize = lp->log_size;

		/*
		 * If we haven't written any log files, write the first one
		 * so that checkpoint gets a valid ckp_lsn value.
		 */
		if (IS_INIT_LSN(lp->lsn) &&
		    (ret = __log_newfile(dblp, NULL, 0)) != 0)
			goto err;

		/* Initialize replication's next-expected LSN value. */
		lp->ready_lsn = lp->lsn;
	} else {
		/*
		 * A process joining the region may have reset the log file
		 * size, too.  If so, it only affects the next log file we
		 * create.  We need to check that the size is reasonable given
		 * the buffer size in the region.
		 */
		 if (dbenv->lg_size != 0) {
			if ((ret =
			    __log_check_sizes(dbenv, dbenv->lg_size, 0)) != 0)
				goto err;

			lp->log_nsize = dbenv->lg_size;
		 }

		/* Migrate persistent flags from the region into the DB_ENV. */
		if (lp->db_log_autoremove)
			F_SET(dbenv, DB_ENV_LOG_AUTOREMOVE);
		if (lp->db_log_inmemory)
			F_SET(dbenv, DB_ENV_LOG_INMEMORY);
	}

	R_UNLOCK(dbenv, &dblp->reginfo);
	return (0);

err:	dbenv->lg_handle = NULL;
	if (dblp->reginfo.addr != NULL) {
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
	LOG *lp;
	int ret;
	void *p;
#ifdef  HAVE_MUTEX_SYSTEM_RESOURCES
	u_int8_t *addr;
#endif

	/*
	 * This is the first point where we can validate the buffer size,
	 * because we know all three settings have been configured (file size,
	 * buffer size and the in-memory flag).
	 */
	if ((ret =
	   __log_check_sizes(dbenv, dbenv->lg_size, dbenv->lg_bsize)) != 0)
		return (ret);

	if ((ret = __db_shalloc(&dblp->reginfo,
	    sizeof(*lp), MUTEX_ALIGN, &dblp->reginfo.primary)) != 0)
		goto mem_err;
	dblp->reginfo.rp->primary =
	    R_OFFSET(&dblp->reginfo, dblp->reginfo.primary);
	lp = dblp->reginfo.primary;
	memset(lp, 0, sizeof(*lp));

	lp->fid_max = 0;
	SH_TAILQ_INIT(&lp->fq);
	lp->free_fid_stack = INVALID_ROFF;
	lp->free_fids = lp->free_fids_alloced = 0;

	/* Initialize LOG LSNs. */
	INIT_LSN(lp->lsn);
	INIT_LSN(lp->t_lsn);

	/*
	 * It's possible to be waiting for an LSN of [1][0], if a replication
	 * client gets the first log record out of order.  An LSN of [0][0]
	 * signifies that we're not waiting.
	 */
	ZERO_LSN(lp->waiting_lsn);

	/*
	 * Log makes note of the fact that it ran into a checkpoint on
	 * startup if it did so, as a recovery optimization.  A zero
	 * LSN signifies that it hasn't found one [yet].
	 */
	ZERO_LSN(lp->cached_ckp_lsn);

#ifdef  HAVE_MUTEX_SYSTEM_RESOURCES
	/* Allocate room for the log maintenance info and initialize it. */
	if ((ret = __db_shalloc(&dblp->reginfo,
	    sizeof(REGMAINT) + LG_MAINT_SIZE, 0, &addr)) != 0)
		goto mem_err;
	__db_maintinit(&dblp->reginfo, addr, LG_MAINT_SIZE);
	lp->maint_off = R_OFFSET(&dblp->reginfo, addr);
#endif

	if ((ret = __db_mutex_setup(dbenv, &dblp->reginfo, &lp->fq_mutex,
	    MUTEX_NO_RLOCK)) != 0)
		return (ret);

	/*
	 * We must create a place for the flush mutex separately; mutexes have
	 * to be aligned to MUTEX_ALIGN, and the only way to guarantee that is
	 * to make sure they're at the beginning of a shalloc'ed chunk.
	 */
	if ((ret = __db_shalloc(&dblp->reginfo,
	    sizeof(DB_MUTEX), MUTEX_ALIGN, &flush_mutexp)) != 0)
		goto mem_err;
	if ((ret = __db_mutex_setup(dbenv, &dblp->reginfo, flush_mutexp,
	    MUTEX_NO_RLOCK)) != 0)
		return (ret);
	lp->flush_mutex_off = R_OFFSET(&dblp->reginfo, flush_mutexp);

	/* Initialize the buffer. */
	if ((ret = __db_shalloc(&dblp->reginfo, dbenv->lg_bsize, 0, &p)) != 0) {
mem_err:	__db_err(dbenv, "Unable to allocate memory for the log buffer");
		return (ret);
	}
	lp->regionmax = dbenv->lg_regionmax;
	lp->buffer_off = R_OFFSET(&dblp->reginfo, p);
	lp->buffer_size = dbenv->lg_bsize;
	lp->log_size = lp->log_nsize = dbenv->lg_size;

	/* Initialize the commit Queue. */
	SH_TAILQ_INIT(&lp->free_commits);
	SH_TAILQ_INIT(&lp->commits);
	lp->ncommit = 0;

	/* Initialize the logfiles list for in-memory logs. */
	SH_TAILQ_INIT(&lp->logfiles);
	SH_TAILQ_INIT(&lp->free_logfiles);

	/*
	 * Fill in the log's persistent header.  Don't fill in the log file
	 * sizes, as they may change at any time and so have to be filled in
	 * as each log file is created.
	 */
	lp->persist.magic = DB_LOGMAGIC;
	lp->persist.version = DB_LOGVERSION;
	lp->persist.mode = (u_int32_t)dbenv->db_mode;

	/* Migrate persistent flags from the DB_ENV into the region. */
	if (F_ISSET(dbenv, DB_ENV_LOG_AUTOREMOVE))
		lp->db_log_autoremove = 1;
	if (F_ISSET(dbenv, DB_ENV_LOG_INMEMORY))
		lp->db_log_inmemory = 1;

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
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);
	F_SET(logc, DB_LOG_LOCKED);
	memset(&dbt, 0, sizeof(dbt));
	if ((ret = __log_c_get(logc, &lsn, &dbt, DB_SET)) != 0)
		goto err;

	/*
	 * Read to the end of the file.  This may fail at some point, so
	 * turn off error messages.
	 */
	F_SET(logc, DB_LOG_SILENT_ERR);
	while (__log_c_get(logc, &lsn, &dbt, DB_NEXT) == 0) {
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
	lp->a_off = 0;
	lp->b_off = 0;
	lp->w_off = lp->lsn.offset;

skipsearch:
	if (FLD_ISSET(dbenv->verbose, DB_VERB_RECOVERY))
		__db_msg(dbenv,
		    "Finding last valid log LSN: file: %lu offset %lu",
		    (u_long)lp->lsn.file, (u_long)lp->lsn.offset);

err:	if (logc != NULL)
		(void)__log_c_close(logc);

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
	LOG *lp;
	logfile_validity logval_status, status;
	struct __db_filestart *filestart;
	u_int32_t clv, logval;
	int cnt, fcnt, ret;
	const char *dir;
	char *c, **names, *p, *q, savech;

	dbenv = dblp->dbenv;
	lp = dblp->reginfo.primary;
	logval_status = status = DB_LV_NONEXISTENT;

	/* Return a value of 0 as the log file number on failure. */
	*valp = 0;

	if (lp->db_log_inmemory) {
		filestart = find_first ?
		    SH_TAILQ_FIRST(&lp->logfiles, __db_filestart) :
		    SH_TAILQ_LAST(&lp->logfiles, links, __db_filestart);
		if (filestart != NULL) {
			*valp = filestart->file;
			logval_status = DB_LV_NORMAL;
		}
		*statusp = logval_status;
		return (0);
	}

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
		clv = (u_int32_t)atol(names[cnt] + (sizeof(LFPREFIX) - 1));

		/*
		 * If searching for the first log file, we want to return the
		 * oldest log file we can read, or, if no readable log files
		 * exist, the newest log file we can't read (the crossover
		 * point between the old and new versions of the log file).
		 *
		 * If we're searching for the last log file, we want to return
		 * the newest log file, period.
		 *
		 * Readable log files should never precede unreadable log
		 * files, that would mean the admin seriously screwed up.
		 */
		if (find_first) {
			if (logval != 0 &&
			    status != DB_LV_OLD_UNREADABLE && clv > logval)
				continue;
		} else
			if (logval != 0 && clv < logval)
				continue;

		if ((ret = __log_valid(dblp, clv, 1, NULL, 0, &status)) != 0) {
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
 * PUBLIC: int __log_valid __P((DB_LOG *, u_int32_t, int,
 * PUBLIC:     DB_FH **, u_int32_t, logfile_validity *));
 */
int
__log_valid(dblp, number, set_persist, fhpp, flags, statusp)
	DB_LOG *dblp;
	u_int32_t number;
	int set_persist;
	DB_FH **fhpp;
	u_int32_t flags;
	logfile_validity *statusp;
{
	DB_CIPHER *db_cipher;
	DB_ENV *dbenv;
	DB_FH *fhp;
	HDR *hdr;
	LOG *lp;
	LOGP *persist;
	logfile_validity status;
	size_t hdrsize, nr, recsize;
	int is_hmac, ret;
	u_int8_t *tmp;
	char *fname;

	dbenv = dblp->dbenv;
	db_cipher = dbenv->crypto_handle;
	fhp = NULL;
	persist = NULL;
	status = DB_LV_NORMAL;
	tmp = NULL;

	/* Return the file handle to our caller, on request */
	if (fhpp != NULL)
		*fhpp = NULL;

	if (flags == 0)
		flags = DB_OSO_RDONLY | DB_OSO_SEQ;
	/* Try to open the log file. */
	if ((ret = __log_name(dblp, number, &fname, &fhp, flags)) != 0) {
		__os_free(dbenv, fname);
		return (ret);
	}

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
		goto err;

	hdr = (HDR *)tmp;
	persist = (LOGP *)(tmp + hdrsize);
	/*
	 * Try to read the header.  This can fail if the log is truncated, or
	 * if we find a preallocated log file where the header has not yet been
	 * written, so we need to check whether the header is zero-filled.
	 */
	if ((ret = __os_read(dbenv, fhp, tmp, recsize + hdrsize, &nr)) != 0 ||
	    nr != recsize + hdrsize ||
	    (hdr->len == 0 && persist->magic == 0 && persist->log_size == 0)) {
		if (ret == 0)
			status = DB_LV_INCOMPLETE;
		else
			/*
			 * The error was a fatal read error, not just an
			 * incompletely initialized log file.
			 */
			__db_err(dbenv, "Ignoring log file: %s: %s",
			    fname, db_strerror(ret));
		goto err;
	}

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
	 * Set our status code to indicate whether the log file belongs to an
	 * unreadable or readable old version; leave it alone if and only if
	 * the log file version is the current one.
	 */
	if (persist->version > DB_LOGVERSION) {
		/* This is a fatal error--the log file is newer than DB. */
		__db_err(dbenv,
		    "Unacceptable log file %s: unsupported log version %lu",
		    fname, (u_long)persist->version);
		ret = EINVAL;
		goto err;
	} else if (persist->version < DB_LOGOLDVER) {
		status = DB_LV_OLD_UNREADABLE;
		/* This is a non-fatal error, but give some feedback. */
		__db_err(dbenv,
		    "Skipping log file %s: historic log version %lu",
		    fname, (u_long)persist->version);
		/*
		 * We don't want to set persistent info based on an unreadable
		 * region, so jump to "err".
		 */
		goto err;
	} else if (persist->version < DB_LOGVERSION)
		status = DB_LV_OLD_READABLE;

	/*
	 * Only if we have a current log do we verify the checksum.  We could
	 * not check the checksum before checking the magic and version because
	 * old log headers put the length and checksum in a different location.
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
	 * Override the current log file size.
	 *
	 * XXX
	 * Always use the persistent header's mode, regardless of what was set
	 * in the current environment.  We've always done it this way, but it's
	 * probably a bug -- I can't think of a way not-changing the mode would
	 * be a problem, though.
	 */
	if (set_persist) {
		lp = dblp->reginfo.primary;
		lp->log_size = persist->log_size;
		lp->persist.mode = persist->mode;
	}

err:	if (fname != NULL)
		__os_free(dbenv, fname);
	if (ret == 0 && fhpp != NULL)
		*fhpp = fhp;
	else
		/* Must close on error or if we only used it locally. */
		(void)__os_closehandle(dbenv, fhp);
	if (tmp != NULL)
		__os_free(dbenv, tmp);

	*statusp = status;

	return (ret);
}

/*
 * __log_dbenv_refresh --
 *	Clean up after the log system on a close or failed open.
 *
 * PUBLIC: int __log_dbenv_refresh __P((DB_ENV *));
 */
int
__log_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	LOG *lp;
	REGINFO *reginfo;
	int ret, t_ret;

	dblp = dbenv->lg_handle;
	reginfo = &dblp->reginfo;
	lp = reginfo->primary;

	/* We may have opened files as part of XA; if so, close them. */
	F_SET(dblp, DBLOG_RECOVER);
	ret = __dbreg_close_files(dbenv);

	/*
	 * If a private region, return the memory to the heap.  Not needed for
	 * filesystem-backed or system shared memory regions, that memory isn't
	 * owned by any particular process.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		/* Discard the flush mutex. */
		__db_shalloc_free(reginfo,
		    R_ADDR(reginfo, lp->flush_mutex_off));

		/* Discard the buffer. */
		__db_shalloc_free(reginfo, R_ADDR(reginfo, lp->buffer_off));

		/* Discard stack of free file IDs. */
		if (lp->free_fid_stack != INVALID_ROFF)
			__db_shalloc_free(reginfo,
			    R_ADDR(reginfo, lp->free_fid_stack));
	}

	/* Discard the per-thread lock. */
	if (dblp->mutexp != NULL)
		__db_mutex_free(dbenv, reginfo, dblp->mutexp);

	/* Detach from the region. */
	if ((t_ret = __db_r_detach(dbenv, reginfo, 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Close open files, release allocated memory. */
	if (dblp->lfhp != NULL) {
		if ((t_ret =
		    __os_closehandle(dbenv, dblp->lfhp)) != 0 && ret == 0)
			ret = t_ret;
		dblp->lfhp = NULL;
	}
	if (dblp->dbentry != NULL)
		__os_free(dbenv, dblp->dbentry);

	__os_free(dbenv, dblp);

	dbenv->lg_handle = NULL;
	return (ret);
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
	/*
	 * This routine is called in two cases: when discarding the mutexes
	 * from a previous Berkeley DB run, during recovery, and two, when
	 * discarding the mutexes as we shut down the database environment.
	 * In the latter case, we also need to discard shared memory segments,
	 * this is the last time we use them, and the last region-specific
	 * call we make.
	 */
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	LOG *lp;

	lp = R_ADDR(infop, infop->rp->primary);

	/* Destroy mutexes. */
	__db_shlocks_destroy(infop, R_ADDR(infop, lp->maint_off));
	if (infop->primary != NULL && F_ISSET(dbenv, DB_ENV_PRIVATE))
		__db_shalloc_free(infop, R_ADDR(infop, lp->maint_off));
#endif
	if (infop->primary != NULL && F_ISSET(dbenv, DB_ENV_PRIVATE))
		__db_shalloc_free(infop, infop->primary);
}

/*
 * __log_vtruncate
 *	This is a virtual truncate.  We set up the log indicators to
 * make everyone believe that the given record is the last one in the
 * log.  Returns with the next valid LSN (i.e., the LSN of the next
 * record to be written). This is used in replication to discard records
 * in the log file that do not agree with the master.
 *
 * PUBLIC: int __log_vtruncate __P((DB_ENV *, DB_LSN *, DB_LSN *, DB_LSN *));
 */
int
__log_vtruncate(dbenv, lsn, ckplsn, trunclsn)
	DB_ENV *dbenv;
	DB_LSN *lsn, *ckplsn, *trunclsn;
{
	DBT log_dbt;
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN end_lsn;
	DB_MUTEX *flush_mutexp;
	LOG *lp;
	u_int32_t bytes, c_len;
	int ret, t_ret;

	/* Need to find out the length of this soon-to-be-last record. */
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);
	memset(&log_dbt, 0, sizeof(log_dbt));
	ret = __log_c_get(logc, lsn, &log_dbt, DB_SET);
	c_len = logc->c_len;
	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		return (ret);

	/* Now do the truncate. */
	dblp = (DB_LOG *)dbenv->lg_handle;
	lp = (LOG *)dblp->reginfo.primary;

	R_LOCK(dbenv, &dblp->reginfo);

	/*
	 * Flush the log so we can simply initialize the in-memory buffer
	 * after the truncate.
	 */
	if ((ret = __log_flush_int(dblp, NULL, 0)) != 0)
		goto err;

	end_lsn = lp->lsn;
	lp->lsn = *lsn;
	lp->len = c_len;
	lp->lsn.offset += lp->len;

	if (lp->db_log_inmemory &&
	    (ret = __log_inmem_lsnoff(dblp, &lp->lsn, &lp->b_off)) != 0)
		goto err;

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
			    ((lp->lsn.file - ckplsn->file) - 1);
		bytes += lp->lsn.offset;
	} else
		bytes = lp->lsn.offset - ckplsn->offset;

	lp->stat.st_wc_mbytes += bytes / MEGABYTE;
	lp->stat.st_wc_bytes += bytes % MEGABYTE;

	/*
	 * If the saved lsn is greater than our new end of log, reset it
	 * to our current end of log.
	 */
	flush_mutexp = R_ADDR(&dblp->reginfo, lp->flush_mutex_off);
	MUTEX_LOCK(dbenv, flush_mutexp);
	if (log_compare(&lp->s_lsn, lsn) > 0)
		lp->s_lsn = lp->lsn;
	MUTEX_UNLOCK(dbenv, flush_mutexp);

	/* Initialize the in-region buffer to a pristine state. */
	ZERO_LSN(lp->f_lsn);
	lp->w_off = lp->lsn.offset;

	if (trunclsn != NULL)
		*trunclsn = lp->lsn;

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
 * PUBLIC: int __log_is_outdated __P((DB_ENV *, u_int32_t, int *));
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
	struct __db_filestart *filestart;

	dblp = dbenv->lg_handle;

	if (F_ISSET(dbenv, DB_ENV_LOG_INMEMORY)) {
		R_LOCK(dbenv, &dblp->reginfo);
		lp = (LOG *)dblp->reginfo.primary;
		filestart = SH_TAILQ_FIRST(&lp->logfiles, __db_filestart);
		*outdatedp = (fnum < filestart->file);
		R_UNLOCK(dbenv, &dblp->reginfo);
		return (0);
	}

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
	DB_FH *fhp;
	DB_LOG *dblp;
	LOG *lp;
	struct __db_filestart *filestart, *nextstart;
	size_t nbytes, len, nw;
	u_int32_t fn, mbytes, bytes;
	u_int8_t buf[4096];
	int ret;
	char *fname;

	dblp = dbenv->lg_handle;
	DB_ASSERT(log_compare(from_lsn, to_lsn) <= 0);
	if (log_compare(from_lsn, to_lsn) > 0) {
		__db_err(dbenv,
		    "Warning: truncating to point beyond end of log");
		return (0);
	}

	lp = (LOG *)dblp->reginfo.primary;
	if (lp->db_log_inmemory) {
		/*
		 * Remove the first file if it is invalidated by this write.
		 * Log records can't be bigger than a file, so we only need to
		 * check the first file.
		 */
		for (filestart = SH_TAILQ_FIRST(&lp->logfiles, __db_filestart);
		    filestart != NULL && from_lsn->file < filestart->file;
		    filestart = nextstart) {
			nextstart = SH_TAILQ_NEXT(filestart,
			    links, __db_filestart);
			SH_TAILQ_REMOVE(&lp->logfiles, filestart,
			    links, __db_filestart);
			SH_TAILQ_INSERT_HEAD(&lp->free_logfiles, filestart,
			    links, __db_filestart);
		}

		return (0);
	}

	/* Close any open file handles so unlinks don't fail. */
	if (dblp->lfhp != NULL) {
		(void)__os_closehandle(dbenv, dblp->lfhp);
		dblp->lfhp = NULL;
	}

	/* Throw away any extra log files that we have around. */
	for (fn = from_lsn->file + 1;; fn++) {
		if (__log_name(dblp, fn, &fname, &fhp, DB_OSO_RDONLY) != 0) {
			__os_free(dbenv, fname);
			break;
		}
		(void)__os_closehandle(dbenv, fhp);
		ret = __os_unlink(dbenv, fname);
		__os_free(dbenv, fname);
		if (ret != 0)
			return (ret);
	}

	/* We removed some log files; have to 0 to end of file. */
	if ((ret =
	    __log_name(dblp, from_lsn->file, &fname, &dblp->lfhp, 0)) != 0)
		return (ret);
	__os_free(dbenv, fname);
	if ((ret = __os_ioinfo(dbenv,
	    NULL, dblp->lfhp, &mbytes, &bytes, NULL)) != 0)
		goto err;
	DB_ASSERT((mbytes * MEGABYTE + bytes) >= from_lsn->offset);
	len = (mbytes * MEGABYTE + bytes) - from_lsn->offset;

	memset(buf, 0, sizeof(buf));

	/* Initialize the write position. */
	if ((ret = __os_seek(dbenv,
	    dblp->lfhp, 0, 0, from_lsn->offset, 0, DB_OS_SEEK_SET)) != 0)
		goto err;

	while (len > 0) {
		nbytes = len > sizeof(buf) ? sizeof(buf) : len;
		if ((ret =
		    __os_write(dbenv, dblp->lfhp, buf, nbytes, &nw)) != 0)
			goto err;
		len -= nbytes;
	}

err:	(void)__os_closehandle(dbenv, dblp->lfhp);
	dblp->lfhp = NULL;

	return (ret);
}

/*
 * __log_inmem_lsnoff --
 *	Find the offset in the buffer of a given LSN.
 *
 * PUBLIC: int __log_inmem_lsnoff __P((DB_LOG *, DB_LSN *, size_t *));
 */
int
__log_inmem_lsnoff(dblp, lsn, offsetp)
	DB_LOG *dblp;
	DB_LSN *lsn;
	size_t *offsetp;
{
	LOG *lp;
	struct __db_filestart *filestart;

	lp = (LOG *)dblp->reginfo.primary;

	SH_TAILQ_FOREACH(filestart, &lp->logfiles, links, __db_filestart)
		if (filestart->file == lsn->file) {
			*offsetp =
			    (filestart->b_off + lsn->offset) % lp->buffer_size;
			return (0);
		}

	return (DB_NOTFOUND);
}

/*
 * __log_inmem_newfile --
 *	Records the offset of the beginning of a new file in the in-memory
 *	buffer.
 *
 * PUBLIC: int __log_inmem_newfile __P((DB_LOG *, u_int32_t));
 */
int
__log_inmem_newfile(dblp, file)
	DB_LOG *dblp;
	u_int32_t file;
{
	HDR hdr;
	LOG *lp;
	struct __db_filestart *filestart;
	int ret;
#ifdef DIAGNOSTIC
	struct __db_filestart *first, *last;
#endif

	lp = (LOG *)dblp->reginfo.primary;

	/*
	 * We write an empty header at the end of every in-memory log file.
	 * This is used during cursor traversal to indicate when to switch the
	 * LSN to the next file.
	 */
	if (file > 1) {
		memset(&hdr, 0, sizeof(HDR));
		__log_inmem_copyin(dblp, lp->b_off, &hdr, sizeof(HDR));
		lp->b_off = (lp->b_off + sizeof(HDR)) % lp->buffer_size;
	}

	filestart = SH_TAILQ_FIRST(&lp->free_logfiles, __db_filestart);
	if (filestart == NULL) {
		if ((ret = __db_shalloc(&dblp->reginfo,
		    sizeof(struct __db_filestart), 0, &filestart)) != 0)
			return (ret);
		memset(filestart, 0, sizeof(*filestart));
	} else
		SH_TAILQ_REMOVE(&lp->free_logfiles, filestart,
		    links, __db_filestart);

	filestart->file = file;
	filestart->b_off = lp->b_off;

#ifdef DIAGNOSTIC
	first = SH_TAILQ_FIRST(&lp->logfiles, __db_filestart);
	last = SH_TAILQ_LAST(&(lp)->logfiles, links, __db_filestart);

	/* Check that we don't wrap. */
	DB_ASSERT(!first || first == last ||
	    RINGBUF_LEN(lp, first->b_off, lp->b_off) ==
	    RINGBUF_LEN(lp, first->b_off, last->b_off) +
	    RINGBUF_LEN(lp, last->b_off, lp->b_off));
#endif

	SH_TAILQ_INSERT_TAIL(&lp->logfiles, filestart, links);
	return (0);
}

/*
 * __log_inmem_chkspace --
 *	Ensure that the requested amount of space is available in the buffer,
 *	and invalidate the region.
 *      Note: assumes that the region lock is held on entry.
 *
 * PUBLIC: int __log_inmem_chkspace __P((DB_LOG *, size_t));
 */
int
__log_inmem_chkspace(dblp, len)
	DB_LOG *dblp;
	size_t len;
{
	LOG *lp;
	DB_LSN active_lsn, old_active_lsn;
	struct __db_filestart *filestart;

	lp = dblp->reginfo.primary;

	DB_ASSERT(lp->db_log_inmemory);

	/*
	 * Allow room for an extra header so that we don't need to check for
	 * space when switching files.
	 */
	len += sizeof(HDR);

	/*
	 * If transactions are enabled and we're about to fill available space,
	 * update the active LSN and recheck.  If transactions aren't enabled,
	 * don't even bother checking: in that case we can always overwrite old
	 * log records, because we're never going to abort.
	 */
	while (TXN_ON(dblp->dbenv) &&
	    RINGBUF_LEN(lp, lp->b_off, lp->a_off) <= len) {
		old_active_lsn = lp->active_lsn;
		active_lsn = lp->lsn;

		/*
		 * Drop the log region lock so we don't hold it while
		 * taking the transaction region lock.
		 */
		R_UNLOCK(dblp->dbenv, &dblp->reginfo);
		__txn_getactive(dblp->dbenv, &active_lsn);
		R_LOCK(dblp->dbenv, &dblp->reginfo);
		active_lsn.offset = 0;

		/* If we didn't make any progress, give up. */
		if (log_compare(&active_lsn, &old_active_lsn) == 0) {
			__db_err(dblp->dbenv,
      "In-memory log buffer is full (an active transaction spans the buffer)");
			return (DB_LOG_BUFFER_FULL);
		}

		/* Make sure we're moving the region LSN forwards. */
		if (log_compare(&active_lsn, &lp->active_lsn) > 0) {
			lp->active_lsn = active_lsn;
			(void)__log_inmem_lsnoff(dblp, &active_lsn,
			    &lp->a_off);
		}
	}

	/*
	 * Remove the first file if it is invalidated by this write.
	 * Log records can't be bigger than a file, so we only need to
	 * check the first file.
	 */
	filestart = SH_TAILQ_FIRST(&lp->logfiles, __db_filestart);
	if (filestart != NULL &&
	    RINGBUF_LEN(lp, lp->b_off, filestart->b_off) <= len) {
		SH_TAILQ_REMOVE(&lp->logfiles, filestart,
		    links, __db_filestart);
		SH_TAILQ_INSERT_HEAD(&lp->free_logfiles, filestart,
		    links, __db_filestart);
		lp->f_lsn.file = filestart->file + 1;
	}

	return (0);
}

/*
 * __log_inmem_copyout --
 *	Copies the given number of bytes from the buffer -- no checking.
 *      Note: assumes that the region lock is held on entry.
 *
 * PUBLIC: void __log_inmem_copyout __P((DB_LOG *, size_t, void *, size_t));
 */
void
__log_inmem_copyout(dblp, offset, buf, size)
	DB_LOG *dblp;
	size_t offset;
	void *buf;
	size_t size;
{
	LOG *lp;
	size_t nbytes;

	lp = (LOG *)dblp->reginfo.primary;
	nbytes = (offset + size < lp->buffer_size) ?
	    size : lp->buffer_size - offset;
	memcpy(buf, dblp->bufp + offset, nbytes);
	if (nbytes < size)
		memcpy((u_int8_t *)buf + nbytes, dblp->bufp, size - nbytes);
}

/*
 * __log_inmem_copyin --
 *	Copies the given number of bytes into the buffer -- no checking.
 *      Note: assumes that the region lock is held on entry.
 *
 * PUBLIC: void __log_inmem_copyin __P((DB_LOG *, size_t, void *, size_t));
 */
void
__log_inmem_copyin(dblp, offset, buf, size)
	DB_LOG *dblp;
	size_t offset;
	void *buf;
	size_t size;
{
	LOG *lp;
	size_t nbytes;

	lp = (LOG *)dblp->reginfo.primary;
	nbytes = (offset + size < lp->buffer_size) ?
	    size : lp->buffer_size - offset;
	memcpy(dblp->bufp + offset, buf, nbytes);
	if (nbytes < size)
		memcpy(dblp->bufp, (u_int8_t *)buf + nbytes, size - nbytes);
}
