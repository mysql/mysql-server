/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log_get.c,v 11.32 2001/01/11 18:19:53 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

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

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

/*
 * log_get --
 *	Get a log record.
 */
int
log_get(dbenv, alsn, dbt, flags)
	DB_ENV *dbenv;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	DB_LOG *dblp;
	DB_LSN saved_lsn;
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_log_get(dbenv, alsn, dbt, flags));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lg_handle, DB_INIT_LOG);

	/* Validate arguments. */
	if (flags != DB_CHECKPOINT && flags != DB_CURRENT &&
	    flags != DB_FIRST && flags != DB_LAST &&
	    flags != DB_NEXT && flags != DB_PREV && flags != DB_SET)
		return (__db_ferr(dbenv, "log_get", 1));

	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		if (flags == DB_NEXT || flags == DB_PREV || flags == DB_CURRENT)
			return (__db_ferr(dbenv, "log_get", 1));
		if (!F_ISSET(dbt,
		    DB_DBT_MALLOC | DB_DBT_REALLOC | DB_DBT_USERMEM))
			return (__db_ferr(dbenv, "threaded data", 1));
	}

	dblp = dbenv->lg_handle;
	R_LOCK(dbenv, &dblp->reginfo);

	/*
	 * The alsn field is only initialized if DB_SET is the flag, so this
	 * assignment causes uninitialized memory complaints for other flag
	 * values.
	 */
#ifdef	UMRW
	if (flags == DB_SET)
		saved_lsn = *alsn;
	else
		ZERO_LSN(saved_lsn);
#else
	saved_lsn = *alsn;
#endif

	/*
	 * If we get one of the log's header records, repeat the operation.
	 * This assumes that applications don't ever request the log header
	 * records by LSN, but that seems reasonable to me.
	 */
	if ((ret = __log_get(dblp,
	    alsn, dbt, flags, 0)) == 0 && alsn->offset == 0) {
		switch (flags) {
		case DB_FIRST:
			flags = DB_NEXT;
			break;
		case DB_LAST:
			flags = DB_PREV;
			break;
		}
		if (F_ISSET(dbt, DB_DBT_MALLOC)) {
			__os_free(dbt->data, dbt->size);
			dbt->data = NULL;
		}
		ret = __log_get(dblp, alsn, dbt, flags, 0);
	}
	if (ret != 0)
		*alsn = saved_lsn;

	R_UNLOCK(dbenv, &dblp->reginfo);

	return (ret);
}

/*
 * __log_get --
 *	Get a log record; internal version.
 *
 * PUBLIC: int __log_get __P((DB_LOG *, DB_LSN *, DBT *, u_int32_t, int));
 */
int
__log_get(dblp, alsn, dbt, flags, silent)
	DB_LOG *dblp;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
	int silent;
{
	DB_ENV *dbenv;
	DB_LSN nlsn;
	HDR hdr;
	LOG *lp;
	const char *fail;
	char *np, *tbuf;
	int cnt, ret;
	logfile_validity status;
	size_t len, nr;
	u_int32_t offset;
	u_int8_t *p;
	void *shortp, *readp;

	lp = dblp->reginfo.primary;
	fail = np = tbuf = NULL;
	dbenv = dblp->dbenv;

	nlsn = dblp->c_lsn;
	switch (flags) {
	case DB_CHECKPOINT:
		nlsn = lp->chkpt_lsn;
		if (IS_ZERO_LSN(nlsn)) {
			/* No db_err. The caller may expect this. */
			ret = ENOENT;
			goto err2;
		}
		break;
	case DB_NEXT:				/* Next log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* Increment the cursor by the cursor record size. */
			nlsn.offset += dblp->c_len;
			break;
		}
		/* FALLTHROUGH */
	case DB_FIRST:				/* Find the first log record. */
		/* Find the first log file. */
		if ((ret = __log_find(dblp, 1, &cnt, &status)) != 0)
			goto err2;

		/*
		 * We want any readable version, so either DB_LV_NORMAL
		 * or DB_LV_OLD_READABLE is acceptable here.  If it's
		 * not one of those two, there is no first log record that
		 * we can read.
		 */
		if (status != DB_LV_NORMAL && status != DB_LV_OLD_READABLE) {
			ret = DB_NOTFOUND;
			goto err2;
		}

		/*
		 * We may have only entered records in the buffer, and not
		 * yet written a log file.  If no log files were found and
		 * there's anything in the buffer, it belongs to file 1.
		 */
		if (cnt == 0)
			cnt = 1;

		nlsn.file = cnt;
		nlsn.offset = 0;
		break;
	case DB_CURRENT:			/* Current log record. */
		break;
	case DB_PREV:				/* Previous log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* If at start-of-file, move to the previous file. */
			if (nlsn.offset == 0) {
				if (nlsn.file == 1 ||
				    __log_valid(dblp,
					nlsn.file - 1, 0, &status) != 0)
					return (DB_NOTFOUND);

				if (status != DB_LV_NORMAL &&
				    status != DB_LV_OLD_READABLE)
					return (DB_NOTFOUND);

				--nlsn.file;
				nlsn.offset = dblp->c_off;
			} else
				nlsn.offset = dblp->c_off;
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:				/* Last log record. */
		nlsn.file = lp->lsn.file;
		nlsn.offset = lp->lsn.offset - lp->len;
		break;
	case DB_SET:				/* Set log record. */
		nlsn = *alsn;
		break;
	}

	if (0) {				/* Move to the next file. */
next_file:	++nlsn.file;
		nlsn.offset = 0;
	}

	/* Return 1 if the request is past the end of the log. */
	if (nlsn.file > lp->lsn.file ||
	    (nlsn.file == lp->lsn.file && nlsn.offset >= lp->lsn.offset))
		return (DB_NOTFOUND);

	/* If we've switched files, discard the current file handle. */
	if (dblp->c_lsn.file != nlsn.file &&
	    F_ISSET(&dblp->c_fh, DB_FH_VALID)) {
		(void)__os_closehandle(&dblp->c_fh);
	}

	/* If the entire record is in the in-memory buffer, copy it out. */
	if (nlsn.file == lp->lsn.file && nlsn.offset >= lp->w_off) {
		/* Copy the header. */
		p = dblp->bufp + (nlsn.offset - lp->w_off);
		memcpy(&hdr, p, sizeof(HDR));

		/* Copy the record. */
		len = hdr.len - sizeof(HDR);
		if ((ret = __db_retcopy(NULL, dbt, p + sizeof(HDR),
		    len, &dblp->c_dbt.data, &dblp->c_dbt.ulen)) != 0)
			goto err2;
		goto cksum;
	}

	shortp = NULL;

	/* Acquire a file descriptor. */
	if (!F_ISSET(&dblp->c_fh, DB_FH_VALID)) {
		if ((ret = __log_name(dblp, nlsn.file,
		    &np, &dblp->c_fh, DB_OSO_RDONLY | DB_OSO_SEQ)) != 0) {
			fail = np;
			goto err1;
		}
		__os_freestr(np);
		np = NULL;
	}

	/* See if we've already read this */
	if (nlsn.file == dblp->r_file && nlsn.offset > dblp->r_off
	     && nlsn.offset + sizeof(HDR) < dblp->r_off + dblp->r_size)
		goto got_header;

	/*
	 * Seek to the header offset and read the header.  Because the file
	 * may be pre-allocated, we have to make sure that we're not reading
	 * past the information in the start of the in-memory buffer.
	 */

	readp = &hdr;
	offset = nlsn.offset;
	if (nlsn.file == lp->lsn.file && offset + sizeof(HDR) > lp->w_off)
		nr = lp->w_off - offset;
	else if (dblp->readbufp == NULL)
		nr = sizeof(HDR);
	else  {
		nr = lp->buffer_size;
		readp = dblp->readbufp;
		dblp->r_file = nlsn.file;
		/* Going backwards.  Put the current in the middle. */
		if (flags == DB_PREV || flags == DB_LAST) {
			if (offset <= lp->buffer_size/2)
				offset = 0;
			else
				offset = offset - lp->buffer_size/2;
		}
		if (nlsn.file == lp->lsn.file && offset + nr > lp->lsn.offset)
			nr = lp->lsn.offset - offset;
		dblp->r_off = offset;
	}

	if ((ret = __os_seek(dblp->dbenv,
	    &dblp->c_fh, 0, 0, offset, 0, DB_OS_SEEK_SET)) != 0) {
		fail = "seek";
		goto err1;
	}
	if ((ret = __os_read(dblp->dbenv, &dblp->c_fh, readp, nr, &nr)) != 0) {
		fail = "read";
		goto err1;
	}
	if (nr < sizeof(HDR)) {
		/* If read returns EOF, try the next file. */
		if (nr == 0) {
			if (flags != DB_NEXT || nlsn.file == lp->lsn.file)
				goto corrupt;
			goto next_file;
		}

		if (dblp->readbufp != NULL)
			memcpy((u_int8_t *) &hdr, readp, nr);

		/*
		 * If read returns a short count the rest of the record has
		 * to be in the in-memory buffer.
		 */
		if (lp->b_off < sizeof(HDR) - nr)
			goto corrupt;

		/* Get the rest of the header from the in-memory buffer. */
		memcpy((u_int8_t *)&hdr + nr, dblp->bufp, sizeof(HDR) - nr);

		if (hdr.len == 0)
			goto next_file;

		shortp = dblp->bufp + (sizeof(HDR) - nr);
	}

	else if (dblp->readbufp != NULL) {
		dblp->r_size = nr;
got_header:	memcpy((u_int8_t *)&hdr,
		    dblp->readbufp + (nlsn.offset - dblp->r_off), sizeof(HDR));
	}

	/*
	 * Check for buffers of 0's, that's what we usually see during recovery,
	 * although it's certainly not something on which we can depend.  Check
	 * for impossibly large records.  The malloc should fail later, but we
	 * have customers that run mallocs that handle allocation failure as a
	 * fatal error.
	 */
	if (hdr.len == 0)
		goto next_file;
	if (hdr.len <= sizeof(HDR) || hdr.len > lp->persist.lg_max)
		goto corrupt;
	len = hdr.len - sizeof(HDR);

	/* If we've already moved to the in-memory buffer, fill from there. */
	if (shortp != NULL) {
		if (lp->b_off < ((u_int8_t *)shortp - dblp->bufp) + len)
			goto corrupt;
		if ((ret = __db_retcopy(NULL, dbt, shortp, len,
		    &dblp->c_dbt.data, &dblp->c_dbt.ulen)) != 0)
			goto err2;
		goto cksum;
	}

	if (dblp->readbufp != NULL) {
		if (nlsn.offset + hdr.len < dblp->r_off + dblp->r_size) {
			if ((ret = __db_retcopy(NULL, dbt, dblp->readbufp +
			     (nlsn.offset - dblp->r_off) + sizeof(HDR),
			     len, &dblp->c_dbt.data, &dblp->c_dbt.ulen)) != 0)
				goto err2;
			goto cksum;
		} else if ((ret = __os_seek(dblp->dbenv, &dblp->c_fh, 0,
		    0, nlsn.offset + sizeof(HDR), 0, DB_OS_SEEK_SET)) != 0) {
			fail = "seek";
			goto err1;
		}
	}

	/*
	 * Allocate temporary memory to hold the record.
	 *
	 * XXX
	 * We're calling malloc(3) with a region locked.  This isn't
	 * a good idea.
	 */
	if ((ret = __os_malloc(dbenv, len, NULL, &tbuf)) != 0)
		goto err1;

	/*
	 * Read the record into the buffer.  If read returns a short count,
	 * there was an error or the rest of the record is in the in-memory
	 * buffer.  Note, the information may be garbage if we're in recovery,
	 * so don't read past the end of the buffer's memory.
	 *
	 * Because the file may be pre-allocated, we have to make sure that
	 * we're not reading past the information in the start of the in-memory
	 * buffer.
	 */
	if (nlsn.file == lp->lsn.file &&
	    nlsn.offset + sizeof(HDR) + len > lp->w_off)
		nr = lp->w_off - (nlsn.offset + sizeof(HDR));
	else
		nr = len;
	if ((ret = __os_read(dblp->dbenv, &dblp->c_fh, tbuf, nr, &nr)) != 0) {
		fail = "read";
		goto err1;
	}
	if (len - nr > lp->buffer_size)
		goto corrupt;
	if (nr != len) {
		if (lp->b_off < len - nr)
			goto corrupt;

		/* Get the rest of the record from the in-memory buffer. */
		memcpy((u_int8_t *)tbuf + nr, dblp->bufp, len - nr);
	}

	/* Copy the record into the user's DBT. */
	if ((ret = __db_retcopy(NULL, dbt, tbuf, len,
	    &dblp->c_dbt.data, &dblp->c_dbt.ulen)) != 0)
		goto err2;
	__os_free(tbuf, 0);
	tbuf = NULL;

cksum:	/*
	 * If the user specified a partial record read, the checksum can't
	 * match.  It's not an obvious thing to do, but a user testing for
	 * the length of a record might do it.
	 */
	if (!F_ISSET(dbt, DB_DBT_PARTIAL) &&
	    hdr.cksum != __ham_func4(NULL, dbt->data, dbt->size)) {
		if (!silent)
			__db_err(dbenv, "log_get: checksum mismatch");
		goto corrupt;
	}

	/* Update the cursor and the return lsn. */
	dblp->c_off = hdr.prev;
	dblp->c_len = hdr.len;
	dblp->c_lsn = nlsn;
	*alsn = nlsn;

	return (0);

corrupt:/*
	 * This is the catchall -- for some reason we didn't find enough
	 * information or it wasn't reasonable information, and it wasn't
	 * because a system call failed.
	 */
	ret = EIO;
	fail = "read";

err1:	if (!silent) {
		if (fail == NULL)
			__db_err(dbenv, "log_get: %s", db_strerror(ret));
		else
			__db_err(dbenv,
			    "log_get: %s: %s", fail, db_strerror(ret));
	}

err2:	if (np != NULL)
		__os_freestr(np);
	if (tbuf != NULL)
		__os_free(tbuf, 0);
	return (ret);
}
