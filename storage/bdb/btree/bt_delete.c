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
 * $Id: bt_delete.c,v 11.49 2004/02/27 12:38:28 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/btree.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"

/*
 * __bam_ditem --
 *	Delete one or more entries from a page.
 *
 * PUBLIC: int __bam_ditem __P((DBC *, PAGE *, u_int32_t));
 */
int
__bam_ditem(dbc, h, indx)
	DBC *dbc;
	PAGE *h;
	u_int32_t indx;
{
	BINTERNAL *bi;
	BKEYDATA *bk;
	DB *dbp;
	DB_MPOOLFILE *mpf;
	u_int32_t nbytes;
	int ret;
	db_indx_t *inp;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	inp = P_INP(dbp, h);

	switch (TYPE(h)) {
	case P_IBTREE:
		bi = GET_BINTERNAL(dbp, h, indx);
		switch (B_TYPE(bi->type)) {
		case B_DUPLICATE:
		case B_KEYDATA:
			nbytes = BINTERNAL_SIZE(bi->len);
			break;
		case B_OVERFLOW:
			nbytes = BINTERNAL_SIZE(bi->len);
			if ((ret =
			    __db_doff(dbc, ((BOVERFLOW *)bi->data)->pgno)) != 0)
				return (ret);
			break;
		default:
			return (__db_pgfmt(dbp->dbenv, PGNO(h)));
		}
		break;
	case P_IRECNO:
		nbytes = RINTERNAL_SIZE;
		break;
	case P_LBTREE:
		/*
		 * If it's a duplicate key, discard the index and don't touch
		 * the actual page item.
		 *
		 * !!!
		 * This works because no data item can have an index matching
		 * any other index so even if the data item is in a key "slot",
		 * it won't match any other index.
		 */
		if ((indx % 2) == 0) {
			/*
			 * Check for a duplicate after us on the page.  NOTE:
			 * we have to delete the key item before deleting the
			 * data item, otherwise the "indx + P_INDX" calculation
			 * won't work!
			 */
			if (indx + P_INDX < (u_int32_t)NUM_ENT(h) &&
			    inp[indx] == inp[indx + P_INDX])
				return (__bam_adjindx(dbc,
				    h, indx, indx + O_INDX, 0));
			/*
			 * Check for a duplicate before us on the page.  It
			 * doesn't matter if we delete the key item before or
			 * after the data item for the purposes of this one.
			 */
			if (indx > 0 && inp[indx] == inp[indx - P_INDX])
				return (__bam_adjindx(dbc,
				    h, indx, indx - P_INDX, 0));
		}
		/* FALLTHROUGH */
	case P_LDUP:
	case P_LRECNO:
		bk = GET_BKEYDATA(dbp, h, indx);
		switch (B_TYPE(bk->type)) {
		case B_DUPLICATE:
			nbytes = BOVERFLOW_SIZE;
			break;
		case B_OVERFLOW:
			nbytes = BOVERFLOW_SIZE;
			if ((ret = __db_doff(
			    dbc, (GET_BOVERFLOW(dbp, h, indx))->pgno)) != 0)
				return (ret);
			break;
		case B_KEYDATA:
			nbytes = BKEYDATA_SIZE(bk->len);
			break;
		default:
			return (__db_pgfmt(dbp->dbenv, PGNO(h)));
		}
		break;
	default:
		return (__db_pgfmt(dbp->dbenv, PGNO(h)));
	}

	/* Delete the item and mark the page dirty. */
	if ((ret = __db_ditem(dbc, h, indx, nbytes)) != 0)
		return (ret);
	if ((ret = __memp_fset(mpf, h, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	return (0);
}

/*
 * __bam_adjindx --
 *	Adjust an index on the page.
 *
 * PUBLIC: int __bam_adjindx __P((DBC *, PAGE *, u_int32_t, u_int32_t, int));
 */
int
__bam_adjindx(dbc, h, indx, indx_copy, is_insert)
	DBC *dbc;
	PAGE *h;
	u_int32_t indx, indx_copy;
	int is_insert;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	db_indx_t copy, *inp;
	int ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	inp = P_INP(dbp, h);

	/* Log the change. */
	if (DBC_LOGGING(dbc)) {
	    if ((ret = __bam_adj_log(dbp, dbc->txn, &LSN(h), 0,
		PGNO(h), &LSN(h), indx, indx_copy, (u_int32_t)is_insert)) != 0)
			return (ret);
	} else
		LSN_NOT_LOGGED(LSN(h));

	/* Shuffle the indices and mark the page dirty. */
	if (is_insert) {
		copy = inp[indx_copy];
		if (indx != NUM_ENT(h))
			memmove(&inp[indx + O_INDX], &inp[indx],
			    sizeof(db_indx_t) * (NUM_ENT(h) - indx));
		inp[indx] = copy;
		++NUM_ENT(h);
	} else {
		--NUM_ENT(h);
		if (indx != NUM_ENT(h))
			memmove(&inp[indx], &inp[indx + O_INDX],
			    sizeof(db_indx_t) * (NUM_ENT(h) - indx));
	}
	if ((ret = __memp_fset(mpf, h, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	return (0);
}

/*
 * __bam_dpages --
 *	Delete a set of locked pages.
 *
 * PUBLIC: int __bam_dpages __P((DBC *, EPG *));
 */
int
__bam_dpages(dbc, stack_epg)
	DBC *dbc;
	EPG *stack_epg;
{
	BTREE_CURSOR *cp;
	BINTERNAL *bi;
	DB *dbp;
	DBT a, b;
	DB_LOCK c_lock, p_lock;
	DB_MPOOLFILE *mpf;
	EPG *epg;
	PAGE *child, *parent;
	db_indx_t nitems;
	db_pgno_t pgno, root_pgno;
	db_recno_t rcnt;
	int done, ret, t_ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * We have the entire stack of deletable pages locked.
	 *
	 * Btree calls us with a pointer to the beginning of a stack, where
	 * the first page in the stack is to have a single item deleted, and
	 * the rest of the pages are to be removed.
	 *
	 * Recno calls us with a pointer into the middle of the stack, where
	 * the referenced page is to have a single item deleted, and pages
	 * after the stack reference are to be removed.
	 *
	 * First, discard any pages that we don't care about.
	 */
	ret = 0;
	for (epg = cp->sp; epg < stack_epg; ++epg) {
		if ((t_ret = __memp_fput(mpf, epg->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		if ((t_ret = __TLPUT(dbc, epg->lock)) != 0 && ret == 0)
			ret = t_ret;
	}
	if (ret != 0)
		goto err;

	/*
	 * !!!
	 * There is an interesting deadlock situation here.  We have to relink
	 * the leaf page chain around the leaf page being deleted.  Consider
	 * a cursor walking through the leaf pages, that has the previous page
	 * read-locked and is waiting on a lock for the page we're deleting.
	 * It will deadlock here.  Before we unlink the subtree, we relink the
	 * leaf page chain.
	 */
	if ((ret = __bam_relink(dbc, cp->csp->page, NULL)) != 0)
		goto err;

	/*
	 * Delete the last item that references the underlying pages that are
	 * to be deleted, and adjust cursors that reference that page.  Then,
	 * save that page's page number and item count and release it.  If
	 * the application isn't retaining locks because it's running without
	 * transactions, this lets the rest of the tree get back to business
	 * immediately.
	 */
	if ((ret = __bam_ditem(dbc, epg->page, epg->indx)) != 0)
		goto err;
	if ((ret = __bam_ca_di(dbc, PGNO(epg->page), epg->indx, -1)) != 0)
		goto err;

	pgno = PGNO(epg->page);
	nitems = NUM_ENT(epg->page);

	ret = __memp_fput(mpf, epg->page, 0);
	if ((t_ret = __TLPUT(dbc, epg->lock)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		goto err_inc;

	/* Free the rest of the pages in the stack. */
	while (++epg <= cp->csp) {
		/*
		 * Delete page entries so they will be restored as part of
		 * recovery.  We don't need to do cursor adjustment here as
		 * the pages are being emptied by definition and so cannot
		 * be referenced by a cursor.
		 */
		if (NUM_ENT(epg->page) != 0) {
			DB_ASSERT(NUM_ENT(epg->page) == 1);

			if ((ret = __bam_ditem(dbc, epg->page, epg->indx)) != 0)
				goto err;
		}

		ret = __db_free(dbc, epg->page);
		epg->page = NULL;
		if ((t_ret = __TLPUT(dbc, epg->lock)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto err_inc;
	}

	if (0) {
err_inc:	++epg;
err:		for (; epg <= cp->csp; ++epg) {
			if (epg->page != NULL)
				(void)__memp_fput(mpf, epg->page, 0);
			(void)__TLPUT(dbc, epg->lock);
		}
		BT_STK_CLR(cp);
		return (ret);
	}
	BT_STK_CLR(cp);

	/*
	 * If we just deleted the next-to-last item from the root page, the
	 * tree can collapse one or more levels.  While there remains only a
	 * single item on the root page, write lock the last page referenced
	 * by the root page and copy it over the root page.
	 */
	root_pgno = cp->root;
	if (pgno != root_pgno || nitems != 1)
		return (0);

	for (done = 0; !done;) {
		/* Initialize. */
		parent = child = NULL;
		LOCK_INIT(p_lock);
		LOCK_INIT(c_lock);

		/* Lock the root. */
		pgno = root_pgno;
		if ((ret =
		    __db_lget(dbc, 0, pgno, DB_LOCK_WRITE, 0, &p_lock)) != 0)
			goto stop;
		if ((ret = __memp_fget(mpf, &pgno, 0, &parent)) != 0)
			goto stop;

		if (NUM_ENT(parent) != 1)
			goto stop;

		switch (TYPE(parent)) {
		case P_IBTREE:
			/*
			 * If this is overflow, then try to delete it.
			 * The child may or may not still point at it.
			 */
			bi = GET_BINTERNAL(dbp, parent, 0);
			if (B_TYPE(bi->type) == B_OVERFLOW)
				if ((ret = __db_doff(dbc,
				    ((BOVERFLOW *)bi->data)->pgno)) != 0)
					goto stop;
			pgno = bi->pgno;
			break;
		case P_IRECNO:
			pgno = GET_RINTERNAL(dbp, parent, 0)->pgno;
			break;
		default:
			goto stop;
		}

		/* Lock the child page. */
		if ((ret =
		    __db_lget(dbc, 0, pgno, DB_LOCK_WRITE, 0, &c_lock)) != 0)
			goto stop;
		if ((ret = __memp_fget(mpf, &pgno, 0, &child)) != 0)
			goto stop;

		/* Log the change. */
		if (DBC_LOGGING(dbc)) {
			memset(&a, 0, sizeof(a));
			a.data = child;
			a.size = dbp->pgsize;
			memset(&b, 0, sizeof(b));
			b.data = P_ENTRY(dbp, parent, 0);
			b.size = TYPE(parent) == P_IRECNO ? RINTERNAL_SIZE :
			    BINTERNAL_SIZE(((BINTERNAL *)b.data)->len);
			if ((ret = __bam_rsplit_log(dbp, dbc->txn,
			    &child->lsn, 0, PGNO(child), &a, PGNO(parent),
			    RE_NREC(parent), &b, &parent->lsn)) != 0)
				goto stop;
		} else
			LSN_NOT_LOGGED(child->lsn);

		/*
		 * Make the switch.
		 *
		 * One fixup -- internal pages below the top level do not store
		 * a record count, so we have to preserve it if we're not
		 * converting to a leaf page.  Note also that we are about to
		 * overwrite the parent page, including its LSN.  This is OK
		 * because the log message we wrote describing this update
		 * stores its LSN on the child page.  When the child is copied
		 * onto the parent, the correct LSN is copied into place.
		 */
		COMPQUIET(rcnt, 0);
		if (F_ISSET(cp, C_RECNUM) && LEVEL(child) > LEAFLEVEL)
			rcnt = RE_NREC(parent);
		memcpy(parent, child, dbp->pgsize);
		PGNO(parent) = root_pgno;
		if (F_ISSET(cp, C_RECNUM) && LEVEL(child) > LEAFLEVEL)
			RE_NREC_SET(parent, rcnt);

		/* Mark the pages dirty. */
		if ((ret = __memp_fset(mpf, parent, DB_MPOOL_DIRTY)) != 0)
			goto stop;
		if ((ret = __memp_fset(mpf, child, DB_MPOOL_DIRTY)) != 0)
			goto stop;

		/* Adjust the cursors. */
		if ((ret = __bam_ca_rsplit(dbc, PGNO(child), root_pgno)) != 0)
			goto stop;

		/*
		 * Free the page copied onto the root page and discard its
		 * lock.  (The call to __db_free() discards our reference
		 * to the page.)
		 */
		if ((ret = __db_free(dbc, child)) != 0) {
			child = NULL;
			goto stop;
		}
		child = NULL;

		if (0) {
stop:			done = 1;
		}
		if ((t_ret = __TLPUT(dbc, p_lock)) != 0 && ret == 0)
			ret = t_ret;
		if (parent != NULL &&
		    (t_ret = __memp_fput(mpf, parent, 0)) != 0 && ret == 0)
			ret = t_ret;
		if ((t_ret = __TLPUT(dbc, c_lock)) != 0 && ret == 0)
			ret = t_ret;
		if (child != NULL &&
		    (t_ret = __memp_fput(mpf, child, 0)) != 0 && ret == 0)
			ret = t_ret;
	}

	return (ret);
}

/*
 * __bam_relink --
 *	Relink around a deleted page.
 *
 * PUBLIC: int __bam_relink __P((DBC *, PAGE *, PAGE **));
 */
int
__bam_relink(dbc, pagep, new_next)
	DBC *dbc;
	PAGE *pagep, **new_next;
{
	DB *dbp;
	PAGE *np, *pp;
	DB_LOCK npl, ppl;
	DB_LSN *nlsnp, *plsnp, ret_lsn;
	DB_MPOOLFILE *mpf;
	int ret, t_ret;

	dbp = dbc->dbp;
	np = pp = NULL;
	LOCK_INIT(npl);
	LOCK_INIT(ppl);
	nlsnp = plsnp = NULL;
	mpf = dbp->mpf;
	ret = 0;

	/*
	 * Retrieve and lock the one/two pages.  For a remove, we may need
	 * two pages (the before and after).  For an add, we only need one
	 * because, the split took care of the prev.
	 */
	if (pagep->next_pgno != PGNO_INVALID) {
		if ((ret = __db_lget(dbc,
		    0, pagep->next_pgno, DB_LOCK_WRITE, 0, &npl)) != 0)
			goto err;
		if ((ret = __memp_fget(mpf, &pagep->next_pgno, 0, &np)) != 0) {
			ret = __db_pgerr(dbp, pagep->next_pgno, ret);
			goto err;
		}
		nlsnp = &np->lsn;
	}
	if (pagep->prev_pgno != PGNO_INVALID) {
		if ((ret = __db_lget(dbc,
		    0, pagep->prev_pgno, DB_LOCK_WRITE, 0, &ppl)) != 0)
			goto err;
		if ((ret = __memp_fget(mpf, &pagep->prev_pgno, 0, &pp)) != 0) {
			ret = __db_pgerr(dbp, pagep->prev_pgno, ret);
			goto err;
		}
		plsnp = &pp->lsn;
	}

	/* Log the change. */
	if (DBC_LOGGING(dbc)) {
		if ((ret = __bam_relink_log(dbp, dbc->txn, &ret_lsn, 0,
		    pagep->pgno, &pagep->lsn, pagep->prev_pgno, plsnp,
		    pagep->next_pgno, nlsnp)) != 0)
			goto err;
	} else
		LSN_NOT_LOGGED(ret_lsn);
	if (np != NULL)
		np->lsn = ret_lsn;
	if (pp != NULL)
		pp->lsn = ret_lsn;
	pagep->lsn = ret_lsn;

	/*
	 * Modify and release the two pages.
	 *
	 * !!!
	 * The parameter new_next gets set to the page following the page we
	 * are removing.  If there is no following page, then new_next gets
	 * set to NULL.
	 */
	if (np != NULL) {
		np->prev_pgno = pagep->prev_pgno;
		if (new_next == NULL)
			ret = __memp_fput(mpf, np, DB_MPOOL_DIRTY);
		else {
			*new_next = np;
			ret = __memp_fset(mpf, np, DB_MPOOL_DIRTY);
		}
		if ((t_ret = __TLPUT(dbc, npl)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto err;
	} else if (new_next != NULL)
		*new_next = NULL;

	if (pp != NULL) {
		pp->next_pgno = pagep->next_pgno;
		ret = __memp_fput(mpf, pp, DB_MPOOL_DIRTY);
		if ((t_ret = __TLPUT(dbc, ppl)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto err;
	}
	return (0);

err:	if (np != NULL)
		(void)__memp_fput(mpf, np, 0);
	(void)__TLPUT(dbc, npl);
	if (pp != NULL)
		(void)__memp_fput(mpf, pp, 0);
	(void)__TLPUT(dbc, ppl);
	return (ret);
}
