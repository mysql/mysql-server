/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
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
 *
 * $Id: db_overflow.c,v 11.54 2004/03/28 17:17:50 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/mp.h"

/*
 * Big key/data code.
 *
 * Big key and data entries are stored on linked lists of pages.  The initial
 * reference is a structure with the total length of the item and the page
 * number where it begins.  Each entry in the linked list contains a pointer
 * to the next page of data, and so on.
 */

/*
 * __db_goff --
 *	Get an offpage item.
 *
 * PUBLIC: int __db_goff __P((DB *, DBT *,
 * PUBLIC:     u_int32_t, db_pgno_t, void **, u_int32_t *));
 */
int
__db_goff(dbp, dbt, tlen, pgno, bpp, bpsz)
	DB *dbp;
	DBT *dbt;
	u_int32_t tlen;
	db_pgno_t pgno;
	void **bpp;
	u_int32_t *bpsz;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_indx_t bytes;
	u_int32_t curoff, needed, start;
	u_int8_t *p, *src;
	int ret;

	dbenv = dbp->dbenv;
	mpf = dbp->mpf;

	/*
	 * Check if the buffer is big enough; if it is not and we are
	 * allowed to malloc space, then we'll malloc it.  If we are
	 * not (DB_DBT_USERMEM), then we'll set the dbt and return
	 * appropriately.
	 */
	if (F_ISSET(dbt, DB_DBT_PARTIAL)) {
		start = dbt->doff;
		if (start > tlen)
			needed = 0;
		else if (dbt->dlen > tlen - start)
			needed = tlen - start;
		else
			needed = dbt->dlen;
	} else {
		start = 0;
		needed = tlen;
	}

	/* Allocate any necessary memory. */
	if (F_ISSET(dbt, DB_DBT_USERMEM)) {
		if (needed > dbt->ulen) {
			dbt->size = needed;
			return (DB_BUFFER_SMALL);
		}
	} else if (F_ISSET(dbt, DB_DBT_MALLOC)) {
		if ((ret = __os_umalloc(dbenv, needed, &dbt->data)) != 0)
			return (ret);
	} else if (F_ISSET(dbt, DB_DBT_REALLOC)) {
		if ((ret = __os_urealloc(dbenv, needed, &dbt->data)) != 0)
			return (ret);
	} else if (bpsz != NULL && (*bpsz == 0 || *bpsz < needed)) {
		if ((ret = __os_realloc(dbenv, needed, bpp)) != 0)
			return (ret);
		*bpsz = needed;
		dbt->data = *bpp;
	} else if (bpp != NULL)
		dbt->data = *bpp;
	else {
		DB_ASSERT(
		    F_ISSET(dbt,
		    DB_DBT_USERMEM | DB_DBT_MALLOC | DB_DBT_REALLOC) ||
		    bpsz != NULL || bpp != NULL);
		return (DB_BUFFER_SMALL);
	}

	/*
	 * Step through the linked list of pages, copying the data on each
	 * one into the buffer.  Never copy more than the total data length.
	 */
	dbt->size = needed;
	for (curoff = 0, p = dbt->data; pgno != PGNO_INVALID && needed > 0;) {
		if ((ret = __memp_fget(mpf, &pgno, 0, &h)) != 0)
			return (ret);

		/* Check if we need any bytes from this page. */
		if (curoff + OV_LEN(h) >= start) {
			src = (u_int8_t *)h + P_OVERHEAD(dbp);
			bytes = OV_LEN(h);
			if (start > curoff) {
				src += start - curoff;
				bytes -= start - curoff;
			}
			if (bytes > needed)
				bytes = needed;
			memcpy(p, src, bytes);
			p += bytes;
			needed -= bytes;
		}
		curoff += OV_LEN(h);
		pgno = h->next_pgno;
		(void)__memp_fput(mpf, h, 0);
	}
	return (0);
}

/*
 * __db_poff --
 *	Put an offpage item.
 *
 * PUBLIC: int __db_poff __P((DBC *, const DBT *, db_pgno_t *));
 */
int
__db_poff(dbc, dbt, pgnop)
	DBC *dbc;
	const DBT *dbt;
	db_pgno_t *pgnop;
{
	DB *dbp;
	DBT tmp_dbt;
	DB_LSN new_lsn, null_lsn;
	DB_MPOOLFILE *mpf;
	PAGE *pagep, *lastp;
	db_indx_t pagespace;
	u_int32_t sz;
	u_int8_t *p;
	int ret, t_ret;

	/*
	 * Allocate pages and copy the key/data item into them.  Calculate the
	 * number of bytes we get for pages we fill completely with a single
	 * item.
	 */
	dbp = dbc->dbp;
	mpf = dbp->mpf;
	pagespace = P_MAXSPACE(dbp, dbp->pgsize);

	ret = 0;
	lastp = NULL;
	for (p = dbt->data,
	    sz = dbt->size; sz > 0; p += pagespace, sz -= pagespace) {
		/*
		 * Reduce pagespace so we terminate the loop correctly and
		 * don't copy too much data.
		 */
		if (sz < pagespace)
			pagespace = sz;

		/*
		 * Allocate and initialize a new page and copy all or part of
		 * the item onto the page.  If sz is less than pagespace, we
		 * have a partial record.
		 */
		if ((ret = __db_new(dbc, P_OVERFLOW, &pagep)) != 0)
			break;
		if (DBC_LOGGING(dbc)) {
			tmp_dbt.data = p;
			tmp_dbt.size = pagespace;
			ZERO_LSN(null_lsn);
			if ((ret = __db_big_log(dbp, dbc->txn,
			    &new_lsn, 0, DB_ADD_BIG, PGNO(pagep),
			    lastp ? PGNO(lastp) : PGNO_INVALID,
			    PGNO_INVALID, &tmp_dbt, &LSN(pagep),
			    lastp == NULL ? &null_lsn : &LSN(lastp),
			    &null_lsn)) != 0) {
				if (lastp != NULL)
					(void)__memp_fput(mpf,
					    lastp, DB_MPOOL_DIRTY);
				lastp = pagep;
				break;
			}
		} else
			LSN_NOT_LOGGED(new_lsn);

		/* Move LSN onto page. */
		if (lastp != NULL)
			LSN(lastp) = new_lsn;
		LSN(pagep) = new_lsn;

		P_INIT(pagep, dbp->pgsize,
		    PGNO(pagep), PGNO_INVALID, PGNO_INVALID, 0, P_OVERFLOW);
		OV_LEN(pagep) = pagespace;
		OV_REF(pagep) = 1;
		memcpy((u_int8_t *)pagep + P_OVERHEAD(dbp), p, pagespace);

		/*
		 * If this is the first entry, update the user's info.
		 * Otherwise, update the entry on the last page filled
		 * in and release that page.
		 */
		if (lastp == NULL)
			*pgnop = PGNO(pagep);
		else {
			lastp->next_pgno = PGNO(pagep);
			pagep->prev_pgno = PGNO(lastp);
			(void)__memp_fput(mpf, lastp, DB_MPOOL_DIRTY);
		}
		lastp = pagep;
	}
	if (lastp != NULL &&
	    (t_ret = __memp_fput(mpf, lastp, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_ovref --
 *	Increment/decrement the reference count on an overflow page.
 *
 * PUBLIC: int __db_ovref __P((DBC *, db_pgno_t, int32_t));
 */
int
__db_ovref(dbc, pgno, adjust)
	DBC *dbc;
	db_pgno_t pgno;
	int32_t adjust;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	int ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;

	if ((ret = __memp_fget(mpf, &pgno, 0, &h)) != 0)
		return (__db_pgerr(dbp, pgno, ret));

	if (DBC_LOGGING(dbc)) {
		if ((ret = __db_ovref_log(dbp,
		    dbc->txn, &LSN(h), 0, h->pgno, adjust, &LSN(h))) != 0) {
			(void)__memp_fput(mpf, h, 0);
			return (ret);
		}
	} else
		LSN_NOT_LOGGED(LSN(h));
	OV_REF(h) += adjust;

	(void)__memp_fput(mpf, h, DB_MPOOL_DIRTY);
	return (0);
}

/*
 * __db_doff --
 *	Delete an offpage chain of overflow pages.
 *
 * PUBLIC: int __db_doff __P((DBC *, db_pgno_t));
 */
int
__db_doff(dbc, pgno)
	DBC *dbc;
	db_pgno_t pgno;
{
	DB *dbp;
	PAGE *pagep;
	DB_LSN null_lsn;
	DB_MPOOLFILE *mpf;
	DBT tmp_dbt;
	int ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;

	do {
		if ((ret = __memp_fget(mpf, &pgno, 0, &pagep)) != 0)
			return (__db_pgerr(dbp, pgno, ret));

		DB_ASSERT(TYPE(pagep) == P_OVERFLOW);
		/*
		 * If it's referenced by more than one key/data item,
		 * decrement the reference count and return.
		 */
		if (OV_REF(pagep) > 1) {
			(void)__memp_fput(mpf, pagep, 0);
			return (__db_ovref(dbc, pgno, -1));
		}

		if (DBC_LOGGING(dbc)) {
			tmp_dbt.data = (u_int8_t *)pagep + P_OVERHEAD(dbp);
			tmp_dbt.size = OV_LEN(pagep);
			ZERO_LSN(null_lsn);
			if ((ret = __db_big_log(dbp, dbc->txn,
			    &LSN(pagep), 0, DB_REM_BIG,
			    PGNO(pagep), PREV_PGNO(pagep),
			    NEXT_PGNO(pagep), &tmp_dbt,
			    &LSN(pagep), &null_lsn, &null_lsn)) != 0) {
				(void)__memp_fput(mpf, pagep, 0);
				return (ret);
			}
		} else
			LSN_NOT_LOGGED(LSN(pagep));
		pgno = pagep->next_pgno;
		OV_LEN(pagep) = 0;
		if ((ret = __db_free(dbc, pagep)) != 0)
			return (ret);
	} while (pgno != PGNO_INVALID);

	return (0);
}

/*
 * __db_moff --
 *	Match on overflow pages.
 *
 * Given a starting page number and a key, return <0, 0, >0 to indicate if the
 * key on the page is less than, equal to or greater than the key specified.
 * We optimize this by doing chunk at a time comparison unless the user has
 * specified a comparison function.  In this case, we need to materialize
 * the entire object and call their comparison routine.
 *
 * PUBLIC: int __db_moff __P((DB *, const DBT *, db_pgno_t, u_int32_t,
 * PUBLIC:     int (*)(DB *, const DBT *, const DBT *), int *));
 */
int
__db_moff(dbp, dbt, pgno, tlen, cmpfunc, cmpp)
	DB *dbp;
	const DBT *dbt;
	db_pgno_t pgno;
	u_int32_t tlen;
	int (*cmpfunc) __P((DB *, const DBT *, const DBT *)), *cmpp;
{
	DBT local_dbt;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	void *buf;
	u_int32_t bufsize, cmp_bytes, key_left;
	u_int8_t *p1, *p2;
	int ret;

	mpf = dbp->mpf;

	/*
	 * If there is a user-specified comparison function, build a
	 * contiguous copy of the key, and call it.
	 */
	if (cmpfunc != NULL) {
		memset(&local_dbt, 0, sizeof(local_dbt));
		buf = NULL;
		bufsize = 0;

		if ((ret = __db_goff(dbp,
		    &local_dbt, tlen, pgno, &buf, &bufsize)) != 0)
			return (ret);
		/* Pass the key as the first argument */
		*cmpp = cmpfunc(dbp, dbt, &local_dbt);
		__os_free(dbp->dbenv, buf);
		return (0);
	}

	/* While there are both keys to compare. */
	for (*cmpp = 0, p1 = dbt->data,
	    key_left = dbt->size; key_left > 0 && pgno != PGNO_INVALID;) {
		if ((ret = __memp_fget(mpf, &pgno, 0, &pagep)) != 0)
			return (ret);

		cmp_bytes = OV_LEN(pagep) < key_left ? OV_LEN(pagep) : key_left;
		tlen -= cmp_bytes;
		key_left -= cmp_bytes;
		for (p2 = (u_int8_t *)pagep + P_OVERHEAD(dbp);
		    cmp_bytes-- > 0; ++p1, ++p2)
			if (*p1 != *p2) {
				*cmpp = (long)*p1 - (long)*p2;
				break;
			}
		pgno = NEXT_PGNO(pagep);
		if ((ret = __memp_fput(mpf, pagep, 0)) != 0)
			return (ret);
		if (*cmpp != 0)
			return (0);
	}
	if (key_left > 0)		/* DBT is longer than the page key. */
		*cmpp = 1;
	else if (tlen > 0)		/* DBT is shorter than the page key. */
		*cmpp = -1;
	else
		*cmpp = 0;

	return (0);
}
