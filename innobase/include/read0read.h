/******************************************************
Cursor read

(c) 1997 Innobase Oy

Created 2/16/1997 Heikki Tuuri
*******************************************************/

#ifndef read0read_h
#define read0read_h

#include "univ.i"


#include "ut0byte.h"
#include "ut0lst.h"
#include "trx0trx.h"
#include "read0types.h"

/*************************************************************************
Opens a read view where exactly the transactions serialized before this
point in time are seen in the view. */

read_view_t*
read_view_open_now(
/*===============*/
				/* out, own: read view struct */
	trx_t*		cr_trx,	/* in: creating transaction, or NULL */
	mem_heap_t*	heap);	/* in: memory heap from which allocated */
/*************************************************************************
Makes a copy of the oldest existing read view, or opens a new. The view
must be closed with ..._close. */

read_view_t*
read_view_oldest_copy_or_open_new(
/*==============================*/
				/* out, own: read view struct */
	trx_t*		cr_trx,	/* in: creating transaction, or NULL */
	mem_heap_t*	heap);	/* in: memory heap from which allocated */
/*************************************************************************
Closes a read view. */

void
read_view_close(
/*============*/
	read_view_t*	view);	/* in: read view */
/*************************************************************************
Checks if a read view sees the specified transaction. */
UNIV_INLINE
ibool
read_view_sees_trx_id(
/*==================*/
				/* out: TRUE if sees */
	read_view_t*	view,	/* in: read view */
	dulint		trx_id);	/* in: trx id */


/* Read view lists the trx ids of those transactions for which a consistent
read should not see the modifications to the database. */

struct read_view_struct{
	ibool	can_be_too_old;	/* TRUE if the system has had to purge old
				versions which this read view should be able
				to access: the read view can bump into the
				DB_MISSING_HISTORY error */
	dulint	low_limit_no;	/* The view does not need to see the undo
				logs for transactions whose transaction number
				is strictly smaller (<) than this value: they
				can be removed in purge if not needed by other
				views */
	dulint	low_limit_id;	/* The read should not see any transaction
				with trx id >= this value */
	dulint	up_limit_id;	/* The read should see all trx ids which
				are strictly smaller (<) than this value */
	ulint	n_trx_ids;	/* Number of cells in the trx_ids array */
	dulint*	trx_ids;	/* Additional trx ids which the read should
				not see: typically, these are the active
				transactions at the time when the read is
				serialized, except the reading transaction
				itself; the trx ids in this array are in a
				descending order */
	trx_t*	creator;	/* Pointer to the creating transaction, or
				NULL if used in purge */
	UT_LIST_NODE_T(read_view_t) view_list;
				/* List of read views in trx_sys */
};

#ifndef UNIV_NONINL
#include "read0read.ic"
#endif

#endif 
