/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: bt_curadj.c,v 11.20 2001/01/17 16:15:49 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"
#include "txn.h"

static int __bam_opd_cursor __P((DB *, DBC *, db_pgno_t, u_int32_t, u_int32_t));

#ifdef DEBUG
/*
 * __bam_cprint --
 *	Display the current internal cursor.
 *
 * PUBLIC: void __bam_cprint __P((DBC *));
 */
void
__bam_cprint(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;

	cp = (BTREE_CURSOR *)dbc->internal;

	fprintf(stderr, "\tinternal: ovflsize: %lu", (u_long)cp->ovflsize);
	if (dbc->dbtype == DB_RECNO)
		fprintf(stderr, " recno: %lu", (u_long)cp->recno);
	if (F_ISSET(cp, C_DELETED))
		fprintf(stderr, " (deleted)");
	fprintf(stderr, "\n");
}
#endif

/*
 * Cursor adjustments are logged if they are for subtransactions.  This is
 * because it's possible for a subtransaction to adjust cursors which will
 * still be active after the subtransaction aborts, and so which must be
 * restored to their previous locations.  Cursors that can be both affected
 * by our cursor adjustments and active after our transaction aborts can
 * only be found in our parent transaction -- cursors in other transactions,
 * including other child transactions of our parent, must have conflicting
 * locker IDs, and so cannot be affected by adjustments in this transaction.
 */

/*
 * __bam_ca_delete --
 *	Update the cursors when items are deleted and when already deleted
 *	items are overwritten.  Return the number of relevant cursors found.
 *
 * PUBLIC: int __bam_ca_delete __P((DB *, db_pgno_t, u_int32_t, int));
 */
int
__bam_ca_delete(dbp, pgno, indx, delete)
	DB *dbp;
	db_pgno_t pgno;
	u_int32_t indx;
	int delete;
{
	BTREE_CURSOR *cp;
	DB *ldbp;
	DB_ENV *dbenv;
	DBC *dbc;
	int count;		/* !!!: Has to contain max number of cursors. */

	dbenv = dbp->dbenv;

	/*
	 * Adjust the cursors.  We have the page write locked, so the
	 * only other cursors that can be pointing at a page are
	 * those in the same thread of control.  Unfortunately, we don't
	 * know that they're using the same DB handle, so traverse
	 * all matching DB handles in the same DB_ENV, then all cursors
	 * on each matching DB handle.
	 *
	 * Each cursor is single-threaded, so we only need to lock the
	 * list of DBs and then the list of cursors in each DB.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (count = 0, ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			cp = (BTREE_CURSOR *)dbc->internal;
			if (cp->pgno == pgno && cp->indx == indx) {
				if (delete)
					F_SET(cp, C_DELETED);
				else
					F_CLR(cp, C_DELETED);
				++count;
			}
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	return (count);
}

/*
 * __ram_ca_delete --
 *	Return the number of relevant cursors.
 *
 * PUBLIC: int __ram_ca_delete __P((DB *, db_pgno_t));
 */
int
__ram_ca_delete(dbp, root_pgno)
	DB *dbp;
	db_pgno_t root_pgno;
{
	DB *ldbp;
	DBC *dbc;
	DB_ENV *dbenv;
	int found;

	found = 0;
	dbenv = dbp->dbenv;

	/*
	 * Review the cursors.  See the comment in __bam_ca_delete().
	 */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    found == 0 && ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    found == 0 && dbc != NULL; dbc = TAILQ_NEXT(dbc, links))
			if (dbc->internal->root == root_pgno)
				found = 1;
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);
	return (found);
}

/*
 * __bam_ca_di --
 *	Adjust the cursors during a delete or insert.
 *
 * PUBLIC: int __bam_ca_di __P((DBC *, db_pgno_t, u_int32_t, int));
 */
int
__bam_ca_di(my_dbc, pgno, indx, adjust)
	DBC *my_dbc;
	db_pgno_t pgno;
	u_int32_t indx;
	int adjust;
{
	DB *dbp, *ldbp;
	DB_ENV *dbenv;
	DB_LSN lsn;
	DB_TXN *my_txn;
	DBC *dbc;
	DBC_INTERNAL *cp;
	int found, ret;

	dbp = my_dbc->dbp;
	dbenv = dbp->dbenv;

	my_txn = IS_SUBTRANSACTION(my_dbc->txn) ? my_dbc->txn : NULL;

	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 */
	found = 0;
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			if (dbc->dbtype == DB_RECNO)
				continue;
			cp = dbc->internal;
			if (cp->pgno == pgno && cp->indx >= indx) {
				/* Cursor indices should never be negative. */
				DB_ASSERT(cp->indx != 0 || adjust > 0);

				cp->indx += adjust;
				if (my_txn != NULL && dbc->txn != my_txn)
					found = 1;
			}
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	if (found != 0 && DB_LOGGING(my_dbc)) {
		if ((ret = __bam_curadj_log(dbenv,
		    my_dbc->txn, &lsn, 0, dbp->log_fileid,
		    DB_CA_DI, pgno, 0, 0, adjust, indx, 0)) != 0)
			return (ret);
	}

	return (0);
}

/*
 * __bam_opd_cursor -- create a new opd cursor.
 */
static int
__bam_opd_cursor(dbp, dbc, first, tpgno, ti)
	DB *dbp;
	DBC *dbc;
	db_pgno_t tpgno;
	u_int32_t first, ti;
{
	BTREE_CURSOR *cp, *orig_cp;
	DBC *dbc_nopd;
	int ret;

	orig_cp = (BTREE_CURSOR *)dbc->internal;
	dbc_nopd = NULL;

	/*
	 * Allocate a new cursor and create the stack.  If duplicates
	 * are sorted, we've just created an off-page duplicate Btree.
	 * If duplicates aren't sorted, we've just created a Recno tree.
	 */
	if ((ret = __db_c_newopd(dbc, tpgno, &dbc_nopd)) != 0)
		return (ret);

	cp = (BTREE_CURSOR *)dbc_nopd->internal;
	cp->pgno = tpgno;
	cp->indx = ti;

	if (dbp->dup_compare == NULL) {
		/*
		 * Converting to off-page Recno trees is tricky.  The
		 * record number for the cursor is the index + 1 (to
		 * convert to 1-based record numbers).
		 */
		cp->recno = ti + 1;
	}

	/*
	 * Transfer the deleted flag from the top-level cursor to the
	 * created one.
	 */
	if (F_ISSET(orig_cp, C_DELETED)) {
		F_SET(cp, C_DELETED);
		F_CLR(orig_cp, C_DELETED);
	}

	/* Stack the cursors and reset the initial cursor's index. */
	orig_cp->opd = dbc_nopd;
	orig_cp->indx = first;
	return (0);
}

/*
 * __bam_ca_dup --
 *	Adjust the cursors when moving items from a leaf page to a duplicates
 *	page.
 *
 * PUBLIC: int __bam_ca_dup __P((DBC *,
 * PUBLIC:    u_int32_t, db_pgno_t, u_int32_t, db_pgno_t, u_int32_t));
 */
int
__bam_ca_dup(my_dbc, first, fpgno, fi, tpgno, ti)
	DBC *my_dbc;
	db_pgno_t fpgno, tpgno;
	u_int32_t first, fi, ti;
{
	BTREE_CURSOR *orig_cp;
	DB *dbp, *ldbp;
	DBC *dbc;
	DB_ENV *dbenv;
	DB_LSN lsn;
	DB_TXN *my_txn;
	int found, ret;

	dbp = my_dbc->dbp;
	dbenv = dbp->dbenv;
	my_txn = IS_SUBTRANSACTION(my_dbc->txn) ? my_dbc->txn : NULL;

	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 */
	found = 0;
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
loop:		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			/* Find cursors pointing to this record. */
			orig_cp = (BTREE_CURSOR *)dbc->internal;
			if (orig_cp->pgno != fpgno || orig_cp->indx != fi)
				continue;

			/*
			 * Since we rescan the list see if this is already
			 * converted.
			 */
			if (orig_cp->opd != NULL)
				continue;

			MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
			if ((ret = __bam_opd_cursor(dbp,
			    dbc, first, tpgno, ti)) !=0)
				return (ret);
			if (my_txn != NULL && dbc->txn != my_txn)
				found = 1;
			/* We released the MUTEX to get a cursor, start over. */
			goto loop;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	if (found != 0 && DB_LOGGING(my_dbc)) {
		if ((ret = __bam_curadj_log(dbenv,
		    my_dbc->txn, &lsn, 0, dbp->log_fileid,
		    DB_CA_DUP, fpgno, tpgno, 0, first, fi, ti)) != 0)
			return (ret);
	}
	return (0);
}

/*
 * __bam_ca_undodup --
 *	Adjust the cursors when returning items to a leaf page
 *	from a duplicate page.
 *	Called only during undo processing.
 *
 * PUBLIC: int __bam_ca_undodup __P((DB *,
 * PUBLIC:    u_int32_t, db_pgno_t, u_int32_t, u_int32_t));
 */
int
__bam_ca_undodup(dbp, first, fpgno, fi, ti)
	DB *dbp;
	db_pgno_t fpgno;
	u_int32_t first, fi, ti;
{
	BTREE_CURSOR *orig_cp;
	DB *ldbp;
	DBC *dbc;
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
loop:		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			orig_cp = (BTREE_CURSOR *)dbc->internal;

			if (orig_cp->pgno != fpgno ||
			    orig_cp->indx != first ||
			    ((BTREE_CURSOR *)orig_cp->opd->internal)->indx
			    != ti)
				continue;
			MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
			if ((ret = orig_cp->opd->c_close(orig_cp->opd)) != 0)
				return (ret);
			orig_cp->opd = NULL;
			orig_cp->indx = fi;
			/*
			 * We released the MUTEX to free a cursor,
			 * start over.
			 */
			goto loop;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	return (0);
}

/*
 * __bam_ca_rsplit --
 *	Adjust the cursors when doing reverse splits.
 *
 * PUBLIC: int __bam_ca_rsplit __P((DBC *, db_pgno_t, db_pgno_t));
 */
int
__bam_ca_rsplit(my_dbc, fpgno, tpgno)
	DBC* my_dbc;
	db_pgno_t fpgno, tpgno;
{
	DB *dbp, *ldbp;
	DBC *dbc;
	DB_ENV *dbenv;
	DB_LSN lsn;
	DB_TXN *my_txn;
	int found, ret;

	dbp = my_dbc->dbp;
	dbenv = dbp->dbenv;
	my_txn = IS_SUBTRANSACTION(my_dbc->txn) ? my_dbc->txn : NULL;

	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 */
	found = 0;
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			if (dbc->dbtype == DB_RECNO)
				continue;
			if (dbc->internal->pgno == fpgno) {
				dbc->internal->pgno = tpgno;
				if (my_txn != NULL && dbc->txn != my_txn)
					found = 1;
			}
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	if (found != 0 && DB_LOGGING(my_dbc)) {
		if ((ret = __bam_curadj_log(dbenv,
		    my_dbc->txn, &lsn, 0, dbp->log_fileid,
		    DB_CA_RSPLIT, fpgno, tpgno, 0, 0, 0, 0)) != 0)
			return (ret);
	}
	return (0);
}

/*
 * __bam_ca_split --
 *	Adjust the cursors when splitting a page.
 *
 * PUBLIC: int __bam_ca_split __P((DBC *,
 * PUBLIC:    db_pgno_t, db_pgno_t, db_pgno_t, u_int32_t, int));
 */
int
__bam_ca_split(my_dbc, ppgno, lpgno, rpgno, split_indx, cleft)
	DBC *my_dbc;
	db_pgno_t ppgno, lpgno, rpgno;
	u_int32_t split_indx;
	int cleft;
{
	DB *dbp, *ldbp;
	DBC *dbc;
	DBC_INTERNAL *cp;
	DB_ENV *dbenv;
	DB_LSN lsn;
	DB_TXN *my_txn;
	int found, ret;

	dbp = my_dbc->dbp;
	dbenv = dbp->dbenv;
	my_txn = IS_SUBTRANSACTION(my_dbc->txn) ? my_dbc->txn : NULL;

	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 *
	 * If splitting the page that a cursor was on, the cursor has to be
	 * adjusted to point to the same record as before the split.  Most
	 * of the time we don't adjust pointers to the left page, because
	 * we're going to copy its contents back over the original page.  If
	 * the cursor is on the right page, it is decremented by the number of
	 * records split to the left page.
	 */
	found = 0;
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			if (dbc->dbtype == DB_RECNO)
				continue;
			cp = dbc->internal;
			if (cp->pgno == ppgno) {
				if (my_txn != NULL && dbc->txn != my_txn)
					found = 1;
				if (cp->indx < split_indx) {
					if (cleft)
						cp->pgno = lpgno;
				} else {
					cp->pgno = rpgno;
					cp->indx -= split_indx;
				}
			}
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);

	if (found != 0 && DB_LOGGING(my_dbc)) {
		if ((ret = __bam_curadj_log(dbenv, my_dbc->txn,
		    &lsn, 0, dbp->log_fileid, DB_CA_SPLIT, ppgno, rpgno,
		    cleft ? lpgno : PGNO_INVALID, 0, split_indx, 0)) != 0)
			return (ret);
	}

	return (0);
}

/*
 * __bam_ca_undosplit --
 *	Adjust the cursors when undoing a split of a page.
 *	If we grew a level we will execute this for both the
 *	left and the right pages.
 *	Called only during undo processing.
 *
 * PUBLIC: void __bam_ca_undosplit __P((DB *,
 * PUBLIC:    db_pgno_t, db_pgno_t, db_pgno_t, u_int32_t));
 */
void
__bam_ca_undosplit(dbp, frompgno, topgno, lpgno, split_indx)
	DB *dbp;
	db_pgno_t frompgno, topgno, lpgno;
	u_int32_t split_indx;
{
	DB *ldbp;
	DBC *dbc;
	DB_ENV *dbenv;
	DBC_INTERNAL *cp;

	dbenv = dbp->dbenv;

	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 *
	 * When backing out a split, we move the cursor back
	 * to the original offset and bump it by the split_indx.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbenv->dblist_mutexp);
	for (ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			if (dbc->dbtype == DB_RECNO)
				continue;
			cp = dbc->internal;
			if (cp->pgno == topgno) {
				cp->pgno = frompgno;
				cp->indx += split_indx;
			} else if (cp->pgno == lpgno)
				cp->pgno = frompgno;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbenv->dblist_mutexp);
}
