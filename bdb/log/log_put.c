/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log_put.c,v 11.26 2000/11/30 00:58:40 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_page.h"
#include "log.h"
#include "hash.h"
#include "clib_ext.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int __log_fill __P((DB_LOG *, DB_LSN *, void *, u_int32_t));
static int __log_flush __P((DB_LOG *, const DB_LSN *));
static int __log_newfh __P((DB_LOG *));
static int __log_putr __P((DB_LOG *, DB_LSN *, const DBT *, u_int32_t));
static int __log_open_files __P((DB_ENV *));
static int __log_write __P((DB_LOG *, void *, u_int32_t));

/*
 * log_put --
 *	Write a log record.
 */
int
log_put(dbenv, lsn, dbt, flags)
	DB_ENV *dbenv;
	DB_LSN *lsn;
	const DBT *dbt;
	u_int32_t flags;
{
	DB_LOG *dblp;
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_log_put(dbenv, lsn, dbt, flags));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lg_handle, DB_INIT_LOG);

	/* Validate arguments. */
	if (flags != 0 && flags != DB_CHECKPOINT &&
	    flags != DB_CURLSN && flags != DB_FLUSH)
		return (__db_ferr(dbenv, "log_put", 0));

	dblp = dbenv->lg_handle;
	R_LOCK(dbenv, &dblp->reginfo);
	ret = __log_put(dbenv, lsn, dbt, flags);
	R_UNLOCK(dbenv, &dblp->reginfo);
	return (ret);
}

/*
 * __log_put --
 *	Write a log record; internal version.
 *
 * PUBLIC: int __log_put __P((DB_ENV *, DB_LSN *, const DBT *, u_int32_t));
 */
int
__log_put(dbenv, lsn, dbt, flags)
	DB_ENV *dbenv;
	DB_LSN *lsn;
	const DBT *dbt;
	u_int32_t flags;
{
	DBT t;
	DB_LOG *dblp;
	LOG *lp;
	u_int32_t lastoff;
	int ret;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	/*
	 * If the application just wants to know where we are, fill in
	 * the information.  Currently used by the transaction manager
	 * to avoid writing TXN_begin records.
	 */
	if (flags == DB_CURLSN) {
		lsn->file = lp->lsn.file;
		lsn->offset = lp->lsn.offset;
		return (0);
	}

	/* If this information won't fit in the file, swap files. */
	if (lp->lsn.offset + sizeof(HDR) + dbt->size > lp->persist.lg_max) {
		if (sizeof(HDR) +
		    sizeof(LOGP) + dbt->size > lp->persist.lg_max) {
			__db_err(dbenv,
			    "log_put: record larger than maximum file size");
			return (EINVAL);
		}

		/* Flush the log. */
		if ((ret = __log_flush(dblp, NULL)) != 0)
			return (ret);

		/*
		 * Save the last known offset from the previous file, we'll
		 * need it to initialize the persistent header information.
		 */
		lastoff = lp->lsn.offset;

		/* Point the current LSN to the new file. */
		++lp->lsn.file;
		lp->lsn.offset = 0;

		/* Reset the file write offset. */
		lp->w_off = 0;
	} else
		lastoff = 0;

	/* Initialize the LSN information returned to the user. */
	lsn->file = lp->lsn.file;
	lsn->offset = lp->lsn.offset;

	/*
	 * Insert persistent information as the first record in every file.
	 * Note that the previous length is wrong for the very first record
	 * of the log, but that's okay, we check for it during retrieval.
	 */
	if (lp->lsn.offset == 0) {
		t.data = &lp->persist;
		t.size = sizeof(LOGP);
		if ((ret = __log_putr(dblp, lsn,
		    &t, lastoff == 0 ? 0 : lastoff - lp->len)) != 0)
			return (ret);

		/*
		 * Record files open in this log.
		 * If we are recovering then we are in the
		 * process of outputting the files, don't do
		 * it again.
		 */
		if (!F_ISSET(dblp, DBLOG_RECOVER) &&
		    (ret = __log_open_files(dbenv)) != 0)
			return (ret);

		/* Update the LSN information returned to the user. */
		lsn->file = lp->lsn.file;
		lsn->offset = lp->lsn.offset;
	}

	/* Write the application's log record. */
	if ((ret = __log_putr(dblp, lsn, dbt, lp->lsn.offset - lp->len)) != 0)
		return (ret);

	/*
	 * On a checkpoint, we:
	 *	Put out the checkpoint record (above).
	 *	Save the LSN of the checkpoint in the shared region.
	 *	Append the set of file name information into the log.
	 */
	if (flags == DB_CHECKPOINT) {
		lp->chkpt_lsn = *lsn;
		if ((ret = __log_open_files(dbenv)) != 0)
			return (ret);
	}

	/*
	 * On a checkpoint or when flush is requested, we:
	 *	Flush the current buffer contents to disk.
	 *	Sync the log to disk.
	 */
	if (flags == DB_FLUSH || flags == DB_CHECKPOINT)
		if ((ret = __log_flush(dblp, NULL)) != 0)
			return (ret);

	/*
	 * On a checkpoint, we:
	 *	Save the time the checkpoint was written.
	 *	Reset the bytes written since the last checkpoint.
	 */
	if (flags == DB_CHECKPOINT) {
		(void)time(&lp->chkpt);
		lp->stat.st_wc_bytes = lp->stat.st_wc_mbytes = 0;
	}
	return (0);
}

/*
 * __log_putr --
 *	Actually put a record into the log.
 */
static int
__log_putr(dblp, lsn, dbt, prev)
	DB_LOG *dblp;
	DB_LSN *lsn;
	const DBT *dbt;
	u_int32_t prev;
{
	HDR hdr;
	LOG *lp;
	int ret;

	lp = dblp->reginfo.primary;

	/*
	 * Initialize the header.  If we just switched files, lsn.offset will
	 * be 0, and what we really want is the offset of the previous record
	 * in the previous file.  Fortunately, prev holds the value we want.
	 */
	hdr.prev = prev;
	hdr.len = sizeof(HDR) + dbt->size;
	hdr.cksum = __ham_func4(NULL, dbt->data, dbt->size);

	if ((ret = __log_fill(dblp, lsn, &hdr, sizeof(HDR))) != 0)
		return (ret);
	lp->len = sizeof(HDR);
	lp->lsn.offset += sizeof(HDR);

	if ((ret = __log_fill(dblp, lsn, dbt->data, dbt->size)) != 0)
		return (ret);
	lp->len += dbt->size;
	lp->lsn.offset += dbt->size;
	return (0);
}

/*
 * log_flush --
 *	Write all records less than or equal to the specified LSN.
 */
int
log_flush(dbenv, lsn)
	DB_ENV *dbenv;
	const DB_LSN *lsn;
{
	DB_LOG *dblp;
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_log_flush(dbenv, lsn));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lg_handle, DB_INIT_LOG);

	dblp = dbenv->lg_handle;
	R_LOCK(dbenv, &dblp->reginfo);
	ret = __log_flush(dblp, lsn);
	R_UNLOCK(dbenv, &dblp->reginfo);
	return (ret);
}

/*
 * __log_flush --
 *	Write all records less than or equal to the specified LSN; internal
 *	version.
 */
static int
__log_flush(dblp, lsn)
	DB_LOG *dblp;
	const DB_LSN *lsn;
{
	DB_LSN t_lsn;
	LOG *lp;
	int current, ret;

	ret = 0;
	lp = dblp->reginfo.primary;

	/*
	 * If no LSN specified, flush the entire log by setting the flush LSN
	 * to the last LSN written in the log.  Otherwise, check that the LSN
	 * isn't a non-existent record for the log.
	 */
	if (lsn == NULL) {
		t_lsn.file = lp->lsn.file;
		t_lsn.offset = lp->lsn.offset - lp->len;
		lsn = &t_lsn;
	} else
		if (lsn->file > lp->lsn.file ||
		    (lsn->file == lp->lsn.file &&
		    lsn->offset > lp->lsn.offset - lp->len)) {
			__db_err(dblp->dbenv,
			    "log_flush: LSN past current end-of-log");
			return (EINVAL);
		}

	/*
	 * If the LSN is less than or equal to the last-sync'd LSN, we're done.
	 * Note, the last-sync LSN saved in s_lsn is the LSN of the first byte
	 * after the byte we absolutely know was written to disk, so the test
	 * is <, not <=.
	 */
	if (lsn->file < lp->s_lsn.file ||
	    (lsn->file == lp->s_lsn.file && lsn->offset < lp->s_lsn.offset))
		return (0);

	/*
	 * We may need to write the current buffer.  We have to write the
	 * current buffer if the flush LSN is greater than or equal to the
	 * buffer's starting LSN.
	 */
	current = 0;
	if (lp->b_off != 0 && log_compare(lsn, &lp->f_lsn) >= 0) {
		if ((ret = __log_write(dblp, dblp->bufp, lp->b_off)) != 0)
			return (ret);

		lp->b_off = 0;
		current = 1;
	}

	/*
	 * It's possible that this thread may never have written to this log
	 * file.  Acquire a file descriptor if we don't already have one.
	 * One last check -- if we're not writing anything from the current
	 * buffer, don't bother.  We have nothing to write and nothing to
	 * sync.
	 */
	if (dblp->lfname != lp->lsn.file) {
		if (!current)
			return (0);
		if ((ret = __log_newfh(dblp)) != 0)
			return (ret);
	}

	/* Sync all writes to disk. */
	if ((ret = __os_fsync(dblp->dbenv, &dblp->lfh)) != 0)
		return (__db_panic(dblp->dbenv, ret));
	++lp->stat.st_scount;

	/* Set the last-synced LSN, using the on-disk write offset. */
	lp->s_lsn.file = lp->f_lsn.file;
	lp->s_lsn.offset = lp->w_off;

	return (0);
}

/*
 * __log_fill --
 *	Write information into the log.
 */
static int
__log_fill(dblp, lsn, addr, len)
	DB_LOG *dblp;
	DB_LSN *lsn;
	void *addr;
	u_int32_t len;
{
	LOG *lp;
	u_int32_t bsize, nrec;
	size_t nw, remain;
	int ret;

	lp = dblp->reginfo.primary;
	bsize = lp->buffer_size;

	while (len > 0) {			/* Copy out the data. */
		/*
		 * If we're beginning a new buffer, note the user LSN to which
		 * the first byte of the buffer belongs.  We have to know this
		 * when flushing the buffer so that we know if the in-memory
		 * buffer needs to be flushed.
		 */
		if (lp->b_off == 0)
			lp->f_lsn = *lsn;

		/*
		 * If we're on a buffer boundary and the data is big enough,
		 * copy as many records as we can directly from the data.
		 */
		if (lp->b_off == 0 && len >= bsize) {
			nrec = len / bsize;
			if ((ret = __log_write(dblp, addr, nrec * bsize)) != 0)
				return (ret);
			addr = (u_int8_t *)addr + nrec * bsize;
			len -= nrec * bsize;
			++lp->stat.st_wcount_fill;
			continue;
		}

		/* Figure out how many bytes we can copy this time. */
		remain = bsize - lp->b_off;
		nw = remain > len ? len : remain;
		memcpy(dblp->bufp + lp->b_off, addr, nw);
		addr = (u_int8_t *)addr + nw;
		len -= nw;
		lp->b_off += nw;

		/* If we fill the buffer, flush it. */
		if (lp->b_off == bsize) {
			if ((ret = __log_write(dblp, dblp->bufp, bsize)) != 0)
				return (ret);
			lp->b_off = 0;
			++lp->stat.st_wcount_fill;
		}
	}
	return (0);
}

/*
 * __log_write --
 *	Write the log buffer to disk.
 */
static int
__log_write(dblp, addr, len)
	DB_LOG *dblp;
	void *addr;
	u_int32_t len;
{
	LOG *lp;
	size_t nw;
	int ret;

	/*
	 * If we haven't opened the log file yet or the current one
	 * has changed, acquire a new log file.
	 */
	lp = dblp->reginfo.primary;
	if (!F_ISSET(&dblp->lfh, DB_FH_VALID) || dblp->lfname != lp->lsn.file)
		if ((ret = __log_newfh(dblp)) != 0)
			return (ret);

	/*
	 * Seek to the offset in the file (someone may have written it
	 * since we last did).
	 */
	if ((ret =
	    __os_seek(dblp->dbenv,
	    &dblp->lfh, 0, 0, lp->w_off, 0, DB_OS_SEEK_SET)) != 0 ||
	    (ret = __os_write(dblp->dbenv, &dblp->lfh, addr, len, &nw)) != 0)
		return (__db_panic(dblp->dbenv, ret));
	if (nw != len) {
		__db_err(dblp->dbenv, "Short write while writing log");
		return (EIO);
	}

	/* Reset the buffer offset and update the seek offset. */
	lp->w_off += len;

	/* Update written statistics. */
	if ((lp->stat.st_w_bytes += len) >= MEGABYTE) {
		lp->stat.st_w_bytes -= MEGABYTE;
		++lp->stat.st_w_mbytes;
	}
	if ((lp->stat.st_wc_bytes += len) >= MEGABYTE) {
		lp->stat.st_wc_bytes -= MEGABYTE;
		++lp->stat.st_wc_mbytes;
	}
	++lp->stat.st_wcount;

	return (0);
}

/*
 * log_file --
 *	Map a DB_LSN to a file name.
 */
int
log_file(dbenv, lsn, namep, len)
	DB_ENV *dbenv;
	const DB_LSN *lsn;
	char *namep;
	size_t len;
{
	DB_LOG *dblp;
	int ret;
	char *name;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_log_file(dbenv, lsn, namep, len));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lg_handle, DB_INIT_LOG);

	dblp = dbenv->lg_handle;
	R_LOCK(dbenv, &dblp->reginfo);
	ret = __log_name(dblp, lsn->file, &name, NULL, 0);
	R_UNLOCK(dbenv, &dblp->reginfo);
	if (ret != 0)
		return (ret);

	/* Check to make sure there's enough room and copy the name. */
	if (len < strlen(name) + 1) {
		*namep = '\0';
		__db_err(dbenv, "log_file: name buffer is too short");
		return (EINVAL);
	}
	(void)strcpy(namep, name);
	__os_freestr(name);

	return (0);
}

/*
 * __log_newfh --
 *	Acquire a file handle for the current log file.
 */
static int
__log_newfh(dblp)
	DB_LOG *dblp;
{
	LOG *lp;
	int ret;
	char *name;

	/* Close any previous file descriptor. */
	if (F_ISSET(&dblp->lfh, DB_FH_VALID))
		(void)__os_closehandle(&dblp->lfh);

	/* Get the path of the new file and open it. */
	lp = dblp->reginfo.primary;
	dblp->lfname = lp->lsn.file;

	/*
	 * Adding DB_OSO_LOG to the flags may add additional platform-specific
	 * optimizations.  On WinNT, the logfile is preallocated, which may
	 * have a time penalty at startup, but have better overall throughput.
	 * We are not certain that this works reliably, so enable at your own
	 * risk.
	 *
	 * XXX:
	 * Initialize the log file size.  This is a hack to push the log's
	 * maximum size down into the Windows __os_open routine, because it
	 * wants to pre-allocate it.
	 */
	dblp->lfh.log_size = dblp->dbenv->lg_max;
	if ((ret = __log_name(dblp, dblp->lfname,
	    &name, &dblp->lfh,
	    DB_OSO_CREATE |/* DB_OSO_LOG |*/ DB_OSO_SEQ)) != 0)
		__db_err(dblp->dbenv,
		    "log_put: %s: %s", name, db_strerror(ret));

	__os_freestr(name);
	return (ret);
}

/*
 * __log_name --
 *	Return the log name for a particular file, and optionally open it.
 *
 * PUBLIC: int __log_name __P((DB_LOG *,
 * PUBLIC:     u_int32_t, char **, DB_FH *, u_int32_t));
 */
int
__log_name(dblp, filenumber, namep, fhp, flags)
	DB_LOG *dblp;
	u_int32_t filenumber, flags;
	char **namep;
	DB_FH *fhp;
{
	LOG *lp;
	int ret;
	char *oname;
	char old[sizeof(LFPREFIX) + 5 + 20], new[sizeof(LFPREFIX) + 10 + 20];

	lp = dblp->reginfo.primary;

	/*
	 * !!!
	 * The semantics of this routine are bizarre.
	 *
	 * The reason for all of this is that we need a place where we can
	 * intercept requests for log files, and, if appropriate, check for
	 * both the old-style and new-style log file names.  The trick is
	 * that all callers of this routine that are opening the log file
	 * read-only want to use an old-style file name if they can't find
	 * a match using a new-style name.  The only down-side is that some
	 * callers may check for the old-style when they really don't need
	 * to, but that shouldn't mess up anything, and we only check for
	 * the old-style name when we've already failed to find a new-style
	 * one.
	 *
	 * Create a new-style file name, and if we're not going to open the
	 * file, return regardless.
	 */
	(void)snprintf(new, sizeof(new), LFNAME, filenumber);
	if ((ret = __db_appname(dblp->dbenv,
	    DB_APP_LOG, NULL, new, 0, NULL, namep)) != 0 || fhp == NULL)
		return (ret);

	/* Open the new-style file -- if we succeed, we're done. */
	if ((ret = __os_open(dblp->dbenv,
	    *namep, flags, lp->persist.mode, fhp)) == 0)
		return (0);

	/*
	 * The open failed... if the DB_RDONLY flag isn't set, we're done,
	 * the caller isn't interested in old-style files.
	 */
	if (!LF_ISSET(DB_OSO_RDONLY)) {
		__db_err(dblp->dbenv,
		    "%s: log file open failed: %s", *namep, db_strerror(ret));
		return (__db_panic(dblp->dbenv, ret));
	}

	/* Create an old-style file name. */
	(void)snprintf(old, sizeof(old), LFNAME_V1, filenumber);
	if ((ret = __db_appname(dblp->dbenv,
	    DB_APP_LOG, NULL, old, 0, NULL, &oname)) != 0)
		goto err;

	/*
	 * Open the old-style file -- if we succeed, we're done.  Free the
	 * space allocated for the new-style name and return the old-style
	 * name to the caller.
	 */
	if ((ret = __os_open(dblp->dbenv,
	    oname, flags, lp->persist.mode, fhp)) == 0) {
		__os_freestr(*namep);
		*namep = oname;
		return (0);
	}

	/*
	 * Couldn't find either style of name -- return the new-style name
	 * for the caller's error message.  If it's an old-style name that's
	 * actually missing we're going to confuse the user with the error
	 * message, but that implies that not only were we looking for an
	 * old-style name, but we expected it to exist and we weren't just
	 * looking for any log file.  That's not a likely error.
	 */
err:	__os_freestr(oname);
	return (ret);
}

static int
__log_open_files(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	DB_LSN r_unused;
	DBT fid_dbt, t;
	FNAME *fnp;
	LOG *lp;
	int ret;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
		if (fnp->ref == 0)	/* Entry not in use. */
			continue;
		if (fnp->name_off != INVALID_ROFF) {
			memset(&t, 0, sizeof(t));
			t.data = R_ADDR(&dblp->reginfo, fnp->name_off);
			t.size = strlen(t.data) + 1;
		}
		memset(&fid_dbt, 0, sizeof(fid_dbt));
		fid_dbt.data = fnp->ufid;
		fid_dbt.size = DB_FILE_ID_LEN;
		/*
		 * Output LOG_CHECKPOINT records which will be
		 * processed during the OPENFILES pass of recovery.
		 * At the end of recovery we want to output the
		 * files that were open so that a future recovery
		 * run will have the correct files open during
		 * a backward pass.  For this we output LOG_CLOSE
		 * records so that the files will be closed on
		 * the forward pass.
		 */
		if ((ret = __log_register_log(dbenv,
		    NULL, &r_unused, 0,
		    F_ISSET(dblp, DBLOG_RECOVER) ? LOG_CLOSE : LOG_CHECKPOINT,
		    fnp->name_off == INVALID_ROFF ? NULL : &t,
		    &fid_dbt, fnp->id, fnp->s_type, fnp->meta_pgno)) != 0)
			return (ret);
	}
	return (0);
}
