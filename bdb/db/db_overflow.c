/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
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
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_overflow.c,v 11.21 2000/11/30 00:58:32 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_am.h"
#include "db_verify.h"

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
	PAGE *h;
	db_indx_t bytes;
	u_int32_t curoff, needed, start;
	u_int8_t *p, *src;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * Check if the buffer is big enough; if it is not and we are
	 * allowed to malloc space, then we'll malloc it.  If we are
	 * not (DB_DBT_USERMEM), then we'll set the dbt and return
	 * appropriately.
	 */
	if (F_ISSET(dbt, DB_DBT_PARTIAL)) {
		start = dbt->doff;
		needed = dbt->dlen;
	} else {
		start = 0;
		needed = tlen;
	}

	/* Allocate any necessary memory. */
	if (F_ISSET(dbt, DB_DBT_USERMEM)) {
		if (needed > dbt->ulen) {
			dbt->size = needed;
			return (ENOMEM);
		}
	} else if (F_ISSET(dbt, DB_DBT_MALLOC)) {
		if ((ret = __os_malloc(dbenv,
		    needed, dbp->db_malloc, &dbt->data)) != 0)
			return (ret);
	} else if (F_ISSET(dbt, DB_DBT_REALLOC)) {
		if ((ret = __os_realloc(dbenv,
		    needed, dbp->db_realloc, &dbt->data)) != 0)
			return (ret);
	} else if (*bpsz == 0 || *bpsz < needed) {
		if ((ret = __os_realloc(dbenv, needed, NULL, bpp)) != 0)
			return (ret);
		*bpsz = needed;
		dbt->data = *bpp;
	} else
		dbt->data = *bpp;

	/*
	 * Step through the linked list of pages, copying the data on each
	 * one into the buffer.  Never copy more than the total data length.
	 */
	dbt->size = needed;
	for (curoff = 0, p = dbt->data; pgno != PGNO_INVALID && needed > 0;) {
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0) {
			(void)__db_pgerr(dbp, pgno);
			return (ret);
		}
		/* Check if we need any bytes from this page. */
		if (curoff + OV_LEN(h) >= start) {
			src = (u_int8_t *)h + P_OVERHEAD;
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
		memp_fput(dbp->mpf, h, 0);
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
	PAGE *pagep, *lastp;
	DB_LSN new_lsn, null_lsn;
	DBT tmp_dbt;
	db_indx_t pagespace;
	u_int32_t sz;
	u_int8_t *p;
	int ret;

	/*
	 * Allocate pages and copy the key/data item into them.  Calculate the
	 * number of bytes we get for pages we fill completely with a single
	 * item.
	 */
	dbp = dbc->dbp;
	pagespace = P_MAXSPACE(dbp->pgsize);

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
			return (ret);
		if (DB_LOGGING(dbc)) {
			tmp_dbt.data = p;
			tmp_dbt.size = pagespace;
			ZERO_LSN(null_lsn);
			if ((ret = __db_big_log(dbp->dbenv, dbc->txn,
			    &new_lsn, 0, DB_ADD_BIG, dbp->log_fileid,
			    PGNO(pagep), lastp ? PGNO(lastp) : PGNO_INVALID,
			    PGNO_INVALID, &tmp_dbt, &LSN(pagep),
			    lastp == NULL ? &null_lsn : &LSN(lastp),
			    &null_lsn)) != 0)
				return (ret);

			/* Move lsn onto page. */
			if (lastp)
				LSN(lastp) = new_lsn;
			LSN(pagep) = new_lsn;
		}

		P_INIT(pagep, dbp->pgsize,
		    PGNO(pagep), PGNO_INVALID, PGNO_INVALID, 0, P_OVERFLOW);
		OV_LEN(pagep) = pagespace;
		OV_REF(pagep) = 1;
		memcpy((u_int8_t *)pagep + P_OVERHEAD, p, pagespace);

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
			(void)memp_fput(dbp->mpf, lastp, DB_MPOOL_DIRTY);
		}
		lastp = pagep;
	}
	(void)memp_fput(dbp->mpf, lastp, DB_MPOOL_DIRTY);
	return (0);
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
	PAGE *h;
	int ret;

	dbp = dbc->dbp;
	if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0) {
		(void)__db_pgerr(dbp, pgno);
		return (ret);
	}

	if (DB_LOGGING(dbc))
		if ((ret = __db_ovref_log(dbp->dbenv, dbc->txn,
		    &LSN(h), 0, dbp->log_fileid, h->pgno, adjust,
		    &LSN(h))) != 0)
			return (ret);
	OV_REF(h) += adjust;

	(void)memp_fput(dbp->mpf, h, DB_MPOOL_DIRTY);
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
	DBT tmp_dbt;
	int ret;

	dbp = dbc->dbp;
	do {
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &pagep)) != 0) {
			(void)__db_pgerr(dbp, pgno);
			return (ret);
		}

		DB_ASSERT(TYPE(pagep) == P_OVERFLOW);
		/*
		 * If it's referenced by more than one key/data item,
		 * decrement the reference count and return.
		 */
		if (OV_REF(pagep) > 1) {
			(void)memp_fput(dbp->mpf, pagep, 0);
			return (__db_ovref(dbc, pgno, -1));
		}

		if (DB_LOGGING(dbc)) {
			tmp_dbt.data = (u_int8_t *)pagep + P_OVERHEAD;
			tmp_dbt.size = OV_LEN(pagep);
			ZERO_LSN(null_lsn);
			if ((ret = __db_big_log(dbp->dbenv, dbc->txn,
			    &LSN(pagep), 0, DB_REM_BIG, dbp->log_fileid,
			    PGNO(pagep), PREV_PGNO(pagep), NEXT_PGNO(pagep),
			    &tmp_dbt, &LSN(pagep), &null_lsn, &null_lsn)) != 0)
				return (ret);
		}
		pgno = pagep->next_pgno;
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
	PAGE *pagep;
	DBT local_dbt;
	void *buf;
	u_int32_t bufsize, cmp_bytes, key_left;
	u_int8_t *p1, *p2;
	int ret;

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
		__os_free(buf, bufsize);
		return (0);
	}

	/* While there are both keys to compare. */
	for (*cmpp = 0, p1 = dbt->data,
	    key_left = dbt->size; key_left > 0 && pgno != PGNO_INVALID;) {
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &pagep)) != 0)
			return (ret);

		cmp_bytes = OV_LEN(pagep) < key_left ? OV_LEN(pagep) : key_left;
		tlen -= cmp_bytes;
		key_left -= cmp_bytes;
		for (p2 =
		    (u_int8_t *)pagep + P_OVERHEAD; cmp_bytes-- > 0; ++p1, ++p2)
			if (*p1 != *p2) {
				*cmpp = (long)*p1 - (long)*p2;
				break;
			}
		pgno = NEXT_PGNO(pagep);
		if ((ret = memp_fput(dbp->mpf, pagep, 0)) != 0)
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

/*
 * __db_vrfy_overflow --
 *	Verify overflow page.
 *
 * PUBLIC: int __db_vrfy_overflow __P((DB *, VRFY_DBINFO *, PAGE *, db_pgno_t,
 * PUBLIC:     u_int32_t));
 */
int
__db_vrfy_overflow(dbp, vdp, h, pgno, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	PAGE *h;
	db_pgno_t pgno;
	u_int32_t flags;
{
	VRFY_PAGEINFO *pip;
	int isbad, ret, t_ret;

	isbad = 0;
	if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
		return (ret);

	if ((ret = __db_vrfy_datapage(dbp, vdp, h, pgno, flags)) != 0) {
		if (ret == DB_VERIFY_BAD)
			isbad = 1;
		else
			goto err;
	}

	pip->refcount = OV_REF(h);
	if (pip->refcount < 1) {
		EPRINT((dbp->dbenv,
		    "Overflow page %lu has zero reference count",
		    (u_long)pgno));
		isbad = 1;
	}

	/* Just store for now. */
	pip->olen = HOFFSET(h);

err:	if ((t_ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
		ret = t_ret;
	return ((ret == 0 && isbad == 1) ? DB_VERIFY_BAD : ret);
}

/*
 * __db_vrfy_ovfl_structure --
 *	Walk a list of overflow pages, avoiding cycles and marking
 *	pages seen.
 *
 * PUBLIC: int __db_vrfy_ovfl_structure
 * PUBLIC:     __P((DB *, VRFY_DBINFO *, db_pgno_t, u_int32_t, u_int32_t));
 */
int
__db_vrfy_ovfl_structure(dbp, vdp, pgno, tlen, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	u_int32_t tlen;
	u_int32_t flags;
{
	DB *pgset;
	VRFY_PAGEINFO *pip;
	db_pgno_t next, prev;
	int isbad, p, ret, t_ret;
	u_int32_t refcount;

	pgset = vdp->pgset;
	DB_ASSERT(pgset != NULL);
	isbad = 0;

	/* This shouldn't happen, but just to be sure. */
	if (!IS_VALID_PGNO(pgno))
		return (DB_VERIFY_BAD);

	/*
	 * Check the first prev_pgno;  it ought to be PGNO_INVALID,
	 * since there's no prev page.
	 */
	if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
		return (ret);

	/* The refcount is stored on the first overflow page. */
	refcount = pip->refcount;

	if (pip->type != P_OVERFLOW) {
		EPRINT((dbp->dbenv,
		    "Overflow page %lu of invalid type",
		    (u_long)pgno, (u_long)pip->type));
		ret = DB_VERIFY_BAD;
		goto err;		/* Unsafe to continue. */
	}

	prev = pip->prev_pgno;
	if (prev != PGNO_INVALID) {
		EPRINT((dbp->dbenv,
		    "First overflow page %lu has a prev_pgno", (u_long)pgno));
		isbad = 1;
	}

	for (;;) {
		/*
		 * This is slightly gross.  Btree leaf pages reference
		 * individual overflow trees multiple times if the overflow page
		 * is the key to a duplicate set.  The reference count does not
		 * reflect this multiple referencing.  Thus, if this is called
		 * during the structure verification of a btree leaf page, we
		 * check to see whether we've seen it from a leaf page before
		 * and, if we have, adjust our count of how often we've seen it
		 * accordingly.
		 *
		 * (This will screw up if it's actually referenced--and
		 * correctly refcounted--from two different leaf pages, but
		 * that's a very unlikely brokenness that we're not checking for
		 * anyway.)
		 */

		if (LF_ISSET(ST_OVFL_LEAF)) {
			if (F_ISSET(pip, VRFY_OVFL_LEAFSEEN)) {
				if ((ret =
				    __db_vrfy_pgset_dec(pgset, pgno)) != 0)
					goto err;
			} else
				F_SET(pip, VRFY_OVFL_LEAFSEEN);
		}

		if ((ret = __db_vrfy_pgset_get(pgset, pgno, &p)) != 0)
			goto err;

		/*
		 * We may have seen this elsewhere, if the overflow entry
		 * has been promoted to an internal page.
		 */
		if ((u_int32_t)p > refcount) {
			EPRINT((dbp->dbenv,
			    "Page %lu encountered twice in overflow traversal",
			    (u_long)pgno));
			ret = DB_VERIFY_BAD;
			goto err;
		}
		if ((ret = __db_vrfy_pgset_inc(pgset, pgno)) != 0)
			goto err;

		/* Keep a running tab on how much of the item we've seen. */
		tlen -= pip->olen;

		/* Send feedback to the application about our progress. */
		if (!LF_ISSET(DB_SALVAGE))
			__db_vrfy_struct_feedback(dbp, vdp);

		next = pip->next_pgno;

		/* Are we there yet? */
		if (next == PGNO_INVALID)
			break;

		/*
		 * We've already checked this when we saved it, but just
		 * to be sure...
		 */
		if (!IS_VALID_PGNO(next)) {
			DB_ASSERT(0);
			EPRINT((dbp->dbenv,
			    "Overflow page %lu has bad next_pgno",
			    (u_long)pgno));
			ret = DB_VERIFY_BAD;
			goto err;
		}

		if ((ret = __db_vrfy_putpageinfo(vdp, pip)) != 0 ||
		    (ret = __db_vrfy_getpageinfo(vdp, next, &pip)) != 0)
			return (ret);
		if (pip->prev_pgno != pgno) {
			EPRINT((dbp->dbenv,
			    "Overflow page %lu has bogus prev_pgno value",
			    (u_long)next));
			isbad = 1;
			/*
			 * It's safe to continue because we have separate
			 * cycle detection.
			 */
		}

		pgno = next;
	}

	if (tlen > 0) {
		isbad = 1;
		EPRINT((dbp->dbenv,
		    "Overflow item incomplete on page %lu", (u_long)pgno));
	}

err:	if ((t_ret = __db_vrfy_putpageinfo(vdp, pip)) != 0 && ret == 0)
		ret = t_ret;
	return ((ret == 0 && isbad == 1) ? DB_VERIFY_BAD : ret);
}

/*
 * __db_safe_goff --
 *	Get an overflow item, very carefully, from an untrusted database,
 *	in the context of the salvager.
 *
 * PUBLIC: int __db_safe_goff __P((DB *, VRFY_DBINFO *, db_pgno_t,
 * PUBLIC:     DBT *, void **, u_int32_t));
 */
int
__db_safe_goff(dbp, vdp, pgno, dbt, buf, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	DBT *dbt;
	void **buf;
	u_int32_t flags;
{
	PAGE *h;
	int ret, err_ret;
	u_int32_t bytesgot, bytes;
	u_int8_t *src, *dest;

	ret = DB_VERIFY_BAD;
	err_ret = 0;
	bytesgot = bytes = 0;

	while ((pgno != PGNO_INVALID) && (IS_VALID_PGNO(pgno))) {
		/*
		 * Mark that we're looking at this page;  if we've seen it
		 * already, quit.
		 */
		if ((ret = __db_salvage_markdone(vdp, pgno)) != 0)
			break;

		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0)
			break;

		/*
		 * Make sure it's really an overflow page, unless we're
		 * being aggressive, in which case we pretend it is.
		 */
		if (!LF_ISSET(DB_AGGRESSIVE) && TYPE(h) != P_OVERFLOW) {
			ret = DB_VERIFY_BAD;
			break;
		}

		src = (u_int8_t *)h + P_OVERHEAD;
		bytes = OV_LEN(h);

		if (bytes + P_OVERHEAD > dbp->pgsize)
			bytes = dbp->pgsize - P_OVERHEAD;

		if ((ret = __os_realloc(dbp->dbenv,
		    bytesgot + bytes, 0, buf)) != 0)
			break;

		dest = (u_int8_t *)*buf + bytesgot;
		bytesgot += bytes;

		memcpy(dest, src, bytes);

		pgno = NEXT_PGNO(h);
		/* Not much we can do here--we don't want to quit. */
		if ((ret = memp_fput(dbp->mpf, h, 0)) != 0)
			err_ret = ret;
	}

	if (ret == 0) {
		dbt->size = bytesgot;
		dbt->data = *buf;
	}

	return ((err_ret != 0 && ret == 0) ? err_ret : ret);
}
