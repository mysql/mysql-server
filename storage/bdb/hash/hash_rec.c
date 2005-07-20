/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	Margo Seltzer.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	The President and Fellows of Harvard University.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
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
 * $Id: hash_rec.c,v 11.83 2004/10/25 16:52:13 ubell Exp $
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
#include "dbinc/hash.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"

static int __ham_alloc_pages __P((DB *, __ham_groupalloc_args *, DB_LSN *));

/*
 * __ham_insdel_recover --
 *
 * PUBLIC: int __ham_insdel_recover
 * PUBLIC:     __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_insdel_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_insdel_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t flags, opcode;
	int cmp_n, cmp_p, ret, type;

	pagep = NULL;
	COMPQUIET(info, NULL);

	REC_PRINT(__ham_insdel_print);
	REC_INTRO(__ham_insdel_read, 1);

	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (DB_UNDO(op)) {
			if (ret == DB_PAGE_NOTFOUND)
				goto done;
			else {
				ret = __db_pgerr(file_dbp, argp->pgno, ret);
				goto out;
			}
		}
#ifdef HAVE_FTRUNCATE
		/* If the page is not here then it was later truncated. */
		if (!IS_ZERO_LSN(argp->pagelsn))
			goto done;
#endif
		/*
		 * This page was created by a group allocation and
		 * the file may not have been extend yet.
		 * Create the page if necessary.
		 */
		if ((ret = __memp_fget(mpf,
		     &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		}
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);
	/*
	 * Two possible things going on:
	 * redo a delete/undo a put: delete the item from the page.
	 * redo a put/undo a delete: add the item to the page.
	 * If we are undoing a delete, then the information logged is the
	 * entire entry off the page, not just the data of a dbt.  In
	 * this case, we want to copy it back onto the page verbatim.
	 * We do this by calling __putitem with the type H_OFFPAGE instead
	 * of H_KEYDATA.
	 */

	opcode = OPCODE_OF(argp->opcode);
	flags = 0;
	if ((opcode == DELPAIR && cmp_n == 0 && DB_UNDO(op)) ||
	    (opcode == PUTPAIR && cmp_p == 0 && DB_REDO(op))) {
		/*
		 * Need to redo a PUT or undo a delete.  If we are undoing a
		 * delete, we've got to restore the item back to its original
		 * position.  That's a royal pain in the butt (because we do
		 * not store item lengths on the page), but there's no choice.
		 */
		if (opcode != DELPAIR ||
		    argp->ndx == (u_int32_t)NUM_ENT(pagep)) {
			__ham_putitem(file_dbp, pagep, &argp->key,
			    DB_UNDO(op) || PAIR_ISKEYBIG(argp->opcode) ?
			    H_OFFPAGE : H_KEYDATA);

			if (PAIR_ISDATADUP(argp->opcode))
				type = H_DUPLICATE;
			else if (DB_UNDO(op) || PAIR_ISDATABIG(argp->opcode))
				type = H_OFFPAGE;
			else
				type = H_KEYDATA;
			__ham_putitem(file_dbp, pagep, &argp->data, type);
		} else
			__ham_reputpair(file_dbp, pagep,
			    argp->ndx, &argp->key, &argp->data);

		LSN(pagep) = DB_REDO(op) ? *lsnp : argp->pagelsn;
		flags = DB_MPOOL_DIRTY;

	} else if ((opcode == DELPAIR && cmp_p == 0 && DB_REDO(op)) ||
	    (opcode == PUTPAIR && cmp_n == 0 && DB_UNDO(op))) {
		/* Need to undo a put or redo a delete. */
		__ham_dpair(file_dbp, pagep, argp->ndx);
		LSN(pagep) = DB_REDO(op) ? *lsnp : argp->pagelsn;
		flags = DB_MPOOL_DIRTY;
	}

	if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;
	pagep = NULL;

	/* Return the previous LSN. */
done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __ham_newpage_recover --
 *	This log message is used when we add/remove overflow pages.  This
 *	message takes care of the pointer chains, not the data on the pages.
 *
 * PUBLIC: int __ham_newpage_recover
 * PUBLIC:     __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_newpage_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_newpage_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t flags;
	int cmp_n, cmp_p, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);

	REC_PRINT(__ham_newpage_print);
	REC_INTRO(__ham_newpage_read, 1);

	REC_FGET(mpf, argp->new_pgno, &pagep, ppage);

	/*
	 * There are potentially three pages we need to check: the one
	 * that we created/deleted, the one before it and the one after
	 * it.
	 */

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);

	flags = 0;
	if ((cmp_p == 0 && DB_REDO(op) && argp->opcode == PUTOVFL) ||
	    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == DELOVFL)) {
		/* Redo a create new page or undo a delete new page. */
		P_INIT(pagep, file_dbp->pgsize, argp->new_pgno,
		    argp->prev_pgno, argp->next_pgno, 0, P_HASH);
		flags = DB_MPOOL_DIRTY;
	} else if ((cmp_p == 0 && DB_REDO(op) && argp->opcode == DELOVFL) ||
	    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == PUTOVFL)) {
		/*
		 * Redo a delete or undo a create new page.  All we
		 * really need to do is change the LSN.
		 */
		flags = DB_MPOOL_DIRTY;
	}

	if (flags)
		LSN(pagep) = DB_REDO(op) ? *lsnp : argp->pagelsn;

	if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;
	pagep = NULL;

	/* Now do the prev page. */
ppage:	if (argp->prev_pgno != PGNO_INVALID) {
		REC_FGET(mpf, argp->prev_pgno, &pagep, npage);

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);
		CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->prevlsn);
		flags = 0;

		if ((cmp_p == 0 && DB_REDO(op) && argp->opcode == PUTOVFL) ||
		    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == DELOVFL)) {
			/* Redo a create new page or undo a delete new page. */
			pagep->next_pgno = argp->new_pgno;
			flags = DB_MPOOL_DIRTY;
		} else if ((cmp_p == 0 &&
		    DB_REDO(op) && argp->opcode == DELOVFL) ||
		    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == PUTOVFL)) {
			/* Redo a delete or undo a create new page. */
			pagep->next_pgno = argp->next_pgno;
			flags = DB_MPOOL_DIRTY;
		}

		if (flags)
			LSN(pagep) = DB_REDO(op) ? *lsnp : argp->prevlsn;

		if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
			goto out;
		pagep = NULL;
	}

	/* Now time to do the next page */
npage:	if (argp->next_pgno != PGNO_INVALID) {
		REC_FGET(mpf, argp->next_pgno, &pagep, done);

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->nextlsn);
		CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->nextlsn);
		flags = 0;

		if ((cmp_p == 0 && DB_REDO(op) && argp->opcode == PUTOVFL) ||
		    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == DELOVFL)) {
			/* Redo a create new page or undo a delete new page. */
			pagep->prev_pgno = argp->new_pgno;
			flags = DB_MPOOL_DIRTY;
		} else if ((cmp_p == 0 &&
		    DB_REDO(op) && argp->opcode == DELOVFL) ||
		    (cmp_n == 0 && DB_UNDO(op) && argp->opcode == PUTOVFL)) {
			/* Redo a delete or undo a create new page. */
			pagep->prev_pgno = argp->prev_pgno;
			flags = DB_MPOOL_DIRTY;
		}

		if (flags)
			LSN(pagep) = DB_REDO(op) ? *lsnp : argp->nextlsn;

		if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
			goto out;
		pagep = NULL;
	}
done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __ham_replace_recover --
 *	This log message refers to partial puts that are local to a single
 *	page.  You can think of them as special cases of the more general
 *	insdel log message.
 *
 * PUBLIC: int __ham_replace_recover
 * PUBLIC:    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_replace_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_replace_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	DBT dbt;
	PAGE *pagep;
	u_int32_t flags;
	u_int32_t change;
	int cmp_n, cmp_p, is_plus, ret;
	u_int8_t *hk;

	pagep = NULL;
	COMPQUIET(info, NULL);

	REC_PRINT(__ham_replace_print);
	REC_INTRO(__ham_replace_read, 1);

	REC_FGET(mpf, argp->pgno, &pagep, done);

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);

	memset(&dbt, 0, sizeof(dbt));
	flags = 0;

	/*
	 * Before we know the direction of the transformation we will
	 * determine the size differential; then once we know if we are
	 * redoing or undoing, we'll adjust the sign (is_plus) appropriately.
	 */
	if (argp->newitem.size > argp->olditem.size) {
		change = argp->newitem.size - argp->olditem.size;
		is_plus = 1;
	} else {
		change = argp->olditem.size - argp->newitem.size;
		is_plus = 0;
	}
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Reapply the change as specified. */
		dbt.data = argp->newitem.data;
		dbt.size = argp->newitem.size;
		LSN(pagep) = *lsnp;
		/*
		 * The is_plus flag is set properly to reflect
		 * newitem.size - olditem.size.
		 */
		flags = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Undo the already applied change. */
		dbt.data = argp->olditem.data;
		dbt.size = argp->olditem.size;
		/*
		 * Invert is_plus to reflect sign of
		 * olditem.size - newitem.size.
		 */
		is_plus = !is_plus;
		LSN(pagep) = argp->pagelsn;
		flags = DB_MPOOL_DIRTY;
	}

	if (flags) {
		__ham_onpage_replace(file_dbp, pagep,
		    argp->ndx, argp->off, change, is_plus, &dbt);
		if (argp->makedup) {
			hk = P_ENTRY(file_dbp, pagep, argp->ndx);
			if (DB_REDO(op))
				HPAGE_PTYPE(hk) = H_DUPLICATE;
			else
				HPAGE_PTYPE(hk) = H_KEYDATA;
		}
	}

	if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __ham_splitdata_recover --
 *
 * PUBLIC: int __ham_splitdata_recover
 * PUBLIC:    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_splitdata_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_splitdata_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t flags;
	int cmp_n, cmp_p, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);

	REC_PRINT(__ham_splitdata_print);
	REC_INTRO(__ham_splitdata_read, 1);

	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (DB_UNDO(op)) {
			if (ret == DB_PAGE_NOTFOUND)
				goto done;
			else {
				ret = __db_pgerr(file_dbp, argp->pgno, ret);
				goto out;
			}
		}
#ifdef HAVE_FTRUNCATE
		/* If the page is not here then it was later truncated. */
		if (!IS_ZERO_LSN(argp->pagelsn))
			goto done;
#endif
		/*
		 * This page was created by a group allocation and
		 * the file may not have been extend yet.
		 * Create the page if necessary.
		 */
		if ((ret = __memp_fget(mpf,
		     &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0) {
			ret = __db_pgerr(file_dbp, argp->pgno, ret);
			goto out;
		}
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);

	/*
	 * There are two types of log messages here, one for the old page
	 * and one for the new pages created.  The original image in the
	 * SPLITOLD record is used for undo.  The image in the SPLITNEW
	 * is used for redo.  We should never have a case where there is
	 * a redo operation and the SPLITOLD record is on disk, but not
	 * the SPLITNEW record.  Therefore, we only have work to do when
	 * redo NEW messages and undo OLD messages, but we have to update
	 * LSNs in both cases.
	 */
	flags = 0;
	if (cmp_p == 0 && DB_REDO(op)) {
		if (argp->opcode == SPLITNEW)
			/* Need to redo the split described. */
			memcpy(pagep, argp->pageimage.data,
			    argp->pageimage.size);
		LSN(pagep) = *lsnp;
		flags = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		if (argp->opcode == SPLITOLD) {
			/* Put back the old image. */
			memcpy(pagep, argp->pageimage.data,
			    argp->pageimage.size);
		} else
			P_INIT(pagep, file_dbp->pgsize, argp->pgno,
			    PGNO_INVALID, PGNO_INVALID, 0, P_HASH);
		LSN(pagep) = argp->pagelsn;
		flags = DB_MPOOL_DIRTY;
	}
	if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __ham_copypage_recover --
 *	Recovery function for copypage.
 *
 * PUBLIC: int __ham_copypage_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_copypage_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_copypage_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t flags;
	int cmp_n, cmp_p, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);

	REC_PRINT(__ham_copypage_print);
	REC_INTRO(__ham_copypage_read, 1);

	flags = 0;

	/* This is the bucket page. */
	REC_FGET(mpf, argp->pgno, &pagep, donext);

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);

	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		memcpy(pagep, argp->page.data, argp->page.size);
		PGNO(pagep) = argp->pgno;
		PREV_PGNO(pagep) = PGNO_INVALID;
		LSN(pagep) = *lsnp;
		flags = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		P_INIT(pagep, file_dbp->pgsize, argp->pgno, PGNO_INVALID,
		    argp->next_pgno, 0, P_HASH);
		LSN(pagep) = argp->pagelsn;
		flags = DB_MPOOL_DIRTY;
	}
	if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;
	pagep = NULL;

donext:	/* Now fix up the "next" page. */
	REC_FGET(mpf, argp->next_pgno, &pagep, do_nn);

	/* For REDO just update the LSN. For UNDO copy page back. */
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->nextlsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->nextlsn);
	flags = 0;
	if (cmp_p == 0 && DB_REDO(op)) {
		LSN(pagep) = *lsnp;
		flags = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		memcpy(pagep, argp->page.data, argp->page.size);
		flags = DB_MPOOL_DIRTY;
	}
	if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;
	pagep = NULL;

	/* Now fix up the next's next page. */
do_nn:	if (argp->nnext_pgno == PGNO_INVALID)
		goto done;

	REC_FGET(mpf, argp->nnext_pgno, &pagep, done);

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->nnextlsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->nnextlsn);

	flags = 0;
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Need to redo update described. */
		PREV_PGNO(pagep) = argp->pgno;
		LSN(pagep) = *lsnp;
		flags = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Need to undo update described. */
		PREV_PGNO(pagep) = argp->next_pgno;
		LSN(pagep) = argp->nnextlsn;
		flags = DB_MPOOL_DIRTY;
	}
	if ((ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __ham_metagroup_recover --
 *	Recovery function for metagroup.
 *
 * PUBLIC: int __ham_metagroup_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_metagroup_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_metagroup_args *argp;
	HASH_CURSOR *hcp;
	DB *file_dbp;
	DBMETA *mmeta;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	u_int32_t flags, mmeta_flags;
	int cmp_n, cmp_p, did_recover, groupgrow, ret;

	COMPQUIET(info, NULL);
	mmeta_flags = 0;
	mmeta = NULL;
	REC_PRINT(__ham_metagroup_print);
	REC_INTRO(__ham_metagroup_read, 1);

	/*
	 * This logs the virtual create of pages pgno to pgno + bucket
	 * If HAVE_FTRUNCATE is not supported the mpool page-allocation is not
	 * transaction protected, we can never undo it.  Even in an abort,
	 * we have to allocate these pages to the hash table if they
	 * were actually created.  In particular, during disaster
	 * recovery the metapage may be before this point if we
	 * are rolling backward.  If the file has not been extended
	 * then the metapage could not have been updated.
	 * The log record contains:
	 * bucket: new bucket being allocated.
	 * pgno: page number of the new bucket.
	 * if bucket is a power of 2, then we allocated a whole batch of
	 * pages; if it's not, then we simply allocated one new page.
	 */
	groupgrow = (u_int32_t)(1 << __db_log2(argp->bucket + 1)) ==
	    argp->bucket + 1;
	pgno = argp->pgno;
	if (argp->newalloc)
		pgno += argp->bucket;

	flags = 0;
	pagep = NULL;
#ifndef HAVE_FTRUNCATE
	flags = DB_MPOOL_CREATE;
#endif
	ret = __memp_fget(mpf, &pgno, flags, &pagep);

#ifdef HAVE_FTRUNCATE
	/* If we are undoing, then we don't want to create the page. */
	if (ret != 0 && DB_REDO(op))
		ret = __memp_fget(mpf, &pgno, DB_MPOOL_CREATE, &pagep);
	else if (ret == DB_PAGE_NOTFOUND) {
		groupgrow = 0;
		goto do_meta;
	}
#endif
	if (ret != 0) {
		if (ret != ENOSPC)
			goto out;
		pgno = 0;
		groupgrow = 0;
		goto do_meta;
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->pagelsn);

	flags = 0;
	if (cmp_p == 0 && DB_REDO(op)) {
		pagep->lsn = *lsnp;
		flags = DB_MPOOL_DIRTY;
	}
	else if (cmp_n == 0 && DB_UNDO(op)) {
#ifdef HAVE_FTRUNCATE
		/* If this record allocated the pages give them back. */
		if (argp->newalloc) {
			if (pagep != NULL && (ret =
			     __memp_fput(mpf, pagep, DB_MPOOL_DISCARD)) != 0)
				goto out;
			pagep = NULL;
			if ((ret = __memp_ftruncate(mpf, argp->pgno, 0)) != 0)
				goto out;
		} else
#endif
		{
			/*
			 * Otherwise just roll the page back to its
			 * previous state.
			 */
			pagep->lsn = argp->pagelsn;
			flags = DB_MPOOL_DIRTY;
		}
	}
	if (pagep != NULL && (ret = __memp_fput(mpf, pagep, flags)) != 0)
		goto out;

do_meta:
	/* Now we have to update the meta-data page. */
	hcp = (HASH_CURSOR *)dbc->internal;
	if ((ret = __ham_get_meta(dbc)) != 0)
		goto out;
	cmp_n = log_compare(lsnp, &hcp->hdr->dbmeta.lsn);
	cmp_p = log_compare(&hcp->hdr->dbmeta.lsn, &argp->metalsn);
	CHECK_LSN(op, cmp_p, &hcp->hdr->dbmeta.lsn, &argp->metalsn);
	did_recover = 0;
	if (cmp_p == 0 && DB_REDO(op)) {
		/* Redo the actual updating of bucket counts. */
		++hcp->hdr->max_bucket;
		if (groupgrow) {
			hcp->hdr->low_mask = hcp->hdr->high_mask;
			hcp->hdr->high_mask =
			    (argp->bucket + 1) | hcp->hdr->low_mask;
		}
		hcp->hdr->dbmeta.lsn = *lsnp;
		did_recover = 1;
	} else if (cmp_n == 0 && DB_UNDO(op)) {
		/* Undo the actual updating of bucket counts. */
		--hcp->hdr->max_bucket;
		if (groupgrow) {
			hcp->hdr->high_mask = hcp->hdr->low_mask;
			hcp->hdr->low_mask = hcp->hdr->high_mask >> 1;
		}
		hcp->hdr->dbmeta.lsn = argp->metalsn;
		did_recover = 1;
	}

	/*
	 * Now we need to fix up the spares array.  Each entry in the
	 * spares array indicates the beginning page number for the
	 * indicated doubling.  We need to fill this in whenever the
	 * spares array is invalid, if we never reclaim pages then
	 * we have to allocate the pages to the
	 * spares array in both the redo and undo cases.
	 */
	if (groupgrow &&
#ifdef HAVE_FTRUNCATE
	    !DB_UNDO(op) &&
#endif
	    hcp->hdr->spares[__db_log2(argp->bucket + 1) + 1] == PGNO_INVALID) {
		hcp->hdr->spares[__db_log2(argp->bucket + 1) + 1] =
		    (argp->pgno - argp->bucket) - 1;
		did_recover = 1;
	}
#ifdef HAVE_FTRUNCATE
	if (cmp_n == 0 && groupgrow && DB_UNDO(op)) {
		hcp->hdr->spares[
		    __db_log2(argp->bucket + 1) + 1] = PGNO_INVALID;
		did_recover = 1;
	}
#endif

	/*
	 * Finally, we need to potentially fix up the last_pgno field
	 * in the master meta-data page (which may or may not be the
	 * same as the hash header page).
	 */
	if (argp->mmpgno != argp->mpgno) {
		if ((ret = __memp_fget(mpf, &argp->mmpgno, 0, &mmeta)) != 0)
			goto out;
		mmeta_flags = 0;
		cmp_n = log_compare(lsnp, &mmeta->lsn);
		cmp_p = log_compare(&mmeta->lsn, &argp->mmetalsn);
		if (cmp_p == 0 && DB_REDO(op))
			mmeta->lsn = *lsnp;
		else if (cmp_n == 0 && DB_UNDO(op))
			mmeta->lsn = argp->mmetalsn;
	} else
		mmeta = (DBMETA *)hcp->hdr;

#ifdef HAVE_FTRUNCATE
	if (cmp_n == 0 && DB_UNDO(op))
		mmeta->last_pgno = argp->last_pgno;
	else if (DB_REDO(op))
#endif
	if (mmeta->last_pgno < pgno)
		mmeta->last_pgno = pgno;
	mmeta_flags = DB_MPOOL_DIRTY;

	if (argp->mmpgno != argp->mpgno &&
	    (ret = __memp_fput(mpf, mmeta, mmeta_flags)) != 0)
		goto out;
	mmeta = NULL;

	if (did_recover)
		F_SET(hcp, H_DIRTY);

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (mmeta != NULL)
		(void)__memp_fput(mpf, mmeta, 0);
	if (dbc != NULL)
		(void)__ham_release_meta(dbc);
	if (ret == ENOENT && op == DB_TXN_BACKWARD_ALLOC)
		ret = 0;

	REC_CLOSE;
}

/*
 * __ham_groupalloc_recover --
 *	Recover the batch creation of a set of pages for a new database.
 *
 * PUBLIC: int __ham_groupalloc_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_groupalloc_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_groupalloc_args *argp;
	DBMETA *mmeta;
	DB_MPOOLFILE *mpf;
	DB *file_dbp;
	DBC *dbc;
	PAGE *pagep;
	db_pgno_t pgno;
	int cmp_n, cmp_p, modified, ret;

	mmeta = NULL;
	modified = 0;
	REC_PRINT(__ham_groupalloc_print);
	REC_INTRO(__ham_groupalloc_read, 0);

	pgno = PGNO_BASE_MD;
	if ((ret = __memp_fget(mpf, &pgno, 0, &mmeta)) != 0) {
		if (DB_REDO(op)) {
			ret = __db_pgerr(file_dbp, pgno, ret);
			goto out;
		} else
			goto done;
	}

	cmp_n = log_compare(lsnp, &LSN(mmeta));
	cmp_p = log_compare(&LSN(mmeta), &argp->meta_lsn);
	CHECK_LSN(op, cmp_p, &LSN(mmeta), &argp->meta_lsn);

	/*
	 * Basically, we used mpool to allocate a chunk of pages.
	 * We need to either add those to a free list (in the undo
	 * case) or initialize them (in the redo case).
	 *
	 * If we are redoing and this is a hash subdatabase, it's possible
	 * that the pages were never allocated, so we'd better check for
	 * that and handle it here.
	 */
	pgno = argp->start_pgno + argp->num - 1;
	if (DB_REDO(op)) {
		if ((ret = __ham_alloc_pages(file_dbp, argp, lsnp)) != 0)
			goto out;
		if (cmp_p == 0) {
			LSN(mmeta) = *lsnp;
			modified = 1;
		}
	} else if (DB_UNDO(op)) {
		/*
		 * Fetch the last page and determine if it is in
		 * the post allocation state.
		 */
		pagep = NULL;
		if ((ret = __memp_fget(mpf, &pgno, 0, &pagep)) == 0) {
			if (log_compare(&pagep->lsn, lsnp) != 0) {
				if ((ret = __memp_fput(mpf,
				     pagep, DB_MPOOL_DISCARD)) != 0)
					goto out;
				pagep = NULL;
			}
		} else if (ret != DB_PAGE_NOTFOUND)
			goto out;
#ifdef HAVE_FTRUNCATE
		COMPQUIET(info, NULL);
		/*
		 * If the last page was allocated then truncate back
		 * to the first page.
		 */
		if (pagep != NULL) {
			if ((ret =
			    __memp_fput(mpf, pagep, DB_MPOOL_DISCARD)) != 0)
				goto out;
			if ((ret =
			     __memp_ftruncate(mpf, argp->start_pgno, 0)) != 0)
				goto out;
		}

		/*
		 * If we are rolling back the metapage, then make
		 * sure it reflects the the correct last_pgno.
		 */
		if (cmp_n == 0) {
			mmeta->last_pgno = argp->last_pgno;
			modified = 1;
		}
		pgno = 0;
#else
		/*
		 * Reset the last page back to its preallocation state.
		 */
		if (pagep != NULL) {
			if (log_compare(&pagep->lsn, lsnp) == 0)
				ZERO_LSN(pagep->lsn);

			if ((ret =
			    __memp_fput(mpf, pagep, DB_MPOOL_DIRTY)) != 0)
				goto out;
		}
		/*
		 * Put the pages into the limbo list and free them later.
		 */
		if ((ret = __db_add_limbo(dbenv,
		    info, argp->fileid, argp->start_pgno, argp->num)) != 0)
			goto out;
#endif
		if (cmp_n == 0) {
			LSN(mmeta) = argp->meta_lsn;
			modified = 1;
		}
	}

	/*
	 * In both REDO and UNDO, we have grown the file and need to make
	 * sure that last_pgno is correct.  If we HAVE_FTRUNCATE pgno
	 * will only be valid on REDO.
	 */
	if (pgno > mmeta->last_pgno) {
		mmeta->last_pgno = pgno;
		modified = 1;
	}

done:	if (ret == 0)
		*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (mmeta != NULL)
		(void)__memp_fput(mpf, mmeta, modified ? DB_MPOOL_DIRTY : 0);

	if (ret == ENOENT && op == DB_TXN_BACKWARD_ALLOC)
		ret = 0;
	REC_CLOSE;
}

/*
 * __ham_alloc_pages --
 *
 * Called during redo of a file create.  We create new pages in the file
 * using the MPOOL_NEW_GROUP flag.  We then log the meta-data page with a
 * __crdel_metasub message.  If we manage to crash without the newly written
 * pages getting to disk (I'm not sure this can happen anywhere except our
 * test suite?!), then we need to go through a recreate the final pages.
 * Hash normally has holes in its files and handles them appropriately.
 */
static int
__ham_alloc_pages(dbp, argp, lsnp)
	DB *dbp;
	__ham_groupalloc_args *argp;
	DB_LSN *lsnp;
{
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	int ret;

	mpf = dbp->mpf;

	/* Read the last page of the allocation. */
	pgno = argp->start_pgno + argp->num - 1;

	/* If the page exists, and it has been initialized, then we're done. */
	if ((ret = __memp_fget(mpf, &pgno, 0, &pagep)) == 0) {
		if (NUM_ENT(pagep) == 0 && IS_ZERO_LSN(pagep->lsn))
			goto reinit_page;
		if ((ret = __memp_fput(mpf, pagep, 0)) != 0)
			return (ret);
		return (0);
	}

	/* Had to create the page. */
	if ((ret = __memp_fget(mpf, &pgno, DB_MPOOL_CREATE, &pagep)) != 0)
		return (__db_pgerr(dbp, pgno, ret));

reinit_page:
	/* Initialize the newly allocated page. */
	P_INIT(pagep, dbp->pgsize, pgno, PGNO_INVALID, PGNO_INVALID, 0, P_HASH);
	pagep->lsn = *lsnp;

	if ((ret = __memp_fput(mpf, pagep, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	return (0);
}

/*
 * __ham_curadj_recover --
 *	Undo cursor adjustments if a subtransaction fails.
 *
 * PUBLIC: int __ham_curadj_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_curadj_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_curadj_args *argp;
	DB_MPOOLFILE *mpf;
	DB *file_dbp;
	DBC *dbc;
	int ret;
	HASH_CURSOR *hcp;

	COMPQUIET(info, NULL);
	REC_PRINT(__ham_curadj_print);
	REC_INTRO(__ham_curadj_read, 0);

	if (op != DB_TXN_ABORT)
		goto done;

	/*
	 * Undo the adjustment by reinitializing the the cursor to look like
	 * the one that was used to do the adjustment, then we invert the
	 * add so that undo the adjustment.
	 */
	hcp = (HASH_CURSOR *)dbc->internal;
	hcp->pgno = argp->pgno;
	hcp->indx = argp->indx;
	hcp->dup_off = argp->dup_off;
	hcp->order = argp->order;
	if (!argp->add)
		F_SET(hcp, H_DELETED);
	(void)__ham_c_update(dbc, argp->len, !argp->add, argp->is_dup);

done:	*lsnp = argp->prev_lsn;
out:	REC_CLOSE;
}

/*
 * __ham_chgpg_recover --
 *	Undo cursor adjustments if a subtransaction fails.
 *
 * PUBLIC: int __ham_chgpg_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__ham_chgpg_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__ham_chgpg_args *argp;
	BTREE_CURSOR *opdcp;
	DB_MPOOLFILE *mpf;
	DB *file_dbp, *ldbp;
	DBC *dbc;
	int ret;
	DBC *cp;
	HASH_CURSOR *lcp;
	u_int32_t order, indx;

	COMPQUIET(info, NULL);
	REC_PRINT(__ham_chgpg_print);
	REC_INTRO(__ham_chgpg_read, 0);

	if (op != DB_TXN_ABORT)
		goto done;

	/* Overloaded fields for DB_HAM_DEL*PG */
	indx = argp->old_indx;
	order = argp->new_indx;

	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, file_dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == file_dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, file_dbp->mutexp);

		for (cp = TAILQ_FIRST(&ldbp->active_queue); cp != NULL;
		    cp = TAILQ_NEXT(cp, links)) {
			lcp = (HASH_CURSOR *)cp->internal;

			switch (argp->mode) {
			case DB_HAM_DELFIRSTPG:
				if (lcp->pgno != argp->new_pgno)
					break;
				if (lcp->indx != indx ||
				    !F_ISSET(lcp, H_DELETED) ||
				    lcp->order >= order) {
					lcp->pgno = argp->old_pgno;
					if (lcp->indx == indx)
						lcp->order -= order;
				}
				break;
			case DB_HAM_DELMIDPG:
			case DB_HAM_DELLASTPG:
				if (lcp->pgno == argp->new_pgno &&
				    lcp->indx == indx &&
				    F_ISSET(lcp, H_DELETED) &&
				    lcp->order >= order) {
					lcp->pgno = argp->old_pgno;
					lcp->order -= order;
					lcp->indx = 0;
				}
				break;
			case DB_HAM_CHGPG:
				/*
				 * If we're doing a CHGPG, we're undoing
				 * the move of a non-deleted item to a
				 * new page.  Any cursors with the deleted
				 * flag set do not belong to this item;
				 * don't touch them.
				 */
				if (F_ISSET(lcp, H_DELETED))
					break;
				/* FALLTHROUGH */
			case DB_HAM_SPLIT:
				if (lcp->pgno == argp->new_pgno &&
				    lcp->indx == argp->new_indx) {
					lcp->indx = argp->old_indx;
					lcp->pgno = argp->old_pgno;
				}
				break;
			case DB_HAM_DUP:
				if (lcp->opd == NULL)
					break;
				opdcp = (BTREE_CURSOR *)lcp->opd->internal;
				if (opdcp->pgno != argp->new_pgno ||
				    opdcp->indx != argp->new_indx)
					break;

				if (F_ISSET(opdcp, C_DELETED))
					F_SET(lcp, H_DELETED);
				/*
				 * We can't close a cursor while we have the
				 * dbp mutex locked, since c_close reacquires
				 * it.  It should be safe to drop the mutex
				 * here, though, since newly opened cursors
				 * are put only at the end of the tailq and
				 * the cursor we're adjusting can't be closed
				 * under us.
				 */
				MUTEX_THREAD_UNLOCK(dbenv, file_dbp->mutexp);
				if ((ret = __db_c_close(lcp->opd)) != 0)
					goto out;
				MUTEX_THREAD_LOCK(dbenv, file_dbp->mutexp);
				lcp->opd = NULL;
				break;
			}
		}
		MUTEX_THREAD_UNLOCK(dbenv, file_dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

done:	*lsnp = argp->prev_lsn;
out:	REC_CLOSE;
}
