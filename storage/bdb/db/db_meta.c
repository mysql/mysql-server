/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
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
 * $Id: db_meta.c,v 12.22 2005/10/27 01:46:34 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/db_am.h"

static void __db_init_meta __P((DB *, void *, db_pgno_t, u_int32_t));
#ifdef HAVE_FTRUNCATE
static void __db_freelist_sort __P((struct pglist *, u_int32_t));
static int  __db_pglistcmp __P((const void *, const void *));
static int  __db_truncate_freelist __P((DBC *, DBMETA *,
      PAGE *, db_pgno_t *, u_int32_t, u_int32_t));
#endif

/*
 * __db_init_meta --
 *	Helper function for __db_new that initializes the important fields in
 * a meta-data page (used instead of P_INIT).  We need to make sure that we
 * retain the page number and LSN of the existing page.
 */
static void
__db_init_meta(dbp, p, pgno, pgtype)
	DB *dbp;
	void *p;
	db_pgno_t pgno;
	u_int32_t pgtype;
{
	DB_LSN save_lsn;
	DBMETA *meta;

	meta = (DBMETA *)p;
	save_lsn = meta->lsn;
	memset(meta, 0, sizeof(DBMETA));
	meta->lsn = save_lsn;
	meta->pagesize = dbp->pgsize;
	if (F_ISSET(dbp, DB_AM_CHKSUM))
		FLD_SET(meta->metaflags, DBMETA_CHKSUM);
	meta->pgno = pgno;
	meta->type = (u_int8_t)pgtype;
}

/*
 * __db_new --
 *	Get a new page, preferably from the freelist.
 *
 * PUBLIC: int __db_new __P((DBC *, u_int32_t, PAGE **));
 */
int
__db_new(dbc, type, pagepp)
	DBC *dbc;
	u_int32_t type;
	PAGE **pagepp;
{
	DBMETA *meta;
	DB *dbp;
	DB_LOCK metalock;
	DB_LSN lsn;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_pgno_t last, *list, pgno, newnext;
	u_int32_t meta_flags;
	int extend, ret, t_ret;

	meta = NULL;
	meta_flags = 0;
	dbp = dbc->dbp;
	mpf = dbp->mpf;
	h = NULL;
	newnext = PGNO_INVALID;

	pgno = PGNO_BASE_MD;
	if ((ret = __db_lget(dbc,
	    LCK_ALWAYS, pgno, DB_LOCK_WRITE, 0, &metalock)) != 0)
		goto err;
	if ((ret = __memp_fget(mpf, &pgno, 0, &meta)) != 0)
		goto err;
	last = meta->last_pgno;
	if (meta->free == PGNO_INVALID) {
		if (FLD_ISSET(type, P_DONTEXTEND)) {
			*pagepp = NULL;
			goto err;
		}
		last = pgno = meta->last_pgno + 1;
		ZERO_LSN(lsn);
		extend = 1;
	} else {
		pgno = meta->free;
		if ((ret = __memp_fget(mpf, &pgno, 0, &h)) != 0)
			goto err;

		/*
		 * We want to take the first page off the free list and
		 * then set meta->free to the that page's next_pgno, but
		 * we need to log the change first.
		 */
		newnext = h->next_pgno;
		lsn = h->lsn;
		extend = 0;
	}

	FLD_CLR(type, P_DONTEXTEND);

	/*
	 * Log the allocation before fetching the new page.  If we
	 * don't have room in the log then we don't want to tell
	 * mpool to extend the file.
	 */
	if (DBC_LOGGING(dbc)) {
		if ((ret = __db_pg_alloc_log(dbp, dbc->txn, &LSN(meta), 0,
		    &LSN(meta), PGNO_BASE_MD, &lsn,
		    pgno, (u_int32_t)type, newnext, meta->last_pgno)) != 0)
			goto err;
	} else
		LSN_NOT_LOGGED(LSN(meta));

	meta_flags = DB_MPOOL_DIRTY;
	meta->free = newnext;

	if (extend == 1) {
		if ((ret = __memp_fget(mpf, &pgno, DB_MPOOL_NEW, &h)) != 0)
			goto err;
		DB_ASSERT(last == pgno);
		meta->last_pgno = pgno;
		ZERO_LSN(h->lsn);
		h->pgno = pgno;
	}
	LSN(h) = LSN(meta);

	DB_ASSERT(TYPE(h) == P_INVALID);

	if (TYPE(h) != P_INVALID)
		return (__db_panic(dbp->dbenv, EINVAL));

	ret = __memp_fput(mpf, (PAGE *)meta, DB_MPOOL_DIRTY);
	meta = NULL;
	if ((t_ret = __TLPUT(dbc, metalock)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		goto err;

	switch (type) {
		case P_BTREEMETA:
		case P_HASHMETA:
		case P_QAMMETA:
			__db_init_meta(dbp, h, h->pgno, type);
			break;
		default:
			P_INIT(h, dbp->pgsize,
			    h->pgno, PGNO_INVALID, PGNO_INVALID, 0, type);
			break;
	}

	/* Fix up the sorted free list if necessary. */
#ifdef HAVE_FTRUNCATE
	if (extend == 0) {
		u_int32_t nelems = 0;

		if ((ret = __memp_get_freelist(dbp->mpf, &nelems, &list)) != 0)
			goto err;
		if (nelems != 0) {
			DB_ASSERT(h->pgno == list[0]);
			memmove(list, &list[1], (nelems - 1) * sizeof(*list));
			if ((ret = __memp_extend_freelist(
			    dbp->mpf, nelems - 1, &list)) != 0)
				goto err;
		}
	}
#else
	COMPQUIET(list, NULL);
#endif

	/*
	 * If dirty reads are enabled and we are in a transaction, we could
	 * abort this allocation after the page(s) pointing to this
	 * one have their locks downgraded.  This would permit dirty readers
	 * to access this page which is ok, but they must be off the
	 * page when we abort.  We never lock overflow pages or off page
	 * duplicate trees.
	 */
	if (type != P_OVERFLOW && !F_ISSET(dbc, DBC_OPD) &&
	     F_ISSET(dbc->dbp, DB_AM_READ_UNCOMMITTED) && dbc->txn != NULL) {
		if ((ret = __db_lget(dbc, 0,
		    h->pgno, DB_LOCK_WWRITE, 0, &metalock)) != 0)
			goto err;
	}

	*pagepp = h;
	return (0);

err:	if (h != NULL)
		(void)__memp_fput(mpf, h, 0);
	if (meta != NULL)
		(void)__memp_fput(mpf, meta, meta_flags);
	(void)__TLPUT(dbc, metalock);
	return (ret);
}

/*
 * __db_free --
 *	Add a page to the head of the freelist.
 *
 * PUBLIC: int __db_free __P((DBC *, PAGE *));
 */
int
__db_free(dbc, h)
	DBC *dbc;
	PAGE *h;
{
	DBMETA *meta;
	DB *dbp;
	DBT ddbt, ldbt;
	DB_LOCK metalock;
	DB_MPOOLFILE *mpf;
	db_pgno_t last_pgno, *lp, next_pgno, pgno, prev_pgno;
	u_int32_t dirty_flag, lflag, nelem;
	int do_truncate, ret, t_ret;
#ifdef HAVE_FTRUNCATE
	db_pgno_t *list;
	u_int32_t position, start;
#endif

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	prev_pgno = PGNO_INVALID;
	nelem = 0;
	meta = NULL;
	do_truncate = 0;
	lp = NULL;

	/*
	 * Retrieve the metadata page.  If we are not keeping a sorted
	 * free list put the page at the head of the the free list.
	 * If we are keeping a sorted free list, for truncation,
	 * then figure out where this page belongs and either
	 * link it in or truncate the file as much as possible.
	 * If either the lock get or page get routines
	 * fail, then we need to put the page with which we were called
	 * back because our caller assumes we take care of it.
	 */
	dirty_flag = 0;
	pgno = PGNO_BASE_MD;
	if ((ret = __db_lget(dbc,
	    LCK_ALWAYS, pgno, DB_LOCK_WRITE, 0, &metalock)) != 0)
		goto err;
	if ((ret = __memp_fget(mpf, &pgno, 0, &meta)) != 0)
		goto err1;

	last_pgno = meta->last_pgno;
	next_pgno = meta->free;

	DB_ASSERT(h->pgno != next_pgno);

#ifdef HAVE_FTRUNCATE
	/*
	 * If we are maintaining a sorted free list see if we either have a
	 * new truncation point or the page goes somewhere in the middle of
	 * the list.  If it goes in the middle of the list, we will drop the
	 * meta page and get the previous page.
	 */
	if ((ret = __memp_get_freelist(mpf, &nelem, &list)) != 0)
		goto err;
	if (list == NULL)
		goto no_sort;

	if (h->pgno != last_pgno) {
		/*
		 * Put the page number in the sorted list.
		 * Finds its position and the previous page,
		 * extend the list, make room and insert.
		 */
		position = 0;
		if (nelem != 0) {
			__db_freelist_pos(h->pgno, list, nelem, &position);

			DB_ASSERT(h->pgno != list[position]);

			/* Get the previous page if this is not the smallest. */
			if (position != 0 || h->pgno > list[0])
				prev_pgno = list[position];
		}

		/* Put the page number into the list. */
		if ((ret = __memp_extend_freelist(mpf, nelem + 1, &list)) != 0)
			return (ret);
		if (prev_pgno != PGNO_INVALID)
			lp = &list[position + 1];
		else
			lp = list;
		if (nelem != 0 && position != nelem)
			memmove(lp + 1, lp,
			    (size_t)((u_int8_t*)&list[nelem] - (u_int8_t*)lp));
		*lp = h->pgno;
	} else if (nelem != 0) {
		/* Find the truncation point. */
		for (lp = &list[nelem - 1]; lp >= list; lp--)
			if (--last_pgno != *lp)
				break;
		if (lp < list || last_pgno < h->pgno - 1)
			do_truncate = 1;
		last_pgno = meta->last_pgno;
	}

no_sort:
	if (prev_pgno != PGNO_INVALID) {
		if ((ret = __memp_fput(mpf, meta, 0)) != 0)
			goto err1;
		meta = NULL;
		pgno = prev_pgno;
		if ((ret = __memp_fget(mpf, &pgno, 0, &meta)) != 0)
			goto err1;
		next_pgno = NEXT_PGNO(meta);
	}
#endif

	/* Log the change. */
	if (DBC_LOGGING(dbc)) {
		memset(&ldbt, 0, sizeof(ldbt));
		ldbt.data = h;
		ldbt.size = P_OVERHEAD(dbp);
		switch (h->type) {
		case P_HASH:
		case P_IBTREE:
		case P_IRECNO:
		case P_LBTREE:
		case P_LRECNO:
		case P_LDUP:
			if (h->entries > 0) {
				ldbt.size += h->entries * sizeof(db_indx_t);
				ddbt.data = (u_int8_t *)h + HOFFSET(h);
				ddbt.size = dbp->pgsize - HOFFSET(h);
				if ((ret = __db_pg_freedata_log(dbp, dbc->txn,
				     &LSN(meta), 0, h->pgno, &LSN(meta), pgno,
				     &ldbt, next_pgno, last_pgno, &ddbt)) != 0)
					goto err1;
				goto logged;
			}
			break;
		case P_HASHMETA:
			ldbt.size = sizeof(HMETA);
			break;
		case P_BTREEMETA:
			ldbt.size = sizeof(BTMETA);
			break;
		case P_OVERFLOW:
			ldbt.size += OV_LEN(h);
			break;
		default:
			DB_ASSERT(h->type != P_QAMDATA);
		}

		/*
		 * If we are truncating the file, we need to make sure
		 * the logging happens before the truncation.  If we
		 * are truncating multiple pages we don't need to flush the
		 * log here as it will be flushed by __db_truncate_freelist.
		 */
		lflag = 0;
#ifdef HAVE_FTRUNCATE
		if (do_truncate == 0 && h->pgno == last_pgno)
			lflag = DB_FLUSH;
#endif
		if ((ret = __db_pg_free_log(dbp,
		      dbc->txn, &LSN(meta), lflag, h->pgno,
		      &LSN(meta), pgno, &ldbt, next_pgno, last_pgno)) != 0)
			goto err1;
	} else
		LSN_NOT_LOGGED(LSN(meta));
logged:	LSN(h) = LSN(meta);

#ifdef HAVE_FTRUNCATE
	if (do_truncate) {
		start = (u_int32_t) (lp - list) + 1;
		meta->last_pgno--;
		ret = __db_truncate_freelist(
		      dbc, meta, h, list, start, nelem);
		h = NULL;
	} else if (h->pgno == last_pgno) {
		if ((ret = __memp_fput(mpf, h, DB_MPOOL_DISCARD)) != 0)
			goto err;
		/* Give the page back to the OS. */
		if ((ret = __memp_ftruncate(mpf, last_pgno, 0)) != 0)
			goto err;
		DB_ASSERT(meta->pgno == PGNO_BASE_MD);
		meta->last_pgno--;
		h = NULL;
	} else
#endif

	{
		/*
		 * If we are not truncating the page then we
		 * reinitialize it and put it at the head of
		 * the free list.
		 */
		P_INIT(h, dbp->pgsize,
		    h->pgno, PGNO_INVALID, next_pgno, 0, P_INVALID);
#ifdef DIAGNOSTIC
		memset((u_int8_t *) h + P_OVERHEAD(dbp),
		    CLEAR_BYTE, dbp->pgsize - P_OVERHEAD(dbp));
#endif
		if (prev_pgno == PGNO_INVALID)
			meta->free = h->pgno;
		else
			NEXT_PGNO(meta) = h->pgno;
	}

	/* Discard the metadata or previous page. */
err1:	if (meta != NULL && (t_ret =
	    __memp_fput(mpf, (PAGE *)meta, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __TLPUT(dbc, metalock)) != 0 && ret == 0)
		ret = t_ret;

	/* Discard the caller's page reference. */
	dirty_flag = DB_MPOOL_DIRTY;
err:	if (h != NULL &&
	    (t_ret = __memp_fput(mpf, h, dirty_flag)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * XXX
	 * We have to unlock the caller's page in the caller!
	 */
	return (ret);
}

#ifdef HAVE_FTRUNCATE
/*
 * __db_freelist_pos -- find the position of a page in the freelist.
 *	The list is sorted, we do a binary search.
 *
 * PUBLIC: #ifdef HAVE_FTRUNCATE
 * PUBLIC: void __db_freelist_pos __P((db_pgno_t,
 * PUBLIC:       db_pgno_t *, u_int32_t, u_int32_t *));
 * PUBLIC: #endif
 */
void
__db_freelist_pos(pgno, list, nelem, posp)
	db_pgno_t pgno;
	db_pgno_t *list;
	u_int32_t nelem;
	u_int32_t *posp;
{
	u_int32_t base, indx, lim;

	indx = 0;
	for (base = 0, lim = nelem; lim != 0; lim >>= 1) {
		indx = base + (lim >> 1);
		if (pgno == list[indx]) {
			*posp = indx;
			return;
		}
		if (pgno > list[indx]) {
			base = indx + 1;
			--lim;
		}
	}
	if (base != 0)
		base--;
	*posp = base;
	return;
}

static int
__db_pglistcmp(a, b)
	const void *a, *b;
{
	struct pglist *ap, *bp;

	ap = (struct pglist *)a;
	bp = (struct pglist *)b;

	return ((ap->pgno > bp->pgno) ? 1 : (ap->pgno < bp->pgno) ? -1: 0);
}

/*
 * __db_freelist_sort -- sort a list of free pages.
 */
static void
__db_freelist_sort(list, nelems)
	struct pglist *list;
	u_int32_t nelems;
{
	qsort(list, (size_t)nelems, sizeof(struct pglist), __db_pglistcmp);
}

/*
 * __db_pg_truncate -- sort the freelist and find the truncation point.
 *
 * PUBLIC: #ifdef HAVE_FTRUNCATE
 * PUBLIC: int __db_pg_truncate __P((DB_MPOOLFILE *, struct pglist *list,
 * PUBLIC:    DB_COMPACT *, u_int32_t *, db_pgno_t *, DB_LSN *, int));
 * PUBLIC: #endif
 */
int
__db_pg_truncate(mpf, list, c_data, nelemp, last_pgno, lsnp, in_recovery)
	DB_MPOOLFILE *mpf;
	struct pglist *list;
	DB_COMPACT *c_data;
	u_int32_t *nelemp;
	db_pgno_t *last_pgno;
	DB_LSN *lsnp;
	int in_recovery;
{
	PAGE *h;
	struct pglist *lp;
	db_pgno_t pgno;
	u_int32_t nelems;
	int modified, ret;

	ret = 0;

	nelems = *nelemp;
	/* Sort the list */
	__db_freelist_sort(list, nelems);

	/* Find the truncation point. */
	pgno = *last_pgno;
	lp = &list[nelems - 1];
	while (nelems != 0) {
		if (lp->pgno != pgno)
			break;
		pgno--;
		nelems--;
		lp--;
	}

	/*
	 * Figure out what (if any) pages can be truncated immediately and
	 * record the place from which we can truncate, so we can do the
	 * memp_ftruncate below.  We also use this to avoid ever putting
	 * these pages on the freelist, which we are about to relink.
	 */
	for (lp = list; lp < &list[nelems]; lp++) {
		if ((ret = __memp_fget(mpf, &lp->pgno, 0, &h)) != 0) {
			/* Page may have been truncated later. */
			if (in_recovery && ret == DB_PAGE_NOTFOUND) {
				ret = 0;
				continue;
			}
			goto err;
		}
		modified = 0;
		if (!in_recovery || log_compare(&LSN(h), &lp->lsn) == 0) {
			if (lp == &list[nelems - 1])
				NEXT_PGNO(h) = PGNO_INVALID;
			else
				NEXT_PGNO(h) = lp[1].pgno;
			DB_ASSERT(NEXT_PGNO(h) < *last_pgno);

			LSN(h) = *lsnp;
			modified = 1;
		}
		if ((ret = __memp_fput(mpf, h,
		    modified ? DB_MPOOL_DIRTY : 0)) != 0)
			goto err;
	}

	if (pgno != *last_pgno) {
		if ((ret = __memp_ftruncate(mpf,
		    pgno + 1, in_recovery ? MP_TRUNC_RECOVER : 0)) != 0)
			goto err;
		if (c_data)
			c_data->compact_pages_truncated += *last_pgno - pgno;
		*last_pgno = pgno;
	}
	*nelemp = nelems;

err:	return (ret);
}

/*
 * __db_free_truncate --
 *	Truncate free pages at the end of the file.
 *
 * PUBLIC: #ifdef HAVE_FTRUNCATE
 * PUBLIC: int __db_free_truncate __P((DB *, DB_TXN *, u_int32_t,
 * PUBLIC:    DB_COMPACT *, struct pglist **, u_int32_t *, db_pgno_t *));
 * PUBLIC: #endif
 */
int
__db_free_truncate(dbp, txn, flags, c_data, listp, nelemp, last_pgnop)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t flags;
	DB_COMPACT *c_data;
	struct pglist **listp;
	u_int32_t *nelemp;
	db_pgno_t *last_pgnop;
{
	DBC *dbc;
	DB_ENV *dbenv;
	DBMETA *meta;
	DBT ddbt;
	DB_LOCK metalock;
	DB_LSN null_lsn;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_pgno_t pgno;
	u_int32_t nelems;
	struct pglist *list, *lp;
	int ret, t_ret;
	size_t size;

	COMPQUIET(flags, 0);
	list = NULL;
	meta = NULL;
	dbenv = dbp->dbenv;
	mpf = dbp->mpf;
	h = NULL;
	nelems = 0;
	if (listp != NULL) {
		*listp = NULL;
		DB_ASSERT(nelemp != NULL);
		*nelemp = 0;
	}

	if ((ret = __db_cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		return (ret);

	pgno = PGNO_BASE_MD;
	if ((ret = __db_lget(dbc,
	    LCK_ALWAYS, pgno, DB_LOCK_WRITE, 0, &metalock)) != 0)
		goto err;
	if ((ret = __memp_fget(mpf, &pgno, 0, &meta)) != 0)
		goto err;

	if (last_pgnop != NULL)
		*last_pgnop = meta->last_pgno;
	if ((pgno = meta->free) == PGNO_INVALID)
		goto done;

	size = 128;
	if ((ret = __os_malloc(dbenv, size * sizeof(*list), &list)) != 0)
		goto err;
	lp = list;

	do {
		if (lp == &list[size]) {
			size *= 2;
			if ((ret = __os_realloc(dbenv,
			    size * sizeof(*list), &list)) != 0)
				goto err;
			lp = &list[size / 2];
		}
		if ((ret = __memp_fget(mpf, &pgno, 0, &h)) != 0)
			goto err;

		lp->pgno = pgno;
		lp->lsn = LSN(h);
		pgno = NEXT_PGNO(h);
		if ((ret = __memp_fput(mpf, h, 0)) != 0)
			goto err;
		lp++;
	} while (pgno != PGNO_INVALID);
	nelems = (u_int32_t)(lp - list);

	/* Log the current state of the free list */
	if (DBC_LOGGING(dbc)) {
		ddbt.data = list;
		ddbt.size = nelems * sizeof(*lp);
		ZERO_LSN(null_lsn);
		if ((ret = __db_pg_sort_log(dbp,
		     dbc->txn, &LSN(meta), DB_FLUSH, PGNO_BASE_MD, &LSN(meta),
		     PGNO_INVALID, &null_lsn, meta->last_pgno, &ddbt)) != 0)
			goto err;
	} else
		LSN_NOT_LOGGED(LSN(meta));

	if ((ret = __db_pg_truncate(mpf, list, c_data,
	    &nelems, &meta->last_pgno, &LSN(meta), 0)) != 0)
		goto err;

	if (nelems == 0)
		meta->free = PGNO_INVALID;
	else
		meta->free = list[0].pgno;

done:	if (last_pgnop != NULL)
		*last_pgnop = meta->last_pgno;

	/*
	 * The truncate point is the number of pages in the free
	 * list back from the last page.  The number of pages
	 * in the free list are the number that we can swap in.
	 */
	if (c_data)
		c_data->compact_truncate = (u_int32_t)meta->last_pgno - nelems;

	if (nelems != 0 && listp != NULL) {
		*listp = list;
		*nelemp = nelems;
		list = NULL;
	}

err:	if (list != NULL)
		__os_free(dbenv, list);
	if (meta != NULL && (t_ret =
	     __memp_fput(mpf, (PAGE *)meta, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __TLPUT(dbc, metalock)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

static int
__db_truncate_freelist(dbc, meta, h, list, start, nelem)
	DBC *dbc;
	DBMETA *meta;
	PAGE *h;
	db_pgno_t *list;
	u_int32_t start, nelem;
{
	DB *dbp;
	DB_LSN null_lsn;
	DB_MPOOLFILE *mpf;
	DBT ddbt;
	PAGE *last_free, *pg;
	db_pgno_t *lp;
	struct pglist *plist, *pp;
	int ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	plist = NULL;
	last_free = NULL;

	if (start != 0 &&
	    (ret = __memp_fget(mpf, &list[start - 1], 0, &last_free)) != 0)
		goto err;

	if (DBC_LOGGING(dbc)) {
		if ((ret = __os_malloc(dbp->dbenv,
		     (nelem - start) * sizeof(*pp), &plist)) != 0)
			goto err;

		pp = plist;
		for (lp = &list[start]; lp < &list[nelem]; lp++) {
			pp->pgno = *lp;
			if ((ret = __memp_fget(mpf, lp, 0, &pg)) != 0)
				goto err;
			pp->lsn = LSN(pg);
			if ((ret = __memp_fput(mpf, pg, DB_MPOOL_DISCARD)) != 0)
				goto err;
			pp++;
		}
		ddbt.data = plist;
		ddbt.size = (nelem - start) * sizeof(*pp);
		ZERO_LSN(null_lsn);
		if (last_free != NULL) {
			if ((ret = __db_pg_sort_log(dbp, dbc->txn, &LSN(meta),
			     DB_FLUSH, PGNO(meta), &LSN(meta), PGNO(last_free),
			     &LSN(last_free), meta->last_pgno, &ddbt)) != 0)
				goto err;
		} else if ((ret = __db_pg_sort_log(dbp, dbc->txn,
		     &LSN(meta), DB_FLUSH, PGNO(meta), &LSN(meta),
		     PGNO_INVALID, &null_lsn, meta->last_pgno, &ddbt)) != 0)
			goto err;
	} else
		LSN_NOT_LOGGED(LSN(meta));
	if (last_free != NULL)
		LSN(last_free) = LSN(meta);

	if ((ret = __memp_fput(mpf, h, DB_MPOOL_DISCARD)) != 0)
		goto err;
	h = NULL;
	if ((ret = __memp_ftruncate(mpf, list[start], 0)) != 0)
		goto err;
	meta->last_pgno = list[start] - 1;

	if (start == 0)
		meta->free = PGNO_INVALID;
	else {
		NEXT_PGNO(last_free) = PGNO_INVALID;
		if ((ret = __memp_fput(mpf, last_free, DB_MPOOL_DIRTY)) != 0)
			goto err;
		last_free = NULL;
	}

	/* Shrink the number of elements in the list. */
	ret = __memp_extend_freelist(mpf, start, &list);

err:	if (plist != NULL)
		__os_free(dbp->dbenv, plist);

	/* We need to put the page on error. */
	if (h != NULL)
		(void)__memp_fput(mpf, h, 0);
	if (last_free != NULL)
		(void)__memp_fput(mpf, last_free, 0);

	return (ret);
}
#endif

#ifdef DEBUG
/*
 * __db_lprint --
 *	Print out the list of locks currently held by a cursor.
 *
 * PUBLIC: int __db_lprint __P((DBC *));
 */
int
__db_lprint(dbc)
	DBC *dbc;
{
	DB_ENV *dbenv;
	DB *dbp;
	DB_LOCKREQ req;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	if (LOCKING_ON(dbenv)) {
		req.op = DB_LOCK_DUMP;
		(void)__lock_vec(dbenv, dbc->locker, 0, &req, 1, NULL);
	}
	return (0);
}
#endif

/*
 * __db_lget --
 *	The standard lock get call.
 *
 * PUBLIC: int __db_lget __P((DBC *,
 * PUBLIC:     int, db_pgno_t, db_lockmode_t, u_int32_t, DB_LOCK *));
 */
int
__db_lget(dbc, action, pgno, mode, lkflags, lockp)
	DBC *dbc;
	int action;
	db_pgno_t pgno;
	db_lockmode_t mode;
	u_int32_t lkflags;
	DB_LOCK *lockp;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_LOCKREQ couple[3], *reqp;
	DB_TXN *txn;
	int has_timeout, i, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	txn = dbc->txn;

	/*
	 * We do not always check if we're configured for locking before
	 * calling __db_lget to acquire the lock.
	 */
	if (CDB_LOCKING(dbenv) ||
	    !LOCKING_ON(dbenv) || F_ISSET(dbc, DBC_COMPENSATE) ||
	    (F_ISSET(dbc, DBC_RECOVER) &&
	    (action != LCK_ROLLBACK || IS_REP_CLIENT(dbenv))) ||
	    (action != LCK_ALWAYS && F_ISSET(dbc, DBC_OPD))) {
		LOCK_INIT(*lockp);
		return (0);
	}

	dbc->lock.pgno = pgno;
	if (lkflags & DB_LOCK_RECORD)
		dbc->lock.type = DB_RECORD_LOCK;
	else
		dbc->lock.type = DB_PAGE_LOCK;
	lkflags &= ~DB_LOCK_RECORD;
	if (action == LCK_ROLLBACK)
		lkflags |= DB_LOCK_ABORT;

	/*
	 * If the transaction enclosing this cursor has DB_LOCK_NOWAIT set,
	 * pass that along to the lock call.
	 */
	if (DB_NONBLOCK(dbc))
		lkflags |= DB_LOCK_NOWAIT;

	if (F_ISSET(dbc, DBC_READ_UNCOMMITTED) && mode == DB_LOCK_READ)
		mode = DB_LOCK_READ_UNCOMMITTED;

	has_timeout = F_ISSET(dbc, DBC_RECOVER) ||
	    (txn != NULL && F_ISSET(txn, TXN_LOCKTIMEOUT));

	/*
	 * Transactional locking.
	 * Hold on to the previous read lock only if we are in full isolation.
	 * COUPLE_ALWAYS indicates we are holding an interior node which need
	 *	not be isolated.
	 * Downgrade write locks if we are supporting dirty readers.
	 */
	if ((action != LCK_COUPLE && action != LCK_COUPLE_ALWAYS) ||
	    !LOCK_ISSET(*lockp))
		action = 0;
	else if (dbc->txn == NULL || action == LCK_COUPLE_ALWAYS)
		action = LCK_COUPLE;
	else if (F_ISSET(dbc,
	    DBC_READ_COMMITTED) && lockp->mode == DB_LOCK_READ)
		action = LCK_COUPLE;
	else if (F_ISSET(dbc,
	    DBC_READ_UNCOMMITTED) && lockp->mode == DB_LOCK_READ_UNCOMMITTED)
		action = LCK_COUPLE;
	else if (F_ISSET(dbc->dbp,
	    DB_AM_READ_UNCOMMITTED) && lockp->mode == DB_LOCK_WRITE)
		action = LCK_DOWNGRADE;
	else
		action = 0;

	i = 0;
	switch (action) {
	default:
		if (has_timeout)
			goto couple;
		ret = __lock_get(dbenv,
		    dbc->locker, lkflags, &dbc->lock_dbt, mode, lockp);
		break;

	case LCK_DOWNGRADE:
		couple[0].op = DB_LOCK_GET;
		couple[0].obj = NULL;
		couple[0].lock = *lockp;
		couple[0].mode = DB_LOCK_WWRITE;
		UMRW_SET(couple[0].timeout);
		i++;
		/* FALLTHROUGH */
	case LCK_COUPLE:
couple:		couple[i].op = has_timeout? DB_LOCK_GET_TIMEOUT : DB_LOCK_GET;
		couple[i].obj = &dbc->lock_dbt;
		couple[i].mode = mode;
		UMRW_SET(couple[i].timeout);
		i++;
		if (has_timeout)
			couple[0].timeout =
			     F_ISSET(dbc, DBC_RECOVER) ? 0 : txn->lock_timeout;
		if (action == LCK_COUPLE || action == LCK_DOWNGRADE) {
			couple[i].op = DB_LOCK_PUT;
			couple[i].lock = *lockp;
			i++;
		}

		ret = __lock_vec(dbenv,
		    dbc->locker, lkflags, couple, i, &reqp);
		if (ret == 0 || reqp == &couple[i - 1])
			*lockp = i == 1 ? couple[0].lock : couple[i - 2].lock;
		break;
	}

	if (txn != NULL && ret == DB_LOCK_DEADLOCK)
		F_SET(txn, TXN_DEADLOCK);
	return ((ret == DB_LOCK_NOTGRANTED &&
	     !F_ISSET(dbenv, DB_ENV_TIME_NOTGRANTED)) ? DB_LOCK_DEADLOCK : ret);
}

/*
 * __db_lput --
 *	The standard lock put call.
 *
 * PUBLIC: int __db_lput __P((DBC *, DB_LOCK *));
 */
int
__db_lput(dbc, lockp)
	DBC *dbc;
	DB_LOCK *lockp;
{
	DB_ENV *dbenv;
	DB_LOCKREQ couple[2], *reqp;
	int action, ret;

	/*
	 * Transactional locking.
	 * Hold on to the read locks only if we are in full isolation.
	 * Downgrade write locks if we are supporting dirty readers.
	 */
	if (F_ISSET(dbc->dbp,
	    DB_AM_READ_UNCOMMITTED) && lockp->mode == DB_LOCK_WRITE)
		action = LCK_DOWNGRADE;
	else if (dbc->txn == NULL)
		action = LCK_COUPLE;
	else if (F_ISSET(dbc,
	    DBC_READ_COMMITTED) && lockp->mode == DB_LOCK_READ)
		action = LCK_COUPLE;
	else if (F_ISSET(dbc,
	    DBC_READ_UNCOMMITTED) && lockp->mode == DB_LOCK_READ_UNCOMMITTED)
		action = LCK_COUPLE;
	else
		action = 0;

	dbenv = dbc->dbp->dbenv;
	switch (action) {
	case LCK_COUPLE:
		ret = __lock_put(dbenv, lockp);
		break;
	case LCK_DOWNGRADE:
		couple[0].op = DB_LOCK_GET;
		couple[0].obj = NULL;
		couple[0].mode = DB_LOCK_WWRITE;
		couple[0].lock = *lockp;
		UMRW_SET(couple[0].timeout);
		couple[1].op = DB_LOCK_PUT;
		couple[1].lock = *lockp;
		ret = __lock_vec(dbenv, dbc->locker, 0, couple, 2, &reqp);
		if (ret == 0 || reqp == &couple[1])
			*lockp = couple[0].lock;
		break;
	default:
		ret = 0;
		break;
	}

	return (ret);
}
