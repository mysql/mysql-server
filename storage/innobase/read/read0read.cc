/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file read/read0read.cc
Cursor read

Created 2/16/1997 Heikki Tuuri
*******************************************************/

#include "read0read.h"

#ifdef UNIV_NONINL
#include "read0read.ic"
#endif

#include "srv0srv.h"
#include "trx0sys.h"

#include <algorithm>

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
If the secondary key record is delete-marked, its corresponding
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
FACT C: Purge does not remove any delete-marked row that is visible
-------
in any cursor read view.

PROOF: We know that:
 1: Currently active read views in trx_sys_t::view_list are ordered by
    read_view_t::low_limit_no in descending order, that is,
    newest read view first.

 2: Purge clones the oldest read view and uses that to determine whether there
    are any active transactions that can see the to be purged records.

Therefore any joining or active transaction will not have a view older
than the purge view, according to 1.

When purge needs to remove a delete-marked row from a secondary index,
it will first check that the DB_TRX_ID value of the corresponding
record in the clustered index is older than the purge view. It will
also check if there is a newer version of the row (clustered index
record) that is not delete-marked in the secondary index. If such a
row exists and is collation-equal to the delete-marked secondary index
record then purge will not remove the secondary index record.

Delete-marked clustered index records will be removed by
row_purge_remove_clust_if_poss(), unless the clustered index record
(and its DB_ROLL_PTR) has been updated. Every new version of the
clustered index record will update DB_ROLL_PTR, pointing to a new UNDO
log entry that allows the old version to be reconstructed. The
DB_ROLL_PTR in the oldest remaining version in the old-version chain
may be pointing to garbage (an undo log record discarded by purge),
but it will never be dereferenced, because the purge view is older
than any active transaction.

For details see: row_vers_old_has_index_entry() and row_purge_poss_sec()

Some additional issues:

What if trx_sys->view_list == NULL and some transaction T1 and Purge both
try to open read_view at same time. Only one can acquire trx_sys->mutex.
In which order will the views be opened? Should it matter? If no, why?

The order does not matter. No new transactions can be created and no running
transaction can commit or rollback (or free views).
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

	view = static_cast<read_view_t*>(
		mem_heap_alloc(
			heap, sizeof(*view) + n * sizeof(*view->trx_ids)));

	view->n_trx_ids = n;
	view->trx_ids = (trx_id_t*) &view[1];

	return(view);
}

/*********************************************************************//**
Clones a read view object. This function will allocate space for two read
views contiguously iff the creator_trx_id > 0. One identical in size
and content as @param view (starting at returned pointer) and another view
immediately following the trx_ids array. The second view will have and
extra slot for a trx_id_t element iff view->creator_trx_id > 0.
@return	read view struct */
UNIV_INLINE
read_view_t*
read_view_clone(
/*============*/
	const read_view_t*	view,	/*!< in: view to clone */
	mem_heap_t*		heap)	/*!< in: memory heap
					from which allocated */
{
	ulint		sz;
	read_view_t*	clone;
	read_view_t*	new_view;

	ut_ad(mutex_own(&trx_sys->mutex));

	/* Allocate space for two views. */

	sz = sizeof(*view) + view->n_trx_ids * sizeof(*view->trx_ids);

	if (view->creator_trx_id > 0) {
		/* Add an extra trx_id_t slot for the new view. */
		clone = static_cast<read_view_t*>(
			mem_heap_alloc(heap, (sz * 2) + sizeof(trx_id_t)));
	} else {
		/* No creator trx id, we can use the view as is for purge. */

		clone = static_cast<read_view_t*>(mem_heap_alloc(heap, sz));
	}

	/* Only the contents of the old view are important, the new view
	will be created from this and so we don't copy that across. */

	memcpy(clone, view, sz);

	clone->trx_ids = reinterpret_cast<trx_id_t*>(&clone[1]);

	if (view->creator_trx_id > 0) {
		new_view = (read_view_t*) &clone->trx_ids[clone->n_trx_ids];
		new_view->trx_ids = (trx_id_t*) &new_view[1];

		new_view->n_trx_ids = clone->n_trx_ids + 1;

		ut_a(new_view->n_trx_ids == view->n_trx_ids + 1);
	}

	return(clone);
}

/*********************************************************************//**
Insert the view in the proper order into the trx_sys->view_list. The
read view list is ordered by read_view_t::low_limit_no in descending order. */
UNIV_INTERN
void
read_view_add(
/*==========*/
	read_view_t*	view)		/*!< in: view to add to */
{
	read_view_t*	elem;
	read_view_t*	prev_elem;

	ut_ad(mutex_own(&trx_sys->mutex));
	ut_ad(read_view_validate(view));

	/* Find the correct slot for insertion. */
	for (elem = UT_LIST_GET_FIRST(trx_sys->view_list), prev_elem = NULL;
	     elem != NULL && view->low_limit_no < elem->low_limit_no;
	     prev_elem = elem, elem = UT_LIST_GET_NEXT(view_list, elem)) {
		/* No op */
	}

	if (prev_elem == NULL) {
		UT_LIST_ADD_FIRST(view_list, trx_sys->view_list, view);
	} else {
		UT_LIST_INSERT_AFTER(
			view_list, trx_sys->view_list, prev_elem, view);
	}

	ut_ad(read_view_list_validate());
}

/** Functor to create thew view trx_ids array. */
struct	CreateView {

	CreateView(read_view_t*	view)
		: m_view(view)
	{
		  m_n_trx = m_view->n_trx_ids;
		  m_view->n_trx_ids = 0;
	}

	void	operator()(const trx_t* trx)
	{
		ut_ad(mutex_own(&trx_sys->mutex));
		ut_ad(trx->in_rw_trx_list);

		/* trx->state cannot change from or to NOT_STARTED
		while we are holding the trx_sys->mutex. It may change
		from ACTIVE to PREPARED or COMMITTED. */

		ut_ad(trx->id > 0);

		if (trx->id != m_view->creator_trx_id
		    && !trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY)) {

			ut_ad(m_n_trx > m_view->n_trx_ids);

			m_view->trx_ids[m_view->n_trx_ids++] = trx->id;

			/* NOTE that a transaction whose trx number is <
			trx_sys->max_trx_id can still be active, if it is
			in the middle of its commit! Note that when a
			transaction starts, we initialize trx->no to
			TRX_ID_MAX. */

			/* trx->no is protected by trx_sys->mutex, which
			we are holding. It is assigned by trx_commit()
			before lock_trx_release_locks() assigns
			trx->state = TRX_STATE_COMMITTED_IN_MEMORY. */

			if (m_view->low_limit_no > trx->no) {
				m_view->low_limit_no = trx->no;
			}
		}
	}

	read_view_t*	m_view;
	ulint		m_n_trx;
};

/*********************************************************************//**
Opens a read view where exactly the transactions serialized before this
point in time are seen in the view.
@return	own: read view struct */
static
read_view_t*
read_view_open_now_low(
/*===================*/
	trx_id_t	cr_trx_id,	/*!< in: trx_id of creating
					transaction, or 0 used in purge */
	mem_heap_t*	heap,		/*!< in: memory heap from which
					allocated */
	bool		purge)		/*!< in: true if purge view */
{
	read_view_t*	view;
	ulint		n_trx = UT_LIST_GET_LEN(trx_sys->rw_trx_list);

	ut_ad(mutex_own(&trx_sys->mutex));

	view = read_view_create_low(n_trx, heap);

	view->creator_trx_id = cr_trx_id;

	/* No future transactions should be visible in the view */

	view->low_limit_no = view->low_limit_id = trx_sys->max_trx_id;

	/* No active transaction should be visible, except cr_trx */

	ut_list_map(trx_sys->rw_trx_list, &trx_t::trx_list, CreateView(view));

	if (view->n_trx_ids > 0) {
		/* The last active transaction has the smallest id: */
		view->up_limit_id = view->trx_ids[view->n_trx_ids - 1];
	} else {
		view->up_limit_id = view->low_limit_id;
	}

	ut_ad(view->up_limit_id <= view->low_limit_id);

	/* Purge views are not added to the view list. */
	if (!purge) {
		read_view_add(view);
	}

	return(view);
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
	ut_ad(!srv_read_only_mode);

	read_view_t*	view;

	mutex_enter(&trx_sys->mutex);

	view = read_view_open_now_low(cr_trx_id, heap, false);

	mutex_exit(&trx_sys->mutex);

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
read_view_purge_open(
/*=================*/
	mem_heap_t*	heap)		/*!< in: memory heap from which
					allocated */
{
	read_view_t*	view;
	read_view_t*	oldest_view;

	mutex_enter(&trx_sys->mutex);

	oldest_view = UT_LIST_GET_LAST(trx_sys->view_list);

	if (oldest_view == NULL) {

		view = read_view_open_now_low(0, heap, true);

		mutex_exit(&trx_sys->mutex);

		return(view);
	}

	/* Clone the oldest view, if the creator_trx_id is > 0 then
	allocate space for two views, the oldest and the new purge view. */

	oldest_view = read_view_clone(oldest_view, heap);

	ut_ad(read_view_validate(oldest_view));

	mutex_exit(&trx_sys->mutex);

	trx_id_t	creator_trx_id = oldest_view->creator_trx_id;

	if (creator_trx_id > 0) {

		view = reinterpret_cast<read_view_t*>(
			&oldest_view->trx_ids[oldest_view->n_trx_ids]);

		/* Add the creator transaction id in the trx_ids
		array in the correct slot.  */

		ulint	i;
		ulint	insert_done = 0;

		for (i = 0; i < oldest_view->n_trx_ids; ++i) {
			trx_id_t	id;

			id = oldest_view->trx_ids[i - insert_done];

			if (insert_done == 0 && creator_trx_id > id) {
				id = creator_trx_id;
				insert_done = 1;
			}

			view->trx_ids[i] = id;
		}

		if (insert_done == 0) {
			view->trx_ids[i] = creator_trx_id;
		} else {
			ut_a(i > 0);
			view->trx_ids[i] = oldest_view->trx_ids[i - 1];
		}

		view->creator_trx_id = 0;

		view->low_limit_no = oldest_view->low_limit_no;
		view->low_limit_id = oldest_view->low_limit_id;
		view->up_limit_id = oldest_view->up_limit_id;
	} else {
		/* We can use the cloned view as is. */
		view = oldest_view;
	}

	if (view->n_trx_ids > 0) {

		/* The last active transaction has the smallest id. However
		it may be larger than the oldest view's up_limit_id because
		the oldest view's creator id can be larger and that will be
		in the tx_ids array. */

		view->up_limit_id = std::min(
			view->trx_ids[view->n_trx_ids - 1],
			view->up_limit_id);
	}

	ut_ad(view->up_limit_id <= view->low_limit_id);

	return(view);
}

/*********************************************************************//**
Closes a consistent read view for MySQL. This function is called at an SQL
statement end if the trx isolation level is <= TRX_ISO_READ_COMMITTED. */
UNIV_INTERN
void
read_view_close_for_mysql(
/*======================*/
	trx_t*		trx)	/*!< in: trx which has a read view */
{
	if (!srv_read_only_mode) {

		read_view_remove(trx->read_view, false);

		mem_heap_empty(trx->read_view_heap);

		trx->read_view = NULL;
	}
}
