/******************************************************
Row versions

(c) 1997 Innobase Oy

Created 2/6/1997 Heikki Tuuri
*******************************************************/

#ifndef row0vers_h
#define row0vers_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "rem0types.h"
#include "mtr0mtr.h"
#include "read0types.h"

/*********************************************************************
Finds out if an active transaction has inserted or modified a secondary
index record. NOTE: the kernel mutex is temporarily released in this
function! */

trx_t*
row_vers_impl_x_locked_off_kernel(
/*==============================*/
				/* out: NULL if committed, else the active
				transaction; NOTE that the kernel mutex is
				temporarily released! */
	rec_t*		rec,	/* in: record in a secondary index */
	dict_index_t*	index);	/* in: the secondary index */
/*********************************************************************
Finds out if we must preserve a delete marked earlier version of a clustered
index record, because it is >= the purge view. */

ibool
row_vers_must_preserve_del_marked(
/*==============================*/
			/* out: TRUE if earlier version should be preserved */
	dulint	trx_id,	/* in: transaction id in the version */
	mtr_t*	mtr);	/* in: mtr holding the latch on the clustered index
			record; it will also hold the latch on purge_view */
/*********************************************************************
Finds out if a version of the record, where the version >= the current
purge view, should have ientry as its secondary index entry. We check
if there is any not delete marked version of the record where the trx
id >= purge view, and the secondary index entry == ientry; exactly in
this case we return TRUE. */

ibool
row_vers_old_has_index_entry(
/*=========================*/
				/* out: TRUE if earlier version should have */
	ibool		also_curr,/* in: TRUE if also rec is included in the
				versions to search; otherwise only versions
				prior to it are searched */
	rec_t*		rec,	/* in: record in the clustered index; the
				caller must have a latch on the page */
	mtr_t*		mtr,	/* in: mtr holding the latch on rec; it will
				also hold the latch on purge_view */
	dict_index_t*	index,	/* in: the secondary index */
	dtuple_t*	ientry);	/* in: the secondary index entry */
/*********************************************************************
Constructs the version of a clustered index record which a consistent
read should see. We assume that the trx id stored in rec is such that
the consistent read should not see rec in its present version. */

ulint
row_vers_build_for_consistent_read(
/*===============================*/
				/* out: DB_SUCCESS or DB_MISSING_HISTORY */
	rec_t*		rec,	/* in: record in a clustered index; the
				caller must have a latch on the page; this
				latch locks the top of the stack of versions
				of this records */
	mtr_t*		mtr,	/* in: mtr holding the latch on rec; it will
				also hold the latch on purge_view */
	dict_index_t*	index,	/* in: the clustered index */
	read_view_t*	view,	/* in: the consistent read view */
	mem_heap_t*	in_heap,/* in: memory heap from which the memory for
				old_vers is allocated; memory for possible
				intermediate versions is allocated and freed
				locally within the function */
	rec_t**		old_vers);/* out, own: old version, or NULL if the
				record does not exist in the view, that is,
				it was freshly inserted afterwards */


#ifndef UNIV_NONINL
#include "row0vers.ic"
#endif

#endif 
