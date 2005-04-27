/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_rec.c,v 11.35 2002/08/08 03:57:49 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/log.h"
#include "dbinc/hash.h"

/*
 * PUBLIC: int __db_addrem_recover
 * PUBLIC:    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 *
 * This log message is generated whenever we add or remove a duplicate
 * to/from a duplicate page.  On recover, we just do the opposite.
 */
int
__db_addrem_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_addrem_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t change;
	int cmp_n, cmp_p, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__db_addrem_print);
	REC_INTRO(__db_addrem_read, 1);

	if ((ret = mpf->get(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (DB_UNDO(op)) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			goto done;
		} else
			if ((ret = mpf->get(mpf,
			    &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);
	change = 0;
	if ((cmp_p == 0 && DB_REDO(op) && argp->opcode == DB_ADD_DUP) ||
	    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == DB_REM_DUP)) {

		/* Need to redo an add, or undo a delete. */
		if ((ret = __db_pitem(dbc, pagep, argp->indx, argp->nbytes,
		    argp->hdr.size == 0 ? NULL : &argp->hdr,
		    argp->dbt.size == 0 ? NULL : &argp->dbt)) != 0)
			goto out;

		change = DB_MPOOL_DIRTY;

	} else if ((cmp_n == 0 && DB_UNDO(op) && argp->opcode == DB_ADD_DUP) ||
	    (cmp_p == 0 && DB_REDO(op) && argp->opcode == DB_REM_DUP)) {
		/* Need to undo an add, or redo a delete. */
		if ((ret = __db_ditem(dbc,
		    pagep, argp->indx, argp->nbytes)) != 0)
			goto out;
		change = DB_MPOOL_DIRTY;
	}

	if (change) {
		if (DB_REDO(op))
			LSN(pagep) = *lsnp;
		else
			LSN(pagep) = argp->pagelsn;
	}

	if ((ret = mpf->put(mpf, pagep, change)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)mpf->put(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * PUBLIC: int __db_big_recover
 * PUBLIC:     __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_big_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_big_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t change;
	int cmp_n, cmp_p, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__db_big_print);
	REC_INTRO(__db_big_read, 1);

	if ((ret = mpf->get(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (DB_UNDO(op)) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			ret = 0;
			goto ppage;
		} else
			if ((ret = mpf->get(mpf,
			    &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;
	}

	/*
	 * There are three pages we need to check.  The one on which we are
	 * adding data, the previous one whose next_pointer may have
	 * been updated, and the next one whose prev_pointer may have
	 * been updated.
	 */
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);
	change = 0;
	if ((cmp_p == 0 && DB_REDO(op) && argp->opcode == DB_ADD_BIG) ||
	    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == DB_REM_BIG)) {
		/* We are either redo-ing an add, or undoing a delete. */
		P_INIT(pagep, file_dbp->pgsize, argp->pgno, argp->prev_pgno,
			argp->next_pgno, 0, P_OVERFLOW);
		OV_LEN(pagep) = argp->dbt.size;
		OV_REF(pagep) = 1;
		memcpy((u_int8_t *)pagep + P_OVERHEAD(file_dbp), argp->dbt.data,
		    argp->dbt.size);
		PREV_PGNO(pagep) = argp->prev_pgno;
		change = DB_MPOOL_DIRTY;
	} else if ((cmp_n == 0 && DB_UNDO(op) && argp->opcode == DB_ADD_BIG) ||
	    (cmp_p == 0 && DB_REDO(op) && argp->opcode == DB_REM_BIG)) {
		/*
		 * We are either undo-ing an add or redo-ing a delete.
		 * The page is about to be reclaimed in either case, so
		 * there really isn't anything to do here.
		 */
		change = DB_MPOOL_DIRTY;
	}
	if (change)
		LSN(pagep) = DB_REDO(op) ? *lsnp : argp->pagelsn;

	if ((ret = mpf->put(mpf, pagep, change)) != 0)
		goto out;
	pagep = NULL;

	/*
	 * We only delete a whole chain of overflow.
	 * Each page is handled individually
	 */
	if (argp->opcode == DB_REM_BIG)
		goto done;

	/* Now check the previous page. */
ppage:	if (argp->prev_pgno != PGNO_INVALID) {
		change = 0;
		if ((ret = mpf->get(mpf, &argp->prev_pgno, 0, &pagep)) != 0) {
			if (DB_UNDO(op)) {
				/*
				 * We are undoing and the page doesn't exist.
				 * That is equivalent to having a pagelsn of 0,
				 * so we would not have to undo anything.  In
				 * this case, don't bother creating a page.
				 */
				*lsnp = argp->prev_lsn;
				ret = 0;
				goto npage;
			} else
				if ((ret = mpf->get(mpf, &argp->prev_pgno,
				    DB_MPOOL_CREATE, &pagep)) != 0)
					goto out;
		}

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);
		CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->prevlsn);

		if (cmp_p == 0 && DB_REDO(op) && argp->opcode == DB_ADD_BIG) {
			/* Redo add, undo delete. */
			NEXT_PGNO(pagep) = argp->pgno;
			change = DB_MPOOL_DIRTY;
		} else if (cmp_n == 0 &&
		    DB_UNDO(op) && argp->opcode == DB_ADD_BIG) {
			/* Redo delete, undo add. */
			NEXT_PGNO(pagep) = argp->next_pgno;
			change = DB_MPOOL_DIRTY;
		}
		if (change)
			LSN(pagep) = DB_REDO(op) ? *lsnp : argp->prevlsn;
		if ((ret = mpf->put(mpf, pagep, change)) != 0)
			goto out;
	}
	pagep = NULL;

	/* Now check the next page.  Can only be set on a delete. */
npage:	if (argp->next_pgno != PGNO_INVALID) {
		change = 0;
		if ((ret = mpf->get(mpf, &argp->next_pgno, 0, &pagep)) != 0) {
			if (DB_UNDO(op)) {
				/*
				 * We are undoing and the page doesn't exist.
				 * That is equivalent to having a pagelsn of 0,
				 * so we would not have to undo anything.  In
				 * this case, don't bother creating a page.
				 */
				goto done;
			} else
				if ((ret = mpf->get(mpf, &argp->next_pgno,
				    DB_MPOOL_CREATE, &pagep)) != 0)
					goto out;
		}

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->nextlsn);
		CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->nextlsn);
		if (cmp_p == 0 && DB_REDO(op)) {
			PREV_PGNO(pagep) = PGNO_INVALID;
			change = DB_MPOOL_DIRTY;
		} else if (cmp_n == 0 && DB_UNDO(op)) {
			PREV_PGNO(pagep) = argp->pgno;
			change = DB_MPOOL_DIRTY;
		}
		if (change)
			LSN(pagep) = DB_REDO(op) ? *lsnp : argp->nextlsn;
		if ((ret = mpf->put(mpf, pagep, change)) != 0)
			goto out;
	}
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)mpf->put(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __db_ovref_recover --
 *	Recovery function for __db_ovref().
 *
 * PUBLIC: int __db_ovref_recover
 * PUBLIC:     __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_ovref_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_ovref_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__db_ovref_print);
	REC_INTRO(__db_ovref_read, 1);

	if ((ret = mpf->get(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (DB_UNDO(op))
			goto done;
		__db_pgerr(file_dbp, argp->pgno, ret);
		goto out;
	}

	modified = 0;
	cmp = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(op, cmp, &LSN(pagep), &argp->lsn);
	if (cmp == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		OV_REF(pagep) += argp->adjust;

		pagep->lsn = *lsnp;
		modified = 1;
	} else if (log_compare(lsnp, &LSN(pagep)) == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		OV_REF(pagep) -= argp->adjust;

		pagep->lsn = argp->lsn;
		modified = 1;
	}
	if ((ret = mpf->put(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)mpf->put(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __db_relink_recover --
 *	Recovery function for relink.
 *
 * PUBLIC: int __db_relink_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_relink_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_relink_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__db_relink_print);
	REC_INTRO(__db_relink_read, 1);

	/*
	 * There are up to three pages we need to check -- the page, and the
	 * previous and next pages, if they existed.  For a page add operation,
	 * the current page is the result of a split and is being recovered
	 * elsewhere, so all we need do is recover the next page.
	 */
	if ((ret = mpf->get(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (DB_REDO(op)) {
			__db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		}
		goto next2;
	}
	modified = 0;
	if (argp->opcode == DB_ADD_PAGE)
		goto next1;

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
next1:	if ((ret = mpf->put(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

next2:	if ((ret = mpf->get(mpf, &argp->next, 0, &pagep)) != 0) {
		if (DB_REDO(op)) {
			__db_pgerr(file_dbp, argp->next, ret);
			goto out;
		}
		goto prev;
	}
	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn_next);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn_next);
	if ((argp->opcode == DB_REM_PAGE && cmp_p == 0 && DB_REDO(op)) ||
	    (argp->opcode == DB_ADD_PAGE && cmp_n == 0 && DB_UNDO(op))) {
		/* Redo the remove or undo the add. */
		pagep->prev_pgno = argp->prev;

		modified = 1;
	} else if ((argp->opcode == DB_REM_PAGE && cmp_n == 0 && DB_UNDO(op)) ||
	    (argp->opcode == DB_ADD_PAGE && cmp_p == 0 && DB_REDO(op))) {
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
	if ((ret = mpf->put(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;
	if (argp->opcode == DB_ADD_PAGE)
		goto done;

prev:	if ((ret = mpf->get(mpf, &argp->prev, 0, &pagep)) != 0) {
		if (DB_REDO(op)) {
			__db_pgerr(file_dbp, argp->prev, ret);
			goto out;
		}
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
	if ((ret = mpf->put(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)mpf->put(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __db_debug_recover --
 *	Recovery function for debug.
 *
 * PUBLIC: int __db_debug_recover __P((DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_debug_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_debug_args *argp;
	int ret;

	COMPQUIET(dbenv, NULL);
	COMPQUIET(op, DB_TXN_ABORT);
	COMPQUIET(info, NULL);

	REC_PRINT(__db_debug_print);
	REC_NOOP_INTRO(__db_debug_read);

	*lsnp = argp->prev_lsn;
	ret = 0;

	REC_NOOP_CLOSE;
}

/*
 * __db_noop_recover --
 *	Recovery function for noop.
 *
 * PUBLIC: int __db_noop_recover __P((DB_ENV *,
 * PUBLIC:      DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_noop_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_noop_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t change;
	int cmp_n, cmp_p, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__db_noop_print);
	REC_INTRO(__db_noop_read, 0);

	if ((ret = mpf->get(mpf, &argp->pgno, 0, &pagep)) != 0)
		goto out;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->prevlsn);
	change = 0;
	if (cmp_p == 0 && DB_REDO(op)) {
		LSN(pagep) = *lsnp;
		change = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		LSN(pagep) = argp->prevlsn;
		change = DB_MPOOL_DIRTY;
	}
	ret = mpf->put(mpf, pagep, change);
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
out:	if (pagep != NULL)
		(void)mpf->put(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __db_pg_alloc_recover --
 *	Recovery function for pg_alloc.
 *
 * PUBLIC: int __db_pg_alloc_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_pg_alloc_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_pg_alloc_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DBMETA *meta;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	int cmp_n, cmp_p, created, level, modified, ret;

	meta = NULL;
	pagep = NULL;
	REC_PRINT(__db_pg_alloc_print);
	REC_INTRO(__db_pg_alloc_read, 0);

	/*
	 * Fix up the allocated page.  If we're redoing the operation, we have
	 * to get the page (creating it if it doesn't exist), and update its
	 * LSN.  If we're undoing the operation, we have to reset the page's
	 * LSN and put it on the free list.
	 *
	 * Fix up the metadata page.  If we're redoing the operation, we have
	 * to get the metadata page and update its LSN and its free pointer.
	 * If we're undoing the operation and the page was ever created, we put
	 * it on the freelist.
	 */
	pgno = PGNO_BASE_MD;
	if ((ret = mpf->get(mpf, &pgno, 0, &meta)) != 0) {
		/* The metadata page must always exist on redo. */
		if (DB_REDO(op)) {
			__db_pgerr(file_dbp, pgno, ret);
			goto out;
		} else
			goto done;
	}
	created = modified = 0;
	if ((ret = mpf->get(mpf, &argp->pgno, 0, &pagep)) != 0) {
		/*
		 * We have to be able to identify if a page was newly
		 * created so we can recover it properly.  We cannot simply
		 * look for an empty header, because hash uses a pgin
		 * function that will set the header.  Instead, we explicitly
		 * try for the page without CREATE and if that fails, then
		 * create it.
		 */
		if ((ret =
		    mpf->get(mpf, &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0) {
			__db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		}
		created = modified = 1;
	}

	/* Fix up the allocated page. */
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->page_lsn);

	/*
	 * If an inital allocation is aborted and then reallocated
	 * during an archival restore the log record will have
	 * an LSN for the page but the page will be empty.
	 */
	if (IS_ZERO_LSN(LSN(pagep)))
		cmp_p = 0;
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->page_lsn);
	/*
	 * If we we rolled back this allocation previously during an
	 * archive restore, the page may have the LSN of the meta page
	 * at the point of the roll back.  This will be no more
	 * than the LSN of the metadata page at the time of this allocation.
	 * Another special case we have to handle is if we ended up with a
	 * page of all 0's which can happen if we abort between allocating a
	 * page in mpool and initializing it.  In that case, even if we're
	 * undoing, we need to re-initialize the page.
	 */
	if (DB_REDO(op) &&
	    (cmp_p == 0 ||
	    (IS_ZERO_LSN(argp->page_lsn) &&
	    log_compare(&LSN(pagep), &argp->meta_lsn) <= 0))) {
		/* Need to redo update described. */
		switch (argp->ptype) {
		case P_LBTREE:
		case P_LRECNO:
		case P_LDUP:
			level = LEAFLEVEL;
			break;
		default:
			level = 0;
			break;
		}
		P_INIT(pagep, file_dbp->pgsize,
		    argp->pgno, PGNO_INVALID, PGNO_INVALID, level, argp->ptype);

		pagep->lsn = *lsnp;
		modified = 1;
	} else if (DB_UNDO(op) && (cmp_n == 0 || created)) {
		/*
		 * This is where we handle the case of a 0'd page (pagep->pgno
		 * is equal to PGNO_INVALID).
		 * Undo the allocation, reinitialize the page and
		 * link its next pointer to the free list.
		 */
		P_INIT(pagep, file_dbp->pgsize,
		    argp->pgno, PGNO_INVALID, argp->next, 0, P_INVALID);

		pagep->lsn = argp->page_lsn;
		modified = 1;
	}

	/*
	 * If the page was newly created, put it on the limbo list.
	 */
	if (IS_ZERO_LSN(LSN(pagep)) &&
	    IS_ZERO_LSN(argp->page_lsn) && DB_UNDO(op)) {
		/* Put the page in limbo.*/
		if ((ret = __db_add_limbo(dbenv,
		    info, argp->fileid, argp->pgno, 1)) != 0)
			goto out;
	}

	if ((ret = mpf->put(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

	/* Fix up the metadata page. */
	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(meta));
	cmp_p = log_compare(&LSN(meta), &argp->meta_lsn);
	CHECK_LSN(op, cmp_p, &LSN(meta), &argp->meta_lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		LSN(meta) = *lsnp;
		meta->free = argp->next;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		LSN(meta) = argp->meta_lsn;

		/*
		 * If the page has a zero LSN then its newly created
		 * and will go into limbo rather than directly on the
		 * free list.
		 */
		if (!IS_ZERO_LSN(argp->page_lsn))
			meta->free = argp->pgno;
		modified = 1;
	}
	if ((ret = mpf->put(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	meta = NULL;
	/*
	 * This could be the metapage from a subdb which is read from disk
	 * to recover its creation.
	 */
	if (F_ISSET(file_dbp, DB_AM_SUBDB))
		switch (argp->type) {
		case P_BTREEMETA:
		case P_HASHMETA:
		case P_QAMMETA:
			file_dbp->sync(file_dbp, 0);
			break;
		}

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)mpf->put(mpf, pagep, 0);
	if (meta != NULL)
		(void)mpf->put(mpf, meta, 0);
	if (ret == ENOENT && op == DB_TXN_BACKWARD_ALLOC)
		ret = 0;
	REC_CLOSE;
}

/*
 * __db_pg_free_recover --
 *	Recovery function for pg_free.
 *
 * PUBLIC: int __db_pg_free_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_pg_free_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_pg_free_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DBMETA *meta;
	DB_LSN copy_lsn;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	int cmp_n, cmp_p, modified, ret;

	COMPQUIET(info, NULL);
	meta = NULL;
	pagep = NULL;
	REC_PRINT(__db_pg_free_print);
	REC_INTRO(__db_pg_free_read, 1);

	/*
	 * Fix up the freed page.  If we're redoing the operation we get the
	 * page and explicitly discard its contents, then update its LSN.  If
	 * we're undoing the operation, we get the page and restore its header.
	 * Create the page if necessary, we may be freeing an aborted
	 * create.
	 */
	if ((ret = mpf->get(mpf, &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
		goto out;
	modified = 0;
	(void)__ua_memcpy(&copy_lsn, &LSN(argp->header.data), sizeof(DB_LSN));
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &copy_lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &copy_lsn);
	if (DB_REDO(op) &&
	    (cmp_p == 0 ||
	    (IS_ZERO_LSN(copy_lsn) &&
	    log_compare(&LSN(pagep), &argp->meta_lsn) <= 0))) {
		/* Need to redo update described. */
		P_INIT(pagep, file_dbp->pgsize,
		    argp->pgno, PGNO_INVALID, argp->next, 0, P_INVALID);
		pagep->lsn = *lsnp;

		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		memcpy(pagep, argp->header.data, argp->header.size);

		modified = 1;
	}
	if ((ret = mpf->put(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

	/*
	 * Fix up the metadata page.  If we're redoing or undoing the operation
	 * we get the page and update its LSN and free pointer.
	 */
	pgno = PGNO_BASE_MD;
	if ((ret = mpf->get(mpf, &pgno, 0, &meta)) != 0) {
		/* The metadata page must always exist. */
		__db_pgerr(file_dbp, pgno, ret);
		goto out;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(meta));
	cmp_p = log_compare(&LSN(meta), &argp->meta_lsn);
	CHECK_LSN(op, cmp_p, &LSN(meta), &argp->meta_lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo the deallocation. */
		meta->free = argp->pgno;
		LSN(meta) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo the deallocation. */
		meta->free = argp->next;
		LSN(meta) = argp->meta_lsn;
		modified = 1;
	}
	if ((ret = mpf->put(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	meta = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)mpf->put(mpf, pagep, 0);
	if (meta != NULL)
		(void)mpf->put(mpf, meta, 0);
	REC_CLOSE;
}

/*
 * __db_cksum_recover --
 *	Recovery function for checksum failure log record.
 *
 * PUBLIC: int __db_cksum_recover __P((DB_ENV *,
 * PUBLIC:      DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_cksum_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_cksum_args *argp;

	int ret;

	COMPQUIET(info, NULL);
	COMPQUIET(lsnp, NULL);
	COMPQUIET(op, DB_TXN_ABORT);

	REC_PRINT(__db_cksum_print);

	if ((ret = __db_cksum_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	/*
	 * We had a checksum failure -- the only option is to run catastrophic
	 * recovery.
	 */
	if (F_ISSET(dbenv, DB_ENV_FATAL))
		ret = 0;
	else {
		__db_err(dbenv,
		    "Checksum failure requires catastrophic recovery");
		ret = __db_panic(dbenv, DB_RUNRECOVERY);
	}

	__os_free(dbenv, argp);
	return (ret);
}
