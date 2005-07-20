/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: log_get.c,v 11.110 2004/09/17 22:00:31 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/db_page.h"
#include "dbinc/hmac.h"
#include "dbinc/log.h"
#include "dbinc/hash.h"

typedef enum { L_ALREADY, L_ACQUIRED, L_NONE } RLOCK;

static int __log_c_close_pp __P((DB_LOGC *, u_int32_t));
static int __log_c_get_pp __P((DB_LOGC *, DB_LSN *, DBT *, u_int32_t));
static int __log_c_get_int __P((DB_LOGC *, DB_LSN *, DBT *, u_int32_t));
static int __log_c_hdrchk __P((DB_LOGC *, DB_LSN *, HDR *, int *));
static int __log_c_incursor __P((DB_LOGC *, DB_LSN *, HDR *, u_int8_t **));
static int __log_c_inregion __P((DB_LOGC *,
	       DB_LSN *, RLOCK *, DB_LSN *, HDR *, u_int8_t **));
static int __log_c_io __P((DB_LOGC *,
	       u_int32_t, u_int32_t, void *, size_t *, int *));
static int __log_c_ondisk __P((DB_LOGC *,
	       DB_LSN *, DB_LSN *, u_int32_t, HDR *, u_int8_t **, int *));
static int __log_c_set_maxrec __P((DB_LOGC *, char *));
static int __log_c_shortread __P((DB_LOGC *, DB_LSN *, int));

/*
 * __log_cursor_pp --
 *	DB_ENV->log_cursor
 *
 * PUBLIC: int __log_cursor_pp __P((DB_ENV *, DB_LOGC **, u_int32_t));
 */
int
__log_cursor_pp(dbenv, logcp, flags)
	DB_ENV *dbenv;
	DB_LOGC **logcp;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_cursor", DB_INIT_LOG);

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB_ENV->log_cursor", flags, 0)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __log_cursor(dbenv, logcp);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __log_cursor --
 *	Create a log cursor.
 *
 * PUBLIC: int __log_cursor __P((DB_ENV *, DB_LOGC **));
 */
int
__log_cursor(dbenv, logcp)
	DB_ENV *dbenv;
	DB_LOGC **logcp;
{
	DB_LOGC *logc;
	int ret;

	*logcp = NULL;

	/* Allocate memory for the cursor. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_LOGC), &logc)) != 0)
		return (ret);

	logc->bp_size = DB_LOGC_BUF_SIZE;
	/*
	 * Set this to something positive.
	 */
	logc->bp_maxrec = MEGABYTE;
	if ((ret = __os_malloc(dbenv, logc->bp_size, &logc->bp)) != 0) {
		__os_free(dbenv, logc);
		return (ret);
	}

	logc->dbenv = dbenv;
	logc->close = __log_c_close_pp;
	logc->get = __log_c_get_pp;

	*logcp = logc;
	return (0);
}

/*
 * __log_c_close_pp --
 *	DB_LOGC->close pre/post processing.
 */
static int
__log_c_close_pp(logc, flags)
	DB_LOGC *logc;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int rep_check, ret;

	dbenv = logc->dbenv;

	PANIC_CHECK(dbenv);
	if ((ret = __db_fchk(dbenv, "DB_LOGC->close", flags, 0)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __log_c_close(logc);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __log_c_close --
 *	DB_LOGC->close.
 *
 * PUBLIC: int __log_c_close __P((DB_LOGC *));
 */
int
__log_c_close(logc)
	DB_LOGC *logc;
{
	DB_ENV *dbenv;

	dbenv = logc->dbenv;

	if (logc->c_fhp != NULL) {
		(void)__os_closehandle(dbenv, logc->c_fhp);
		logc->c_fhp = NULL;
	}

	if (logc->c_dbt.data != NULL)
		__os_free(dbenv, logc->c_dbt.data);

	__os_free(dbenv, logc->bp);
	__os_free(dbenv, logc);

	return (0);
}

/*
 * __log_c_get_pp --
 *	DB_LOGC->get pre/post processing.
 */
static int
__log_c_get_pp(logc, alsn, dbt, flags)
	DB_LOGC *logc;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int rep_check, ret;

	dbenv = logc->dbenv;

	PANIC_CHECK(dbenv);

	/* Validate arguments. */
	switch (flags) {
	case DB_CURRENT:
	case DB_FIRST:
	case DB_LAST:
	case DB_NEXT:
	case DB_PREV:
		break;
	case DB_SET:
		if (IS_ZERO_LSN(*alsn)) {
			__db_err(dbenv, "DB_LOGC->get: invalid LSN: %lu/%lu",
			    (u_long)alsn->file, (u_long)alsn->offset);
			return (EINVAL);
		}
		break;
	default:
		return (__db_ferr(dbenv, "DB_LOGC->get", 1));
	}

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __log_c_get(logc, alsn, dbt, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __log_c_get --
 *	DB_LOGC->get.
 *
 * PUBLIC: int __log_c_get __P((DB_LOGC *, DB_LSN *, DBT *, u_int32_t));
 */
int
__log_c_get(logc, alsn, dbt, flags)
	DB_LOGC *logc;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_LSN saved_lsn;
	int ret;

	dbenv = logc->dbenv;

	/*
	 * On error, we take care not to overwrite the caller's LSN.  This
	 * is because callers looking for the end of the log loop using the
	 * DB_NEXT flag, and expect to take the last successful lsn out of
	 * the passed-in structure after DB_LOGC->get fails with DB_NOTFOUND.
	 *
	 * !!!
	 * This line is often flagged an uninitialized memory read during a
	 * Purify or similar tool run, as the application didn't initialize
	 * *alsn.  If the application isn't setting the DB_SET flag, there is
	 * no reason it should have initialized *alsn, but we can't know that
	 * and we want to make sure we never overwrite whatever the application
	 * put in there.
	 */
	saved_lsn = *alsn;

	/*
	 * If we get one of the log's header records as a result of doing a
	 * DB_FIRST, DB_NEXT, DB_LAST or DB_PREV, repeat the operation, log
	 * file header records aren't useful to applications.
	 */
	if ((ret = __log_c_get_int(logc, alsn, dbt, flags)) != 0) {
		*alsn = saved_lsn;
		return (ret);
	}
	if (alsn->offset == 0 && (flags == DB_FIRST ||
	    flags == DB_NEXT || flags == DB_LAST || flags == DB_PREV)) {
		switch (flags) {
		case DB_FIRST:
			flags = DB_NEXT;
			break;
		case DB_LAST:
			flags = DB_PREV;
			break;
		case DB_NEXT:
		case DB_PREV:
		default:
			break;
		}
		if (F_ISSET(dbt, DB_DBT_MALLOC)) {
			__os_free(dbenv, dbt->data);
			dbt->data = NULL;
		}
		if ((ret = __log_c_get_int(logc, alsn, dbt, flags)) != 0) {
			*alsn = saved_lsn;
			return (ret);
		}
	}

	return (0);
}

/*
 * __log_c_get_int --
 *	Get a log record; internal version.
 */
static int
__log_c_get_int(logc, alsn, dbt, flags)
	DB_LOGC *logc;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	DB_CIPHER *db_cipher;
	DB_ENV *dbenv;
	DB_LOG *dblp;
	DB_LSN last_lsn, nlsn;
	HDR hdr;
	LOG *lp;
	RLOCK rlock;
	logfile_validity status;
	u_int32_t cnt;
	u_int8_t *rp;
	int eof, is_hmac, ret;

	dbenv = logc->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	is_hmac = 0;

	/*
	 * We don't acquire the log region lock until we need it, and we
	 * release it as soon as we're done.
	 */
	rlock = F_ISSET(logc, DB_LOG_LOCKED) ? L_ALREADY : L_NONE;

	nlsn = logc->c_lsn;
	switch (flags) {
	case DB_NEXT:				/* Next log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* Increment the cursor by the cursor record size. */
			nlsn.offset += logc->c_len;
			break;
		}
		flags = DB_FIRST;
		/* FALLTHROUGH */
	case DB_FIRST:				/* First log record. */
		/* Find the first log file. */
		if ((ret = __log_find(dblp, 1, &cnt, &status)) != 0)
			goto err;

		/*
		 * DB_LV_INCOMPLETE:
		 *	Theoretically, the log file we want could be created
		 *	but not yet written, the "first" log record must be
		 *	in the log buffer.
		 * DB_LV_NORMAL:
		 * DB_LV_OLD_READABLE:
		 *	We found a log file we can read.
		 * DB_LV_NONEXISTENT:
		 *	No log files exist, the "first" log record must be in
		 *	the log buffer.
		 * DB_LV_OLD_UNREADABLE:
		 *	No readable log files exist, we're at the cross-over
		 *	point between two versions.  The "first" log record
		 *	must be in the log buffer.
		 */
		switch (status) {
		case DB_LV_INCOMPLETE:
			DB_ASSERT(lp->lsn.file == cnt);
			/* FALLTHROUGH */
		case DB_LV_NORMAL:
		case DB_LV_OLD_READABLE:
			nlsn.file = cnt;
			break;
		case DB_LV_NONEXISTENT:
			nlsn.file = 1;
			DB_ASSERT(lp->lsn.file == nlsn.file);
			break;
		case DB_LV_OLD_UNREADABLE:
			nlsn.file = cnt + 1;
			DB_ASSERT(lp->lsn.file == nlsn.file);
			break;
		}
		nlsn.offset = 0;
		break;
	case DB_CURRENT:			/* Current log record. */
		break;
	case DB_PREV:				/* Previous log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* If at start-of-file, move to the previous file. */
			if (nlsn.offset == 0) {
				if (nlsn.file == 1) {
					ret = DB_NOTFOUND;
					goto err;
				}
				if ((!lp->db_log_inmemory &&
				    (__log_valid(dblp, nlsn.file - 1, 0, NULL,
				    0, &status) != 0 ||
				    (status != DB_LV_NORMAL &&
				    status != DB_LV_OLD_READABLE)))) {
					ret = DB_NOTFOUND;
					goto err;
				}

				--nlsn.file;
			}
			nlsn.offset = logc->c_prev;
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:				/* Last log record. */
		if (rlock == L_NONE) {
			rlock = L_ACQUIRED;
			R_LOCK(dbenv, &dblp->reginfo);
		}
		nlsn.file = lp->lsn.file;
		nlsn.offset = lp->lsn.offset - lp->len;
		break;
	case DB_SET:				/* Set log record. */
		nlsn = *alsn;
		break;
	default:
		DB_ASSERT(0);
		ret = EINVAL;
		goto err;
	}

	if (0) {				/* Move to the next file. */
next_file:	++nlsn.file;
		nlsn.offset = 0;
	}

	/*
	 * The above switch statement should have set nlsn to the lsn of
	 * the requested record.
	 */

	if (CRYPTO_ON(dbenv)) {
		hdr.size = HDR_CRYPTO_SZ;
		is_hmac = 1;
	} else {
		hdr.size = HDR_NORMAL_SZ;
		is_hmac = 0;
	}
	/* Check to see if the record is in the cursor's buffer. */
	if ((ret = __log_c_incursor(logc, &nlsn, &hdr, &rp)) != 0)
		goto err;
	if (rp != NULL)
		goto cksum;

	/*
	 * Look to see if we're moving backward in the log with the last record
	 * coming from the disk -- it means the record can't be in the region's
	 * buffer.  Else, check the region's buffer.
	 *
	 * If the record isn't in the region's buffer, then either logs are
	 * in-memory, and we're done, or  we're going to have to read the
	 * record from disk.  We want to make a point of not reading past the
	 * end of the logical log (after recovery, there may be data after the
	 * end of the logical log, not to mention the log file may have been
	 * pre-allocated).  So, zero out last_lsn, and initialize it inside
	 * __log_c_inregion -- if it's still zero when we check it in
	 * __log_c_ondisk, that's OK, it just means the logical end of the log
	 * isn't an issue for this request.
	 */
	ZERO_LSN(last_lsn);
	if (!F_ISSET(logc, DB_LOG_DISK) ||
	    log_compare(&nlsn, &logc->c_lsn) > 0) {
		F_CLR(logc, DB_LOG_DISK);

		if ((ret = __log_c_inregion(logc,
		    &nlsn, &rlock, &last_lsn, &hdr, &rp)) != 0)
			goto err;
		if (rp != NULL)
			goto cksum;
		if (lp->db_log_inmemory)
			goto nohdr;
	}

	/*
	 * We have to read from an on-disk file to retrieve the record.
	 * If we ever can't retrieve the record at offset 0, we're done,
	 * return EOF/DB_NOTFOUND.
	 *
	 * Discard the region lock if we're still holding it, the on-disk
	 * reading routines don't need it.
	 */
	if (rlock == L_ACQUIRED) {
		rlock = L_NONE;
		R_UNLOCK(dbenv, &dblp->reginfo);
	}
	if ((ret = __log_c_ondisk(
	    logc, &nlsn, &last_lsn, flags, &hdr, &rp, &eof)) != 0)
		goto err;
	if (eof) {
		/*
		 * Only DB_NEXT automatically moves to the next file, and
		 * it only happens once.
		 */
		if (flags != DB_NEXT || nlsn.offset == 0)
			return (DB_NOTFOUND);
		goto next_file;
	}
	F_SET(logc, DB_LOG_DISK);

cksum:	/*
	 * Discard the region lock if we're still holding it.  (The path to
	 * get here is that we acquired the lock because of the caller's
	 * flag argument, but we found the record in the cursor's buffer.
	 * Improbable, but it's easy to avoid.
	 */
	if (rlock == L_ACQUIRED) {
		rlock = L_NONE;
		R_UNLOCK(dbenv, &dblp->reginfo);
	}

	/*
	 * Checksum: there are two types of errors -- a configuration error
	 * or a checksum mismatch.  The former is always bad.  The latter is
	 * OK if we're searching for the end of the log, and very, very bad
	 * if we're reading random log records.
	 */
	db_cipher = dbenv->crypto_handle;
	if ((ret = __db_check_chksum(dbenv, db_cipher,
	    hdr.chksum, rp + hdr.size, hdr.len - hdr.size, is_hmac)) != 0) {
		if (F_ISSET(logc, DB_LOG_SILENT_ERR)) {
			if (ret == 0 || ret == -1)
				ret = EIO;
		} else if (ret == -1) {
			__db_err(dbenv,
		    "DB_LOGC->get: log record LSN %lu/%lu: checksum mismatch",
			    (u_long)nlsn.file, (u_long)nlsn.offset);
			__db_err(dbenv,
		    "DB_LOGC->get: catastrophic recovery may be required");
			ret = __db_panic(dbenv, DB_RUNRECOVERY);
		}
		goto err;
	}

	/*
	 * If we got a 0-length record, that means we're in the midst of
	 * some bytes that got 0'd as the result of a vtruncate.  We're
	 * going to have to retry.
	 */
	if (hdr.len == 0) {
nohdr:		switch (flags) {
		case DB_FIRST:
		case DB_NEXT:
			/* Zero'd records always indicate the end of a file. */
			goto next_file;
		case DB_LAST:
		case DB_PREV:
			/*
			 * We should never get here.  If we recover a log
			 * file with 0's at the end, we'll treat the 0'd
			 * headers as the end of log and ignore them.  If
			 * we're reading backwards from another file, then
			 * the first record in that new file should have its
			 * prev field set correctly.
			 */
			__db_err(dbenv,
		"Encountered zero length records while traversing backwards");
			DB_ASSERT(0);
			ret = __db_panic(dbenv, DB_RUNRECOVERY);
			goto err;
		case DB_SET:
		default:
			/* Return the 0-length record. */
			break;
		}
	}

	/* Copy the record into the user's DBT. */
	if ((ret = __db_retcopy(dbenv, dbt, rp + hdr.size,
	    (u_int32_t)(hdr.len - hdr.size),
	    &logc->c_dbt.data, &logc->c_dbt.ulen)) != 0)
		goto err;

	if (CRYPTO_ON(dbenv)) {
		if ((ret = db_cipher->decrypt(dbenv, db_cipher->data,
		    hdr.iv, dbt->data, hdr.len - hdr.size)) != 0) {
			ret = EAGAIN;
			goto err;
		}
		/*
		 * Return the original log record size to the user,
		 * even though we've allocated more than that, possibly.
		 * The log record is decrypted in the user dbt, not in
		 * the buffer, so we must do this here after decryption,
		 * not adjust the len passed to the __db_retcopy call.
		 */
		dbt->size = hdr.orig_size;
	}

	/* Update the cursor and the returned LSN. */
	*alsn = nlsn;
	logc->c_lsn = nlsn;
	logc->c_len = hdr.len;
	logc->c_prev = hdr.prev;

err:	if (rlock == L_ACQUIRED)
		R_UNLOCK(dbenv, &dblp->reginfo);

	return (ret);
}

/*
 * __log_c_incursor --
 *	Check to see if the requested record is in the cursor's buffer.
 */
static int
__log_c_incursor(logc, lsn, hdr, pp)
	DB_LOGC *logc;
	DB_LSN *lsn;
	HDR *hdr;
	u_int8_t **pp;
{
	u_int8_t *p;
	int eof;

	*pp = NULL;

	/*
	 * Test to see if the requested LSN could be part of the cursor's
	 * buffer.
	 *
	 * The record must be part of the same file as the cursor's buffer.
	 * The record must start at a byte offset equal to or greater than
	 * the cursor buffer.
	 * The record must not start at a byte offset after the cursor
	 * buffer's end.
	 */
	if (logc->bp_lsn.file != lsn->file)
		return (0);
	if (logc->bp_lsn.offset > lsn->offset)
		return (0);
	if (logc->bp_lsn.offset + logc->bp_rlen <= lsn->offset + hdr->size)
		return (0);

	/*
	 * Read the record's header and check if the record is entirely held
	 * in the buffer.  If the record is not entirely held, get it again.
	 * (The only advantage in having part of the record locally is that
	 * we might avoid a system call because we already have the HDR in
	 * memory.)
	 *
	 * If the header check fails for any reason, it must be because the
	 * LSN is bogus.  Fail hard.
	 */
	p = logc->bp + (lsn->offset - logc->bp_lsn.offset);
	memcpy(hdr, p, hdr->size);
	if (__log_c_hdrchk(logc, lsn, hdr, &eof))
		return (DB_NOTFOUND);
	if (eof || logc->bp_lsn.offset + logc->bp_rlen < lsn->offset + hdr->len)
		return (0);

	*pp = p;				/* Success. */

	return (0);
}

/*
 * __log_c_inregion --
 *	Check to see if the requested record is in the region's buffer.
 */
static int
__log_c_inregion(logc, lsn, rlockp, last_lsn, hdr, pp)
	DB_LOGC *logc;
	DB_LSN *lsn, *last_lsn;
	RLOCK *rlockp;
	HDR *hdr;
	u_int8_t **pp;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	LOG *lp;
	size_t b_region, len, nr;
	u_int32_t b_disk;
	int eof, ret;
	u_int8_t *p;

	dbenv = logc->dbenv;
	dblp = dbenv->lg_handle;
	lp = ((DB_LOG *)logc->dbenv->lg_handle)->reginfo.primary;

	ret = 0;
	b_region = 0;
	*pp = NULL;

	/* If we haven't yet acquired the log region lock, do so. */
	if (*rlockp == L_NONE) {
		*rlockp = L_ACQUIRED;
		R_LOCK(dbenv, &dblp->reginfo);
	}

	/*
	 * The routines to read from disk must avoid reading past the logical
	 * end of the log, so pass that information back to it.
	 *
	 * Since they're reading directly from the disk, they must also avoid
	 * reading past the offset we've written out.  If the log was
	 * truncated, it's possible that there are zeroes or garbage on
	 * disk after this offset, and the logical end of the log can
	 * come later than this point if the log buffer isn't empty.
	 */
	*last_lsn = lp->lsn;
	if (!lp->db_log_inmemory && last_lsn->offset > lp->w_off)
		last_lsn->offset = lp->w_off;

	/*
	 * Test to see if the requested LSN could be part of the region's
	 * buffer.
	 *
	 * During recovery, we read the log files getting the information to
	 * initialize the region.  In that case, the region's lsn field will
	 * not yet have been filled in, use only the disk.
	 *
	 * The record must not start at a byte offset after the region buffer's
	 * end, since that means the request is for a record after the end of
	 * the log.  Do this test even if the region's buffer is empty -- after
	 * recovery, the log files may continue past the declared end-of-log,
	 * and the disk reading routine will incorrectly attempt to read the
	 * remainder of the log.
	 *
	 * Otherwise, test to see if the region's buffer actually has what we
	 * want:
	 *
	 * The buffer must have some useful content.
	 * The record must be in the same file as the region's buffer and must
	 * start at a byte offset equal to or greater than the region's buffer.
	 */
	if (IS_ZERO_LSN(lp->lsn))
		return (0);
	if (log_compare(lsn, &lp->lsn) >= 0)
		return (DB_NOTFOUND);
	else if (lp->db_log_inmemory) {
		if ((ret = __log_inmem_lsnoff(dblp, lsn, &b_region)) != 0)
			return (ret);
	} else if (lp->b_off == 0 || log_compare(lsn, &lp->f_lsn) < 0)
		return (0);

	/*
	 * The current contents of the cursor's buffer will be useless for a
	 * future call, we're about to overwrite it -- trash it rather than
	 * try and make it look correct.
	 */
	logc->bp_rlen = 0;

	/*
	 * If the requested LSN is greater than the region buffer's first
	 * byte, we know the entire record is in the buffer on a good LSN.
	 *
	 * If we're given a bad LSN, the "entire" record might not be in
	 * our buffer in order to fail at the chksum.  __log_c_hdrchk made
	 * sure our dest buffer fits, via bp_maxrec, but we also need to
	 * make sure we don't run off the end of this buffer, the src.
	 *
	 * There is one case where the header check can fail: on a scan through
	 * in-memory logs, when we reach the end of a file we can read an empty
	 * heady.  In that case, it's safe to return zero, here: it will be
	 * caught in our caller.  Otherwise, the LSN is bogus.  Fail hard.
	 */
	if (lp->db_log_inmemory || log_compare(lsn, &lp->f_lsn) > 0) {
		if (!lp->db_log_inmemory)
			b_region = lsn->offset - lp->w_off;
		__log_inmem_copyout(dblp, b_region, hdr, hdr->size);
		if (__log_c_hdrchk(logc, lsn, hdr, &eof) != 0)
			return (DB_NOTFOUND);
		if (eof)
			return (0);
		if (lp->db_log_inmemory) {
			if (RINGBUF_LEN(lp, b_region, lp->b_off) < hdr->len)
				return (DB_NOTFOUND);
		} else if (lsn->offset + hdr->len > lp->w_off + lp->buffer_size)
			return (DB_NOTFOUND);
		if (logc->bp_size <= hdr->len) {
			len = (size_t)DB_ALIGN(hdr->len * 2, 128);
			if ((ret =
			    __os_realloc(logc->dbenv, len, &logc->bp)) != 0)
				 return (ret);
			logc->bp_size = (u_int32_t)len;
		}
		__log_inmem_copyout(dblp, b_region, logc->bp, hdr->len);
		*pp = logc->bp;
		return (0);
	}

	DB_ASSERT(!lp->db_log_inmemory);

	/*
	 * There's a partial record, that is, the requested record starts
	 * in a log file and finishes in the region buffer.  We have to
	 * find out how many bytes of the record are in the region buffer
	 * so we can copy them out into the cursor buffer.  First, check
	 * to see if the requested record is the only record in the region
	 * buffer, in which case we should copy the entire region buffer.
	 *
	 * Else, walk back through the region's buffer to find the first LSN
	 * after the record that crosses the buffer boundary -- we can detect
	 * that LSN, because its "prev" field will reference the record we
	 * want.  The bytes we need to copy from the region buffer are the
	 * bytes up to the record we find.  The bytes we'll need to allocate
	 * to hold the log record are the bytes between the two offsets.
	 */
	b_disk = lp->w_off - lsn->offset;
	if (lp->b_off <= lp->len)
		b_region = (u_int32_t)lp->b_off;
	else
		for (p = dblp->bufp + (lp->b_off - lp->len);;) {
			memcpy(hdr, p, hdr->size);
			if (hdr->prev == lsn->offset) {
				b_region = (u_int32_t)(p - dblp->bufp);
				break;
			}
			p = dblp->bufp + (hdr->prev - lp->w_off);
		}

	/*
	 * If we don't have enough room for the record, we have to allocate
	 * space.  We have to do it while holding the region lock, which is
	 * truly annoying, but there's no way around it.  This call is why
	 * we allocate cursor buffer space when allocating the cursor instead
	 * of waiting.
	 */
	if (logc->bp_size <= b_region + b_disk) {
		len = (size_t)DB_ALIGN((b_region + b_disk) * 2, 128);
		if ((ret = __os_realloc(logc->dbenv, len, &logc->bp)) != 0)
			return (ret);
		logc->bp_size = (u_int32_t)len;
	}

	/* Copy the region's bytes to the end of the cursor's buffer. */
	p = (logc->bp + logc->bp_size) - b_region;
	memcpy(p, dblp->bufp, b_region);

	/* Release the region lock. */
	if (*rlockp == L_ACQUIRED) {
		*rlockp = L_NONE;
		R_UNLOCK(dbenv, &dblp->reginfo);
	}

	/*
	 * Read the rest of the information from disk.  Neither short reads
	 * or EOF are acceptable, the bytes we want had better be there.
	 */
	if (b_disk != 0) {
		p -= b_disk;
		nr = b_disk;
		if ((ret = __log_c_io(
		    logc, lsn->file, lsn->offset, p, &nr, NULL)) != 0)
			return (ret);
		if (nr < b_disk)
			return (__log_c_shortread(logc, lsn, 0));
	}

	/* Copy the header information into the caller's structure. */
	memcpy(hdr, p, hdr->size);

	*pp = p;
	return (0);
}

/*
 * __log_c_ondisk --
 *	Read a record off disk.
 */
static int
__log_c_ondisk(logc, lsn, last_lsn, flags, hdr, pp, eofp)
	DB_LOGC *logc;
	DB_LSN *lsn, *last_lsn;
	u_int32_t flags;
	int *eofp;
	HDR *hdr;
	u_int8_t **pp;
{
	DB_ENV *dbenv;
	size_t len, nr;
	u_int32_t offset;
	int ret;

	dbenv = logc->dbenv;
	*eofp = 0;

	nr = hdr->size;
	if ((ret =
	    __log_c_io(logc, lsn->file, lsn->offset, hdr, &nr, eofp)) != 0)
		return (ret);
	if (*eofp)
		return (0);

	/*
	 * If the read was successful, but we can't read a full header, assume
	 * we've hit EOF.  We can't check that the header has been partially
	 * zeroed out, but it's unlikely that this is caused by a write failure
	 * since the header is written as a single write call and it's less
	 * than sector.
	 */
	if (nr < hdr->size) {
		*eofp = 1;
		return (0);
	}

	/* Check the HDR. */
	if ((ret = __log_c_hdrchk(logc, lsn, hdr, eofp)) != 0)
		return (ret);
	if (*eofp)
		return (0);

	/*
	 * Regardless of how we return, the previous contents of the cursor's
	 * buffer are useless -- trash it.
	 */
	logc->bp_rlen = 0;

	/*
	 * Otherwise, we now (finally!) know how big the record is.  (Maybe
	 * we should have just stuck the length of the record into the LSN!?)
	 * Make sure we have enough space.
	 */
	if (logc->bp_size <= hdr->len) {
		len = (size_t)DB_ALIGN(hdr->len * 2, 128);
		if ((ret = __os_realloc(dbenv, len, &logc->bp)) != 0)
			return (ret);
		logc->bp_size = (u_int32_t)len;
	}

	/*
	 * If we're moving forward in the log file, read this record in at the
	 * beginning of the buffer.  Otherwise, read this record in at the end
	 * of the buffer, making sure we don't try and read before the start
	 * of the file.  (We prefer positioning at the end because transaction
	 * aborts use DB_SET to move backward through the log and we might get
	 * lucky.)
	 *
	 * Read a buffer's worth, without reading past the logical EOF.  The
	 * last_lsn may be a zero LSN, but that's OK, the test works anyway.
	 */
	if (flags == DB_FIRST || flags == DB_NEXT)
		offset = lsn->offset;
	else if (lsn->offset + hdr->len < logc->bp_size)
		offset = 0;
	else
		offset = (lsn->offset + hdr->len) - logc->bp_size;

	nr = logc->bp_size;
	if (lsn->file == last_lsn->file && offset + nr >= last_lsn->offset)
		nr = last_lsn->offset - offset;

	if ((ret =
	    __log_c_io(logc, lsn->file, offset, logc->bp, &nr, eofp)) != 0)
		return (ret);

	/*
	 * We should have at least gotten the bytes up-to-and-including the
	 * record we're reading.
	 */
	if (nr < (lsn->offset + hdr->len) - offset)
		return (__log_c_shortread(logc, lsn, 1));

	/*
	 * Set up the return information.
	 *
	 * !!!
	 * No need to set the bp_lsn.file field, __log_c_io set it for us.
	 */
	logc->bp_rlen = (u_int32_t)nr;
	logc->bp_lsn.offset = offset;

	*pp = logc->bp + (lsn->offset - offset);

	return (0);
}

/*
 * __log_c_hdrchk --
 *
 * Check for corrupted HDRs before we use them to allocate memory or find
 * records.
 *
 * If the log files were pre-allocated, a zero-filled HDR structure is the
 * logical file end.  However, we can see buffers filled with 0's during
 * recovery, too (because multiple log buffers were written asynchronously,
 * and one made it to disk before a different one that logically precedes
 * it in the log file.
 *
 * Check for impossibly large records.  The malloc should fail later, but we
 * have customers that run mallocs that treat all allocation failures as fatal
 * errors.
 *
 * Note that none of this is necessarily something awful happening.  We let
 * the application hand us any LSN they want, and it could be a pointer into
 * the middle of a log record, there's no way to tell.
 */
static int
__log_c_hdrchk(logc, lsn, hdr, eofp)
	DB_LOGC *logc;
	DB_LSN *lsn;
	HDR *hdr;
	int *eofp;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = logc->dbenv;

	/*
	 * Check EOF before we do any other processing.
	 */
	if (eofp != NULL) {
		if (hdr->prev == 0 && hdr->chksum[0] == 0 && hdr->len == 0) {
			*eofp = 1;
			return (0);
		}
		*eofp = 0;
	}

	/*
	 * Sanity check the log record's size.
	 * We must check it after "virtual" EOF above.
	 */
	if (hdr->len <= hdr->size)
		goto err;

	/*
	 * If the cursor's max-record value isn't yet set, it means we aren't
	 * reading these records from a log file and no check is necessary.
	 */
	if (logc->bp_maxrec != 0 && hdr->len > logc->bp_maxrec) {
		/*
		 * If we fail the check, there's the pathological case that
		 * we're reading the last file, it's growing, and our initial
		 * check information was wrong.  Get it again, to be sure.
		 */
		if ((ret = __log_c_set_maxrec(logc, NULL)) != 0) {
			__db_err(dbenv, "DB_LOGC->get: %s", db_strerror(ret));
			return (ret);
		}
		if (logc->bp_maxrec != 0 && hdr->len > logc->bp_maxrec)
			goto err;
	}
	return (0);

err:	if (!F_ISSET(logc, DB_LOG_SILENT_ERR))
		__db_err(dbenv,
		    "DB_LOGC->get: LSN %lu/%lu: invalid log record header",
		    (u_long)lsn->file, (u_long)lsn->offset);
	return (EIO);
}

/*
 * __log_c_io --
 *	Read records from a log file.
 */
static int
__log_c_io(logc, fnum, offset, p, nrp, eofp)
	DB_LOGC *logc;
	u_int32_t fnum, offset;
	void *p;
	size_t *nrp;
	int *eofp;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	int ret;
	char *np;

	dbenv = logc->dbenv;
	dblp = dbenv->lg_handle;

	/*
	 * If we've switched files, discard the current file handle and acquire
	 * a new one.
	 */
	if (logc->c_fhp != NULL && logc->bp_lsn.file != fnum) {
		ret = __os_closehandle(dbenv, logc->c_fhp);
		logc->c_fhp = NULL;
		logc->bp_lsn.file = 0;

		if (ret != 0)
			return (ret);
	}
	if (logc->c_fhp == NULL) {
		if ((ret = __log_name(dblp, fnum,
		    &np, &logc->c_fhp, DB_OSO_RDONLY | DB_OSO_SEQ)) != 0) {
			/*
			 * If we're allowed to return EOF, assume that's the
			 * problem, set the EOF status flag and return 0.
			 */
			if (eofp != NULL) {
				*eofp = 1;
				ret = 0;
			} else if (!F_ISSET(logc, DB_LOG_SILENT_ERR))
				__db_err(dbenv, "DB_LOGC->get: %s: %s",
				    np, db_strerror(ret));
			__os_free(dbenv, np);
			return (ret);
		}

		if ((ret = __log_c_set_maxrec(logc, np)) != 0) {
			__db_err(dbenv,
			    "DB_LOGC->get: %s: %s", np, db_strerror(ret));
			__os_free(dbenv, np);
			return (ret);
		}
		__os_free(dbenv, np);

		logc->bp_lsn.file = fnum;
	}

	/* Seek to the record's offset. */
	if ((ret = __os_seek(dbenv,
	    logc->c_fhp, 0, 0, offset, 0, DB_OS_SEEK_SET)) != 0) {
		if (!F_ISSET(logc, DB_LOG_SILENT_ERR))
			__db_err(dbenv,
			    "DB_LOGC->get: LSN: %lu/%lu: seek: %s",
			    (u_long)fnum, (u_long)offset, db_strerror(ret));
		return (ret);
	}

	/* Read the data. */
	if ((ret = __os_read(dbenv, logc->c_fhp, p, *nrp, nrp)) != 0) {
		if (!F_ISSET(logc, DB_LOG_SILENT_ERR))
			__db_err(dbenv,
			    "DB_LOGC->get: LSN: %lu/%lu: read: %s",
			    (u_long)fnum, (u_long)offset, db_strerror(ret));
		return (ret);
	}

	return (0);
}

/*
 * __log_c_shortread --
 *	Read was short -- return a consistent error message and error.
 */
static int
__log_c_shortread(logc, lsn, check_silent)
	DB_LOGC *logc;
	DB_LSN *lsn;
	int check_silent;
{
	if (!check_silent || !F_ISSET(logc, DB_LOG_SILENT_ERR))
		__db_err(logc->dbenv, "DB_LOGC->get: LSN: %lu/%lu: short read",
		    (u_long)lsn->file, (u_long)lsn->offset);
	return (EIO);
}

/*
 * __log_c_set_maxrec --
 *	Bound the maximum log record size in a log file.
 */
static int
__log_c_set_maxrec(logc, np)
	DB_LOGC *logc;
	char *np;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	LOG *lp;
	u_int32_t mbytes, bytes;
	int ret;

	dbenv = logc->dbenv;
	dblp = dbenv->lg_handle;

	/*
	 * We don't want to try and allocate huge chunks of memory because
	 * applications with error-checking malloc's often consider that a
	 * hard failure.  If we're about to look at a corrupted record with
	 * a bizarre size, we need to know before trying to allocate space
	 * to hold it.  We could read the persistent data at the beginning
	 * of the file but that's hard -- we may have to decrypt it, checksum
	 * it and so on.  Stat the file instead.
	 */
	if (logc->c_fhp != NULL) {
		if ((ret = __os_ioinfo(dbenv, np, logc->c_fhp,
		    &mbytes, &bytes, NULL)) != 0)
			return (ret);
		if (logc->bp_maxrec < (mbytes * MEGABYTE + bytes))
			logc->bp_maxrec = mbytes * MEGABYTE + bytes;
	}

	/*
	 * If reading from the log file currently being written, we could get
	 * an incorrect size, that is, if the cursor was opened on the file
	 * when it had only a few hundred bytes, and then the cursor used to
	 * move forward in the file, after more log records were written, the
	 * original stat value would be wrong.  Use the maximum of the current
	 * log file size and the size of the buffer -- that should represent
	 * the max of any log record currently in the file.
	 *
	 * The log buffer size is set when the environment is opened and never
	 * changed, we don't need a lock on it.
	 */
	lp = dblp->reginfo.primary;
	if (logc->bp_maxrec < lp->buffer_size)
		logc->bp_maxrec = lp->buffer_size;

	return (0);
}
