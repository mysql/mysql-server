/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_rec.c,v 12.12 2005/10/27 01:03:01 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/hash.h"

static int __db_pg_free_recover_int __P((DB_ENV *,
    __db_pg_freedata_args *, DB *, DB_LSN *, DB_MPOOLFILE *, db_recops, int));

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
	REC_INTRO(__db_addrem_read, 1, 1);

	REC_FGET(mpf, argp->pgno, &pagep, done);

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &argp->pagelsn);
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

	if ((ret = __memp_fput(mpf, pagep, change)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
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
	REC_INTRO(__db_big_read, 1, 0);

	REC_FGET(mpf, argp->pgno, &pagep, ppage);

	/*
	 * There are three pages we need to check.  The one on which we are
	 * adding data, the previous one whose next_pointer may have
	 * been updated, and the next one whose prev_pointer may have
	 * been updated.
	 */
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &argp->pagelsn);
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

	if ((ret = __memp_fput(mpf, pagep, change)) != 0)
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
		REC_FGET(mpf, argp->prev_pgno, &pagep, npage);

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);
		CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &argp->prevlsn);

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
		if ((ret = __memp_fput(mpf, pagep, change)) != 0)
			goto out;
	}
	pagep = NULL;

	/* Now check the next page.  Can only be set on a delete. */
npage:	if (argp->next_pgno != PGNO_INVALID) {
		change = 0;
		REC_FGET(mpf, argp->next_pgno, &pagep, done);

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->nextlsn);
		CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &argp->nextlsn);
		if (cmp_p == 0 && DB_REDO(op)) {
			PREV_PGNO(pagep) = PGNO_INVALID;
			change = DB_MPOOL_DIRTY;
		} else if (cmp_n == 0 && DB_UNDO(op)) {
			PREV_PGNO(pagep) = argp->pgno;
			change = DB_MPOOL_DIRTY;
		}
		if (change)
			LSN(pagep) = DB_REDO(op) ? *lsnp : argp->nextlsn;
		if ((ret = __memp_fput(mpf, pagep, change)) != 0)
			goto out;
	}
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
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
	REC_INTRO(__db_ovref_read, 1, 0);

	REC_FGET(mpf, argp->pgno, &pagep, done);

	modified = 0;
	cmp = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(dbenv, op, cmp, &LSN(pagep), &argp->lsn);
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
	REC_INTRO(__db_noop_read, 0, 0);

	REC_FGET(mpf, argp->pgno, &pagep, done);

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);
	CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &argp->prevlsn);
	change = 0;
	if (cmp_p == 0 && DB_REDO(op)) {
		LSN(pagep) = *lsnp;
		change = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		LSN(pagep) = argp->prevlsn;
		change = DB_MPOOL_DIRTY;
	}
	ret = __memp_fput(mpf, pagep, change);
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
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
	int cmp_n, cmp_p, created, level, meta_modified, modified, ret;

	meta = NULL;
	pagep = NULL;
	created = meta_modified = modified = 0;
	REC_PRINT(__db_pg_alloc_print);
	REC_INTRO(__db_pg_alloc_read, 0, 0);

	/*
	 * Fix up the metadata page.  If we're redoing the operation, we have
	 * to get the metadata page and update its LSN and its free pointer.
	 * If we're undoing the operation and the page was ever created, we put
	 * it on the freelist.
	 */
	pgno = PGNO_BASE_MD;
	if ((ret = __memp_fget(mpf, &pgno, 0, &meta)) != 0) {
		/* The metadata page must always exist on redo. */
		if (DB_REDO(op)) {
			ret = __db_pgerr(file_dbp, pgno, ret);
			goto out;
		} else
			goto done;
	}
	cmp_n = log_compare(lsnp, &LSN(meta));
	cmp_p = log_compare(&LSN(meta), &argp->meta_lsn);
	CHECK_LSN(dbenv, op, cmp_p, &LSN(meta), &argp->meta_lsn);
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		LSN(meta) = *lsnp;
		meta->free = argp->next;
		meta_modified = 1;
		if (argp->pgno > meta->last_pgno)
			meta->last_pgno = argp->pgno;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		LSN(meta) = argp->meta_lsn;
		/*
		 * If the page has a zero LSN then its newly created
		 * and will be truncated or go into limbo rather than
		 * directly on the free list.
		 */
		if (!IS_ZERO_LSN(argp->page_lsn))
			meta->free = argp->pgno;
#ifdef HAVE_FTRUNCATE
		/*
		 * With truncate we will restore the file to
		 * its original length.  Without truncate
		 * the last_pgno never goes backward.
		 */
		meta->last_pgno = argp->last_pgno;
#endif
		meta_modified = 1;
	}

#ifdef HAVE_FTRUNCATE
	/*
	 * Check to see if we are keeping a sorted
	 * freelist, if so put this back in the in
	 * memory list.  It must be the first element.
	 */
	if (op == DB_TXN_ABORT && !IS_ZERO_LSN(argp->page_lsn)) {
		db_pgno_t *list;
		u_int32_t nelem;

		if ((ret = __memp_get_freelist(mpf, &nelem, &list)) != 0)
			goto out;
		if (list != NULL) {
			if ((ret =
			    __memp_extend_freelist(mpf, nelem + 1, &list)) != 0)
				goto out;
			if (nelem != 0)
				memmove(list + 1, list, nelem * sizeof(list));
			*list = argp->pgno;
		}
	}
#endif

	/*
	 * Fix up the allocated page. If the page does not exist
	 * and we can truncate it then don't create it.
	 * Otherwise if we're redoing the operation, we have
	 * to get the page (creating it if it doesn't exist), and update its
	 * LSN.  If we're undoing the operation, we have to reset the page's
	 * LSN and put it on the free list, or into limbo..
	 */
	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		/*
		 * We have to be able to identify if a page was newly
		 * created so we can recover it properly.  We cannot simply
		 * look for an empty header, because hash uses a pgin
		 * function that will set the header.  Instead, we explicitly
		 * try for the page without CREATE and if that fails, then
		 * create it.
		 */
#ifdef HAVE_FTRUNCATE
		if (DB_UNDO(op))
			goto do_truncate;
#endif
		if ((ret = __memp_fget(
		    mpf, &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0) {
			if (DB_UNDO(op) && ret == ENOSPC)
				goto do_truncate;
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		}
		created = modified = 1;
	}

	/* Fix up the allocated page. */
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->page_lsn);

	/*
	 * If an initial allocation is aborted and then reallocated during
	 * an archival restore the log record will have an LSN for the page
	 * but the page will be empty.
	 * If we we rolled back this allocation previously during an
	 * archive restore, the page may have INIT_LSN from the limbo list.
	 */
	if (IS_ZERO_LSN(LSN(pagep)) ||
	    (IS_ZERO_LSN(argp->page_lsn) && IS_INIT_LSN(LSN(pagep))))
		cmp_p = 0;

	CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &argp->page_lsn);
	/*
	 * Another special case we have to handle is if we ended up with a
	 * page of all 0's which can happen if we abort between allocating a
	 * page in mpool and initializing it.  In that case, even if we're
	 * undoing, we need to re-initialize the page.
	 */
	if (DB_REDO(op) && cmp_p == 0) {
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

do_truncate:
	/*
	 * If the page was newly created, give it back, if
	 * possible.  Otherwise put it into limbo.
	 */
	if ((pagep == NULL || IS_ZERO_LSN(LSN(pagep))) &&
	    IS_ZERO_LSN(argp->page_lsn) && DB_UNDO(op)) {
#ifdef HAVE_FTRUNCATE
		COMPQUIET(info, NULL);
		/* Discard the page. */
		if (pagep != NULL) {
			if ((ret =
			     __memp_fput(mpf, pagep, DB_MPOOL_DISCARD)) != 0)
				goto out;
			pagep = NULL;
			/* Give the page back to the OS. */
			if (meta->last_pgno <= argp->pgno &&
			    (ret = __memp_ftruncate(mpf, argp->pgno, 0)) != 0)
				goto out;
		}
#else
		/* Put the page in limbo.*/
		if ((ret = __db_add_limbo(dbenv,
		    info, argp->fileid, argp->pgno, 1)) != 0)
			goto out;
		/* The last_pgno grows if this was a new page. */
		if (argp->pgno > meta->last_pgno) {
			meta->last_pgno = argp->pgno;
			meta_modified = 1;
		}
#endif
	}

	if (pagep != NULL &&
	     (ret = __memp_fput(mpf,
	     pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

	if ((ret = __memp_fput(mpf,
	    meta, meta_modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	meta = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	if (meta != NULL)
		(void)__memp_fput(mpf, meta, 0);
	if (ret == ENOENT && op == DB_TXN_BACKWARD_ALLOC)
		ret = 0;
	REC_CLOSE;
}

/*
 * __db_pg_free_recover_int --
 */
static int
__db_pg_free_recover_int(dbenv, argp, file_dbp, lsnp, mpf, op, data)
	DB_ENV *dbenv;
	__db_pg_freedata_args *argp;
	DB *file_dbp;
	DB_LSN *lsnp;
	DB_MPOOLFILE *mpf;
	db_recops op;
	int data;
{
	DBMETA *meta;
	DB_LSN copy_lsn;
	PAGE *pagep, *prevp;
	int cmp_n, cmp_p, is_meta, meta_modified, modified, ret;

	meta = NULL;
	pagep = NULL;
	prevp = NULL;
	meta_modified = modified = 0;

	/*
	 * Get the "metapage".  This will either be the metapage
	 * or the previous page in the free list if we are doing
	 * sorted allocations.  If its a previous page then
	 * we will not be truncating.
	 */
	is_meta = argp->meta_pgno == PGNO_BASE_MD;

	REC_FGET(mpf, argp->meta_pgno, &meta, check_meta);

	if (argp->meta_pgno != PGNO_BASE_MD)
		prevp = (PAGE *)meta;

	cmp_n = log_compare(lsnp, &LSN(meta));
	cmp_p = log_compare(&LSN(meta), &argp->meta_lsn);
	CHECK_LSN(dbenv, op, cmp_p, &LSN(meta), &argp->meta_lsn);

	/*
	 * Fix up the metadata page.  If we're redoing or undoing the operation
	 * we get the page and update its LSN, last and free pointer.
	 */
	if (cmp_p == 0 && DB_REDO(op)) {
#ifdef HAVE_FTRUNCATE
		/*
		 * If we are at the end of the file truncate, otherwise
		 * put on the free list.
		*/
		if (argp->pgno == argp->last_pgno)
			meta->last_pgno = argp->pgno - 1;
		else if (prevp == NULL)
			meta->free = argp->pgno;
		else
			NEXT_PGNO(prevp) = argp->pgno;
#else
		/* Need to redo the deallocation. */
		if (prevp == NULL)
			meta->free = argp->pgno;
		else
			NEXT_PGNO(prevp) = argp->pgno;
		/*
		 * If this was a compensating transaction and
		 * we are a replica, then we never executed the
		 * original allocation which incremented meta->free.
		 */
		if (prevp == NULL && meta->last_pgno < meta->free)
			meta->last_pgno = meta->free;
#endif
		LSN(meta) = *lsnp;
		meta_modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo the deallocation. */
		if (prevp == NULL)
			meta->free = argp->next;
		else
			NEXT_PGNO(prevp) = argp->next;
		LSN(meta) = argp->meta_lsn;
		if (prevp == NULL && meta->last_pgno < argp->pgno)
			meta->last_pgno = argp->pgno;
		meta_modified = 1;
	}

check_meta:
	if (ret != 0 && is_meta) {
		/* The metadata page must always exist. */
		ret = __db_pgerr(file_dbp, argp->meta_pgno, ret);
		goto out;
	}

	/*
	 * Get the freed page.  If we support truncate then don't
	 * create the page if we are going to free it.  If we're
	 * redoing the operation we get the page and explicitly discard
	 * its contents, then update its LSN.  If we're undoing the
	 * operation, we get the page and restore its header.
	 * If we don't support truncate, then we must create the page
	 * and roll it back.
	 */
#ifdef HAVE_FTRUNCATE
	if (DB_REDO(op) || (is_meta && meta->last_pgno < argp->pgno)) {
		if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
			if (ret == DB_PAGE_NOTFOUND)
				goto done;
			goto out;
		}
	} else
#endif
	if ((ret =
	    __memp_fget(mpf, &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
		goto out;

	(void)__ua_memcpy(&copy_lsn, &LSN(argp->header.data), sizeof(DB_LSN));
	cmp_n = IS_ZERO_LSN(LSN(pagep)) ? 0 : log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &copy_lsn);

#ifdef HAVE_FTRUNCATE
	/*
	 * This page got extended by a later allocation,
	 * but its allocation was not in the scope of this
	 * recovery pass.
	 */
	if (IS_ZERO_LSN(LSN(pagep)))
		cmp_p = 0;
#endif

	CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &copy_lsn);
	if (DB_REDO(op) &&
	    (cmp_p == 0 ||
	    (IS_ZERO_LSN(copy_lsn) &&
	    log_compare(&LSN(pagep), &argp->meta_lsn) <= 0))) {
		/* Need to redo the deallocation. */
#ifdef HAVE_FTRUNCATE
		/*
		 * The page can be truncated if it was truncated at runtime
		 * and the current metapage reflects the truncation.
		 */
		if (is_meta && meta->last_pgno <= argp->pgno &&
		    argp->last_pgno <= argp->pgno) {
			if ((ret =
			    __memp_fput(mpf, pagep, DB_MPOOL_DISCARD)) != 0)
				goto out;
			pagep = NULL;
			if ((ret = __memp_ftruncate(mpf, argp->pgno, 0)) != 0)
				goto out;
		} else if (argp->last_pgno == argp->pgno) {
			/* The page was truncated at runtime, zero it out. */
			P_INIT(pagep, 0, PGNO_INVALID,
			    PGNO_INVALID, PGNO_INVALID, 0, P_INVALID);
			ZERO_LSN(pagep->lsn);
			modified = 1;
		} else
#endif
		{
			P_INIT(pagep, file_dbp->pgsize,
			    argp->pgno, PGNO_INVALID, argp->next, 0, P_INVALID);
			pagep->lsn = *lsnp;

			modified = 1;
		}
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to reallocate the page. */
		memcpy(pagep, argp->header.data, argp->header.size);
		if (data)
			memcpy((u_int8_t*)pagep + HOFFSET(pagep),
			     argp->data.data, argp->data.size);

		modified = 1;
	}
	if (pagep != NULL &&
	    (ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

	pagep = NULL;
#ifdef HAVE_FTRUNCATE
	/*
	 * If we are keeping an in memory free list remove this
	 * element from the list.
	 */
	if (op == DB_TXN_ABORT && argp->pgno != argp->last_pgno) {
		db_pgno_t *lp;
		u_int32_t nelem, pos;

		if ((ret = __memp_get_freelist(mpf, &nelem, &lp)) != 0)
			goto out;
		if (lp != NULL) {
			pos = 0;
			if (!is_meta && nelem != 0) {
				__db_freelist_pos(argp->pgno, lp, nelem, &pos);

				DB_ASSERT(argp->pgno == lp[pos]);
				DB_ASSERT(argp->meta_pgno == lp[pos - 1]);
			}

			if (nelem != 0 && pos != nelem)
				memmove(&lp[pos], &lp[pos + 1],
				    (nelem - pos) * sizeof(*lp));

			/* Shrink the list */
			if ((ret =
			    __memp_extend_freelist(mpf, nelem - 1, &lp)) != 0)
				goto out;
		}
	}
done:
#endif
	if (meta != NULL && (ret = __memp_fput(mpf,
	     meta, meta_modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	meta = NULL;

	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	if (meta != NULL)
		(void)__memp_fput(mpf, meta, 0);

	return (ret);
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
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	__db_pg_free_args *argp;
	int ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__db_pg_free_print);
	REC_INTRO(__db_pg_free_read, 1, 0);

	ret = __db_pg_free_recover_int(dbenv,
	     (__db_pg_freedata_args *)argp, file_dbp, lsnp, mpf, op, 0);

done:	*lsnp = argp->prev_lsn;
out:
	REC_CLOSE;
}

/*
 * __db_pg_new_recover --
 *	A new page from the file was put on the free list.
 * This record is only generated during a LIMBO_COMPENSATE.
 *
 * PUBLIC: int __db_pg_new_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_pg_new_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
#ifndef HAVE_FTRUNCATE
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	__db_pg_free_args *argp;
	int ret;

	REC_PRINT(__db_pg_free_print);
	REC_INTRO(__db_pg_free_read, 1, 0);
	COMPQUIET(op, DB_TXN_ABORT);

	if ((ret =
	    __db_add_limbo(dbenv, info, argp->fileid, argp->pgno, 1)) == 0)
		*lsnp = argp->prev_lsn;

done:
out:
	REC_CLOSE;
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(dbtp, NULL);
	COMPQUIET(lsnp, NULL);
	COMPQUIET(op, DB_TXN_PRINT);
	COMPQUIET(info, NULL);
	return (0);
#endif
}

/*
 * __db_pg_freedata_recover --
 *	Recovery function for pg_freedata.
 *
 * PUBLIC: int __db_pg_freedata_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_pg_freedata_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	__db_pg_freedata_args *argp;
	int ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__db_pg_freedata_print);
	REC_INTRO(__db_pg_freedata_read, 1, 0);

	ret = __db_pg_free_recover_int(dbenv, argp, file_dbp, lsnp, mpf, op, 1);

done:	*lsnp = argp->prev_lsn;
out:
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

/*
 * __db_pg_prepare_recover --
 *	Recovery function for pg_prepare.
 *
 * PUBLIC: int __db_pg_prepare_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_pg_prepare_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
#ifndef HAVE_FTRUNCATE
	__db_pg_prepare_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int ret, t_ret;

	REC_PRINT(__db_pg_prepare_print);
	REC_INTRO(__db_pg_prepare_read, 1, 0);

	mpf = file_dbp->mpf;

	/*
	 * If this made it into the limbo list at prepare time then
	 * it was a new free page allocated by an aborted subtransaction.
	 * Only that subtransaction could have toched the page.
	 * All other pages in the free list at this point are
	 * either of the same nature or were put there by this subtransactions
	 * other subtransactions that followed this one.  If
	 * they were put there by this subtransaction the log records
	 * of the following allocations will reflect that.
	 * Note that only one transaction could have had the
	 * metapage locked at the point of the crash.
	 * All this is to say that we can P_INIT this page without
	 * loosing other pages on the free list because they
	 * will be linked in by records earlier in the log for
	 * this transaction which we will roll back.
	 */
	if (op == DB_TXN_ABORT) {
		if ((ret = __memp_fget(
		    mpf, &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;
		P_INIT(pagep, file_dbp->pgsize,
		    argp->pgno, PGNO_INVALID, PGNO_INVALID, 0, P_INVALID);
		ZERO_LSN(pagep->lsn);
		ret = __db_add_limbo(dbenv, info, argp->fileid, argp->pgno, 1);
		if ((t_ret =
		    __memp_fput(mpf, pagep, DB_MPOOL_DIRTY)) != 0 && ret == 0)
			ret = t_ret;
	}

done:	if (ret == 0)
		*lsnp = argp->prev_lsn;
out:	REC_CLOSE;
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(dbtp, NULL);
	COMPQUIET(lsnp, NULL);
	COMPQUIET(op, DB_TXN_PRINT);
	COMPQUIET(info, NULL);
	return (0);
#endif

}

/*
 * __db_pg_init_recover --
 *	Recovery function to reinit pages for truncate.
 *
 * PUBLIC: int __db_pg_init_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_pg_init_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__db_pg_init_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_LSN copy_lsn;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret, type;

	COMPQUIET(info, NULL);
	REC_PRINT(__db_pg_init_print);
	REC_INTRO(__db_pg_init_read, 1, 0);

	mpf = file_dbp->mpf;
	REC_FGET(mpf, argp->pgno, &pagep, done);

	modified = 0;
	(void)__ua_memcpy(&copy_lsn, &LSN(argp->header.data), sizeof(DB_LSN));
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &copy_lsn);
	CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &copy_lsn);

	if (cmp_p == 0 && DB_REDO(op)) {
		if (TYPE(pagep) == P_HASH)
			type = P_HASH;
		else
			type = file_dbp->type == DB_RECNO ? P_LRECNO : P_LBTREE;
		P_INIT(pagep, file_dbp->pgsize, PGNO(pagep), PGNO_INVALID,
		    PGNO_INVALID, TYPE(pagep) == P_HASH ? 0 : 1, type);
		pagep->lsn = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Put the data back on the page. */
		memcpy(pagep, argp->header.data, argp->header.size);
		if (argp->data.size > 0)
			memcpy((u_int8_t*)pagep + HOFFSET(pagep),
			     argp->data.data, argp->data.size);

		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

done:	*lsnp = argp->prev_lsn;
out:
	REC_CLOSE;
}

/*
 * __db_pg_sort_recover --
 *	Recovery function for pg_sort.
 *
 * PUBLIC: int __db_pg_sort_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__db_pg_sort_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
#ifdef HAVE_FTRUNCATE
	__db_pg_sort_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DBMETA *meta;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno, *list;
	u_int32_t felem, nelem;
	struct pglist *pglist, *lp;
	int modified, ret;

	COMPQUIET(info, NULL);

	REC_PRINT(__db_pg_sort_print);
	REC_INTRO(__db_pg_sort_read, 1, 1);

	modified = 0;

	pglist = (struct pglist *) argp->list.data;
	nelem = argp->list.size / sizeof(struct pglist);
	if (DB_REDO(op)) {
		pgno = argp->last_pgno;
		if ((ret = __db_pg_truncate(mpf,
		    pglist, NULL, &nelem, &pgno, lsnp, 1)) != 0)
			goto out;

		if (argp->last_free != PGNO_INVALID) {
			if ((ret = __memp_fget(mpf,
			     &argp->last_free, 0, &meta)) == 0) {
				if (log_compare(&LSN(meta),
				     &argp->last_lsn) == 0) {
					NEXT_PGNO(meta) = PGNO_INVALID;
					LSN(meta) = *lsnp;
					modified = 1;
				}
				if ((ret = __memp_fput(mpf,
				     meta, modified ? DB_MPOOL_DIRTY : 0)) != 0)
					goto out;
				meta = NULL;
				modified = 0;
			} else if (ret != DB_PAGE_NOTFOUND)
				goto out;
		}
		if ((ret = __memp_fget(mpf, &argp->meta, 0, &meta)) != 0)
			goto out;
		if (log_compare(&LSN(meta), &argp->meta_lsn) == 0) {
			if (argp->last_free == PGNO_INVALID) {
				if (nelem == 0)
					meta->free = PGNO_INVALID;
				else
					meta->free = pglist->pgno;
			}
			meta->last_pgno = pgno;
			LSN(meta) = *lsnp;
			modified = 1;
		}
	} else {
		/* Put the free list back in its original order. */
		for (lp = pglist; lp < &pglist[nelem]; lp++) {
			if ((ret = __memp_fget(mpf,
			     &lp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;
			if (IS_ZERO_LSN(LSN(pagep)) ||
			     log_compare(&LSN(pagep), lsnp) == 0) {
				if (lp == &pglist[nelem - 1])
					pgno = PGNO_INVALID;
				else
					pgno = lp[1].pgno;

				P_INIT(pagep, file_dbp->pgsize,
				    lp->pgno, PGNO_INVALID, pgno, 0, P_INVALID);
				LSN(pagep) = lp->lsn;
				modified = 1;
			}
			if ((ret = __memp_fput(mpf,
			     pagep, modified ? DB_MPOOL_DIRTY: 0)) != 0)
				goto out;
		}
		if (argp->last_free != PGNO_INVALID) {
			if ((ret = __memp_fget(mpf,
			     &argp->last_free, 0, &meta)) == 0) {
				if (log_compare(&LSN(meta), lsnp) == 0) {
					NEXT_PGNO(meta) = pglist->pgno;
					LSN(meta) = argp->last_lsn;
					modified = 1;
				}
				if ((ret = __memp_fput(mpf,
				     meta, modified ? DB_MPOOL_DIRTY : 0)) != 0)
					goto out;
			} else if (ret != DB_PAGE_NOTFOUND)
				goto out;
			modified = 0;
			meta = NULL;
		}
		if ((ret = __memp_fget(mpf, &argp->meta, 0, &meta)) != 0)
			goto out;
		if (log_compare(&LSN(meta), lsnp) == 0) {
			meta->last_pgno = argp->last_pgno;
			if (argp->last_pgno == PGNO_INVALID)
				meta->free = pglist->pgno;
			LSN(meta) = argp->meta_lsn;
			modified = 1;
		}
	}
	if (op == DB_TXN_ABORT) {
		if ((ret = __memp_get_freelist(mpf, &felem, &list)) != 0)
			goto out;
		if (list != NULL) {
			DB_ASSERT(felem == 0 ||
			    argp->last_free == list[felem - 1]);
			if ((ret = __memp_extend_freelist(
			    mpf, felem + nelem, &list)) != 0)
				goto out;
			for (lp = pglist; lp < &pglist[nelem]; lp++)
				list[felem++] = lp->pgno;
		}
	}

	if ((ret = __memp_fput(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
#else
	/*
	 * If HAVE_FTRUNCATE is not defined, we'll never see pg_sort records
	 * to recover.
	 */
	COMPQUIET(dbenv, NULL);
	COMPQUIET(dbtp, NULL);
	COMPQUIET(lsnp, NULL);
	COMPQUIET(op,  DB_TXN_ABORT);
	COMPQUIET(info, NULL);
	return (EINVAL);
#endif
}
