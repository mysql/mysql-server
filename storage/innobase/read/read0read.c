/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file read/read0read.c
Cursor read

Created 2/16/1997 Heikki Tuuri
*******************************************************/

#include "read0read.h"

#ifdef UNIV_NONINL
#include "read0read.ic"
#endif

#include "srv0srv.h"
#include "trx0sys.h"

/*
-------------------------------------------------------------------------------
FACT A: Cursor read view on a secondary index sees only committed versions
-------
of the records in the secondary index or those versions of rows created
by transaction which created a cursor before cursor was created even
if transaction which created the cursor has changed that clustered index page.

PROOF: We must show that read goes always to the clustered index record
to see that record is visible in the cursor read view. Consider e.g.
following table and SQL-clauses:

create table t1(a int not null, b int, primary key(a), index(b));
insert into t1 values (1,1),(2,2);
commit;

Now consider that we have a cursor for a query

select b from t1 where b >= 1;

This query will use secondary key on the table t1. Now after the first fetch
on this cursor if we do a update:

update t1 set b = 5 where b = 2;

Now second fetch of the cursor should not see record (2,5) instead it should
see record (2,2).

We also should show that if we have delete t1 where b = 5; we still
can see record (2,2).

When we access a secondary key record maximum transaction id is fetched
from this record and this trx_id is compared to up_limit_id in the view.
If trx_id in the record is greater or equal than up_limit_id in the view
cluster record is accessed.  Because trx_id of the creating
transaction is stored when this view was created to the list of
trx_ids not seen by this read view previous version of the
record is requested to be built. This is build using clustered record.
If the secondary key record is delete  marked it's corresponding
clustered record can be already be purged only if records
trx_id < low_limit_no. Purge can't remove any record deleted by a
transaction which was active when cursor was created. But, we still
may have a deleted secondary key record but no clustered record. But,
this is not a problem because this case is handled in
row_sel_get_clust_rec() function which is called
whenever we note that this read view does not see trx_id in the
record. Thus, we see correct version. Q. E. D.

-------------------------------------------------------------------------------
FACT B: Cursor read view on a clustered index sees only committed versions
-------
of the records in the clustered index or those versions of rows created
by transaction which created a cursor before cursor was created even
if transaction which created the cursor has changed that clustered index page.

PROOF:  Consider e.g.following table and SQL-clauses:

create table t1(a int not null, b int, primary key(a));
insert into t1 values (1),(2);
commit;

Now consider that we have a cursor for a query

select a from t1 where a >= 1;

This query will use clustered key on the table t1. Now after the first fetch
on this cursor if we do a update:

update t1 set a = 5 where a = 2;

Now second fetch of the cursor should not see record (5) instead it should
see record (2).

We also should show that if we have execute delete t1 where a = 5; after
the cursor is opened we still can see record (2).

When accessing clustered record we always check if this read view sees
trx_id stored to clustered record. By default we don't see any changes
if record trx_id >= low_limit_id i.e. change was made transaction
which started after transaction which created the cursor. If row
was changed by the future transaction a previous version of the
clustered record is created. Thus we see only committed version in
this case. We see all changes made by committed transactions i.e.
record trx_id < up_limit_id. In this case we don't need to do anything,
we already see correct version of the record. We don't see any changes
made by active transaction except creating transaction. We have stored
trx_id of creating transaction to list of trx_ids when this view was
created. Thus we can easily see if this record was changed by the
creating transaction. Because we already have clustered record we can
access roll_ptr. Using this roll_ptr we can fetch undo record.
We can now check that undo_no of the undo record is less than undo_no of the
trancaction which created a view when cursor was created. We see this
clustered record only in case when record undo_no is less than undo_no
in the view. If this is not true we build based on undo_rec previous
version of the record. This record is found because purge can't remove
records accessed by active transaction. Thus we see correct version. Q. E. D.
-------------------------------------------------------------------------------
FACT C: Purge does not remove any delete marked row that is visible
-------
to cursor view.

TODO: proof this

*/

/*********************************************************************//**
Creates a read view object.
@return	own: read view struct */
UNIV_INLINE
read_view_t*
read_view_create_low(
/*=================*/
	ulint		n,	/*!< in: number of cells in the trx_ids array */
	mem_heap_t*	heap)	/*!< in: memory heap from which allocated */
{
	read_view_t*	view;

	view = mem_heap_alloc(heap, sizeof(read_view_t));

	view->n_trx_ids = n;
	view->trx_ids = mem_heap_alloc(heap, n * sizeof *view->trx_ids);

	return(view);
}

/*********************************************************************//**
Makes a copy of the oldest existing read view, with the exception that also
the creating trx of the oldest view is set as not visible in the 'copied'
view. Opens a new view if no views currently exist. The view must be closed
with ..._close. This is used in purge.
@return	own: read view struct */
UNIV_INTERN
read_view_t*
read_view_oldest_copy_or_open_new(
/*==============================*/
	trx_id_t	cr_trx_id,	/*!< in: trx_id of creating
					transaction, or 0 used in purge */
	mem_heap_t*	heap)		/*!< in: memory heap from which
					allocated */
{
	read_view_t*	old_view;
	read_view_t*	view_copy;
	ibool		needs_insert	= TRUE;
	ulint		insert_done	= 0;
	ulint		n;
	ulint		i;

	ut_ad(mutex_own(&kernel_mutex));

	old_view = UT_LIST_GET_LAST(trx_sys->view_list);

	if (old_view == NULL) {

		return(read_view_open_now(cr_trx_id, heap));
	}

	n = old_view->n_trx_ids;

	if (old_view->creator_trx_id) {
		n++;
	} else {
		needs_insert = FALSE;
	}

	view_copy = read_view_create_low(n, heap);

	/* Insert the id of the creator in the right place of the descending
	array of ids, if needs_insert is TRUE: */

	i = 0;
	while (i < n) {
		if (needs_insert
		    && (i >= old_view->n_trx_ids
			|| old_view->creator_trx_id
			> read_view_get_nth_trx_id(old_view, i))) {

			read_view_set_nth_trx_id(view_copy, i,
						 old_view->creator_trx_id);
			needs_insert = FALSE;
			insert_done = 1;
		} else {
			read_view_set_nth_trx_id(view_copy, i,
						 read_view_get_nth_trx_id(
							 old_view,
							 i - insert_done));
		}

		i++;
	}

	view_copy->creator_trx_id = cr_trx_id;

	view_copy->low_limit_no = old_view->low_limit_no;
	view_copy->low_limit_id = old_view->low_limit_id;


	if (n > 0) {
		/* The last active transaction has the smallest id: */
		view_copy->up_limit_id = read_view_get_nth_trx_id(
			view_copy, n - 1);
	} else {
		view_copy->up_limit_id = old_view->up_limit_id;
	}

	UT_LIST_ADD_LAST(view_list, trx_sys->view_list, view_copy);

	return(view_copy);
}

/*********************************************************************//**
Opens a read view where exactly the transactions serialized before this
point in time are seen in the view.
@return	own: read view struct */
UNIV_INTERN
read_view_t*
read_view_open_now(
/*===============*/
	trx_id_t	cr_trx_id,	/*!< in: trx_id of creating
					transaction, or 0 used in purge */
	mem_heap_t*	heap)		/*!< in: memory heap from which
					allocated */
{
	read_view_t*	view;
	trx_t*		trx;
	ulint		n;

	ut_ad(mutex_own(&kernel_mutex));

	view = read_view_create_low(UT_LIST_GET_LEN(trx_sys->trx_list), heap);

	view->creator_trx_id = cr_trx_id;
	view->type = VIEW_NORMAL;
	view->undo_no = 0;

	/* No future transactions should be visible in the view */

	view->low_limit_no = trx_sys->max_trx_id;
	view->low_limit_id = view->low_limit_no;

	n = 0;
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	/* No active transaction should be visible, except cr_trx */

	while (trx) {
		if (trx->id != cr_trx_id
		    && (trx->conc_state == TRX_ACTIVE
			|| trx->conc_state == TRX_PREPARED)) {

			read_view_set_nth_trx_id(view, n, trx->id);

			n++;

			/* NOTE that a transaction whose trx number is <
			trx_sys->max_trx_id can still be active, if it is
			in the middle of its commit! Note that when a
			transaction starts, we initialize trx->no to
			IB_ULONGLONG_MAX. */

			if (view->low_limit_no > trx->no) {

				view->low_limit_no = trx->no;
			}
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	view->n_trx_ids = n;

	if (n > 0) {
		/* The last active transaction has the smallest id: */
		view->up_limit_id = read_view_get_nth_trx_id(view, n - 1);
	} else {
		view->up_limit_id = view->low_limit_id;
	}


	UT_LIST_ADD_FIRST(view_list, trx_sys->view_list, view);

	return(view);
}

/*********************************************************************//**
Closes a read view. */
UNIV_INTERN
void
read_view_close(
/*============*/
	read_view_t*	view)	/*!< in: read view */
{
	ut_ad(mutex_own(&kernel_mutex));

	UT_LIST_REMOVE(view_list, trx_sys->view_list, view);
}

/*********************************************************************//**
Closes a consistent read view for MySQL. This function is called at an SQL
statement end if the trx isolation level is <= TRX_ISO_READ_COMMITTED. */
UNIV_INTERN
void
read_view_close_for_mysql(
/*======================*/
	trx_t*	trx)	/*!< in: trx which has a read view */
{
	ut_a(trx->global_read_view);

	mutex_enter(&kernel_mutex);

	read_view_close(trx->global_read_view);

	mem_heap_empty(trx->global_read_view_heap);

	trx->read_view = NULL;
	trx->global_read_view = NULL;

	mutex_exit(&kernel_mutex);
}

/*********************************************************************//**
Prints a read view to stderr. */
UNIV_INTERN
void
read_view_print(
/*============*/
	const read_view_t*	view)	/*!< in: read view */
{
	ulint	n_ids;
	ulint	i;

	if (view->type == VIEW_HIGH_GRANULARITY) {
		fprintf(stderr,
			"High-granularity read view undo_n:o %llu\n",
			(ullint) view->undo_no);
	} else {
		fprintf(stderr, "Normal read view\n");
	}

	fprintf(stderr, "Read view low limit trx n:o " TRX_ID_FMT "\n",
		(ullint) view->low_limit_no);

	fprintf(stderr, "Read view up limit trx id " TRX_ID_FMT "\n",
		(ullint) view->up_limit_id);

	fprintf(stderr, "Read view low limit trx id " TRX_ID_FMT "\n",
		(ullint) view->low_limit_id);

	fprintf(stderr, "Read view individually stored trx ids:\n");

	n_ids = view->n_trx_ids;

	for (i = 0; i < n_ids; i++) {
		fprintf(stderr, "Read view trx id " TRX_ID_FMT "\n",
			(ullint) read_view_get_nth_trx_id(view, i));
	}
}

/*********************************************************************//**
Create a high-granularity consistent cursor view for mysql to be used
in cursors. In this consistent read view modifications done by the
creating transaction after the cursor is created or future transactions
are not visible. */
UNIV_INTERN
cursor_view_t*
read_cursor_view_create_for_mysql(
/*==============================*/
	trx_t*	cr_trx)	/*!< in: trx where cursor view is created */
{
	cursor_view_t*	curview;
	read_view_t*	view;
	mem_heap_t*	heap;
	trx_t*		trx;
	ulint		n;

	ut_a(cr_trx);

	/* Use larger heap than in trx_create when creating a read_view
	because cursors are quite long. */

	heap = mem_heap_create(512);

	curview = (cursor_view_t*) mem_heap_alloc(heap, sizeof(cursor_view_t));
	curview->heap = heap;

	/* Drop cursor tables from consideration when evaluating the need of
	auto-commit */
	curview->n_mysql_tables_in_use = cr_trx->n_mysql_tables_in_use;
	cr_trx->n_mysql_tables_in_use = 0;

	mutex_enter(&kernel_mutex);

	curview->read_view = read_view_create_low(
		UT_LIST_GET_LEN(trx_sys->trx_list), curview->heap);

	view = curview->read_view;
	view->creator_trx_id = cr_trx->id;
	view->type = VIEW_HIGH_GRANULARITY;
	view->undo_no = cr_trx->undo_no;

	/* No future transactions should be visible in the view */

	view->low_limit_no = trx_sys->max_trx_id;
	view->low_limit_id = view->low_limit_no;

	n = 0;
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	/* No active transaction should be visible */

	while (trx) {

		if (trx->conc_state == TRX_ACTIVE
		    || trx->conc_state == TRX_PREPARED) {

			read_view_set_nth_trx_id(view, n, trx->id);

			n++;

			/* NOTE that a transaction whose trx number is <
			trx_sys->max_trx_id can still be active, if it is
			in the middle of its commit! Note that when a
			transaction starts, we initialize trx->no to
			IB_ULONGLONG_MAX. */

			if (view->low_limit_no > trx->no) {

				view->low_limit_no = trx->no;
			}
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	view->n_trx_ids = n;

	if (n > 0) {
		/* The last active transaction has the smallest id: */
		view->up_limit_id = read_view_get_nth_trx_id(view, n - 1);
	} else {
		view->up_limit_id = view->low_limit_id;
	}

	UT_LIST_ADD_FIRST(view_list, trx_sys->view_list, view);

	mutex_exit(&kernel_mutex);

	return(curview);
}

/*********************************************************************//**
Close a given consistent cursor view for mysql and restore global read view
back to a transaction read view. */
UNIV_INTERN
void
read_cursor_view_close_for_mysql(
/*=============================*/
	trx_t*		trx,	/*!< in: trx */
	cursor_view_t*	curview)/*!< in: cursor view to be closed */
{
	ut_a(curview);
	ut_a(curview->read_view);
	ut_a(curview->heap);

	/* Add cursor's tables to the global count of active tables that
	belong to this transaction */
	trx->n_mysql_tables_in_use += curview->n_mysql_tables_in_use;

	mutex_enter(&kernel_mutex);

	read_view_close(curview->read_view);
	trx->read_view = trx->global_read_view;

	mutex_exit(&kernel_mutex);

	mem_heap_free(curview->heap);
}

/*********************************************************************//**
This function sets a given consistent cursor view to a transaction
read view if given consistent cursor view is not NULL. Otherwise, function
restores a global read view to a transaction read view. */
UNIV_INTERN
void
read_cursor_set_for_mysql(
/*======================*/
	trx_t*		trx,	/*!< in: transaction where cursor is set */
	cursor_view_t*	curview)/*!< in: consistent cursor view to be set */
{
	ut_a(trx);

	mutex_enter(&kernel_mutex);

	if (UNIV_LIKELY(curview != NULL)) {
		trx->read_view = curview->read_view;
	} else {
		trx->read_view = trx->global_read_view;
	}

	mutex_exit(&kernel_mutex);
}
