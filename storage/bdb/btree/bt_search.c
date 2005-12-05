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
 * $Id: bt_search.c,v 12.17 2005/11/10 21:17:13 ubell Exp $
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
 * __bam_get_root --
 *	Fetch the root of a tree and see if we want to keep
 * it in the stack.
 *
 * PUBLIC: int __bam_get_root __P((DBC *, db_pgno_t, int, u_int32_t, int *));
 */
int
__bam_get_root(dbc, pg, slevel, flags, stack)
	DBC *dbc;
	db_pgno_t pg;
	int slevel;
	u_int32_t flags;
	int *stack;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_lockmode_t lock_mode;
	int ret, t_ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	cp = (BTREE_CURSOR *)dbc->internal;
	/*
	 * If write-locking pages, we need to know whether or not to acquire a
	 * write lock on a page before getting it.  This depends on how deep it
	 * is in tree, which we don't know until we acquire the root page.  So,
	 * if we need to lock the root page we may have to upgrade it later,
	 * because we won't get the correct lock initially.
	 *
	 * Retrieve the root page.
	 */
try_again:
	*stack = LF_ISSET(S_STACK) &&
	      (dbc->dbtype == DB_RECNO || F_ISSET(cp, C_RECNUM));
	lock_mode = DB_LOCK_READ;
	if (*stack ||
	    LF_ISSET(S_DEL) || (LF_ISSET(S_NEXT) && LF_ISSET(S_WRITE)))
		lock_mode = DB_LOCK_WRITE;
	if ((ret = __db_lget(dbc, 0, pg, lock_mode, 0, &lock)) != 0)
		return (ret);
	if ((ret = __memp_fget(mpf, &pg, 0, &h)) != 0) {
		/* Did not read it, so we can release the lock */
		(void)__LPUT(dbc, lock);
		return (ret);
	}

	/*
	 * Decide if we need to save this page; if we do, write lock it.
	 * We deliberately don't lock-couple on this call.  If the tree
	 * is tiny, i.e., one page, and two threads are busily updating
	 * the root page, we're almost guaranteed deadlocks galore, as
	 * each one gets a read lock and then blocks the other's attempt
	 * for a write lock.
	 */
	if (!*stack &&
	    ((LF_ISSET(S_PARENT) && (u_int8_t)(slevel + 1) >= LEVEL(h)) ||
	    (LF_ISSET(S_WRITE) && LEVEL(h) == LEAFLEVEL) ||
	    (LF_ISSET(S_START) && slevel == LEVEL(h)))) {
		if (!STD_LOCKING(dbc))
			goto no_relock;
		ret = __memp_fput(mpf, h, 0);
		if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			return (ret);
		lock_mode = DB_LOCK_WRITE;
		if ((ret = __db_lget(dbc, 0, pg, lock_mode, 0, &lock)) != 0)
			return (ret);
		if ((ret = __memp_fget(mpf, &pg, 0, &h)) != 0) {
			/* Did not read it, so we can release the lock */
			(void)__LPUT(dbc, lock);
			return (ret);
		}
		if (!((LF_ISSET(S_PARENT) &&
		    (u_int8_t)(slevel + 1) >= LEVEL(h)) ||
		    (LF_ISSET(S_WRITE) && LEVEL(h) == LEAFLEVEL) ||
		    (LF_ISSET(S_START) && slevel == LEVEL(h)))) {
			/* Someone else split the root, start over. */
			ret = __memp_fput(mpf, h, 0);
			if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
				ret = t_ret;
			if (ret != 0)
				return (ret);
			goto try_again;
		}
no_relock:	*stack = 1;
	}
	BT_STK_ENTER(dbp->dbenv, cp, h, 0, lock, lock_mode, ret);

	return (ret);
}

/*
 * __bam_search --
 *	Search a btree for a key.
 *
 * PUBLIC: int __bam_search __P((DBC *, db_pgno_t,
 * PUBLIC:     const DBT *, u_int32_t, int, db_recno_t *, int *));
 */
int
__bam_search(dbc, root_pgno, key, flags, slevel, recnop, exactp)
	DBC *dbc;
	db_pgno_t root_pgno;
	const DBT *key;
	u_int32_t flags;
	int slevel, *exactp;
	db_recno_t *recnop;
{
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_indx_t base, i, indx, *inp, lim;
	db_lockmode_t lock_mode;
	db_pgno_t pg;
	db_recno_t recno;
	int adjust, cmp, deloffset, ret, stack, t_ret;
	int (*func) __P((DB *, const DBT *, const DBT *));

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	cp = (BTREE_CURSOR *)dbc->internal;
	h = NULL;
	t = dbp->bt_internal;
	recno = 0;

	BT_STK_CLR(cp);

	/*
	 * There are several ways we search a btree tree.  The flags argument
	 * specifies if we're acquiring read or write locks, if we position
	 * to the first or last item in a set of duplicates, if we return
	 * deleted items, and if we are locking pairs of pages.  In addition,
	 * if we're modifying record numbers, we have to lock the entire tree
	 * regardless.  See btree.h for more details.
	 */

	if (root_pgno == PGNO_INVALID)
		root_pgno = cp->root;
	if ((ret = __bam_get_root(dbc, root_pgno, slevel, flags, &stack)) != 0)
		return (ret);
	lock_mode = cp->csp->lock_mode;
	lock = cp->csp->lock;
	h = cp->csp->page;

	BT_STK_CLR(cp);

	/* Choose a comparison function. */
	func = F_ISSET(dbc, DBC_OPD) ?
	    (dbp->dup_compare == NULL ? __bam_defcmp : dbp->dup_compare) :
	    t->bt_compare;

	for (;;) {
		inp = P_INP(dbp, h);
		adjust = TYPE(h) == P_LBTREE ? P_INDX : O_INDX;
		if (LF_ISSET(S_MIN | S_MAX)) {
			if (LF_ISSET(S_MIN) || NUM_ENT(h) == 0)
				indx = 0;
			else if (TYPE(h) == P_LBTREE)
				indx = NUM_ENT(h) - 2;
			else
				indx = NUM_ENT(h) - 1;

			if (LEVEL(h) == LEAFLEVEL ||
			     (!LF_ISSET(S_START) && LEVEL(h) == slevel)) {
				if (LF_ISSET(S_NEXT))
					goto get_next;
				goto found;
			}
			goto next;
		}
		/*
		 * Do a binary search on the current page.  If we're searching
		 * a Btree leaf page, we have to walk the indices in groups of
		 * two.  If we're searching an internal page or a off-page dup
		 * page, they're an index per page item.  If we find an exact
		 * match on a leaf page, we're done.
		 */
		for (base = 0,
		    lim = NUM_ENT(h) / (db_indx_t)adjust; lim != 0; lim >>= 1) {
			indx = base + ((lim >> 1) * adjust);
			if ((ret =
			    __bam_cmp(dbp, key, h, indx, func, &cmp)) != 0)
				goto err;
			if (cmp == 0) {
				if (LEVEL(h) == LEAFLEVEL ||
				    (!LF_ISSET(S_START) &&
				    LEVEL(h) == slevel)) {
					if (LF_ISSET(S_NEXT))
						goto get_next;
					goto found;
				}
				goto next;
			}
			if (cmp > 0) {
				base = indx + adjust;
				--lim;
			}
		}

		/*
		 * No match found.  Base is the smallest index greater than
		 * key and may be zero or a last + O_INDX index.
		 *
		 * If it's a leaf page or the stopping point,
		 * return base as the "found" value.
		 * Delete only deletes exact matches.
		 */
		if (LEVEL(h) == LEAFLEVEL ||
		    (!LF_ISSET(S_START) && LEVEL(h) == slevel)) {
			*exactp = 0;

			if (LF_ISSET(S_EXACT)) {
				ret = DB_NOTFOUND;
				goto err;
			}

			if (LF_ISSET(S_STK_ONLY)) {
				BT_STK_NUM(dbp->dbenv, cp, h, base, ret);
				if ((t_ret =
				    __LPUT(dbc, lock)) != 0 && ret == 0)
					ret = t_ret;
				if ((t_ret =
				    __memp_fput(mpf, h, 0)) != 0 && ret == 0)
					ret = t_ret;
				return (ret);
			}
			if (LF_ISSET(S_NEXT)) {
get_next:			/*
				 * The caller could have asked for a NEXT
				 * at the root if the tree recently collapsed.
				 */
				if (PGNO(h) == root_pgno) {
					ret = DB_NOTFOUND;
					goto err;
				}
				/*
				 * Save the root of the subtree
				 * and drop the rest of the subtree
				 * and search down again starting at
				 * the next child.
				 */
				if ((ret = __LPUT(dbc, lock)) != 0)
					goto err;
				if ((ret = __memp_fput(mpf, h, 0)) != 0)
					goto err;
				h = NULL;
				LF_SET(S_MIN);
				LF_CLR(S_NEXT);
				indx = cp->sp->indx + 1;
				if (indx == NUM_ENT(cp->sp->page)) {
					ret = DB_NOTFOUND;
					cp->csp++;
					goto err;
				}
				h = cp->sp->page;
				cp->sp->page = NULL;
				lock = cp->sp->lock;
				LOCK_INIT(cp->sp->lock);
				if ((ret = __bam_stkrel(dbc, STK_NOLOCK)) != 0)
					goto err;
				stack = 1;
				goto next;
			}

			/*
			 * !!!
			 * Possibly returning a deleted record -- DB_SET_RANGE,
			 * DB_KEYFIRST and DB_KEYLAST don't require an exact
			 * match, and we don't want to walk multiple pages here
			 * to find an undeleted record.  This is handled by the
			 * calling routine.
			 */
			if (LF_ISSET(S_DEL) && cp->csp == cp->sp)
				cp->csp++;
			BT_STK_ENTER(dbp->dbenv,
			    cp, h, base, lock, lock_mode, ret);
			if (ret != 0)
				goto err;
			return (0);
		}

		/*
		 * If it's not a leaf page, record the internal page (which is
		 * a parent page for the key).  Decrement the base by 1 if it's
		 * non-zero so that if a split later occurs, the inserted page
		 * will be to the right of the saved page.
		 */
		indx = base > 0 ? base - O_INDX : base;

		/*
		 * If we're trying to calculate the record number, sum up
		 * all the record numbers on this page up to the indx point.
		 */
next:		if (recnop != NULL)
			for (i = 0; i < indx; ++i)
				recno += GET_BINTERNAL(dbp, h, i)->nrecs;

		pg = GET_BINTERNAL(dbp, h, indx)->pgno;

		/* See if we are at the level to start stacking. */
		if (LF_ISSET(S_START) && slevel == LEVEL(h))
			stack = 1;

		if (LF_ISSET(S_STK_ONLY)) {
			if (slevel == LEVEL(h)) {
				BT_STK_NUM(dbp->dbenv, cp, h, indx, ret);
				if ((t_ret =
				    __LPUT(dbc, lock)) != 0 && ret == 0)
					ret = t_ret;
				if ((t_ret =
				    __memp_fput(mpf, h, 0)) != 0 && ret == 0)
					ret = t_ret;
				return (ret);
			}
			BT_STK_NUMPUSH(dbp->dbenv, cp, h, indx, ret);
			(void)__memp_fput(mpf, h, 0);
			h = NULL;
			if ((ret = __db_lget(dbc,
			    LCK_COUPLE_ALWAYS, pg, lock_mode, 0, &lock)) != 0) {
				/*
				 * Discard our lock and return on failure.  This
				 * is OK because it only happens when descending
				 * the tree holding read-locks.
				 */
				(void)__LPUT(dbc, lock);
				return (ret);
			}
		} else if (stack) {
			/* Return if this is the lowest page wanted. */
			if (LF_ISSET(S_PARENT) && slevel == LEVEL(h)) {
				BT_STK_ENTER(dbp->dbenv,
				    cp, h, indx, lock, lock_mode, ret);
				if (ret != 0)
					goto err;
				return (0);
			}
			if (LF_ISSET(S_DEL) && NUM_ENT(h) > 1) {
				/*
				 * There was a page with a singleton pointer
				 * to a non-empty subtree.
				 */
				cp->csp--;
				if ((ret = __bam_stkrel(dbc, STK_NOLOCK)) != 0)
					goto err;
				stack = 0;
				goto do_del;
			}
			BT_STK_PUSH(dbp->dbenv,
			    cp, h, indx, lock, lock_mode, ret);
			if (ret != 0)
				goto err;
			h = NULL;

			lock_mode = DB_LOCK_WRITE;
			if ((ret =
			    __db_lget(dbc, 0, pg, lock_mode, 0, &lock)) != 0)
				goto err;
		} else {
			/*
			 * Decide if we want to return a reference to the next
			 * page in the return stack.  If so, lock it and never
			 * unlock it.
			 */
			if ((LF_ISSET(S_PARENT) &&
			    (u_int8_t)(slevel + 1) >= (LEVEL(h) - 1)) ||
			    (LEVEL(h) - 1) == LEAFLEVEL)
				stack = 1;

			/*
			 * Returning a subtree.  See if we have hit the start
			 * point if so save the parent and set stack.
			 * Otherwise free the parent and temporarily
			 * save this one.
			 * For S_DEL we need to find a page with 1 entry.
			 * For S_NEXT we want find the minimal subtree
			 * that contains the key and the next page.
			 * We save pages as long as we are at the right
			 * edge of the subtree.  When we leave the right
			 * edge, then drop the subtree.
			 */
			if (!LF_ISSET(S_DEL | S_NEXT)) {
				if ((ret = __memp_fput(mpf, h, 0)) != 0)
					goto err;
				goto lock_next;
			}

			if ((LF_ISSET(S_DEL) && NUM_ENT(h) == 1)) {
				stack = 1;
				LF_SET(S_WRITE);
				/* Push the parent. */
				cp->csp++;
				/* Push this node. */
				BT_STK_PUSH(dbp->dbenv, cp, h,
				     indx, lock, lock_mode, ret);
				if (ret != 0)
					goto err;
				LOCK_INIT(lock);
			} else {
			/*
			 * See if we want to save the tree so far.
			 * If we are looking for the next key,
			 * then we must save this node if we are
			 * at the end of the page.  If not then
			 * discard anything we have saved so far.
			 * For delete only keep one node until
			 * we find a singleton.
			 */
do_del:				if (cp->csp->page != NULL) {
					if (LF_ISSET(S_NEXT) &&
					     indx == NUM_ENT(h) - 1)
						cp->csp++;
					else if ((ret =
					    __bam_stkrel(dbc, STK_NOLOCK)) != 0)
						goto err;
				}
				/* Save this node. */
				BT_STK_ENTER(dbp->dbenv, cp,
				    h, indx, lock, lock_mode, ret);
				if (ret != 0)
					goto err;
				LOCK_INIT(lock);
			}

lock_next:		h = NULL;

			if (stack && LF_ISSET(S_WRITE))
				lock_mode = DB_LOCK_WRITE;
			if ((ret = __db_lget(dbc,
			    LCK_COUPLE_ALWAYS, pg, lock_mode, 0, &lock)) != 0) {
				/*
				 * If we fail, discard the lock we held.  This
				 * is OK because this only happens when we are
				 * descending the tree holding read-locks.
				 */
				(void)__LPUT(dbc, lock);
				if (LF_ISSET(S_DEL | S_NEXT))
					cp->csp++;
				goto err;
			}
		}
		if ((ret = __memp_fget(mpf, &pg, 0, &h)) != 0)
			goto err;
	}
	/* NOTREACHED */

found:	*exactp = 1;

	/*
	 * If we got here, we know that we have a Btree leaf or off-page
	 * duplicates page.  If it's a Btree leaf page, we have to handle
	 * on-page duplicates.
	 *
	 * If there are duplicates, go to the first/last one.  This is
	 * safe because we know that we're not going to leave the page,
	 * all duplicate sets that are not on overflow pages exist on a
	 * single leaf page.
	 */
	if (TYPE(h) == P_LBTREE && NUM_ENT(h) > P_INDX) {
		if (LF_ISSET(S_DUPLAST))
			while (indx < (db_indx_t)(NUM_ENT(h) - P_INDX) &&
			    inp[indx] == inp[indx + P_INDX])
				indx += P_INDX;
		else if (LF_ISSET(S_DUPFIRST))
			while (indx > 0 &&
			    inp[indx] == inp[indx - P_INDX])
				indx -= P_INDX;
	}

	/*
	 * Now check if we are allowed to return deleted items; if not, then
	 * find the next (or previous) non-deleted duplicate entry.  (We do
	 * not move from the original found key on the basis of the S_DELNO
	 * flag.)
	 */
	DB_ASSERT(recnop == NULL || LF_ISSET(S_DELNO));
	if (LF_ISSET(S_DELNO)) {
		deloffset = TYPE(h) == P_LBTREE ? O_INDX : 0;
		if (LF_ISSET(S_DUPLAST))
			while (B_DISSET(GET_BKEYDATA(dbp,
			    h, indx + deloffset)->type) && indx > 0 &&
			    inp[indx] == inp[indx - adjust])
				indx -= adjust;
		else
			while (B_DISSET(GET_BKEYDATA(dbp,
			    h, indx + deloffset)->type) &&
			    indx < (db_indx_t)(NUM_ENT(h) - adjust) &&
			    inp[indx] == inp[indx + adjust])
				indx += adjust;

		/*
		 * If we weren't able to find a non-deleted duplicate, return
		 * DB_NOTFOUND.
		 */
		if (B_DISSET(GET_BKEYDATA(dbp, h, indx + deloffset)->type)) {
			ret = DB_NOTFOUND;
			goto err;
		}

		/*
		 * Increment the record counter to point to the found element.
		 * Ignore any deleted key/data pairs.  There doesn't need to
		 * be any correction for duplicates, as Btree doesn't support
		 * duplicates and record numbers in the same tree.
		 */
		if (recnop != NULL) {
			DB_ASSERT(TYPE(h) == P_LBTREE);

			for (i = 0; i < indx; i += P_INDX)
				if (!B_DISSET(
				    GET_BKEYDATA(dbp, h, i + O_INDX)->type))
					++recno;

			/* Correct the number for a 0-base. */
			*recnop = recno + 1;
		}
	}

	if (LF_ISSET(S_STK_ONLY)) {
		BT_STK_NUM(dbp->dbenv, cp, h, indx, ret);
		if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
			ret = t_ret;
		if ((t_ret = __memp_fput(mpf, h, 0)) != 0 && ret == 0)
			ret = t_ret;
	} else {
		if (LF_ISSET(S_DEL) && cp->csp == cp->sp)
			cp->csp++;
		BT_STK_ENTER(dbp->dbenv, cp, h, indx, lock, lock_mode, ret);
	}
	if (ret != 0)
		goto err;

	return (0);

err:	if (h != NULL && (t_ret = __memp_fput(mpf, h, 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Keep any not-found page locked for serializability. */
	if ((t_ret = __TLPUT(dbc, lock)) != 0 && ret == 0)
		ret = t_ret;

	BT_STK_POP(cp);
	__bam_stkrel(dbc, 0);

	return (ret);
}

/*
 * __bam_stkrel --
 *	Release all pages currently held in the stack.
 *
 * PUBLIC: int __bam_stkrel __P((DBC *, u_int32_t));
 */
int
__bam_stkrel(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	DB_MPOOLFILE *mpf;
	EPG *epg;
	int ret, t_ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * Release inner pages first.
	 *
	 * The caller must be sure that setting STK_NOLOCK will not effect
	 * either serializability or recoverability.
	 */
	for (ret = 0, epg = cp->sp; epg <= cp->csp; ++epg) {
		if (epg->page != NULL) {
			if (LF_ISSET(STK_CLRDBC) && cp->page == epg->page) {
				cp->page = NULL;
				LOCK_INIT(cp->lock);
			}
			if ((t_ret =
			    __memp_fput(mpf, epg->page, 0)) != 0 && ret == 0)
				ret = t_ret;
			/*
			 * XXX
			 * Temporary fix for #3243 -- under certain deadlock
			 * conditions we call here again and re-free the page.
			 * The correct fix is to never release a stack that
			 * doesn't hold items.
			 */
			epg->page = NULL;
		}
		/*
		 * We set this if we need to release our pins,
		 * but are not logically ready to have the pages
		 * visible.
		 */
		if (LF_ISSET(STK_PGONLY))
			continue;
		if (LF_ISSET(STK_NOLOCK)) {
			if ((t_ret = __LPUT(dbc, epg->lock)) != 0 && ret == 0)
				ret = t_ret;
		} else
			if ((t_ret = __TLPUT(dbc, epg->lock)) != 0 && ret == 0)
				ret = t_ret;
	}

	/* Clear the stack, all pages have been released. */
	if (!LF_ISSET(STK_PGONLY))
		BT_STK_CLR(cp);

	return (ret);
}

/*
 * __bam_stkgrow --
 *	Grow the stack.
 *
 * PUBLIC: int __bam_stkgrow __P((DB_ENV *, BTREE_CURSOR *));
 */
int
__bam_stkgrow(dbenv, cp)
	DB_ENV *dbenv;
	BTREE_CURSOR *cp;
{
	EPG *p;
	size_t entries;
	int ret;

	entries = cp->esp - cp->sp;

	if ((ret = __os_calloc(dbenv, entries * 2, sizeof(EPG), &p)) != 0)
		return (ret);
	memcpy(p, cp->sp, entries * sizeof(EPG));
	if (cp->sp != cp->stack)
		__os_free(dbenv, cp->sp);
	cp->sp = p;
	cp->csp = p + entries;
	cp->esp = p + entries * 2;
	return (0);
}
