/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: bt_recno.c,v 11.65 2001/01/18 14:33:22 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <limits.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"
#include "db_ext.h"
#include "db_shash.h"
#include "lock.h"
#include "lock_ext.h"
#include "qam.h"
#include "txn.h"

static int  __ram_add __P((DBC *, db_recno_t *, DBT *, u_int32_t, u_int32_t));
static int  __ram_delete __P((DB *, DB_TXN *, DBT *, u_int32_t));
static int  __ram_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int  __ram_source __P((DB *));
static int  __ram_sread __P((DBC *, db_recno_t));
static int  __ram_update __P((DBC *, db_recno_t, int));

/*
 * In recno, there are two meanings to the on-page "deleted" flag.  If we're
 * re-numbering records, it means the record was implicitly created.  We skip
 * over implicitly created records if doing a cursor "next" or "prev", and
 * return DB_KEYEMPTY if they're explicitly requested..  If not re-numbering
 * records, it means that the record was implicitly created, or was deleted.
 * We skip over implicitly created or deleted records if doing a cursor "next"
 * or "prev", and return DB_KEYEMPTY if they're explicitly requested.
 *
 * If we're re-numbering records, then we have to detect in the cursor that
 * a record was deleted, and adjust the cursor as necessary on the next get.
 * If we're not re-numbering records, then we can detect that a record has
 * been deleted by looking at the actual on-page record, so we completely
 * ignore the cursor's delete flag.  This is different from the B+tree code.
 * It also maintains whether the cursor references a deleted record in the
 * cursor, and it doesn't always check the on-page value.
 */
#define	CD_SET(cp) {							\
	if (F_ISSET(cp, C_RENUMBER))					\
		F_SET(cp, C_DELETED);					\
}
#define	CD_CLR(cp) {							\
	if (F_ISSET(cp, C_RENUMBER)) {					\
		F_CLR(cp, C_DELETED);					\
		cp->order = INVALID_ORDER;				\
	}								\
}
#define	CD_ISSET(cp)							\
	(F_ISSET(cp, C_RENUMBER) && F_ISSET(cp, C_DELETED))

/*
 * Macros for comparing the ordering of two cursors.
 * cp1 comes before cp2 iff one of the following holds:
 *	cp1's recno is less than cp2's recno
 *	recnos are equal, both deleted, and cp1's order is less than cp2's
 *	recnos are equal, cp1 deleted, and cp2 not deleted
 */
#define	C_LESSTHAN(cp1, cp2)						\
    (((cp1)->recno < (cp2)->recno) ||					\
    (((cp1)->recno == (cp2)->recno) &&					\
    ((CD_ISSET((cp1)) && CD_ISSET((cp2)) && (cp1)->order < (cp2)->order) || \
    (CD_ISSET((cp1)) && !CD_ISSET((cp2))))))

/*
 * cp1 is equal to cp2 iff their recnos and delete flags are identical,
 * and if the delete flag is set their orders are also identical.
 */
#define	C_EQUAL(cp1, cp2)						\
    ((cp1)->recno == (cp2)->recno && CD_ISSET((cp1)) == CD_ISSET((cp2)) && \
    (!CD_ISSET((cp1)) || (cp1)->order == (cp2)->order))

/*
 * Do we need to log the current cursor adjustment?
 */
#define	CURADJ_LOG(dbc)							\
	(DB_LOGGING((dbc)) && (dbc)->txn != NULL && (dbc)->txn->parent != NULL)

/*
 * __ram_open --
 *	Recno open function.
 *
 * PUBLIC: int __ram_open __P((DB *, const char *, db_pgno_t, u_int32_t));
 */
int
__ram_open(dbp, name, base_pgno, flags)
	DB *dbp;
	const char *name;
	db_pgno_t base_pgno;
	u_int32_t flags;
{
	BTREE *t;
	DBC *dbc;
	int ret, t_ret;

	t = dbp->bt_internal;

	/* Initialize the remaining fields/methods of the DB. */
	dbp->del = __ram_delete;
	dbp->put = __ram_put;
	dbp->stat = __bam_stat;

	/* Start up the tree. */
	if ((ret = __bam_read_root(dbp, name, base_pgno, flags)) != 0)
		return (ret);

	/*
	 * If the user specified a source tree, open it and map it in.
	 *
	 * !!!
	 * We don't complain if the user specified transactions or threads.
	 * It's possible to make it work, but you'd better know what you're
	 * doing!
	 */
	if (t->re_source != NULL && (ret = __ram_source(dbp)) != 0)
		return (ret);

	/* If we're snapshotting an underlying source file, do it now. */
	if (F_ISSET(dbp, DB_RE_SNAPSHOT)) {
		/* Allocate a cursor. */
		if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
			return (ret);

		/* Do the snapshot. */
		if ((ret = __ram_update(dbc,
		    DB_MAX_RECORDS, 0)) != 0 && ret == DB_NOTFOUND)
			ret = 0;

		/* Discard the cursor. */
		if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
			ret = t_ret;
	}

	return (0);
}

/*
 * __ram_delete --
 *	Recno db->del function.
 */
static int
__ram_delete(dbp, txn, key, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	BTREE_CURSOR *cp;
	DBC *dbc;
	db_recno_t recno;
	int ret, t_ret;

	PANIC_CHECK(dbp->dbenv);

	/* Check for invalid flags. */
	if ((ret = __db_delchk(dbp,
	    key, flags, F_ISSET(dbp, DB_AM_RDONLY))) != 0)
		return (ret);

	/* Acquire a cursor. */
	if ((ret = dbp->cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, txn, "ram_delete", key, NULL, flags);

	/* Check the user's record number and fill in as necessary. */
	if ((ret = __ram_getno(dbc, key, &recno, 0)) != 0)
		goto err;

	/* Do the delete. */
	cp = (BTREE_CURSOR *)dbc->internal;
	cp->recno = recno;

	ret = __ram_c_del(dbc);

	/* Release the cursor. */
err:	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __ram_put --
 *	Recno db->put function.
 */
static int
__ram_put(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DBC *dbc;
	db_recno_t recno;
	int ret, t_ret;

	PANIC_CHECK(dbp->dbenv);

	/* Check for invalid flags. */
	if ((ret = __db_putchk(dbp,
	    key, data, flags, F_ISSET(dbp, DB_AM_RDONLY), 0)) != 0)
		return (ret);

	/* Allocate a cursor. */
	if ((ret = dbp->cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, txn, "ram_put", key, data, flags);

	/*
	 * If we're appending to the tree, make sure we've read in all of
	 * the backing source file.  Otherwise, check the user's record
	 * number and fill in as necessary.  If we found the record or it
	 * simply didn't exist, add the user's record.
	 */
	if (flags == DB_APPEND)
		ret = __ram_update(dbc, DB_MAX_RECORDS, 0);
	else
		ret = __ram_getno(dbc, key, &recno, 1);
	if (ret == 0 || ret == DB_NOTFOUND)
		ret = __ram_add(dbc, &recno, data, flags, 0);

	/* Discard the cursor. */
	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	/* Return the record number if we're appending to the tree. */
	if (ret == 0 && flags == DB_APPEND)
		ret = __db_retcopy(dbp, key, &recno, sizeof(recno),
		    &dbc->rkey.data, &dbc->rkey.ulen);

	return (ret);
}

/*
 * __ram_c_del --
 *	Recno cursor->c_del function.
 *
 * PUBLIC: int __ram_c_del __P((DBC *));
 */
int
__ram_c_del(dbc)
	DBC *dbc;
{
	BKEYDATA bk;
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	DB_LSN lsn;
	DBT hdr, data;
	EPG *epg;
	int exact, ret, stack;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	t = dbp->bt_internal;
	stack = 0;

	/*
	 * The semantics of cursors during delete are as follows: in
	 * non-renumbering recnos, records are replaced with a marker
	 * containing a delete flag.  If the record referenced by this cursor
	 * has already been deleted, we will detect that as part of the delete
	 * operation, and fail.
	 *
	 * In renumbering recnos, cursors which represent deleted items
	 * are flagged with the C_DELETED flag, and it is an error to
	 * call c_del a second time without an intervening cursor motion.
	 */
	if (CD_ISSET(cp))
		return (DB_KEYEMPTY);

	/* Search the tree for the key; delete only deletes exact matches. */
	if ((ret = __bam_rsearch(dbc, &cp->recno, S_DELETE, 1, &exact)) != 0)
		goto err;
	if (!exact) {
		ret = DB_NOTFOUND;
		goto err;
	}
	stack = 1;
	cp->page = cp->csp->page;
	cp->pgno = cp->csp->page->pgno;
	cp->indx = cp->csp->indx;

	/*
	 * If re-numbering records, the on-page deleted flag can only mean
	 * that this record was implicitly created.  Applications aren't
	 * permitted to delete records they never created, return an error.
	 *
	 * If not re-numbering records, the on-page deleted flag means that
	 * this record was implicitly created, or, was deleted at some time.
	 * The former is an error because applications aren't permitted to
	 * delete records they never created, the latter is an error because
	 * if the record was "deleted", we could never have found it.
	 */
	if (B_DISSET(GET_BKEYDATA(cp->page, cp->indx)->type)) {
		ret = DB_KEYEMPTY;
		goto err;
	}

	if (F_ISSET(cp, C_RENUMBER)) {
		/* Delete the item, adjust the counts, adjust the cursors. */
		if ((ret = __bam_ditem(dbc, cp->page, cp->indx)) != 0)
			goto err;
		__bam_adjust(dbc, -1);
		if (__ram_ca(dbc, CA_DELETE) > 0 &&
		    CURADJ_LOG(dbc) && (ret = __bam_rcuradj_log(dbp->dbenv,
		    dbc->txn, &lsn, 0, dbp->log_fileid, CA_DELETE,
		    cp->root, cp->recno, cp->order)) != 0)
			goto err;

		/*
		 * If the page is empty, delete it.
		 *
		 * We never delete a root page.  First, root pages of primary
		 * databases never go away, recno or otherwise.  However, if
		 * it's the root page of an off-page duplicates database, then
		 * it can be deleted.   We don't delete it here because we have
		 * no way of telling the primary database page holder (e.g.,
		 * the hash access method) that its page element should cleaned
		 * up because the underlying tree is gone.  So, we keep the page
		 * around until the last cursor referencing the empty tree is
		 * are closed, and then clean it up.
		 */
		if (NUM_ENT(cp->page) == 0 && PGNO(cp->page) != cp->root) {
			/*
			 * We already have a locked stack of pages.  However,
			 * there are likely entries in the stack that aren't
			 * going to be emptied by removing the single reference
			 * to the emptied page (or one of its parents).
			 */
			for (epg = cp->sp; epg <= cp->csp; ++epg)
				if (NUM_ENT(epg->page) <= 1)
					break;

			/*
			 * We want to delete a single item out of the last page
			 * that we're not deleting, back up to that page.
			 */
			ret = __bam_dpages(dbc, --epg);

			/*
			 * Regardless of the return from __bam_dpages, it will
			 * discard our stack and pinned page.
			 */
			stack = 0;
			cp->page = NULL;
		}
	} else {
		/* Use a delete/put pair to replace the record with a marker. */
		if ((ret = __bam_ditem(dbc, cp->page, cp->indx)) != 0)
			goto err;

		B_TSET(bk.type, B_KEYDATA, 1);
		bk.len = 0;
		memset(&hdr, 0, sizeof(hdr));
		hdr.data = &bk;
		hdr.size = SSZA(BKEYDATA, data);
		memset(&data, 0, sizeof(data));
		data.data = (void *)"";
		data.size = 0;
		if ((ret = __db_pitem(dbc,
		    cp->page, cp->indx, BKEYDATA_SIZE(0), &hdr, &data)) != 0)
			goto err;
	}

	t->re_modified = 1;

err:	if (stack)
		__bam_stkrel(dbc, STK_CLRDBC);

	return (ret);
}

/*
 * __ram_c_get --
 *	Recno cursor->c_get function.
 *
 * PUBLIC: int __ram_c_get
 * PUBLIC:     __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
 */
int
__ram_c_get(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	int cmp, exact, ret;

	COMPQUIET(pgnop, NULL);

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

retry:	switch (flags) {
	case DB_CURRENT:
		/*
		 * If we're using mutable records and the deleted flag is
		 * set, the cursor is pointing at a nonexistent record;
		 * return an error.
		 */
		if (CD_ISSET(cp))
			return (DB_KEYEMPTY);
		break;
	case DB_NEXT_DUP:
		/*
		 * If we're not in an off-page dup set, we know there's no
		 * next duplicate since recnos don't have them.  If we
		 * are in an off-page dup set, the next item assuredly is
		 * a dup, so we set flags to DB_NEXT and keep going.
		 */
		if (!F_ISSET(dbc, DBC_OPD))
			return (DB_NOTFOUND);
		/* FALLTHROUGH */
	case DB_NEXT_NODUP:
		/*
		 * Recno databases don't have duplicates, set flags to DB_NEXT
		 * and keep going.
		 */
		/* FALLTHROUGH */
	case DB_NEXT:
		flags = DB_NEXT;
		/*
		 * If record numbers are mutable: if we just deleted a record,
		 * we have to avoid incrementing the record number so that we
		 * return the right record by virtue of renumbering the tree.
		 */
		if (CD_ISSET(cp))
			break;

		if (cp->recno != RECNO_OOB) {
			++cp->recno;
			break;
		}
		/* FALLTHROUGH */
	case DB_FIRST:
		flags = DB_NEXT;
		cp->recno = 1;
		break;
	case DB_PREV_NODUP:
		/*
		 * Recno databases don't have duplicates, set flags to DB_PREV
		 * and keep going.
		 */
		/* FALLTHROUGH */
	case DB_PREV:
		flags = DB_PREV;
		if (cp->recno != RECNO_OOB) {
			if (cp->recno == 1) {
				ret = DB_NOTFOUND;
				goto err;
			}
			--cp->recno;
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:
		flags = DB_PREV;
		if (((ret = __ram_update(dbc,
		    DB_MAX_RECORDS, 0)) != 0) && ret != DB_NOTFOUND)
			goto err;
		if ((ret = __bam_nrecs(dbc, &cp->recno)) != 0)
			goto err;
		if (cp->recno == 0) {
			ret = DB_NOTFOUND;
			goto err;
		}
		break;
	case DB_GET_BOTHC:
		/*
		 * If we're doing a join and these are offpage dups,
		 * we want to keep searching forward from after the
		 * current cursor position.  Increment the recno by 1,
		 * then proceed as for a DB_SET.
		 *
		 * Otherwise, we know there are no additional matching
		 * data, as recnos don't have dups.  return DB_NOTFOUND.
		 */
		if (F_ISSET(dbc, DBC_OPD)) {
			cp->recno++;
			break;
		}
		ret = DB_NOTFOUND;
		goto err;
		/* NOTREACHED */
	case DB_GET_BOTH:
		/*
		 * If we're searching a set of off-page dups, we start
		 * a new linear search from the first record.  Otherwise,
		 * we compare the single data item associated with the
		 * requested record for a match.
		 */
		if (F_ISSET(dbc, DBC_OPD)) {
			cp->recno = 1;
			break;
		}
		/* FALLTHROUGH */
	case DB_SET:
	case DB_SET_RANGE:
		if ((ret = __ram_getno(dbc, key, &cp->recno, 0)) != 0)
			goto err;
		break;
	default:
		ret = __db_unknown_flag(dbp->dbenv, "__ram_c_get", flags);
		goto err;
	}

	/*
	 * For DB_PREV, DB_LAST, DB_SET and DB_SET_RANGE, we have already
	 * called __ram_update() to make sure sufficient records have been
	 * read from the backing source file.  Do it now for DB_CURRENT (if
	 * the current record was deleted we may need more records from the
	 * backing file for a DB_CURRENT operation), DB_FIRST and DB_NEXT.
	 */
	if ((flags == DB_NEXT || flags == DB_CURRENT) && ((ret =
	    __ram_update(dbc, cp->recno, 0)) != 0) && ret != DB_NOTFOUND)
		goto err;

	for (;; ++cp->recno) {
		/* Search the tree for the record. */
		if ((ret = __bam_rsearch(dbc, &cp->recno,
		    F_ISSET(dbc, DBC_RMW) ? S_FIND_WR : S_FIND,
		    1, &exact)) != 0)
			goto err;
		if (!exact) {
			ret = DB_NOTFOUND;
			goto err;
		}

		/*
		 * Copy the page into the cursor, discarding any lock we
		 * are currently holding.
		 */
		cp->page = cp->csp->page;
		cp->pgno = cp->csp->page->pgno;
		cp->indx = cp->csp->indx;
		(void)__TLPUT(dbc, cp->lock);
		cp->lock = cp->csp->lock;
		cp->lock_mode = cp->csp->lock_mode;

		/*
		 * If re-numbering records, the on-page deleted flag means this
		 * record was implicitly created.  If not re-numbering records,
		 * the on-page deleted flag means this record was implicitly
		 * created, or, it was deleted at some time.  Regardless, we
		 * skip such records if doing cursor next/prev operations or
		 * walking through off-page duplicates, and fail if they were
		 * requested explicitly by the application.
		 */
		if (B_DISSET(GET_BKEYDATA(cp->page, cp->indx)->type))
			switch (flags) {
			case DB_NEXT:
			case DB_PREV:
				(void)__bam_stkrel(dbc, STK_CLRDBC);
				goto retry;
			case DB_GET_BOTH:
				(void)__bam_stkrel(dbc, STK_CLRDBC);
				continue;
			default:
				ret = DB_KEYEMPTY;
				goto err;
			}

		if (flags == DB_GET_BOTH || flags == DB_GET_BOTHC) {
			if ((ret = __bam_cmp(dbp, data,
			    cp->page, cp->indx, __bam_defcmp, &cmp)) != 0)
				return (ret);
			if (cmp == 0)
				break;
			if (!F_ISSET(dbc, DBC_OPD)) {
				ret = DB_NOTFOUND;
				goto err;
			}
			(void)__bam_stkrel(dbc, STK_CLRDBC);
		} else
			break;
	}

	/* Return the key if the user didn't give us one. */
	if (!F_ISSET(dbc, DBC_OPD)) {
		if (flags != DB_SET && flags != DB_SET_RANGE)
			ret = __db_retcopy(dbp,
			     key, &cp->recno, sizeof(cp->recno),
			     &dbc->rkey.data, &dbc->rkey.ulen);
		F_SET(key, DB_DBT_ISSET);
	}

	/* The cursor was reset, no further delete adjustment is necessary. */
err:	CD_CLR(cp);

	return (ret);
}

/*
 * __ram_c_put --
 *	Recno cursor->c_put function.
 *
 * PUBLIC: int __ram_c_put __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
 */
int
__ram_c_put(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	DB_LSN lsn;
	int exact, nc, ret, t_ret;
	u_int32_t iiflags;
	void *arg;

	COMPQUIET(pgnop, NULL);

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * DB_KEYFIRST and DB_KEYLAST will only be set if we're dealing with
	 * an off-page duplicate tree, they can't be specified at user level.
	 * Translate them into something else.
	 */
	switch (flags) {
	case DB_KEYFIRST:
		cp->recno = 1;
		flags = DB_BEFORE;
		break;
	case DB_KEYLAST:
		if ((ret = __ram_add(dbc, &cp->recno, data, DB_APPEND, 0)) != 0)
			return (ret);
		if (CURADJ_LOG(dbc) && (ret = __bam_rcuradj_log(dbp->dbenv,
		    dbc->txn, &lsn, 0, dbp->log_fileid, CA_ICURRENT,
		    cp->root, cp->recno, cp->order)))
			return (ret);
		return (0);
	}

	/*
	 * If we're putting with a cursor that's marked C_DELETED, we need to
	 * take special care;  the cursor doesn't "really" reference the item
	 * corresponding to its current recno, but instead is "between" that
	 * record and the current one.  Translate the actual insert into
	 * DB_BEFORE, and let the __ram_ca work out the gory details of what
	 * should wind up pointing where.
	 */
	if (CD_ISSET(cp))
		iiflags = DB_BEFORE;
	else
		iiflags = flags;

split:	if ((ret = __bam_rsearch(dbc, &cp->recno, S_INSERT, 1, &exact)) != 0)
		goto err;
	/*
	 * An inexact match is okay;  it just means we're one record past the
	 * end, which is reasonable if we're marked deleted.
	 */
	DB_ASSERT(exact || CD_ISSET(cp));

	cp->page = cp->csp->page;
	cp->pgno = cp->csp->page->pgno;
	cp->indx = cp->csp->indx;

	ret = __bam_iitem(dbc, key, data, iiflags, 0);
	t_ret = __bam_stkrel(dbc, STK_CLRDBC);

	if (t_ret != 0 && (ret == 0 || ret == DB_NEEDSPLIT))
		ret = t_ret;
	else if (ret == DB_NEEDSPLIT) {
		arg = &cp->recno;
		if ((ret = __bam_split(dbc, arg)) != 0)
			goto err;
		goto split;
	}
	if (ret != 0)
		goto err;

	switch (flags) {			/* Adjust the cursors. */
	case DB_AFTER:
		nc = __ram_ca(dbc, CA_IAFTER);

		/*
		 * We only need to adjust this cursor forward if we truly added
		 * the item after the current recno, rather than remapping it
		 * to DB_BEFORE.
		 */
		if (iiflags == DB_AFTER)
			++cp->recno;

		/* Only log if __ram_ca found any relevant cursors. */
		if (nc > 0 && CURADJ_LOG(dbc) &&
		    (ret = __bam_rcuradj_log(dbp->dbenv,
		    dbc->txn, &lsn, 0, dbp->log_fileid, CA_IAFTER,
		    cp->root, cp->recno, cp->order)) != 0)
			goto err;
		break;
	case DB_BEFORE:
		nc = __ram_ca(dbc, CA_IBEFORE);
		--cp->recno;

		/* Only log if __ram_ca found any relevant cursors. */
		if (nc > 0 && CURADJ_LOG(dbc) &&
		    (ret = __bam_rcuradj_log(dbp->dbenv,
		    dbc->txn, &lsn, 0, dbp->log_fileid, CA_IBEFORE,
		    cp->root, cp->recno, cp->order)) != 0)
			goto err;
		break;
	case DB_CURRENT:
		/*
		 * We only need to do an adjustment if we actually
		 * added an item, which we only would have done if the
		 * cursor was marked deleted.
		 *
		 * Only log if __ram_ca found any relevant cursors.
		 */
		if (CD_ISSET(cp) && __ram_ca(dbc, CA_ICURRENT) > 0 &&
		    CURADJ_LOG(dbc) && (ret = __bam_rcuradj_log(
		    dbp->dbenv, dbc->txn, &lsn, 0, dbp->log_fileid,
		    CA_ICURRENT, cp->root, cp->recno, cp->order)) != 0)
			goto err;
		break;
	}

	/* Return the key if we've created a new record. */
	if (!F_ISSET(dbc, DBC_OPD) && (flags == DB_AFTER || flags == DB_BEFORE))
		ret = __db_retcopy(dbp, key, &cp->recno,
		    sizeof(cp->recno), &dbc->rkey.data, &dbc->rkey.ulen);

	/* The cursor was reset, no further delete adjustment is necessary. */
err:	CD_CLR(cp);

	return (ret);
}

/*
 * __ram_ca --
 *	Adjust cursors.  Returns the number of relevant cursors.
 *
 * PUBLIC: int __ram_ca __P((DBC *, ca_recno_arg));
 */
int
__ram_ca(dbc_arg, op)
	DBC *dbc_arg;
	ca_recno_arg op;
{
	BTREE_CURSOR *cp, *cp_arg;
	DB *dbp, *ldbp;
	DB_ENV *dbenv;
	DBC *dbc;
	db_recno_t recno;
	int adjusted, found;
	u_int32_t order;

	dbp = dbc_arg->dbp;
	dbenv = dbp->dbenv;
	cp_arg = (BTREE_CURSOR *)dbc_arg->internal;
	recno = cp_arg->recno;

	found = 0;

	/*
	 * It only makes sense to adjust cursors if we're a renumbering
	 * recno;  we should only be called if this is one.
	 */
	DB_ASSERT(F_ISSET(cp_arg, C_RENUMBER));

	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 */
	/*
	 * If we're doing a delete, we need to find the highest
	 * order of any cursor currently pointing at this item,
	 * so we can assign a higher order to the newly deleted
	 * cursor.  Unfortunately, this requires a second pass through
	 * the cursor list.
	 */
	if (op == CA_DELETE) {
		order = 1;
		for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
		    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
		    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
			MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
			for (dbc = TAILQ_FIRST(&ldbp->active_queue);
			    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
				cp = (BTREE_CURSOR *)dbc->internal;
				if (cp_arg->root == cp->root &&
				    recno == cp->recno && CD_ISSET(cp) &&
				    order <= cp->order)
					order = cp->order + 1;
			}
			MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
		}
	} else
		order = INVALID_ORDER;

	/* Now go through and do the actual adjustments. */
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			cp = (BTREE_CURSOR *)dbc->internal;
			if (cp_arg->root != cp->root)
				continue;
			++found;
			adjusted = 0;
			switch (op) {
			case CA_DELETE:
				if (recno < cp->recno) {
					--cp->recno;
					/*
					 * If the adjustment made them equal,
					 * we have to merge the orders.
					 */
					if (recno == cp->recno && CD_ISSET(cp))
						cp->order += order;
				} else if (recno == cp->recno &&
				    !CD_ISSET(cp)) {
					CD_SET(cp);
					cp->order = order;
				}
				break;
			case CA_IBEFORE:
				/*
				 * IBEFORE is just like IAFTER, except that we
				 * adjust cursors on the current record too.
				 */
				if (C_EQUAL(cp_arg, cp)) {
					++cp->recno;
					adjusted = 1;
				}
				goto iafter;
			case CA_ICURRENT:

				/*
				 * If the original cursor wasn't deleted, we
				 * just did a replacement and so there's no
				 * need to adjust anything--we shouldn't have
				 * gotten this far.  Otherwise, we behave
				 * much like an IAFTER, except that all
				 * cursors pointing to the current item get
				 * marked undeleted and point to the new
				 * item.
				 */
				DB_ASSERT(CD_ISSET(cp_arg));
				if (C_EQUAL(cp_arg, cp)) {
					CD_CLR(cp);
					break;
				}
				/* FALLTHROUGH */
			case CA_IAFTER:
iafter:				if (!adjusted && C_LESSTHAN(cp_arg, cp)) {
					++cp->recno;
					adjusted = 1;
				}
				if (recno == cp->recno && adjusted)
					/*
					 * If we've moved this cursor's recno,
					 * split its order number--i.e.,
					 * decrement it by enough so that
					 * the lowest cursor moved has order 1.
					 * cp_arg->order is the split point,
					 * so decrement by one less than that.
					 */
					cp->order -= (cp_arg->order - 1);
				break;
			}
		}
		MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	return (found);
}

/*
 * __ram_getno --
 *	Check the user's record number, and make sure we've seen it.
 *
 * PUBLIC: int __ram_getno __P((DBC *, const DBT *, db_recno_t *, int));
 */
int
__ram_getno(dbc, key, rep, can_create)
	DBC *dbc;
	const DBT *key;
	db_recno_t *rep;
	int can_create;
{
	DB *dbp;
	db_recno_t recno;

	dbp = dbc->dbp;

	/* Check the user's record number. */
	if ((recno = *(db_recno_t *)key->data) == 0) {
		__db_err(dbp->dbenv, "illegal record number of 0");
		return (EINVAL);
	}
	if (rep != NULL)
		*rep = recno;

	/*
	 * Btree can neither create records nor read them in.  Recno can
	 * do both, see if we can find the record.
	 */
	return (dbc->dbtype == DB_RECNO ?
	    __ram_update(dbc, recno, can_create) : 0);
}

/*
 * __ram_update --
 *	Ensure the tree has records up to and including the specified one.
 */
static int
__ram_update(dbc, recno, can_create)
	DBC *dbc;
	db_recno_t recno;
	int can_create;
{
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	db_recno_t nrecs;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	t = dbp->bt_internal;

	/*
	 * If we can't create records and we've read the entire backing input
	 * file, we're done.
	 */
	if (!can_create && t->re_eof)
		return (0);

	/*
	 * If we haven't seen this record yet, try to get it from the original
	 * file.
	 */
	if ((ret = __bam_nrecs(dbc, &nrecs)) != 0)
		return (ret);
	if (!t->re_eof && recno > nrecs) {
		if ((ret = __ram_sread(dbc, recno)) != 0 && ret != DB_NOTFOUND)
			return (ret);
		if ((ret = __bam_nrecs(dbc, &nrecs)) != 0)
			return (ret);
	}

	/*
	 * If we can create records, create empty ones up to the requested
	 * record.
	 */
	if (!can_create || recno <= nrecs + 1)
		return (0);

	dbc->rdata.dlen = 0;
	dbc->rdata.doff = 0;
	dbc->rdata.flags = 0;
	if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
		if (dbc->rdata.ulen < t->re_len) {
			if ((ret = __os_realloc(dbp->dbenv,
			    t->re_len, NULL, &dbc->rdata.data)) != 0) {
				dbc->rdata.ulen = 0;
				dbc->rdata.data = NULL;
				return (ret);
			}
			dbc->rdata.ulen = t->re_len;
		}
		dbc->rdata.size = t->re_len;
		memset(dbc->rdata.data, t->re_pad, t->re_len);
	} else
		dbc->rdata.size = 0;

	while (recno > ++nrecs)
		if ((ret = __ram_add(dbc,
		    &nrecs, &dbc->rdata, 0, BI_DELETED)) != 0)
			return (ret);
	return (0);
}

/*
 * __ram_source --
 *	Load information about the backing file.
 */
static int
__ram_source(dbp)
	DB *dbp;
{
	BTREE *t;
	char *source;
	int ret;

	t = dbp->bt_internal;

	/* Find the real name, and swap out the one we had before. */
	if ((ret = __db_appname(dbp->dbenv,
	    DB_APP_DATA, NULL, t->re_source, 0, NULL, &source)) != 0)
		return (ret);
	__os_freestr(t->re_source);
	t->re_source = source;

	/*
	 * !!!
	 * It's possible that the backing source file is read-only.  We don't
	 * much care other than we'll complain if there are any modifications
	 * when it comes time to write the database back to the source.
	 */
	if ((t->re_fp = fopen(t->re_source, "r")) == NULL) {
		ret = errno;
		__db_err(dbp->dbenv, "%s: %s", t->re_source, db_strerror(ret));
		return (ret);
	}

	t->re_eof = 0;
	return (0);
}

/*
 * __ram_writeback --
 *	Rewrite the backing file.
 *
 * PUBLIC: int __ram_writeback __P((DB *));
 */
int
__ram_writeback(dbp)
	DB *dbp;
{
	BTREE *t;
	DB_ENV *dbenv;
	DBC *dbc;
	DBT key, data;
	FILE *fp;
	db_recno_t keyno;
	int ret, t_ret;
	u_int8_t delim, *pad;

	t = dbp->bt_internal;
	dbenv = dbp->dbenv;
	fp = NULL;

	/* If the file wasn't modified, we're done. */
	if (!t->re_modified)
		return (0);

	/* If there's no backing source file, we're done. */
	if (t->re_source == NULL) {
		t->re_modified = 0;
		return (0);
	}

	/* Allocate a cursor. */
	if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);

	/*
	 * Read any remaining records into the tree.
	 *
	 * !!!
	 * This is why we can't support transactions when applications specify
	 * backing (re_source) files.  At this point we have to read in the
	 * rest of the records from the file so that we can write all of the
	 * records back out again, which could modify a page for which we'd
	 * have to log changes and which we don't have locked.  This could be
	 * partially fixed by taking a snapshot of the entire file during the
	 * DB->open as DB->open is transaction protected.  But, if a checkpoint
	 * occurs then, the part of the log holding the copy of the file could
	 * be discarded, and that would make it impossible to recover in the
	 * face of disaster.  This could all probably be fixed, but it would
	 * require transaction protecting the backing source file.
	 *
	 * XXX
	 * This could be made to work now that we have transactions protecting
	 * file operations.  Margo has specifically asked for the privilege of
	 * doing this work.
	 */
	if ((ret =
	    __ram_update(dbc, DB_MAX_RECORDS, 0)) != 0 && ret != DB_NOTFOUND)
		return (ret);

	/*
	 * Close any existing file handle and re-open the file, truncating it.
	 */
	if (t->re_fp != NULL) {
		if (fclose(t->re_fp) != 0) {
			ret = errno;
			goto err;
		}
		t->re_fp = NULL;
	}
	if ((fp = fopen(t->re_source, "w")) == NULL) {
		ret = errno;
		__db_err(dbenv, "%s: %s", t->re_source, db_strerror(ret));
		goto err;
	}

	/*
	 * We step through the records, writing each one out.  Use the record
	 * number and the dbp->get() function, instead of a cursor, so we find
	 * and write out "deleted" or non-existent records.
	 */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.size = sizeof(db_recno_t);
	key.data = &keyno;

	/*
	 * We'll need the delimiter if we're doing variable-length records,
	 * and the pad character if we're doing fixed-length records.
	 */
	delim = t->re_delim;
	if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
		if ((ret = __os_malloc(dbenv, t->re_len, NULL, &pad)) != 0)
			goto err;
		memset(pad, t->re_pad, t->re_len);
	} else
		COMPQUIET(pad, NULL);
	for (keyno = 1;; ++keyno) {
		switch (ret = dbp->get(dbp, NULL, &key, &data, 0)) {
		case 0:
			if (fwrite(data.data, 1, data.size, fp) != data.size)
				goto write_err;
			break;
		case DB_KEYEMPTY:
			if (F_ISSET(dbp, DB_RE_FIXEDLEN) &&
			    fwrite(pad, 1, t->re_len, fp) != t->re_len)
				goto write_err;
			break;
		case DB_NOTFOUND:
			ret = 0;
			goto done;
		}
		if (!F_ISSET(dbp, DB_RE_FIXEDLEN) &&
		    fwrite(&delim, 1, 1, fp) != 1) {
write_err:		ret = errno;
			__db_err(dbp->dbenv,
			    "%s: write failed to backing file: %s",
			    t->re_source, strerror(ret));
			goto err;
		}
	}

err:
done:	/* Close the file descriptor. */
	if (fp != NULL && fclose(fp) != 0) {
		if (ret == 0)
			ret = errno;
		__db_err(dbenv, "%s: %s", t->re_source, db_strerror(errno));
	}

	/* Discard the cursor. */
	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	if (ret == 0)
		t->re_modified = 0;

	return (ret);
}

/*
 * __ram_sread --
 *	Read records from a source file.
 */
static int
__ram_sread(dbc, top)
	DBC *dbc;
	db_recno_t top;
{
	BTREE *t;
	DB *dbp;
	DBT data;
	db_recno_t recno;
	size_t len;
	int ch, ret, was_modified;

	t = dbc->dbp->bt_internal;
	dbp = dbc->dbp;
	was_modified = t->re_modified;

	if ((ret = __bam_nrecs(dbc, &recno)) != 0)
		return (ret);

	/* Use the record data return memory, it's only a short-term use. */
	len = F_ISSET(dbp, DB_RE_FIXEDLEN) ? t->re_len : 256;
	if (dbc->rdata.ulen < len) {
		if ((ret = __os_realloc(
		    dbp->dbenv, len, NULL, &dbc->rdata.data)) != 0) {
			dbc->rdata.ulen = 0;
			dbc->rdata.data = NULL;
			return (ret);
		}
		dbc->rdata.ulen = len;
	}

	memset(&data, 0, sizeof(data));
	while (recno < top) {
		data.data = dbc->rdata.data;
		data.size = 0;
		if (F_ISSET(dbp, DB_RE_FIXEDLEN))
			for (len = t->re_len; len > 0; --len) {
				if ((ch = getc(t->re_fp)) == EOF)
					goto eof;
				((u_int8_t *)data.data)[data.size++] = ch;
			}
		else
			for (;;) {
				if ((ch = getc(t->re_fp)) == EOF)
					goto eof;
				if (ch == t->re_delim)
					break;

				((u_int8_t *)data.data)[data.size++] = ch;
				if (data.size == dbc->rdata.ulen) {
					if ((ret = __os_realloc(dbp->dbenv,
					    dbc->rdata.ulen *= 2,
					    NULL, &dbc->rdata.data)) != 0) {
						dbc->rdata.ulen = 0;
						dbc->rdata.data = NULL;
						return (ret);
					} else
						data.data = dbc->rdata.data;
				}
			}

		/*
		 * Another process may have read this record from the input
		 * file and stored it into the database already, in which
		 * case we don't need to repeat that operation.  We detect
		 * this by checking if the last record we've read is greater
		 * or equal to the number of records in the database.
		 */
		if (t->re_last >= recno) {
			++recno;
			if ((ret = __ram_add(dbc, &recno, &data, 0, 0)) != 0)
				goto err;
		}
		++t->re_last;
	}

	if (0) {
eof:		t->re_eof = 1;
		ret = DB_NOTFOUND;
	}
err:	if (!was_modified)
		t->re_modified = 0;

	return (ret);
}

/*
 * __ram_add --
 *	Add records into the tree.
 */
static int
__ram_add(dbc, recnop, data, flags, bi_flags)
	DBC *dbc;
	db_recno_t *recnop;
	DBT *data;
	u_int32_t flags, bi_flags;
{
	BKEYDATA *bk;
	BTREE_CURSOR *cp;
	int exact, ret, stack;

	cp = (BTREE_CURSOR *)dbc->internal;

retry:	/* Find the slot for insertion. */
	if ((ret = __bam_rsearch(dbc, recnop,
	    S_INSERT | (flags == DB_APPEND ? S_APPEND : 0), 1, &exact)) != 0)
		return (ret);
	stack = 1;
	cp->page = cp->csp->page;
	cp->pgno = cp->csp->page->pgno;
	cp->indx = cp->csp->indx;

	/*
	 * The application may modify the data based on the selected record
	 * number.
	 */
	if (flags == DB_APPEND && dbc->dbp->db_append_recno != NULL &&
	    (ret = dbc->dbp->db_append_recno(dbc->dbp, data, *recnop)) != 0)
		goto err;

	/*
	 * If re-numbering records, the on-page deleted flag means this record
	 * was implicitly created.  If not re-numbering records, the on-page
	 * deleted flag means this record was implicitly created, or, it was
	 * deleted at some time.
	 *
	 * If DB_NOOVERWRITE is set and the item already exists in the tree,
	 * return an error unless the item was either marked for deletion or
	 * only implicitly created.
	 */
	if (exact) {
		bk = GET_BKEYDATA(cp->page, cp->indx);
		if (!B_DISSET(bk->type) && flags == DB_NOOVERWRITE) {
			ret = DB_KEYEXIST;
			goto err;
		}
	}

	/*
	 * Select the arguments for __bam_iitem() and do the insert.  If the
	 * key is an exact match, or we're replacing the data item with a
	 * new data item, replace the current item.  If the key isn't an exact
	 * match, we're inserting a new key/data pair, before the search
	 * location.
	 */
	switch (ret = __bam_iitem(dbc,
	    NULL, data, exact ? DB_CURRENT : DB_BEFORE, bi_flags)) {
	case 0:
		/*
		 * Don't adjust anything.
		 *
		 * If we inserted a record, no cursors need adjusting because
		 * the only new record it's possible to insert is at the very
		 * end of the tree.  The necessary adjustments to the internal
		 * page counts were made by __bam_iitem().
		 *
		 * If we overwrote a record, no cursors need adjusting because
		 * future DBcursor->get calls will simply return the underlying
		 * record (there's no adjustment made for the DB_CURRENT flag
		 * when a cursor get operation immediately follows a cursor
		 * delete operation, and the normal adjustment for the DB_NEXT
		 * flag is still correct).
		 */
		break;
	case DB_NEEDSPLIT:
		/* Discard the stack of pages and split the page. */
		(void)__bam_stkrel(dbc, STK_CLRDBC);
		stack = 0;

		if ((ret = __bam_split(dbc, recnop)) != 0)
			goto err;

		goto retry;
		/* NOTREACHED */
	default:
		goto err;
	}

err:	if (stack)
		__bam_stkrel(dbc, STK_CLRDBC);

	return (ret);
}
