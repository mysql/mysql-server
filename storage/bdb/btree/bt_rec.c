/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: bt_rec.c,v 11.70 2004/09/24 00:43:12 bostic Exp $
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
#include "dbinc/log.h"
#include "dbinc/mp.h"

#define	IS_BTREE_PAGE(pagep)						\
	(TYPE(pagep) == P_IBTREE ||					\
	 TYPE(pagep) == P_LBTREE || TYPE(pagep) == P_LDUP)

/*
 * __bam_split_recover --
 *	Recovery function for split.
 *
 * PUBLIC: int __bam_split_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_split_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_split_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *_lp, *lp, *np, *pp, *_rp, *rp, *sp;
	db_pgno_t pgno, root_pgno;
	u_int32_t ptype;
	int cmp, l_update, p_update, r_update, rc, ret, rootsplit, t_ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__bam_split_print);

	mpf = NULL;
	_lp = lp = np = pp = _rp = rp = NULL;
	sp = NULL;

	REC_INTRO(__bam_split_read, 1);

	/*
	 * There are two kinds of splits that we have to recover from.  The
	 * first is a root-page split, where the root page is split from a
	 * leaf page into an internal page and two new leaf pages are created.
	 * The second is where a page is split into two pages, and a new key
	 * is inserted into the parent page.
	 *
	 * DBTs are not aligned in log records, so we need to copy the page
	 * so that we can access fields within it throughout this routine.
	 * Although we could hardcode the unaligned copies in this routine,
	 * we will be calling into regular btree functions with this page,
	 * so it's got to be aligned.  Copying it into allocated memory is
	 * the only way to guarantee this.
	 */
	if ((ret = __os_malloc(dbenv, argp->pg.size, &sp)) != 0)
		goto out;
	memcpy(sp, argp->pg.data, argp->pg.size);

	pgno = PGNO(sp);
	root_pgno = argp->root_pgno;
	rootsplit = root_pgno != PGNO_INVALID;
	REC_FGET(mpf, argp->left, &lp, right);
right:	REC_FGET(mpf, argp->right, &rp, redo);

redo:	if (DB_REDO(op)) {
		l_update = r_update = p_update = 0;
		/*
		 * Decide if we need to resplit the page.
		 *
		 * If this is a root split, then the root has to exist unless
		 * we have truncated it due to a future deallocation.
		 */
		if (rootsplit) {
			REC_FGET(mpf, root_pgno, &pp, do_left);
			cmp =
			    log_compare(&LSN(pp), &LSN(argp->pg.data));
			CHECK_LSN(op,
			    cmp, &LSN(pp), &LSN(argp->pg.data));
			p_update = cmp  == 0;
		}

do_left:	if (lp != NULL) {
			cmp = log_compare(&LSN(lp), &argp->llsn);
			CHECK_LSN(op, cmp, &LSN(lp), &argp->llsn);
			if (cmp == 0)
				l_update = 1;
		}

		if (rp != NULL) {
			cmp = log_compare(&LSN(rp), &argp->rlsn);
			CHECK_LSN(op, cmp, &LSN(rp), &argp->rlsn);
			if (cmp == 0)
				r_update = 1;
		}

		if (!p_update && !l_update && !r_update)
			goto check_next;

		/* Allocate and initialize new left/right child pages. */
		if ((ret = __os_malloc(dbenv, file_dbp->pgsize, &_lp)) != 0 ||
		    (ret = __os_malloc(dbenv, file_dbp->pgsize, &_rp)) != 0)
			goto out;
		if (rootsplit) {
			P_INIT(_lp, file_dbp->pgsize, argp->left,
			    PGNO_INVALID,
			    ISINTERNAL(sp) ? PGNO_INVALID : argp->right,
			    LEVEL(sp), TYPE(sp));
			P_INIT(_rp, file_dbp->pgsize, argp->right,
			    ISINTERNAL(sp) ?  PGNO_INVALID : argp->left,
			    PGNO_INVALID, LEVEL(sp), TYPE(sp));
		} else {
			P_INIT(_lp, file_dbp->pgsize, PGNO(sp),
			    ISINTERNAL(sp) ? PGNO_INVALID : PREV_PGNO(sp),
			    ISINTERNAL(sp) ? PGNO_INVALID : argp->right,
			    LEVEL(sp), TYPE(sp));
			P_INIT(_rp, file_dbp->pgsize, argp->right,
			    ISINTERNAL(sp) ? PGNO_INVALID : sp->pgno,
			    ISINTERNAL(sp) ? PGNO_INVALID : NEXT_PGNO(sp),
			    LEVEL(sp), TYPE(sp));
		}

		/* Split the page. */
		if ((ret = __bam_copy(file_dbp, sp, _lp, 0, argp->indx)) != 0 ||
		    (ret = __bam_copy(file_dbp, sp, _rp, argp->indx,
		    NUM_ENT(sp))) != 0)
			goto out;

		if (l_update) {
			memcpy(lp, _lp, file_dbp->pgsize);
			lp->lsn = *lsnp;
			if ((ret = __memp_fput(mpf, lp, DB_MPOOL_DIRTY)) != 0)
				goto out;
			lp = NULL;
		}

		if (r_update) {
			memcpy(rp, _rp, file_dbp->pgsize);
			rp->lsn = *lsnp;
			if ((ret = __memp_fput(mpf, rp, DB_MPOOL_DIRTY)) != 0)
				goto out;
			rp = NULL;
		}

		/*
		 * If the parent page is wrong, update it.  This is of interest
		 * only if it was a root split, since root splits create parent
		 * pages.  All other splits modify a parent page, but those are
		 * separately logged and recovered.
		 */
		if (rootsplit && p_update) {
			if (IS_BTREE_PAGE(sp)) {
				ptype = P_IBTREE;
				rc = argp->opflags & SPL_NRECS ? 1 : 0;
			} else {
				ptype = P_IRECNO;
				rc = 1;
			}

			P_INIT(pp, file_dbp->pgsize, root_pgno,
			    PGNO_INVALID, PGNO_INVALID, _lp->level + 1, ptype);
			RE_NREC_SET(pp, rc ? __bam_total(file_dbp, _lp) +
			    __bam_total(file_dbp, _rp) : 0);

			pp->lsn = *lsnp;
			if ((ret = __memp_fput(mpf, pp, DB_MPOOL_DIRTY)) != 0)
				goto out;
			pp = NULL;
		}

check_next:	/*
		 * Finally, redo the next-page link if necessary.  This is of
		 * interest only if it wasn't a root split -- inserting a new
		 * page in the tree requires that any following page have its
		 * previous-page pointer updated to our new page.  The next
		 * page must exist because we're redoing the operation.
		 */
		if (!rootsplit && argp->npgno != PGNO_INVALID) {
			if ((ret =
			    __memp_fget(mpf, &argp->npgno, 0, &np)) != 0) {
				if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
				    || DB_REDO(op)
#endif
				    ) {
					ret = __db_pgerr(
					    file_dbp, argp->npgno, ret);
					goto out;
				} else
					goto done;
			}
			cmp = log_compare(&LSN(np), &argp->nlsn);
			CHECK_LSN(op, cmp, &LSN(np), &argp->nlsn);
			if (cmp == 0) {
				PREV_PGNO(np) = argp->right;
				np->lsn = *lsnp;
				if ((ret =
				    __memp_fput(mpf, np, DB_MPOOL_DIRTY)) != 0)
					goto out;
				np = NULL;
			}
		}
	} else {
		/*
		 * If the split page is wrong, replace its contents with the
		 * logged page contents.  If the page doesn't exist, it means
		 * that the create of the page never happened, nor did any of
		 * the adds onto the page that caused the split, and there's
		 * really no undo-ing to be done.
		 */
		if ((ret = __memp_fget(mpf, &pgno, 0, &pp)) != 0) {
			pp = NULL;
			goto lrundo;
		}
		if (log_compare(lsnp, &LSN(pp)) == 0) {
			memcpy(pp, argp->pg.data, argp->pg.size);
			if ((ret = __memp_fput(mpf, pp, DB_MPOOL_DIRTY)) != 0)
				goto out;
			pp = NULL;
		}

		/*
		 * If it's a root split and the left child ever existed, update
		 * its LSN.  (If it's not a root split, we've updated the left
		 * page already -- it's the same as the split page.) If the
		 * right child ever existed, root split or not, update its LSN.
		 * The undo of the page allocation(s) will restore them to the
		 * free list.
		 */
lrundo:		if ((rootsplit && lp != NULL) || rp != NULL) {
			if (rootsplit && lp != NULL &&
			    log_compare(lsnp, &LSN(lp)) == 0) {
				lp->lsn = argp->llsn;
				if ((ret =
				    __memp_fput(mpf, lp, DB_MPOOL_DIRTY)) != 0)
					goto out;
				lp = NULL;
			}
			if (rp != NULL &&
			    log_compare(lsnp, &LSN(rp)) == 0) {
				rp->lsn = argp->rlsn;
				if ((ret =
				    __memp_fput(mpf, rp, DB_MPOOL_DIRTY)) != 0)
					goto out;
				rp = NULL;
			}
		}

		/*
		 * Finally, undo the next-page link if necessary.  This is of
		 * interest only if it wasn't a root split -- inserting a new
		 * page in the tree requires that any following page have its
		 * previous-page pointer updated to our new page.  Since it's
		 * possible that the next-page never existed, we ignore it as
		 * if there's nothing to undo.
		 */
		if (!rootsplit && argp->npgno != PGNO_INVALID) {
			if ((ret =
			    __memp_fget(mpf, &argp->npgno, 0, &np)) != 0) {
				np = NULL;
				goto done;
			}
			if (log_compare(lsnp, &LSN(np)) == 0) {
				PREV_PGNO(np) = argp->left;
				np->lsn = argp->nlsn;
				if (__memp_fput(mpf, np, DB_MPOOL_DIRTY))
					goto out;
				np = NULL;
			}
		}
	}

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	/* Free any pages that weren't dirtied. */
	if (pp != NULL && (t_ret = __memp_fput(mpf, pp, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (lp != NULL && (t_ret = __memp_fput(mpf, lp, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (np != NULL && (t_ret = __memp_fput(mpf, np, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (rp != NULL && (t_ret = __memp_fput(mpf, rp, 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Free any allocated space. */
	if (_lp != NULL)
		__os_free(dbenv, _lp);
	if (_rp != NULL)
		__os_free(dbenv, _rp);
	if (sp != NULL)
		__os_free(dbenv, sp);

	REC_CLOSE;
}

/*
 * __bam_rsplit_recover --
 *	Recovery function for a reverse split.
 *
 * PUBLIC: int __bam_rsplit_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_rsplit_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_rsplit_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_LSN copy_lsn;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno, root_pgno;
	int cmp_n, cmp_p, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__bam_rsplit_print);
	REC_INTRO(__bam_rsplit_read, 1);

	/* Fix the root page. */
	pgno = root_pgno = argp->root_pgno;
	if ((ret = __memp_fget(mpf, &pgno, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, pgno, ret);
			goto out;
		} else
			goto do_page;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->rootlsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->rootlsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		memcpy(pagep, argp->pgdbt.data, argp->pgdbt.size);
		pagep->pgno = root_pgno;
		pagep->lsn = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		P_INIT(pagep, file_dbp->pgsize, root_pgno,
		    argp->nrec, PGNO_INVALID, pagep->level + 1,
		    IS_BTREE_PAGE(pagep) ? P_IBTREE : P_IRECNO);
		if ((ret = __db_pitem(dbc, pagep, 0,
		    argp->rootent.size, &argp->rootent, NULL)) != 0)
			goto out;
		pagep->lsn = argp->rootlsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

do_page:
	/*
	 * Fix the page copied over the root page.  It's possible that the
	 * page never made it to disk, so if we're undo-ing and the page
	 * doesn't exist, it's okay and there's nothing further to do.
	 */
	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		} else
			goto done;
	}
	modified = 0;
	(void)__ua_memcpy(&copy_lsn, &LSN(argp->pgdbt.data), sizeof(DB_LSN));
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &copy_lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &copy_lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		pagep->lsn = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		memcpy(pagep, argp->pgdbt.data, argp->pgdbt.size);
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __bam_adj_recover --
 *	Recovery function for adj.
 *
 * PUBLIC: int __bam_adj_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_adj_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_adj_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__bam_adj_print);
	REC_INTRO(__bam_adj_read, 1);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		} else
			goto done;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		if ((ret = __bam_adjindx(dbc,
		    pagep, argp->indx, argp->indx_copy, argp->is_insert)) != 0)
			goto out;

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		if ((ret = __bam_adjindx(dbc,
		    pagep, argp->indx, argp->indx_copy, !argp->is_insert)) != 0)
			goto out;

		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __bam_cadjust_recover --
 *	Recovery function for the adjust of a count change in an internal
 *	page.
 *
 * PUBLIC: int __bam_cadjust_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_cadjust_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_cadjust_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__bam_cadjust_print);
	REC_INTRO(__bam_cadjust_read, 1);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		} else
			goto done;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		if (IS_BTREE_PAGE(pagep)) {
			GET_BINTERNAL(file_dbp, pagep, argp->indx)->nrecs +=
			    argp->adjust;
			if (argp->opflags & CAD_UPDATEROOT)
				RE_NREC_ADJ(pagep, argp->adjust);
		} else {
			GET_RINTERNAL(file_dbp, pagep, argp->indx)->nrecs +=
			    argp->adjust;
			if (argp->opflags & CAD_UPDATEROOT)
				RE_NREC_ADJ(pagep, argp->adjust);
		}

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		if (IS_BTREE_PAGE(pagep)) {
			GET_BINTERNAL(file_dbp, pagep, argp->indx)->nrecs -=
			    argp->adjust;
			if (argp->opflags & CAD_UPDATEROOT)
				RE_NREC_ADJ(pagep, -(argp->adjust));
		} else {
			GET_RINTERNAL(file_dbp, pagep, argp->indx)->nrecs -=
			    argp->adjust;
			if (argp->opflags & CAD_UPDATEROOT)
				RE_NREC_ADJ(pagep, -(argp->adjust));
		}
		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __bam_cdel_recover --
 *	Recovery function for the intent-to-delete of a cursor record.
 *
 * PUBLIC: int __bam_cdel_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_cdel_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_cdel_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t indx;
	int cmp_n, cmp_p, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__bam_cdel_print);
	REC_INTRO(__bam_cdel_read, 1);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		} else
			goto done;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		indx = argp->indx + (TYPE(pagep) == P_LBTREE ? O_INDX : 0);
		B_DSET(GET_BKEYDATA(file_dbp, pagep, indx)->type);

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		indx = argp->indx + (TYPE(pagep) == P_LBTREE ? O_INDX : 0);
		B_DCLR(GET_BKEYDATA(file_dbp, pagep, indx)->type);

		(void)__bam_ca_delete(file_dbp, argp->pgno, argp->indx, 0);

		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __bam_repl_recover --
 *	Recovery function for page item replacement.
 *
 * PUBLIC: int __bam_repl_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_repl_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_repl_args *argp;
	BKEYDATA *bk;
	DB *file_dbp;
	DBC *dbc;
	DBT dbt;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;
	u_int8_t *p;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__bam_repl_print);
	REC_INTRO(__bam_repl_read, 1);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		} else
			goto done;
	}
	bk = GET_BKEYDATA(file_dbp, pagep, argp->indx);

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/*
		 * Need to redo update described.
		 *
		 * Re-build the replacement item.
		 */
		memset(&dbt, 0, sizeof(dbt));
		dbt.size = argp->prefix + argp->suffix + argp->repl.size;
		if ((ret = __os_malloc(dbenv, dbt.size, &dbt.data)) != 0)
			goto out;
		p = dbt.data;
		memcpy(p, bk->data, argp->prefix);
		p += argp->prefix;
		memcpy(p, argp->repl.data, argp->repl.size);
		p += argp->repl.size;
		memcpy(p, bk->data + (bk->len - argp->suffix), argp->suffix);

		ret = __bam_ritem(dbc, pagep, argp->indx, &dbt);
		__os_free(dbenv, dbt.data);
		if (ret != 0)
			goto out;

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/*
		 * Need to undo update described.
		 *
		 * Re-build the original item.
		 */
		memset(&dbt, 0, sizeof(dbt));
		dbt.size = argp->prefix + argp->suffix + argp->orig.size;
		if ((ret = __os_malloc(dbenv, dbt.size, &dbt.data)) != 0)
			goto out;
		p = dbt.data;
		memcpy(p, bk->data, argp->prefix);
		p += argp->prefix;
		memcpy(p, argp->orig.data, argp->orig.size);
		p += argp->orig.size;
		memcpy(p, bk->data + (bk->len - argp->suffix), argp->suffix);

		ret = __bam_ritem(dbc, pagep, argp->indx, &dbt);
		__os_free(dbenv, dbt.data);
		if (ret != 0)
			goto out;

		/* Reset the deleted flag, if necessary. */
		if (argp->isdeleted)
			B_DSET(GET_BKEYDATA(file_dbp, pagep, argp->indx)->type);

		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __bam_root_recover --
 *	Recovery function for setting the root page on the meta-data page.
 *
 * PUBLIC: int __bam_root_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_root_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_root_args *argp;
	BTMETA *meta;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	int cmp_n, cmp_p, modified, ret;

	meta = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__bam_root_print);
	REC_INTRO(__bam_root_read, 0);

	if ((ret = __memp_fget(mpf, &argp->meta_pgno, 0, &meta)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->meta_pgno, ret);
			goto out;
		} else
			goto done;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(meta));
	cmp_p = log_compare(&LSN(meta), &argp->meta_lsn);
	CHECK_LSN(op, cmp_p, &LSN(meta), &argp->meta_lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		meta->root = argp->root_pgno;
		meta->dbmeta.lsn = *lsnp;
		((BTREE *)file_dbp->bt_internal)->bt_root = meta->root;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Nothing to undo except lsn. */
		meta->dbmeta.lsn = argp->meta_lsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	meta = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (meta != NULL)
		(void)__memp_fput(mpf, meta, 0);
	REC_CLOSE;
}

/*
 * __bam_curadj_recover --
 *	Transaction abort function to undo cursor adjustments.
 *	This should only be triggered by subtransaction aborts.
 *
 * PUBLIC: int __bam_curadj_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_curadj_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_curadj_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	int ret;

	COMPQUIET(info, NULL);
	COMPQUIET(mpf, NULL);

	REC_PRINT(__bam_curadj_print);
	REC_INTRO(__bam_curadj_read, 0);

	ret = 0;
	if (op != DB_TXN_ABORT)
		goto done;

	switch (argp->mode) {
	case DB_CA_DI:
		if ((ret = __bam_ca_di(dbc, argp->from_pgno,
		    argp->from_indx, -(int)argp->first_indx)) != 0)
			goto out;
		break;
	case DB_CA_DUP:
		if ((ret = __bam_ca_undodup(file_dbp, argp->first_indx,
		    argp->from_pgno, argp->from_indx, argp->to_indx)) != 0)
			goto out;
		break;

	case DB_CA_RSPLIT:
		if ((ret =
		    __bam_ca_rsplit(dbc, argp->to_pgno, argp->from_pgno)) != 0)
			goto out;
		break;

	case DB_CA_SPLIT:
		__bam_ca_undosplit(file_dbp, argp->from_pgno,
		    argp->to_pgno, argp->left_pgno, argp->from_indx);
		break;
	}

done:	*lsnp = argp->prev_lsn;
out:	REC_CLOSE;
}

/*
 * __bam_rcuradj_recover --
 *	Transaction abort function to undo cursor adjustments in rrecno.
 *	This should only be triggered by subtransaction aborts.
 *
 * PUBLIC: int __bam_rcuradj_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_rcuradj_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_rcuradj_args *argp;
	BTREE_CURSOR *cp;
	DB *file_dbp;
	DBC *dbc, *rdbc;
	DB_MPOOLFILE *mpf;
	int ret, t_ret;

	COMPQUIET(info, NULL);
	COMPQUIET(mpf, NULL);
	rdbc = NULL;

	REC_PRINT(__bam_rcuradj_print);
	REC_INTRO(__bam_rcuradj_read, 0);

	ret = t_ret = 0;

	if (op != DB_TXN_ABORT)
		goto done;

	/*
	 * We don't know whether we're in an offpage dup set, and
	 * thus don't know whether the dbc REC_INTRO has handed us is
	 * of a reasonable type.  It's certainly unset, so if this is
	 * an offpage dup set, we don't have an OPD cursor.  The
	 * simplest solution is just to allocate a whole new cursor
	 * for our use;  we're only really using it to hold pass some
	 * state into __ram_ca, and this way we don't need to make
	 * this function know anything about how offpage dups work.
	 */
	if ((ret = __db_cursor_int(file_dbp,
		NULL, DB_RECNO, argp->root, 0, DB_LOCK_INVALIDID, &rdbc)) != 0)
		goto out;

	cp = (BTREE_CURSOR *)rdbc->internal;
	F_SET(cp, C_RENUMBER);
	cp->recno = argp->recno;

	switch (argp->mode) {
	case CA_DELETE:
		/*
		 * The way to undo a delete is with an insert.  Since
		 * we're undoing it, the delete flag must be set.
		 */
		F_SET(cp, C_DELETED);
		F_SET(cp, C_RENUMBER);	/* Just in case. */
		cp->order = argp->order;
		(void)__ram_ca(rdbc, CA_ICURRENT);
		break;
	case CA_IAFTER:
	case CA_IBEFORE:
	case CA_ICURRENT:
		/*
		 * The way to undo an insert is with a delete.  The delete
		 * flag is unset to start with.
		 */
		F_CLR(cp, C_DELETED);
		cp->order = INVALID_ORDER;
		(void)__ram_ca(rdbc, CA_DELETE);
		break;
	}

done:	*lsnp = argp->prev_lsn;
out:	if (rdbc != NULL && (t_ret = __db_c_close(rdbc)) != 0 && ret == 0)
		ret = t_ret;
	REC_CLOSE;
}

/*
 * __bam_relink_recover --
 *	Recovery function for relink.
 *
 * PUBLIC: int __bam_relink_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__bam_relink_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__bam_relink_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__bam_relink_print);
	REC_INTRO(__bam_relink_read, 1);

	/*
	 * There are up to three pages we need to check -- the page, and the
	 * previous and next pages, if they existed.  For a page add operation,
	 * the current page is the result of a split and is being recovered
	 * elsewhere, so all we need do is recover the next page.
	 */
	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		} else
			goto next2;
	}
	modified = 0;

	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Redo the relink. */
		pagep->lsn = *lsnp;
		modified = 1;
	} else if (log_compare(lsnp, &LSN(pagep)) == 0 && DB_UNDO(op)) {
		/* Undo the relink. */
		pagep->next_pgno = argp->next;
		pagep->prev_pgno = argp->prev;

		pagep->lsn = argp->lsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

next2: if ((ret = __memp_fget(mpf, &argp->next, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->next, ret);
			goto out;
		} else
			goto prev;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn_next);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn_next);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Redo the remove or undo the add. */
		pagep->prev_pgno = argp->prev;

		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Undo the remove or redo the add. */
		pagep->prev_pgno = argp->pgno;

		modified = 1;
	}
	if (modified == 1) {
		if (DB_UNDO(op))
			pagep->lsn = argp->lsn_next;
		else
			pagep->lsn = *lsnp;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

prev: if ((ret = __memp_fget(mpf, &argp->prev, 0, &pagep)) != 0) {
		if (ret != DB_PAGE_NOTFOUND
#ifndef HAVE_FTRUNCATE
		     || DB_REDO(op)
#endif
		     ) {
			ret = __db_pgerr(file_dbp, argp->prev, ret);
			goto out;
		} else
			goto done;
	}

	modified = 0;
	cmp_p = log_compare(&LSN(pagep), &argp->lsn_prev);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn_prev);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Redo the relink. */
		pagep->next_pgno = argp->next;

		modified = 1;
	} else if (log_compare(lsnp, &LSN(pagep)) == 0 && DB_UNDO(op)) {
		/* Undo the relink. */
		pagep->next_pgno = argp->pgno;

		modified = 1;
	}
	if (modified == 1) {
		if (DB_UNDO(op))
			pagep->lsn = argp->lsn_prev;
		else
			pagep->lsn = *lsnp;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}
