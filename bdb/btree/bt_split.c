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
static const char revid[] = "$Id: bt_split.c,v 11.31 2000/12/22 19:08:27 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <limits.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "lock.h"
#include "btree.h"

static int __bam_broot __P((DBC *, PAGE *, PAGE *, PAGE *));
static int __bam_page __P((DBC *, EPG *, EPG *));
static int __bam_pinsert __P((DBC *, EPG *, PAGE *, PAGE *, int));
static int __bam_psplit __P((DBC *, EPG *, PAGE *, PAGE *, db_indx_t *));
static int __bam_root __P((DBC *, EPG *));
static int __ram_root __P((DBC *, PAGE *, PAGE *, PAGE *));

/*
 * __bam_split --
 *	Split a page.
 *
 * PUBLIC: int __bam_split __P((DBC *, void *));
 */
int
__bam_split(dbc, arg)
	DBC *dbc;
	void *arg;
{
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	enum { UP, DOWN } dir;
	db_pgno_t root_pgno;
	int exact, level, ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	root_pgno = cp->root;

	/*
	 * The locking protocol we use to avoid deadlock to acquire locks by
	 * walking down the tree, but we do it as lazily as possible, locking
	 * the root only as a last resort.  We expect all stack pages to have
	 * been discarded before we're called; we discard all short-term locks.
	 *
	 * When __bam_split is first called, we know that a leaf page was too
	 * full for an insert.  We don't know what leaf page it was, but we
	 * have the key/recno that caused the problem.  We call XX_search to
	 * reacquire the leaf page, but this time get both the leaf page and
	 * its parent, locked.  We then split the leaf page and see if the new
	 * internal key will fit into the parent page.  If it will, we're done.
	 *
	 * If it won't, we discard our current locks and repeat the process,
	 * only this time acquiring the parent page and its parent, locked.
	 * This process repeats until we succeed in the split, splitting the
	 * root page as the final resort.  The entire process then repeats,
	 * as necessary, until we split a leaf page.
	 *
	 * XXX
	 * A traditional method of speeding this up is to maintain a stack of
	 * the pages traversed in the original search.  You can detect if the
	 * stack is correct by storing the page's LSN when it was searched and
	 * comparing that LSN with the current one when it's locked during the
	 * split.  This would be an easy change for this code, but I have no
	 * numbers that indicate it's worthwhile.
	 */
	t = dbp->bt_internal;
	for (dir = UP, level = LEAFLEVEL;; dir == UP ? ++level : --level) {
		/*
		 * Acquire a page and its parent, locked.
		 */
		if ((ret = (dbc->dbtype == DB_BTREE ?
		    __bam_search(dbc, arg, S_WRPAIR, level, NULL, &exact) :
		    __bam_rsearch(dbc,
			(db_recno_t *)arg, S_WRPAIR, level, &exact))) != 0)
			return (ret);

		/*
		 * Split the page if it still needs it (it's possible another
		 * thread of control has already split the page).  If we are
		 * guaranteed that two items will fit on the page, the split
		 * is no longer necessary.
		 */
		if (2 * B_MAXSIZEONPAGE(cp->ovflsize)
		    <= (db_indx_t)P_FREESPACE(cp->csp[0].page)) {
			__bam_stkrel(dbc, STK_NOLOCK);
			return (0);
		}
		ret = cp->csp[0].page->pgno == root_pgno ?
		    __bam_root(dbc, &cp->csp[0]) :
		    __bam_page(dbc, &cp->csp[-1], &cp->csp[0]);
		BT_STK_CLR(cp);

		switch (ret) {
		case 0:
			/* Once we've split the leaf page, we're done. */
			if (level == LEAFLEVEL)
				return (0);

			/* Switch directions. */
			if (dir == UP)
				dir = DOWN;
			break;
		case DB_NEEDSPLIT:
			/*
			 * It's possible to fail to split repeatedly, as other
			 * threads may be modifying the tree, or the page usage
			 * is sufficiently bad that we don't get enough space
			 * the first time.
			 */
			if (dir == DOWN)
				dir = UP;
			break;
		default:
			return (ret);
		}
	}
	/* NOTREACHED */
}

/*
 * __bam_root --
 *	Split the root page of a btree.
 */
static int
__bam_root(dbc, cp)
	DBC *dbc;
	EPG *cp;
{
	DB *dbp;
	DBT log_dbt;
	DB_LSN log_lsn;
	PAGE *lp, *rp;
	db_indx_t split;
	u_int32_t opflags;
	int ret;

	dbp = dbc->dbp;

	/* Yeah, right. */
	if (cp->page->level >= MAXBTREELEVEL) {
		__db_err(dbp->dbenv,
		    "Too many btree levels: %d", cp->page->level);
		ret = ENOSPC;
		goto err;
	}

	/* Create new left and right pages for the split. */
	lp = rp = NULL;
	if ((ret = __db_new(dbc, TYPE(cp->page), &lp)) != 0 ||
	    (ret = __db_new(dbc, TYPE(cp->page), &rp)) != 0)
		goto err;
	P_INIT(lp, dbp->pgsize, lp->pgno,
	    PGNO_INVALID, ISINTERNAL(cp->page) ? PGNO_INVALID : rp->pgno,
	    cp->page->level, TYPE(cp->page));
	P_INIT(rp, dbp->pgsize, rp->pgno,
	    ISINTERNAL(cp->page) ?  PGNO_INVALID : lp->pgno, PGNO_INVALID,
	    cp->page->level, TYPE(cp->page));

	/* Split the page. */
	if ((ret = __bam_psplit(dbc, cp, lp, rp, &split)) != 0)
		goto err;

	/* Log the change. */
	if (DB_LOGGING(dbc)) {
		memset(&log_dbt, 0, sizeof(log_dbt));
		log_dbt.data = cp->page;
		log_dbt.size = dbp->pgsize;
		ZERO_LSN(log_lsn);
		opflags = F_ISSET(
		    (BTREE_CURSOR *)dbc->internal, C_RECNUM) ? SPL_NRECS : 0;
		if ((ret = __bam_split_log(dbp->dbenv, dbc->txn,
		    &LSN(cp->page), 0, dbp->log_fileid, PGNO(lp), &LSN(lp),
		    PGNO(rp), &LSN(rp), (u_int32_t)NUM_ENT(lp), 0, &log_lsn,
		    dbc->internal->root, &log_dbt, opflags)) != 0)
			goto err;
		LSN(lp) = LSN(cp->page);
		LSN(rp) = LSN(cp->page);
	}

	/* Clean up the new root page. */
	if ((ret = (dbc->dbtype == DB_RECNO ?
	    __ram_root(dbc, cp->page, lp, rp) :
	    __bam_broot(dbc, cp->page, lp, rp))) != 0)
		goto err;

	/* Adjust any cursors. */
	if ((ret = __bam_ca_split(dbc,
	    cp->page->pgno, lp->pgno, rp->pgno, split, 1)) != 0)
		goto err;

	/* Success -- write the real pages back to the store. */
	(void)memp_fput(dbp->mpf, cp->page, DB_MPOOL_DIRTY);
	(void)__TLPUT(dbc, cp->lock);
	(void)memp_fput(dbp->mpf, lp, DB_MPOOL_DIRTY);
	(void)memp_fput(dbp->mpf, rp, DB_MPOOL_DIRTY);

	return (0);

err:	if (lp != NULL)
		(void)__db_free(dbc, lp);
	if (rp != NULL)
		(void)__db_free(dbc, rp);
	(void)memp_fput(dbp->mpf, cp->page, 0);
	(void)__TLPUT(dbc, cp->lock);
	return (ret);
}

/*
 * __bam_page --
 *	Split the non-root page of a btree.
 */
static int
__bam_page(dbc, pp, cp)
	DBC *dbc;
	EPG *pp, *cp;
{
	BTREE_CURSOR *bc;
	DBT log_dbt;
	DB_LSN log_lsn;
	DB *dbp;
	DB_LOCK tplock;
	DB_LSN save_lsn;
	PAGE *lp, *rp, *alloc_rp, *tp;
	db_indx_t split;
	u_int32_t opflags;
	int ret, t_ret;

	dbp = dbc->dbp;
	alloc_rp = lp = rp = tp = NULL;
	tplock.off = LOCK_INVALID;
	ret = -1;

	/*
	 * Create a new right page for the split, and fill in everything
	 * except its LSN and page number.
	 *
	 * We malloc space for both the left and right pages, so we don't get
	 * a new page from the underlying buffer pool until we know the split
	 * is going to succeed.  The reason is that we can't release locks
	 * acquired during the get-a-new-page process because metadata page
	 * locks can't be discarded on failure since we may have modified the
	 * free list.  So, if you assume that we're holding a write lock on the
	 * leaf page which ran out of space and started this split (e.g., we
	 * have already written records to the page, or we retrieved a record
	 * from it with the DB_RMW flag set), failing in a split with both a
	 * leaf page locked and the metadata page locked can potentially lock
	 * up the tree badly, because we've violated the rule of always locking
	 * down the tree, and never up.
	 */
	if ((ret = __os_malloc(dbp->dbenv, dbp->pgsize, NULL, &rp)) != 0)
		goto err;
	P_INIT(rp, dbp->pgsize, 0,
	    ISINTERNAL(cp->page) ? PGNO_INVALID : PGNO(cp->page),
	    ISINTERNAL(cp->page) ? PGNO_INVALID : NEXT_PGNO(cp->page),
	    cp->page->level, TYPE(cp->page));

	/*
	 * Create new left page for the split, and fill in everything
	 * except its LSN and next-page page number.
	 */
	if ((ret = __os_malloc(dbp->dbenv, dbp->pgsize, NULL, &lp)) != 0)
		goto err;
	P_INIT(lp, dbp->pgsize, PGNO(cp->page),
	    ISINTERNAL(cp->page) ?  PGNO_INVALID : PREV_PGNO(cp->page),
	    ISINTERNAL(cp->page) ?  PGNO_INVALID : 0,
	    cp->page->level, TYPE(cp->page));

	/*
	 * Split right.
	 *
	 * Only the indices are sorted on the page, i.e., the key/data pairs
	 * aren't, so it's simpler to copy the data from the split page onto
	 * two new pages instead of copying half the data to a new right page
	 * and compacting the left page in place.  Since the left page can't
	 * change, we swap the original and the allocated left page after the
	 * split.
	 */
	if ((ret = __bam_psplit(dbc, cp, lp, rp, &split)) != 0)
		goto err;

	/*
	 * Test to see if we are going to be able to insert the new pages into
	 * the parent page.  The interesting failure here is that the parent
	 * page can't hold the new keys, and has to be split in turn, in which
	 * case we want to release all the locks we can.
	 */
	if ((ret = __bam_pinsert(dbc, pp, lp, rp, 1)) != 0)
		goto err;

	/*
	 * Fix up the previous pointer of any leaf page following the split
	 * page.
	 *
	 * There's interesting deadlock situations here as we try to write-lock
	 * a page that's not in our direct ancestry.  Consider a cursor walking
	 * backward through the leaf pages, that has our following page locked,
	 * and is waiting on a lock for the page we're splitting.  In that case
	 * we're going to deadlock here .  It's probably OK, stepping backward
	 * through the tree isn't a common operation.
	 */
	if (ISLEAF(cp->page) && NEXT_PGNO(cp->page) != PGNO_INVALID) {
		if ((ret = __db_lget(dbc,
		    0, NEXT_PGNO(cp->page), DB_LOCK_WRITE, 0, &tplock)) != 0)
			goto err;
		if ((ret =
		    memp_fget(dbp->mpf, &NEXT_PGNO(cp->page), 0, &tp)) != 0)
			goto err;
	}

	/*
	 * We've got everything locked down we need, and we know the split
	 * is going to succeed.  Go and get the additional page we'll need.
	 */
	if ((ret = __db_new(dbc, TYPE(cp->page), &alloc_rp)) != 0)
		goto err;

	/*
	 * Fix up the page numbers we didn't have before.  We have to do this
	 * before calling __bam_pinsert because it may copy a page number onto
	 * the parent page and it takes the page number from its page argument.
	 */
	PGNO(rp) = NEXT_PGNO(lp) = PGNO(alloc_rp);

	/* Actually update the parent page. */
	if ((ret = __bam_pinsert(dbc, pp, lp, rp, 0)) != 0)
		goto err;

	bc = (BTREE_CURSOR *)dbc->internal;
	/* Log the change. */
	if (DB_LOGGING(dbc)) {
		memset(&log_dbt, 0, sizeof(log_dbt));
		log_dbt.data = cp->page;
		log_dbt.size = dbp->pgsize;
		if (tp == NULL)
			ZERO_LSN(log_lsn);
		opflags = F_ISSET(bc, C_RECNUM) ? SPL_NRECS : 0;
		if ((ret = __bam_split_log(dbp->dbenv, dbc->txn,
		    &LSN(cp->page), 0, dbp->log_fileid, PGNO(cp->page),
		    &LSN(cp->page), PGNO(alloc_rp), &LSN(alloc_rp),
		    (u_int32_t)NUM_ENT(lp),
		    tp == NULL ? 0 : PGNO(tp),
		    tp == NULL ? &log_lsn : &LSN(tp),
		    bc->root, &log_dbt, opflags)) != 0)
			goto err;

		/* Update the LSNs for all involved pages. */
		LSN(alloc_rp) = LSN(cp->page);
		LSN(lp) = LSN(cp->page);
		LSN(rp) = LSN(cp->page);
		if (tp != NULL)
			LSN(tp) = LSN(cp->page);
	}

	/*
	 * Copy the left and right pages into place.  There are two paths
	 * through here.  Either we are logging and we set the LSNs in the
	 * logging path.  However, if we are not logging, then we do not
	 * have valid LSNs on lp or rp.  The correct LSNs to use are the
	 * ones on the page we got from __db_new or the one that was
	 * originally on cp->page.  In both cases, we save the LSN from the
	 * real database page (not a malloc'd one) and reapply it after we
	 * do the copy.
	 */
	save_lsn = alloc_rp->lsn;
	memcpy(alloc_rp, rp, LOFFSET(rp));
	memcpy((u_int8_t *)alloc_rp + HOFFSET(rp),
	    (u_int8_t *)rp + HOFFSET(rp), dbp->pgsize - HOFFSET(rp));
	alloc_rp->lsn = save_lsn;

	save_lsn = cp->page->lsn;
	memcpy(cp->page, lp, LOFFSET(lp));
	memcpy((u_int8_t *)cp->page + HOFFSET(lp),
	    (u_int8_t *)lp + HOFFSET(lp), dbp->pgsize - HOFFSET(lp));
	cp->page->lsn = save_lsn;

	/* Fix up the next-page link. */
	if (tp != NULL)
		PREV_PGNO(tp) = PGNO(rp);

	/* Adjust any cursors. */
	if ((ret = __bam_ca_split(dbc,
	    PGNO(cp->page), PGNO(cp->page), PGNO(rp), split, 0)) != 0)
		goto err;

	__os_free(lp, dbp->pgsize);
	__os_free(rp, dbp->pgsize);

	/*
	 * Success -- write the real pages back to the store.  As we never
	 * acquired any sort of lock on the new page, we release it before
	 * releasing locks on the pages that reference it.  We're finished
	 * modifying the page so it's not really necessary, but it's neater.
	 */
	if ((t_ret =
	    memp_fput(dbp->mpf, alloc_rp, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret =
	    memp_fput(dbp->mpf, pp->page, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;
	(void)__TLPUT(dbc, pp->lock);
	if ((t_ret =
	    memp_fput(dbp->mpf, cp->page, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;
	(void)__TLPUT(dbc, cp->lock);
	if (tp != NULL) {
		if ((t_ret =
		    memp_fput(dbp->mpf, tp, DB_MPOOL_DIRTY)) != 0 && ret == 0)
			ret = t_ret;
		(void)__TLPUT(dbc, tplock);
	}
	return (ret);

err:	if (lp != NULL)
		__os_free(lp, dbp->pgsize);
	if (rp != NULL)
		__os_free(rp, dbp->pgsize);
	if (alloc_rp != NULL)
		(void)__db_free(dbc, alloc_rp);

	if (tp != NULL)
		(void)memp_fput(dbp->mpf, tp, 0);
	if (tplock.off != LOCK_INVALID)
		/* We never updated the next page, we can release it. */
		(void)__LPUT(dbc, tplock);

	(void)memp_fput(dbp->mpf, pp->page, 0);
	if (ret == DB_NEEDSPLIT)
		(void)__LPUT(dbc, pp->lock);
	else
		(void)__TLPUT(dbc, pp->lock);

	(void)memp_fput(dbp->mpf, cp->page, 0);
	if (ret == DB_NEEDSPLIT)
		(void)__LPUT(dbc, cp->lock);
	else
		(void)__TLPUT(dbc, cp->lock);

	return (ret);
}

/*
 * __bam_broot --
 *	Fix up the btree root page after it has been split.
 */
static int
__bam_broot(dbc, rootp, lp, rp)
	DBC *dbc;
	PAGE *rootp, *lp, *rp;
{
	BINTERNAL bi, *child_bi;
	BKEYDATA *child_bk;
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT hdr, data;
	db_pgno_t root_pgno;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * If the root page was a leaf page, change it into an internal page.
	 * We copy the key we split on (but not the key's data, in the case of
	 * a leaf page) to the new root page.
	 */
	root_pgno = cp->root;
	P_INIT(rootp, dbp->pgsize,
	    root_pgno, PGNO_INVALID, PGNO_INVALID, lp->level + 1, P_IBTREE);

	memset(&data, 0, sizeof(data));
	memset(&hdr, 0, sizeof(hdr));

	/*
	 * The btree comparison code guarantees that the left-most key on any
	 * internal btree page is never used, so it doesn't need to be filled
	 * in.  Set the record count if necessary.
	 */
	memset(&bi, 0, sizeof(bi));
	bi.len = 0;
	B_TSET(bi.type, B_KEYDATA, 0);
	bi.pgno = lp->pgno;
	if (F_ISSET(cp, C_RECNUM)) {
		bi.nrecs = __bam_total(lp);
		RE_NREC_SET(rootp, bi.nrecs);
	}
	hdr.data = &bi;
	hdr.size = SSZA(BINTERNAL, data);
	if ((ret =
	    __db_pitem(dbc, rootp, 0, BINTERNAL_SIZE(0), &hdr, NULL)) != 0)
		return (ret);

	switch (TYPE(rp)) {
	case P_IBTREE:
		/* Copy the first key of the child page onto the root page. */
		child_bi = GET_BINTERNAL(rp, 0);

		bi.len = child_bi->len;
		B_TSET(bi.type, child_bi->type, 0);
		bi.pgno = rp->pgno;
		if (F_ISSET(cp, C_RECNUM)) {
			bi.nrecs = __bam_total(rp);
			RE_NREC_ADJ(rootp, bi.nrecs);
		}
		hdr.data = &bi;
		hdr.size = SSZA(BINTERNAL, data);
		data.data = child_bi->data;
		data.size = child_bi->len;
		if ((ret = __db_pitem(dbc, rootp, 1,
		    BINTERNAL_SIZE(child_bi->len), &hdr, &data)) != 0)
			return (ret);

		/* Increment the overflow ref count. */
		if (B_TYPE(child_bi->type) == B_OVERFLOW)
			if ((ret = __db_ovref(dbc,
			    ((BOVERFLOW *)(child_bi->data))->pgno, 1)) != 0)
				return (ret);
		break;
	case P_LDUP:
	case P_LBTREE:
		/* Copy the first key of the child page onto the root page. */
		child_bk = GET_BKEYDATA(rp, 0);
		switch (B_TYPE(child_bk->type)) {
		case B_KEYDATA:
			bi.len = child_bk->len;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rp->pgno;
			if (F_ISSET(cp, C_RECNUM)) {
				bi.nrecs = __bam_total(rp);
				RE_NREC_ADJ(rootp, bi.nrecs);
			}
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			data.data = child_bk->data;
			data.size = child_bk->len;
			if ((ret = __db_pitem(dbc, rootp, 1,
			    BINTERNAL_SIZE(child_bk->len), &hdr, &data)) != 0)
				return (ret);
			break;
		case B_DUPLICATE:
		case B_OVERFLOW:
			bi.len = BOVERFLOW_SIZE;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rp->pgno;
			if (F_ISSET(cp, C_RECNUM)) {
				bi.nrecs = __bam_total(rp);
				RE_NREC_ADJ(rootp, bi.nrecs);
			}
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			data.data = child_bk;
			data.size = BOVERFLOW_SIZE;
			if ((ret = __db_pitem(dbc, rootp, 1,
			    BINTERNAL_SIZE(BOVERFLOW_SIZE), &hdr, &data)) != 0)
				return (ret);

			/* Increment the overflow ref count. */
			if (B_TYPE(child_bk->type) == B_OVERFLOW)
				if ((ret = __db_ovref(dbc,
				    ((BOVERFLOW *)child_bk)->pgno, 1)) != 0)
					return (ret);
			break;
		default:
			return (__db_pgfmt(dbp, rp->pgno));
		}
		break;
	default:
		return (__db_pgfmt(dbp, rp->pgno));
	}
	return (0);
}

/*
 * __ram_root --
 *	Fix up the recno root page after it has been split.
 */
static int
__ram_root(dbc, rootp, lp, rp)
	DBC *dbc;
	PAGE *rootp, *lp, *rp;
{
	DB *dbp;
	DBT hdr;
	RINTERNAL ri;
	db_pgno_t root_pgno;
	int ret;

	dbp = dbc->dbp;
	root_pgno = dbc->internal->root;

	/* Initialize the page. */
	P_INIT(rootp, dbp->pgsize,
	    root_pgno, PGNO_INVALID, PGNO_INVALID, lp->level + 1, P_IRECNO);

	/* Initialize the header. */
	memset(&hdr, 0, sizeof(hdr));
	hdr.data = &ri;
	hdr.size = RINTERNAL_SIZE;

	/* Insert the left and right keys, set the header information. */
	ri.pgno = lp->pgno;
	ri.nrecs = __bam_total(lp);
	if ((ret = __db_pitem(dbc, rootp, 0, RINTERNAL_SIZE, &hdr, NULL)) != 0)
		return (ret);
	RE_NREC_SET(rootp, ri.nrecs);
	ri.pgno = rp->pgno;
	ri.nrecs = __bam_total(rp);
	if ((ret = __db_pitem(dbc, rootp, 1, RINTERNAL_SIZE, &hdr, NULL)) != 0)
		return (ret);
	RE_NREC_ADJ(rootp, ri.nrecs);
	return (0);
}

/*
 * __bam_pinsert --
 *	Insert a new key into a parent page, completing the split.
 */
static int
__bam_pinsert(dbc, parent, lchild, rchild, space_check)
	DBC *dbc;
	EPG *parent;
	PAGE *lchild, *rchild;
	int space_check;
{
	BINTERNAL bi, *child_bi;
	BKEYDATA *child_bk, *tmp_bk;
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT a, b, hdr, data;
	PAGE *ppage;
	RINTERNAL ri;
	db_indx_t off;
	db_recno_t nrecs;
	size_t (*func) __P((DB *, const DBT *, const DBT *));
	u_int32_t n, nbytes, nksize;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	t = dbp->bt_internal;
	ppage = parent->page;

	/* If handling record numbers, count records split to the right page. */
	nrecs = F_ISSET(cp, C_RECNUM) && !space_check ? __bam_total(rchild) : 0;

	/*
	 * Now we insert the new page's first key into the parent page, which
	 * completes the split.  The parent points to a PAGE and a page index
	 * offset, where the new key goes ONE AFTER the index, because we split
	 * to the right.
	 *
	 * XXX
	 * Some btree algorithms replace the key for the old page as well as
	 * the new page.  We don't, as there's no reason to believe that the
	 * first key on the old page is any better than the key we have, and,
	 * in the case of a key being placed at index 0 causing the split, the
	 * key is unavailable.
	 */
	off = parent->indx + O_INDX;

	/*
	 * Calculate the space needed on the parent page.
	 *
	 * Prefix trees: space hack used when inserting into BINTERNAL pages.
	 * Retain only what's needed to distinguish between the new entry and
	 * the LAST entry on the page to its left.  If the keys compare equal,
	 * retain the entire key.  We ignore overflow keys, and the entire key
	 * must be retained for the next-to-leftmost key on the leftmost page
	 * of each level, or the search will fail.  Applicable ONLY to internal
	 * pages that have leaf pages as children.  Further reduction of the
	 * key between pairs of internal pages loses too much information.
	 */
	switch (TYPE(rchild)) {
	case P_IBTREE:
		child_bi = GET_BINTERNAL(rchild, 0);
		nbytes = BINTERNAL_PSIZE(child_bi->len);

		if (P_FREESPACE(ppage) < nbytes)
			return (DB_NEEDSPLIT);
		if (space_check)
			return (0);

		/* Add a new record for the right page. */
		memset(&bi, 0, sizeof(bi));
		bi.len = child_bi->len;
		B_TSET(bi.type, child_bi->type, 0);
		bi.pgno = rchild->pgno;
		bi.nrecs = nrecs;
		memset(&hdr, 0, sizeof(hdr));
		hdr.data = &bi;
		hdr.size = SSZA(BINTERNAL, data);
		memset(&data, 0, sizeof(data));
		data.data = child_bi->data;
		data.size = child_bi->len;
		if ((ret = __db_pitem(dbc, ppage, off,
		    BINTERNAL_SIZE(child_bi->len), &hdr, &data)) != 0)
			return (ret);

		/* Increment the overflow ref count. */
		if (B_TYPE(child_bi->type) == B_OVERFLOW)
			if ((ret = __db_ovref(dbc,
			    ((BOVERFLOW *)(child_bi->data))->pgno, 1)) != 0)
				return (ret);
		break;
	case P_LDUP:
	case P_LBTREE:
		child_bk = GET_BKEYDATA(rchild, 0);
		switch (B_TYPE(child_bk->type)) {
		case B_KEYDATA:
			/*
			 * We set t->bt_prefix to NULL if we have a comparison
			 * callback but no prefix compression callback.  But,
			 * if we're splitting in an off-page duplicates tree,
			 * we still have to do some checking.  If using the
			 * default off-page duplicates comparison routine we
			 * can use the default prefix compression callback. If
			 * not using the default off-page duplicates comparison
			 * routine, we can't do any kind of prefix compression
			 * as there's no way for an application to specify a
			 * prefix compression callback that corresponds to its
			 * comparison callback.
			 */
			if (F_ISSET(dbc, DBC_OPD)) {
				if (dbp->dup_compare == __bam_defcmp)
					func = __bam_defpfx;
				else
					func = NULL;
			} else
				func = t->bt_prefix;

			nbytes = BINTERNAL_PSIZE(child_bk->len);
			nksize = child_bk->len;
			if (func == NULL)
				goto noprefix;
			if (ppage->prev_pgno == PGNO_INVALID && off <= 1)
				goto noprefix;
			tmp_bk = GET_BKEYDATA(lchild, NUM_ENT(lchild) -
			    (TYPE(lchild) == P_LDUP ? O_INDX : P_INDX));
			if (B_TYPE(tmp_bk->type) != B_KEYDATA)
				goto noprefix;
			memset(&a, 0, sizeof(a));
			a.size = tmp_bk->len;
			a.data = tmp_bk->data;
			memset(&b, 0, sizeof(b));
			b.size = child_bk->len;
			b.data = child_bk->data;
			nksize = func(dbp, &a, &b);
			if ((n = BINTERNAL_PSIZE(nksize)) < nbytes)
				nbytes = n;
			else
noprefix:			nksize = child_bk->len;

			if (P_FREESPACE(ppage) < nbytes)
				return (DB_NEEDSPLIT);
			if (space_check)
				return (0);

			memset(&bi, 0, sizeof(bi));
			bi.len = nksize;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rchild->pgno;
			bi.nrecs = nrecs;
			memset(&hdr, 0, sizeof(hdr));
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			memset(&data, 0, sizeof(data));
			data.data = child_bk->data;
			data.size = nksize;
			if ((ret = __db_pitem(dbc, ppage, off,
			    BINTERNAL_SIZE(nksize), &hdr, &data)) != 0)
				return (ret);
			break;
		case B_DUPLICATE:
		case B_OVERFLOW:
			nbytes = BINTERNAL_PSIZE(BOVERFLOW_SIZE);

			if (P_FREESPACE(ppage) < nbytes)
				return (DB_NEEDSPLIT);
			if (space_check)
				return (0);

			memset(&bi, 0, sizeof(bi));
			bi.len = BOVERFLOW_SIZE;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rchild->pgno;
			bi.nrecs = nrecs;
			memset(&hdr, 0, sizeof(hdr));
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			memset(&data, 0, sizeof(data));
			data.data = child_bk;
			data.size = BOVERFLOW_SIZE;
			if ((ret = __db_pitem(dbc, ppage, off,
			    BINTERNAL_SIZE(BOVERFLOW_SIZE), &hdr, &data)) != 0)
				return (ret);

			/* Increment the overflow ref count. */
			if (B_TYPE(child_bk->type) == B_OVERFLOW)
				if ((ret = __db_ovref(dbc,
				    ((BOVERFLOW *)child_bk)->pgno, 1)) != 0)
					return (ret);
			break;
		default:
			return (__db_pgfmt(dbp, rchild->pgno));
		}
		break;
	case P_IRECNO:
	case P_LRECNO:
		nbytes = RINTERNAL_PSIZE;

		if (P_FREESPACE(ppage) < nbytes)
			return (DB_NEEDSPLIT);
		if (space_check)
			return (0);

		/* Add a new record for the right page. */
		memset(&hdr, 0, sizeof(hdr));
		hdr.data = &ri;
		hdr.size = RINTERNAL_SIZE;
		ri.pgno = rchild->pgno;
		ri.nrecs = nrecs;
		if ((ret = __db_pitem(dbc,
		    ppage, off, RINTERNAL_SIZE, &hdr, NULL)) != 0)
			return (ret);
		break;
	default:
		return (__db_pgfmt(dbp, rchild->pgno));
	}

	/*
	 * If a Recno or Btree with record numbers AM page, or an off-page
	 * duplicates tree, adjust the parent page's left page record count.
	 */
	if (F_ISSET(cp, C_RECNUM)) {
		/* Log the change. */
		if (DB_LOGGING(dbc) &&
		    (ret = __bam_cadjust_log(dbp->dbenv, dbc->txn,
		    &LSN(ppage), 0, dbp->log_fileid, PGNO(ppage),
		    &LSN(ppage), parent->indx, -(int32_t)nrecs, 0)) != 0)
			return (ret);

		/* Update the left page count. */
		if (dbc->dbtype == DB_RECNO)
			GET_RINTERNAL(ppage, parent->indx)->nrecs -= nrecs;
		else
			GET_BINTERNAL(ppage, parent->indx)->nrecs -= nrecs;
	}

	return (0);
}

/*
 * __bam_psplit --
 *	Do the real work of splitting the page.
 */
static int
__bam_psplit(dbc, cp, lp, rp, splitret)
	DBC *dbc;
	EPG *cp;
	PAGE *lp, *rp;
	db_indx_t *splitret;
{
	DB *dbp;
	PAGE *pp;
	db_indx_t half, nbytes, off, splitp, top;
	int adjust, cnt, iflag, isbigkey, ret;

	dbp = dbc->dbp;
	pp = cp->page;
	adjust = TYPE(pp) == P_LBTREE ? P_INDX : O_INDX;

	/*
	 * If we're splitting the first (last) page on a level because we're
	 * inserting (appending) a key to it, it's likely that the data is
	 * sorted.  Moving a single item to the new page is less work and can
	 * push the fill factor higher than normal.  If we're wrong it's not
	 * a big deal, we'll just do the split the right way next time.
	 */
	off = 0;
	if (NEXT_PGNO(pp) == PGNO_INVALID &&
	    ((ISINTERNAL(pp) && cp->indx == NUM_ENT(cp->page) - 1) ||
	    (!ISINTERNAL(pp) && cp->indx == NUM_ENT(cp->page))))
		off = NUM_ENT(cp->page) - adjust;
	else if (PREV_PGNO(pp) == PGNO_INVALID && cp->indx == 0)
		off = adjust;

	if (off != 0)
		goto sort;

	/*
	 * Split the data to the left and right pages.  Try not to split on
	 * an overflow key.  (Overflow keys on internal pages will slow down
	 * searches.)  Refuse to split in the middle of a set of duplicates.
	 *
	 * First, find the optimum place to split.
	 *
	 * It's possible to try and split past the last record on the page if
	 * there's a very large record at the end of the page.  Make sure this
	 * doesn't happen by bounding the check at the next-to-last entry on
	 * the page.
	 *
	 * Note, we try and split half the data present on the page.  This is
	 * because another process may have already split the page and left
	 * it half empty.  We don't try and skip the split -- we don't know
	 * how much space we're going to need on the page, and we may need up
	 * to half the page for a big item, so there's no easy test to decide
	 * if we need to split or not.  Besides, if two threads are inserting
	 * data into the same place in the database, we're probably going to
	 * need more space soon anyway.
	 */
	top = NUM_ENT(pp) - adjust;
	half = (dbp->pgsize - HOFFSET(pp)) / 2;
	for (nbytes = 0, off = 0; off < top && nbytes < half; ++off)
		switch (TYPE(pp)) {
		case P_IBTREE:
			if (B_TYPE(GET_BINTERNAL(pp, off)->type) == B_KEYDATA)
				nbytes +=
				   BINTERNAL_SIZE(GET_BINTERNAL(pp, off)->len);
			else
				nbytes += BINTERNAL_SIZE(BOVERFLOW_SIZE);
			break;
		case P_LBTREE:
			if (B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA)
				nbytes +=
				    BKEYDATA_SIZE(GET_BKEYDATA(pp, off)->len);
			else
				nbytes += BOVERFLOW_SIZE;

			++off;
			/* FALLTHROUGH */
		case P_LDUP:
		case P_LRECNO:
			if (B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA)
				nbytes +=
				    BKEYDATA_SIZE(GET_BKEYDATA(pp, off)->len);
			else
				nbytes += BOVERFLOW_SIZE;
			break;
		case P_IRECNO:
			nbytes += RINTERNAL_SIZE;
			break;
		default:
			return (__db_pgfmt(dbp, pp->pgno));
		}
sort:	splitp = off;

	/*
	 * Splitp is either at or just past the optimum split point.  If the
	 * tree type is such that we're going to promote a key to an internal
	 * page, and our current choice is an overflow key, look for something
	 * close by that's smaller.
	 */
	switch (TYPE(pp)) {
	case P_IBTREE:
		iflag = 1;
		isbigkey = B_TYPE(GET_BINTERNAL(pp, off)->type) != B_KEYDATA;
		break;
	case P_LBTREE:
	case P_LDUP:
		iflag = 0;
		isbigkey = B_TYPE(GET_BKEYDATA(pp, off)->type) != B_KEYDATA;
		break;
	default:
		iflag = isbigkey = 0;
	}
	if (isbigkey)
		for (cnt = 1; cnt <= 3; ++cnt) {
			off = splitp + cnt * adjust;
			if (off < (db_indx_t)NUM_ENT(pp) &&
			    ((iflag &&
			    B_TYPE(GET_BINTERNAL(pp,off)->type) == B_KEYDATA) ||
			    B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA)) {
				splitp = off;
				break;
			}
			if (splitp <= (db_indx_t)(cnt * adjust))
				continue;
			off = splitp - cnt * adjust;
			if (iflag ?
			    B_TYPE(GET_BINTERNAL(pp, off)->type) == B_KEYDATA :
			    B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA) {
				splitp = off;
				break;
			}
		}

	/*
	 * We can't split in the middle a set of duplicates.  We know that
	 * no duplicate set can take up more than about 25% of the page,
	 * because that's the point where we push it off onto a duplicate
	 * page set.  So, this loop can't be unbounded.
	 */
	if (TYPE(pp) == P_LBTREE &&
	    pp->inp[splitp] == pp->inp[splitp - adjust])
		for (cnt = 1;; ++cnt) {
			off = splitp + cnt * adjust;
			if (off < NUM_ENT(pp) &&
			    pp->inp[splitp] != pp->inp[off]) {
				splitp = off;
				break;
			}
			if (splitp <= (db_indx_t)(cnt * adjust))
				continue;
			off = splitp - cnt * adjust;
			if (pp->inp[splitp] != pp->inp[off]) {
				splitp = off + adjust;
				break;
			}
		}

	/* We're going to split at splitp. */
	if ((ret = __bam_copy(dbp, pp, lp, 0, splitp)) != 0)
		return (ret);
	if ((ret = __bam_copy(dbp, pp, rp, splitp, NUM_ENT(pp))) != 0)
		return (ret);

	*splitret = splitp;
	return (0);
}

/*
 * __bam_copy --
 *	Copy a set of records from one page to another.
 *
 * PUBLIC: int __bam_copy __P((DB *, PAGE *, PAGE *, u_int32_t, u_int32_t));
 */
int
__bam_copy(dbp, pp, cp, nxt, stop)
	DB *dbp;
	PAGE *pp, *cp;
	u_int32_t nxt, stop;
{
	db_indx_t nbytes, off;

	/*
	 * Copy the rest of the data to the right page.  Nxt is the next
	 * offset placed on the target page.
	 */
	for (off = 0; nxt < stop; ++nxt, ++NUM_ENT(cp), ++off) {
		switch (TYPE(pp)) {
		case P_IBTREE:
			if (B_TYPE(GET_BINTERNAL(pp, nxt)->type) == B_KEYDATA)
				nbytes =
				    BINTERNAL_SIZE(GET_BINTERNAL(pp, nxt)->len);
			else
				nbytes = BINTERNAL_SIZE(BOVERFLOW_SIZE);
			break;
		case P_LBTREE:
			/*
			 * If we're on a key and it's a duplicate, just copy
			 * the offset.
			 */
			if (off != 0 && (nxt % P_INDX) == 0 &&
			    pp->inp[nxt] == pp->inp[nxt - P_INDX]) {
				cp->inp[off] = cp->inp[off - P_INDX];
				continue;
			}
			/* FALLTHROUGH */
		case P_LDUP:
		case P_LRECNO:
			if (B_TYPE(GET_BKEYDATA(pp, nxt)->type) == B_KEYDATA)
				nbytes =
				    BKEYDATA_SIZE(GET_BKEYDATA(pp, nxt)->len);
			else
				nbytes = BOVERFLOW_SIZE;
			break;
		case P_IRECNO:
			nbytes = RINTERNAL_SIZE;
			break;
		default:
			return (__db_pgfmt(dbp, pp->pgno));
		}
		cp->inp[off] = HOFFSET(cp) -= nbytes;
		memcpy(P_ENTRY(cp, off), P_ENTRY(pp, nxt), nbytes);
	}
	return (0);
}
