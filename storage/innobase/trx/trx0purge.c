/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file trx/trx0purge.c
Purge old versions

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0purge.h"

#ifdef UNIV_NONINL
#include "trx0purge.ic"
#endif

#include "fsp0fsp.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "read0read.h"
#include "fut0fut.h"
#include "que0que.h"
#include "row0purge.h"
#include "row0upd.h"
#include "trx0rec.h"
#include "srv0srv.h"
#include "os0thread.h"

/** The global data structure coordinating a purge */
UNIV_INTERN trx_purge_t*	purge_sys = NULL;

/** A dummy undo record used as a return value when we have a whole undo log
which needs no purge */
UNIV_INTERN trx_undo_rec_t	trx_purge_dummy_rec;

#ifdef UNIV_PFS_RWLOCK
/* Key to register trx_purge_latch with performance schema */
UNIV_INTERN mysql_pfs_key_t	trx_purge_latch_key;
#endif /* UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
/* Key to register purge_sys_bh_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	purge_sys_bh_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/*****************************************************************//**
Checks if trx_id is >= purge_view: then it is guaranteed that its update
undo log still exists in the system.
@return TRUE if is sure that it is preserved, also if the function
returns FALSE, it is possible that the undo log still exists in the
system */
UNIV_INTERN
ibool
trx_purge_update_undo_must_exist(
/*=============================*/
	trx_id_t	trx_id)	/*!< in: transaction id */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	if (!read_view_sees_trx_id(purge_sys->view, trx_id)) {

		return(TRUE);
	}

	return(FALSE);
}

/*=================== PURGE RECORD ARRAY =============================*/

/*******************************************************************//**
Stores info of an undo log record during a purge.
@return	pointer to the storage cell */
static
trx_undo_inf_t*
trx_purge_arr_store_info(
/*=====================*/
	trx_id_t	trx_no,	/*!< in: transaction number */
	undo_no_t	undo_no)/*!< in: undo number */
{
	trx_undo_inf_t*	cell;
	trx_undo_arr_t*	arr;
	ulint		i;

	arr = purge_sys->arr;

	for (i = 0;; i++) {
		cell = trx_undo_arr_get_nth_info(arr, i);

		if (!(cell->in_use)) {
			/* Not in use, we may store here */
			cell->undo_no = undo_no;
			cell->trx_no = trx_no;
			cell->in_use = TRUE;

			arr->n_used++;

			return(cell);
		}
	}
}

/*******************************************************************//**
Removes info of an undo log record during a purge. */
UNIV_INLINE
void
trx_purge_arr_remove_info(
/*======================*/
	trx_undo_inf_t*	cell)	/*!< in: pointer to the storage cell */
{
	trx_undo_arr_t*	arr;

	arr = purge_sys->arr;

	cell->in_use = FALSE;

	ut_ad(arr->n_used > 0);

	arr->n_used--;
}

/*******************************************************************//**
Gets the biggest pair of a trx number and an undo number in a purge array. */
static
void
trx_purge_arr_get_biggest(
/*======================*/
	trx_undo_arr_t*	arr,	/*!< in: purge array */
	trx_id_t*	trx_no,	/*!< out: transaction number: 0
				if array is empty */
	undo_no_t*	undo_no)/*!< out: undo number */
{
	trx_undo_inf_t*	cell;
	trx_id_t	pair_trx_no;
	undo_no_t	pair_undo_no;
	ulint		i;
	ulint		n;

	n = arr->n_used;
	pair_trx_no = 0;
	pair_undo_no = 0;

	if (n) {
		for (i = 0;; i++) {
			cell = trx_undo_arr_get_nth_info(arr, i);

			if (!cell->in_use) {
				continue;
			}

			if ((cell->trx_no > pair_trx_no)
			    || ((cell->trx_no == pair_trx_no)
				&& cell->undo_no >= pair_undo_no)) {

				pair_trx_no = cell->trx_no;
				pair_undo_no = cell->undo_no;
			}

			if (!--n) {
				break;
			}
		}
	}

	*trx_no = pair_trx_no;
	*undo_no = pair_undo_no;
}

/****************************************************************//**
Builds a purge 'query' graph. The actual purge is performed by executing
this query graph.
@return	own: the query graph */
static
que_t*
trx_purge_graph_build(void)
/*=======================*/
{
	mem_heap_t*	heap;
	que_fork_t*	fork;
	que_thr_t*	thr;
	/*	que_thr_t*	thr2; */

	heap = mem_heap_create(512);
	fork = que_fork_create(NULL, NULL, QUE_FORK_PURGE, heap);
	fork->trx = purge_sys->trx;

	thr = que_thr_create(fork, heap);

	thr->child = row_purge_node_create(thr, heap);

	/*	thr2 = que_thr_create(fork, fork, heap);

	thr2->child = row_purge_node_create(fork, thr2, heap);	 */

	return(fork);
}

/********************************************************************//**
Creates the global purge system control structure and inits the history
mutex. */
UNIV_INTERN
void
trx_purge_sys_create(
/*=================*/
	ib_bh_t*	ib_bh)	/*!< in, own: UNDO log min binary heap */
{
	ut_ad(mutex_own(&kernel_mutex));

	purge_sys = mem_zalloc(sizeof(trx_purge_t));

	/* Take ownership of ib_bh, we are responsible for freeing it. */
	purge_sys->ib_bh = ib_bh;
	purge_sys->state = TRX_STOP_PURGE;

	purge_sys->n_pages_handled = 0;

	purge_sys->purge_trx_no = 0;
	purge_sys->purge_undo_no = 0;
	purge_sys->next_stored = FALSE;

	rw_lock_create(trx_purge_latch_key,
		       &purge_sys->latch, SYNC_PURGE_LATCH);

	mutex_create(
		purge_sys_bh_mutex_key, &purge_sys->bh_mutex,
		SYNC_PURGE_QUEUE);

	purge_sys->heap = mem_heap_create(256);

	purge_sys->arr = trx_undo_arr_create();

	purge_sys->sess = sess_open();

	purge_sys->trx = purge_sys->sess->trx;

	purge_sys->trx->is_purge = 1;

	ut_a(trx_start_low(purge_sys->trx, ULINT_UNDEFINED));

	purge_sys->query = trx_purge_graph_build();

	purge_sys->view = read_view_oldest_copy_or_open_new(0,
							    purge_sys->heap);
}

/************************************************************************
Frees the global purge system control structure. */
UNIV_INTERN
void
trx_purge_sys_close(void)
/*======================*/
{
	ut_ad(!mutex_own(&kernel_mutex));

	que_graph_free(purge_sys->query);

	ut_a(purge_sys->sess->trx->is_purge);
	purge_sys->sess->trx->conc_state = TRX_NOT_STARTED;
	sess_close(purge_sys->sess);
	purge_sys->sess = NULL;

	if (purge_sys->view != NULL) {
		/* Because acquiring the kernel mutex is a pre-condition
		of read_view_close(). We don't really need it here. */
		mutex_enter(&kernel_mutex);

		read_view_close(purge_sys->view);
		purge_sys->view = NULL;

		mutex_exit(&kernel_mutex);
	}

	trx_undo_arr_free(purge_sys->arr);

	rw_lock_free(&purge_sys->latch);
	mutex_free(&purge_sys->bh_mutex);

	mem_heap_free(purge_sys->heap);

	ib_bh_free(purge_sys->ib_bh);

	mem_free(purge_sys);

	purge_sys = NULL;
}

/*================ UNDO LOG HISTORY LIST =============================*/

/********************************************************************//**
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. */
UNIV_INTERN
void
trx_purge_add_update_undo_to_history(
/*=================================*/
	trx_t*	trx,		/*!< in: transaction */
	page_t*	undo_page,	/*!< in: update undo log header page,
				x-latched */
	mtr_t*	mtr)		/*!< in: mtr */
{
	trx_undo_t*	undo;
	trx_rsegf_t*	rseg_header;
	trx_ulogf_t*	undo_header;

	undo = trx->update_undo;

	ut_ad(undo);

	ut_ad(mutex_own(&undo->rseg->mutex));

	rseg_header = trx_rsegf_get(
		undo->rseg->space, undo->rseg->zip_size, undo->rseg->page_no,
		mtr);

	undo_header = undo_page + undo->hdr_offset;
	/* Add the log as the first in the history list */

	if (undo->state != TRX_UNDO_CACHED) {
		ulint		hist_size;
#ifdef UNIV_DEBUG
		trx_usegf_t*	seg_header = undo_page + TRX_UNDO_SEG_HDR;
#endif /* UNIV_DEBUG */

		/* The undo log segment will not be reused */

		if (UNIV_UNLIKELY(undo->id >= TRX_RSEG_N_SLOTS)) {
			fprintf(stderr,
				"InnoDB: Error: undo->id is %lu\n",
				(ulong) undo->id);
			ut_error;
		}

		trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL, mtr);

		hist_size = mtr_read_ulint(
			rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr);

		ut_ad(undo->size == flst_get_len(
			      seg_header + TRX_UNDO_PAGE_LIST, mtr));

		mlog_write_ulint(
			rseg_header + TRX_RSEG_HISTORY_SIZE,
			hist_size + undo->size, MLOG_4BYTES, mtr);
	}

	flst_add_first(
		rseg_header + TRX_RSEG_HISTORY,
		undo_header + TRX_UNDO_HISTORY_NODE, mtr);

	/* Write the trx number to the undo log header */

	mlog_write_ull(undo_header + TRX_UNDO_TRX_NO, trx->no, mtr);

	/* Write information about delete markings to the undo log header */

	if (!undo->del_marks) {
		mlog_write_ulint(
			undo_header + TRX_UNDO_DEL_MARKS, FALSE,
			MLOG_2BYTES, mtr);
	}

	if (undo->rseg->last_page_no == FIL_NULL) {
		undo->rseg->last_trx_no = trx->no;
		undo->rseg->last_offset = undo->hdr_offset;
		undo->rseg->last_page_no = undo->hdr_page_no;
		undo->rseg->last_del_marks = undo->del_marks;

		/* FIXME: Add a bin heap validate function to check that
		the rseg exists. */
	}

	mutex_enter(&kernel_mutex);
	trx_sys->rseg_history_len++;
	mutex_exit(&kernel_mutex);

	if (!(trx_sys->rseg_history_len % srv_purge_batch_size)) {
		/* Inform the purge thread that there is work to do. */
		srv_wake_purge_thread_if_not_active();
	}
}

/**********************************************************************//**
Frees an undo log segment which is in the history list. Cuts the end of the
history list at the youngest undo log in this segment. */
static
void
trx_purge_free_segment(
/*===================*/
	trx_rseg_t*	rseg,		/*!< in: rollback segment */
	fil_addr_t	hdr_addr,	/*!< in: the file address of log_hdr */
	ulint		n_removed_logs)	/*!< in: count of how many undo logs we
					will cut off from the end of the
					history list */
{
	page_t*		undo_page;
	trx_rsegf_t*	rseg_hdr;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	ibool		freed;
	ulint		seg_size;
	ulint		hist_size;
	ibool		marked		= FALSE;
	mtr_t		mtr;

	/*	fputs("Freeing an update undo log segment\n", stderr); */

loop:
	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));

	rseg_hdr = trx_rsegf_get(rseg->space, rseg->zip_size,
				 rseg->page_no, &mtr);

	undo_page = trx_undo_page_get(rseg->space, rseg->zip_size,
				      hdr_addr.page, &mtr);
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
	log_hdr = undo_page + hdr_addr.boffset;

	/* Mark the last undo log totally purged, so that if the system
	crashes, the tail of the undo log will not get accessed again. The
	list of pages in the undo log tail gets inconsistent during the
	freeing of the segment, and therefore purge should not try to access
	them again. */

	if (!marked) {
		mlog_write_ulint(log_hdr + TRX_UNDO_DEL_MARKS, FALSE,
				 MLOG_2BYTES, &mtr);
		marked = TRUE;
	}

	freed = fseg_free_step_not_header(seg_hdr + TRX_UNDO_FSEG_HEADER,
					  &mtr);
	if (!freed) {
		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);

		goto loop;
	}

	/* The page list may now be inconsistent, but the length field
	stored in the list base node tells us how big it was before we
	started the freeing. */

	seg_size = flst_get_len(seg_hdr + TRX_UNDO_PAGE_LIST, &mtr);

	/* We may free the undo log segment header page; it must be freed
	within the same mtr as the undo log header is removed from the
	history list: otherwise, in case of a database crash, the segment
	could become inaccessible garbage in the file space. */

	flst_cut_end(rseg_hdr + TRX_RSEG_HISTORY,
		     log_hdr + TRX_UNDO_HISTORY_NODE, n_removed_logs, &mtr);

	mutex_enter(&kernel_mutex);
	ut_ad(trx_sys->rseg_history_len >= n_removed_logs);
	trx_sys->rseg_history_len -= n_removed_logs;
	mutex_exit(&kernel_mutex);

	freed = FALSE;

	while (!freed) {
		/* Here we assume that a file segment with just the header
		page can be freed in a few steps, so that the buffer pool
		is not flooded with bufferfixed pages: see the note in
		fsp0fsp.c. */

		freed = fseg_free_step(seg_hdr + TRX_UNDO_FSEG_HEADER,
				       &mtr);
	}

	hist_size = mtr_read_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE,
				   MLOG_4BYTES, &mtr);
	ut_ad(hist_size >= seg_size);

	mlog_write_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE,
			 hist_size - seg_size, MLOG_4BYTES, &mtr);

	ut_ad(rseg->curr_size >= seg_size);

	rseg->curr_size -= seg_size;

	mutex_exit(&(rseg->mutex));

	mtr_commit(&mtr);
}

/********************************************************************//**
Removes unnecessary history data from a rollback segment. */
static
void
trx_purge_truncate_rseg_history(
/*============================*/
	trx_rseg_t*	rseg,		/*!< in: rollback segment */
	trx_id_t	limit_trx_no,	/*!< in: remove update undo logs whose
					trx number is < limit_trx_no */
	undo_no_t	limit_undo_no)	/*!< in: if transaction number is equal
					to limit_trx_no, truncate undo records
					with undo number < limit_undo_no */
{
	fil_addr_t	hdr_addr;
	fil_addr_t	prev_hdr_addr;
	trx_rsegf_t*	rseg_hdr;
	page_t*		undo_page;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	ulint		n_removed_logs	= 0;
	mtr_t		mtr;
	trx_id_t	undo_trx_no;

	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));

	rseg_hdr = trx_rsegf_get(rseg->space, rseg->zip_size,
				 rseg->page_no, &mtr);

	hdr_addr = trx_purge_get_log_from_hist(
		flst_get_last(rseg_hdr + TRX_RSEG_HISTORY, &mtr));
loop:
	if (hdr_addr.page == FIL_NULL) {

		mutex_exit(&(rseg->mutex));

		mtr_commit(&mtr);

		return;
	}

	undo_page = trx_undo_page_get(rseg->space, rseg->zip_size,
				      hdr_addr.page, &mtr);

	log_hdr = undo_page + hdr_addr.boffset;
	undo_trx_no = mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO);

	if (undo_trx_no >= limit_trx_no) {
		if (undo_trx_no == limit_trx_no) {
			trx_undo_truncate_start(rseg, rseg->space,
						hdr_addr.page,
						hdr_addr.boffset,
						limit_undo_no);
		}

		mutex_enter(&kernel_mutex);
		ut_a(trx_sys->rseg_history_len >= n_removed_logs);
		trx_sys->rseg_history_len -= n_removed_logs;
		mutex_exit(&kernel_mutex);

		flst_truncate_end(rseg_hdr + TRX_RSEG_HISTORY,
				  log_hdr + TRX_UNDO_HISTORY_NODE,
				  n_removed_logs, &mtr);

		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);

		return;
	}

	prev_hdr_addr = trx_purge_get_log_from_hist(
		flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));
	n_removed_logs++;

	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	if ((mach_read_from_2(seg_hdr + TRX_UNDO_STATE) == TRX_UNDO_TO_PURGE)
	    && (mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG) == 0)) {

		/* We can free the whole log segment */

		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);

		trx_purge_free_segment(rseg, hdr_addr, n_removed_logs);

		n_removed_logs = 0;
	} else {
		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);
	}

	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));

	rseg_hdr = trx_rsegf_get(rseg->space, rseg->zip_size,
				 rseg->page_no, &mtr);

	hdr_addr = prev_hdr_addr;

	goto loop;
}

/********************************************************************//**
Removes unnecessary history data from rollback segments. NOTE that when this
function is called, the caller must not have any latches on undo log pages! */
static
void
trx_purge_truncate_history(void)
/*============================*/
{
	trx_rseg_t*	rseg;
	trx_id_t	limit_trx_no;
	undo_no_t	limit_undo_no;

	trx_purge_arr_get_biggest(
		purge_sys->arr, &limit_trx_no, &limit_undo_no);

	if (limit_trx_no == 0) {

		limit_trx_no = purge_sys->purge_trx_no;
		limit_undo_no = purge_sys->purge_undo_no;
	}

	/* We play safe and set the truncate limit at most to the purge view
	low_limit number, though this is not necessary */

	if (limit_trx_no >= purge_sys->view->low_limit_no) {
		limit_trx_no = purge_sys->view->low_limit_no;
		limit_undo_no = 0;
	}

	ut_ad(limit_trx_no <= purge_sys->view->low_limit_no);

	for (rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);
	     rseg != NULL;
	     rseg = UT_LIST_GET_NEXT(rseg_list, rseg)) {

		trx_purge_truncate_rseg_history(
			rseg, limit_trx_no, limit_undo_no);
	}
}

/********************************************************************//**
Does a truncate if the purge array is empty. NOTE that when this function is
called, the caller must not have any latches on undo log pages! */
UNIV_INLINE
void
trx_purge_truncate_if_arr_empty(void)
/*=================================*/
{
	static ulint	count;

	if (!(++count % TRX_SYS_N_RSEGS) && purge_sys->arr->n_used == 0) {

		trx_purge_truncate_history();
	}
}

/***********************************************************************//**
Updates the last not yet purged history log info in rseg when we have purged
a whole undo log. Advances also purge_sys->purge_trx_no past the purged log. */
static
void
trx_purge_rseg_get_next_history_log(
/*================================*/
	trx_rseg_t*	rseg)	/*!< in: rollback segment */
{
	page_t*		undo_page;
	trx_ulogf_t*	log_hdr;
	fil_addr_t	prev_log_addr;
	trx_id_t	trx_no;
	ibool		del_marks;
	mtr_t		mtr;
	rseg_queue_t	rseg_queue;
	const void*	ptr;

	mutex_enter(&(rseg->mutex));

	ut_a(rseg->last_page_no != FIL_NULL);

	purge_sys->purge_trx_no = rseg->last_trx_no + 1;
	purge_sys->purge_undo_no = 0;
	purge_sys->next_stored = FALSE;

	mtr_start(&mtr);

	undo_page = trx_undo_page_get_s_latched(
		rseg->space, rseg->zip_size, rseg->last_page_no, &mtr);

	log_hdr = undo_page + rseg->last_offset;

	/* Increase the purge page count by one for every handled log */

	purge_sys->n_pages_handled++;

	prev_log_addr = trx_purge_get_log_from_hist(
		flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));

	if (prev_log_addr.page == FIL_NULL) {
		/* No logs left in the history list */

		rseg->last_page_no = FIL_NULL;

		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);

		mutex_enter(&kernel_mutex);

		/* Add debug code to track history list corruption reported
		on the MySQL mailing list on Nov 9, 2004. The fut0lst.c
		file-based list was corrupt. The prev node pointer was
		FIL_NULL, even though the list length was over 8 million nodes!
		We assume that purge truncates the history list in large
		size pieces, and if we here reach the head of the list, the
		list cannot be longer than 2000 000 undo logs now. */

		if (trx_sys->rseg_history_len > 2000000) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Warning: purge reached the"
				" head of the history list,\n"
				"InnoDB: but its length is still"
				" reported as %lu! Make a detailed bug\n"
				"InnoDB: report, and submit it"
				" to http://bugs.mysql.com\n",
				(ulong) trx_sys->rseg_history_len);
			ut_ad(0);
		}

		mutex_exit(&kernel_mutex);

		return;
	}

	mutex_exit(&(rseg->mutex));
	mtr_commit(&mtr);

	/* Read the trx number and del marks from the previous log header */
	mtr_start(&mtr);

	log_hdr = trx_undo_page_get_s_latched(rseg->space, rseg->zip_size,
					      prev_log_addr.page, &mtr)
		+ prev_log_addr.boffset;

	trx_no = mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO);

	del_marks = mach_read_from_2(log_hdr + TRX_UNDO_DEL_MARKS);

	mtr_commit(&mtr);

	mutex_enter(&(rseg->mutex));

	rseg->last_page_no = prev_log_addr.page;
	rseg->last_offset = prev_log_addr.boffset;
	rseg->last_trx_no = trx_no;
	rseg->last_del_marks = del_marks;

	rseg_queue.rseg = rseg;
	rseg_queue.trx_no = rseg->last_trx_no;

	/* Purge can also produce events, however these are already ordered
	in the rollback segment and any user generated event will be greater
	than the events that Purge produces. ie. Purge can never produce
	events from an empty rollback segment. */

	mutex_enter(&purge_sys->bh_mutex);

	ptr = ib_bh_push(purge_sys->ib_bh, &rseg_queue);
	ut_a(ptr != NULL);

	mutex_exit(&purge_sys->bh_mutex);

	mutex_exit(&(rseg->mutex));
}

/***********************************************************************//**
Chooses the rollback segment with the smallest trx_id.
@return zip_size if log is for a compressed table, ULINT_UNDEFINED if
	no rollback segments to purge, 0 for non compressed tables. */
static
ulint
trx_purge_get_rseg_with_min_trx_id(
/*===============================*/
	trx_purge_t*	purge_sys)		/*!< in/out: purge instance */

{
	ulint		zip_size = 0;

	mutex_enter(&purge_sys->bh_mutex);

	/* Only purge consumes events from the binary heap, user
	threads only produce the events. */

	if (!ib_bh_is_empty(purge_sys->ib_bh)) {
		trx_rseg_t*	rseg;

		rseg = ((rseg_queue_t*) ib_bh_first(purge_sys->ib_bh))->rseg;
		ib_bh_pop(purge_sys->ib_bh);

		mutex_exit(&purge_sys->bh_mutex);

		purge_sys->rseg = rseg;
	} else {
		mutex_exit(&purge_sys->bh_mutex);

		purge_sys->rseg = NULL;

		return(ULINT_UNDEFINED);
	}

	ut_a(purge_sys->rseg != NULL);

	mutex_enter(&purge_sys->rseg->mutex);

	ut_a(purge_sys->rseg->last_page_no != FIL_NULL);

	/* We assume in purge of externally stored fields
	that space id == 0 */
	ut_a(purge_sys->rseg->space == 0);

	zip_size = purge_sys->rseg->zip_size;

	ut_a(purge_sys->purge_trx_no <= purge_sys->rseg->last_trx_no);

	purge_sys->purge_trx_no = purge_sys->rseg->last_trx_no;

	purge_sys->hdr_offset = purge_sys->rseg->last_offset;

	purge_sys->hdr_page_no = purge_sys->rseg->last_page_no;

	mutex_exit(&purge_sys->rseg->mutex);

	return(zip_size);
}

/***********************************************************************//**
Position the purge sys "iterator" on the undo record to use for purging. */
static
void
trx_purge_read_undo_rec(
/*====================*/
	trx_purge_t*	purge_sys,		/*!< in/out: purge instance */
	ulint		zip_size)		/*!< in: block size or 0 */
{
	ulint		page_no;
	ulint		offset = 0;
	ib_uint64_t	undo_no = 0;

	purge_sys->hdr_offset = purge_sys->rseg->last_offset;
	page_no = purge_sys->hdr_page_no = purge_sys->rseg->last_page_no;

	if (purge_sys->rseg->last_del_marks) {
		mtr_t		mtr;
		trx_undo_rec_t*	undo_rec;

		mtr_start(&mtr);

		undo_rec = trx_undo_get_first_rec(
			0 /* System space id */, zip_size,
			purge_sys->hdr_page_no,
			purge_sys->hdr_offset, RW_S_LATCH, &mtr);

		if (undo_rec != NULL) {
			offset = page_offset(undo_rec);
			undo_no = trx_undo_rec_get_undo_no(undo_rec);
			page_no = page_get_page_no(page_align(undo_rec));
		}

		mtr_commit(&mtr);
	}

	purge_sys->offset = offset;
	purge_sys->page_no = page_no;
	purge_sys->purge_undo_no = undo_no;

	purge_sys->next_stored = TRUE;
}

/***********************************************************************//**
Chooses the next undo log to purge and updates the info in purge_sys. This
function is used to initialize purge_sys when the next record to purge is
not known, and also to update the purge system info on the next record when
purge has handled the whole undo log for a transaction. */
static
void
trx_purge_choose_next_log(void)
/*===========================*/
{
	ulint		zip_size;

	ut_ad(purge_sys->next_stored == FALSE);

	zip_size = trx_purge_get_rseg_with_min_trx_id(purge_sys);

	if (purge_sys->rseg != NULL) {

		trx_purge_read_undo_rec(purge_sys, zip_size);
	} else {
		/* There is nothing to do yet. */
		os_thread_yield();
	}
}

/***********************************************************************//**
Gets the next record to purge and updates the info in the purge system.
@return	copy of an undo log record or pointer to the dummy undo log record */
static
trx_undo_rec_t*
trx_purge_get_next_rec(
/*===================*/
	mem_heap_t*	heap)	/*!< in: memory heap where copied */
{
	trx_undo_rec_t*	rec;
	trx_undo_rec_t*	rec_copy;
	trx_undo_rec_t*	rec2;
	trx_undo_rec_t*	next_rec;
	page_t*		undo_page;
	page_t*		page;
	ulint		offset;
	ulint		page_no;
	ulint		space;
	ulint		zip_size;
	ulint		type;
	ulint		cmpl_info;
	mtr_t		mtr;

	ut_ad(purge_sys->next_stored);

	space = purge_sys->rseg->space;
	zip_size = purge_sys->rseg->zip_size;
	page_no = purge_sys->page_no;
	offset = purge_sys->offset;

	if (offset == 0) {
		/* It is the dummy undo log record, which means that there is
		no need to purge this undo log */

		trx_purge_rseg_get_next_history_log(purge_sys->rseg);

		/* Look for the next undo log and record to purge */

		trx_purge_choose_next_log();

		return(&trx_purge_dummy_rec);
	}

	mtr_start(&mtr);

	undo_page = trx_undo_page_get_s_latched(space, zip_size, page_no, &mtr);

	rec = undo_page + offset;

	rec2 = rec;

	for (;;) {
		/* Try first to find the next record which requires a purge
		operation from the same page of the same undo log */

		next_rec = trx_undo_page_get_next_rec(
			rec2, purge_sys->hdr_page_no, purge_sys->hdr_offset);

		if (next_rec == NULL) {
			rec2 = trx_undo_get_next_rec(
				rec2, purge_sys->hdr_page_no,
				purge_sys->hdr_offset, &mtr);
			break;
		}

		rec2 = next_rec;

		type = trx_undo_rec_get_type(rec2);

		if (type == TRX_UNDO_DEL_MARK_REC) {

			break;
		}

		cmpl_info = trx_undo_rec_get_cmpl_info(rec2);

		if (trx_undo_rec_get_extern_storage(rec2)) {
			break;
		}

		if ((type == TRX_UNDO_UPD_EXIST_REC)
		    && !(cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
			break;
		}
	}

	if (rec2 == NULL) {
		mtr_commit(&mtr);

		trx_purge_rseg_get_next_history_log(purge_sys->rseg);

		/* Look for the next undo log and record to purge */

		trx_purge_choose_next_log();

		mtr_start(&mtr);

		undo_page = trx_undo_page_get_s_latched(space, zip_size,
							page_no, &mtr);

		rec = undo_page + offset;
	} else {
		page = page_align(rec2);

		purge_sys->purge_undo_no = trx_undo_rec_get_undo_no(rec2);
		purge_sys->page_no = page_get_page_no(page);
		purge_sys->offset = rec2 - page;

		if (undo_page != page) {
			/* We advance to a new page of the undo log: */
			purge_sys->n_pages_handled++;
		}
	}

	rec_copy = trx_undo_rec_copy(rec, heap);

	mtr_commit(&mtr);

	return(rec_copy);
}

/********************************************************************//**
Fetches the next undo log record from the history list to purge. It must be
released with the corresponding release function.
@return copy of an undo log record or pointer to trx_purge_dummy_rec,
if the whole undo log can skipped in purge; NULL if none left */
UNIV_INTERN
trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
	roll_ptr_t*	roll_ptr,/*!< out: roll pointer to undo record */
	trx_undo_inf_t** cell,	/*!< out: storage cell for the record in the
				purge array */
	mem_heap_t*	heap)	/*!< in: memory heap where copied */
{
	trx_undo_rec_t*	undo_rec;


	if (purge_sys->state == TRX_STOP_PURGE) {
		trx_purge_truncate_if_arr_empty();

		return(NULL);
	} else if (!purge_sys->next_stored) {
		trx_purge_choose_next_log();

		if (!purge_sys->next_stored) {
			purge_sys->state = TRX_STOP_PURGE;

			trx_purge_truncate_if_arr_empty();

			if (srv_print_thread_releases) {
				fprintf(stderr,
					"Purge: No logs left in the"
					" history list; pages handled %lu\n",
					(ulong) purge_sys->n_pages_handled);
			}

			return(NULL);
		}
	}

	if (purge_sys->n_pages_handled >= purge_sys->handle_limit) {

		purge_sys->state = TRX_STOP_PURGE;

		trx_purge_truncate_if_arr_empty();

		return(NULL);
	} else if (purge_sys->purge_trx_no >= purge_sys->view->low_limit_no) {
		purge_sys->state = TRX_STOP_PURGE;

		trx_purge_truncate_if_arr_empty();

		return(NULL);
	}

	/* fprintf(stderr, "Thread %lu purging trx %llu undo record %llu\n",
	os_thread_get_curr_id(),
	(ullint) purge_sys->purge_trx_no,
	(ullint) purge_sys->purge_undo_no); */


	*roll_ptr = trx_undo_build_roll_ptr(
		FALSE, (purge_sys->rseg)->id, purge_sys->page_no,
		purge_sys->offset);

	*cell = trx_purge_arr_store_info(
		purge_sys->purge_trx_no, purge_sys->purge_undo_no);

	ut_ad(purge_sys->purge_trx_no < purge_sys->view->low_limit_no);

	/* The following call will advance the stored values of purge_trx_no
	and purge_undo_no, therefore we had to store them first */

	undo_rec = trx_purge_get_next_rec(heap);

	return(undo_rec);
}

/*******************************************************************//**
Releases a reserved purge undo record. */
UNIV_INTERN
void
trx_purge_rec_release(
/*==================*/
	trx_undo_inf_t*	cell)	/*!< in: storage cell */
{
	trx_purge_arr_remove_info(cell);
}

/*******************************************************************//**
This function runs a purge batch.
@return	number of undo log pages handled in the batch */
UNIV_INTERN
ulint
trx_purge(
/*======*/
	ulint	limit)		/*!< in: the maximum number of records to
				purge in one batch */
{
	que_thr_t*	thr;
	ulint		old_pages_handled;

	ut_a(purge_sys->trx->n_active_thrs == 0);

	rw_lock_x_lock(&purge_sys->latch);

	mutex_enter(&kernel_mutex);

	/* Close and free the old purge view */

	read_view_close(purge_sys->view);
	purge_sys->view = NULL;
	mem_heap_empty(purge_sys->heap);

	/* Determine how much data manipulation language (DML) statements
	need to be delayed in order to reduce the lagging of the purge
	thread. */
	srv_dml_needed_delay = 0; /* in microseconds; default: no delay */

	/* If we cannot advance the 'purge view' because of an old
	'consistent read view', then the DML statements cannot be delayed.
	Also, srv_max_purge_lag <= 0 means 'infinity'. */
	if (srv_max_purge_lag > 0
	    && !UT_LIST_GET_LAST(trx_sys->view_list)) {
		float	ratio = (float) trx_sys->rseg_history_len
			/ srv_max_purge_lag;
		if (ratio > ULINT_MAX / 10000) {
			/* Avoid overflow: maximum delay is 4295 seconds */
			srv_dml_needed_delay = ULINT_MAX;
		} else if (ratio > 1) {
			/* If the history list length exceeds the
			innodb_max_purge_lag, the
			data manipulation statements are delayed
			by at least 5000 microseconds. */
			srv_dml_needed_delay = (ulint) ((ratio - .5) * 10000);
		}
	}

	purge_sys->view = read_view_oldest_copy_or_open_new(
		0, purge_sys->heap);

	mutex_exit(&kernel_mutex);

	rw_lock_x_unlock(&(purge_sys->latch));

	purge_sys->state = TRX_PURGE_ON;

	purge_sys->handle_limit = purge_sys->n_pages_handled + limit;

	old_pages_handled = purge_sys->n_pages_handled;


	mutex_enter(&kernel_mutex);

	thr = que_fork_start_command(purge_sys->query);

	ut_ad(thr);

	mutex_exit(&kernel_mutex);

	if (srv_print_thread_releases) {

		fputs("Starting purge\n", stderr);
	}

	que_run_threads(thr);

	if (srv_print_thread_releases) {

		fprintf(stderr,
			"Purge ends; pages handled %lu\n",
			(ulong) purge_sys->n_pages_handled);
	}

	return((ulint) (purge_sys->n_pages_handled - old_pages_handled));
}

/******************************************************************//**
Prints information of the purge system to stderr. */
UNIV_INTERN
void
trx_purge_sys_print(void)
/*=====================*/
{
	fprintf(stderr, "InnoDB: Purge system view:\n");
	read_view_print(purge_sys->view);

	fprintf(stderr, "InnoDB: Purge trx n:o " TRX_ID_FMT
		", undo n:o " TRX_ID_FMT "\n",
		(ullint) purge_sys->purge_trx_no,
		(ullint) purge_sys->purge_undo_no);
	fprintf(stderr,
		"InnoDB: Purge next stored %lu, page_no %lu, offset %lu,\n"
		"InnoDB: Purge hdr_page_no %lu, hdr_offset %lu\n",
		(ulong) purge_sys->next_stored,
		(ulong) purge_sys->page_no,
		(ulong) purge_sys->offset,
		(ulong) purge_sys->hdr_page_no,
		(ulong) purge_sys->hdr_offset);
}
