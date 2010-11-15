/*****************************************************************************

Copyright (c) 1996, 2010, Innobase Oy. All Rights Reserved.

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
#include "srv0start.h"
#include "os0thread.h"
#include "srv0mon.h"

/** Maximum allowable purge history length.  <=0 means 'infinite'. */
UNIV_INTERN ulong		srv_max_purge_lag = 0;

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
/* Key to register purge_sys_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	purge_sys_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/********************************************************************//**
Fetches the next undo log record from the history list to purge. It must be
released with the corresponding release function.
@return copy of an undo log record or pointer to trx_purge_dummy_rec,
if the whole undo log can skipped in purge; NULL if none left */
static
trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
	roll_ptr_t*	roll_ptr,	/*!< out: roll pointer to undo record */
	mem_heap_t*	heap);		/*!< in: memory heap where copied */

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

	return(!read_view_sees_trx_id(purge_sys->view, trx_id));
}

/****************************************************************//**
Builds a purge 'query' graph. The actual purge is performed by executing
this query graph.
@return	own: the query graph */
static
que_t*
trx_purge_graph_build(
/*==================*/
	trx_t*		trx,			/*!< in: transaction */
	ulint		n_purge_threads)	/*!< in: number of purge
						threads */
{
	ulint		i;
	mem_heap_t*	heap;
	que_fork_t*	fork;

	heap = mem_heap_create(512);
	fork = que_fork_create(NULL, NULL, QUE_FORK_PURGE, heap);
	fork->trx = trx;

	for (i = 0; i < n_purge_threads; ++i) {
		que_thr_t*	thr;

		thr = que_thr_create(fork, heap);

		thr->child = row_purge_node_create(thr, heap);
	}

	return(fork);
}

/********************************************************************//**
Creates the global purge system control structure and inits the history
mutex. */
UNIV_INTERN
void
trx_purge_sys_create(
/*=================*/
	ulint	n_purge_threads)	/*!< in: number of purge threads */
{
	purge_sys = mem_zalloc(sizeof(*purge_sys));

	rw_lock_create(trx_purge_latch_key,
		       &purge_sys->latch, SYNC_PURGE_LATCH);

	mutex_create(purge_sys_mutex_key,
		     &purge_sys->mutex, SYNC_PURGE_SYS);

	purge_sys->heap = mem_heap_create(256);

	/* Handle the case for the traditional mode. */
	if (n_purge_threads == 0) {
		n_purge_threads = 1;
	}

	purge_sys->sess = sess_open();

	purge_sys->trx = purge_sys->sess->trx;

	ut_a(purge_sys->trx->sess == purge_sys->sess);

	trx_mutex_enter(purge_sys->trx);

	/* A purge transaction is not a real transaction, we use a transaction
	here only because the query threads code requires it. It is otherwise
	quite unnecessary. We should get rid of it eventually. */
	purge_sys->trx->id = 0;
	purge_sys->trx->start_time = ut_time();
	purge_sys->trx->state = TRX_STATE_ACTIVE;

	trx_mutex_exit(purge_sys->trx);

	purge_sys->query = trx_purge_graph_build(
		purge_sys->trx, n_purge_threads);

	purge_sys->view = read_view_purge_open(purge_sys->heap);
}

/************************************************************************
Frees the global purge system control structure. */
UNIV_INTERN
void
trx_purge_sys_close(void)
/*======================*/
{
	que_graph_free(purge_sys->query);

	ut_a(purge_sys->trx->id == 0);

	purge_sys->sess->trx->state = TRX_STATE_NOT_STARTED;

	sess_close(purge_sys->sess);

	purge_sys->sess = NULL;

	if (purge_sys->view != NULL) {
		read_view_remove(purge_sys->view);

		purge_sys->view = NULL;
	}

	rw_lock_free(&purge_sys->latch);
	mutex_free(&purge_sys->mutex);

	mem_heap_free(purge_sys->heap);
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
	trx_rseg_t*	rseg;
	trx_rsegf_t*	rseg_header;
	trx_usegf_t*	seg_header;
	trx_ulogf_t*	undo_header;
	trx_upagef_t*	page_header;
	ulint		hist_size;

	undo = trx->update_undo;

	ut_ad(undo);

	rseg = undo->rseg;

	ut_ad(mutex_own(&(rseg->mutex)));

	rseg_header = trx_rsegf_get(rseg->space, rseg->zip_size,
				    rseg->page_no, mtr);

	undo_header = undo_page + undo->hdr_offset;
	seg_header  = undo_page + TRX_UNDO_SEG_HDR;
	page_header = undo_page + TRX_UNDO_PAGE_HDR;

	if (undo->state != TRX_UNDO_CACHED) {
		/* The undo log segment will not be reused */

		if (undo->id >= TRX_RSEG_N_SLOTS) {
			fprintf(stderr,
				"InnoDB: Error: undo->id is %lu\n",
				(ulong) undo->id);
			ut_error;
		}

		trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL, mtr);

		MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_USED);

		hist_size = mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
					   MLOG_4BYTES, mtr);
		ut_ad(undo->size == flst_get_len(
			      seg_header + TRX_UNDO_PAGE_LIST, mtr));

		mlog_write_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
				 hist_size + undo->size, MLOG_4BYTES, mtr);
	}

	/* Add the log as the first in the history list */
	flst_add_first(rseg_header + TRX_RSEG_HISTORY,
		       undo_header + TRX_UNDO_HISTORY_NODE, mtr);

#ifdef HAVE_ATOMIC_BUILTINS
	os_atomic_increment_ulint(&trx_sys->rseg_history_len, 1);
#else
	rw_lock_x_lock(&trx_sys->lock);

	++trx_sys->rseg_history_len;

	rw_lock_x_unlock(&trx_sys->lock);
#endif /* HAVE_ATOMIC_BUILTINS */

	/* Write the trx number to the undo log header */
	mlog_write_ull(undo_header + TRX_UNDO_TRX_NO, trx->no, mtr);
	/* Write information about delete markings to the undo log header */

	if (!undo->del_marks) {
		mlog_write_ulint(undo_header + TRX_UNDO_DEL_MARKS, FALSE,
				 MLOG_2BYTES, mtr);
	}

	if (rseg->last_page_no == FIL_NULL) {
		void*		ptr;
		rseg_queue_t	rseg_queue;

		rseg->last_page_no = undo->hdr_page_no;
		rseg->last_offset = undo->hdr_offset;
		rseg->last_trx_no = trx->no;
		rseg->last_del_marks = undo->del_marks;

		rseg_queue.rseg = rseg;
		rseg_queue.trx_no = rseg->last_trx_no;

		rw_lock_x_lock(&trx_sys->lock);

		ptr = ib_bh_push(trx_sys->ib_bh, &rseg_queue);
		ut_a(ptr);

		rw_lock_x_unlock(&trx_sys->lock);
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
	mtr_t		mtr;
	trx_rsegf_t*	rseg_hdr;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	ulint		seg_size;
	ulint		hist_size;
	ibool		marked		= FALSE;

	/*	fputs("Freeing an update undo log segment\n", stderr); */

	ut_ad(mutex_own(&(purge_sys->mutex)));

	for (;;) {
		page_t*	undo_page;

		mtr_start(&mtr);

		mutex_enter(&rseg->mutex);

		rseg_hdr = trx_rsegf_get(
			rseg->space, rseg->zip_size, rseg->page_no, &mtr);

		undo_page = trx_undo_page_get(
			rseg->space, rseg->zip_size, hdr_addr.page, &mtr);

		seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
		log_hdr = undo_page + hdr_addr.boffset;

		/* Mark the last undo log totally purged, so that if the
		system crashes, the tail of the undo log will not get accessed
		again. The list of pages in the undo log tail gets inconsistent
		during the freeing of the segment, and therefore purge should
		not try to access them again. */

		if (!marked) {
			mlog_write_ulint(
				log_hdr + TRX_UNDO_DEL_MARKS, FALSE,
				MLOG_2BYTES, &mtr);

			marked = TRUE;
		}

		if (fseg_free_step_not_header(
			seg_hdr + TRX_UNDO_FSEG_HEADER, &mtr)) {

			break;
		}

		mutex_exit(&rseg->mutex);
		mtr_commit(&mtr);
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

#ifdef HAVE_ATOMIC_BUILTINS
	os_atomic_decrement_ulint(&trx_sys->rseg_history_len, n_removed_logs);
#else
	rw_lock_x_lock(&trx_sys->lock);

	trx_sys->rseg_history_len -= n_removed_logs;

	rw_lock_x_unlock(&trx_sys->lock);
#endif /* HAVE_ATOMIC_BUILTINS */

	do {

		/* Here we assume that a file segment with just the header
		page can be freed in a few steps, so that the buffer pool
		is not flooded with bufferfixed pages: see the note in
		fsp0fsp.c. */

	} while(!fseg_free_step(seg_hdr + TRX_UNDO_FSEG_HEADER, &mtr));

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
	trx_rseg_t*		rseg,		/*!< in: rollback segment */
	const purge_iter_t*	limit)		/*!< in: truncate offset */
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

	ut_ad(mutex_own(&(purge_sys->mutex)));

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

	if (undo_trx_no >= limit->trx_no) {

		if (undo_trx_no == limit->trx_no) {

			trx_undo_truncate_start(
				rseg, rseg->space, hdr_addr.page,
				hdr_addr.boffset, limit->undo_no);
		}

#ifdef HAVE_ATOMIC_BUILTINS
		os_atomic_decrement_ulint(
			&trx_sys->rseg_history_len, n_removed_logs);
#else
		rw_lock_x_lock(&trx_sys->lock);

		trx_sys->rseg_history_len -= n_removed_logs;

		rw_lock_x_unlock(&trx_sys->lock);
#endif /* HAVE_ATOMIC_BUILTINS */

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
trx_purge_truncate_history(
/*========================*/
	purge_iter_t*		limit,		/*!< in: truncate limit */
	const read_view_t*	view)		/*!< in: purge view */
{
	ulint		i;

	ut_ad(purge_mutex_own());

	/* We play safe and set the truncate limit at most to the purge view
	low_limit number, though this is not necessary */

	if (limit->trx_no >= view->low_limit_no) {
		limit->trx_no = view->low_limit_no;
		limit->undo_no = 0;
	}

	ut_ad(limit->trx_no <= purge_sys->view->low_limit_no);

	for (i = 0; i < TRX_SYS_N_RSEGS; ++i) {
		trx_rseg_t*	rseg = trx_sys->rseg_array[i];

		if (rseg != NULL) {
			ut_a(rseg->id == i);
			trx_purge_truncate_rseg_history(rseg, limit);
		}
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
	void*		ptr;
	page_t*		undo_page;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	fil_addr_t	prev_log_addr;
	trx_id_t	trx_no;
	ibool		del_marks;
	mtr_t		mtr;
	rseg_queue_t	rseg_queue;

	ut_ad(mutex_own(&(purge_sys->mutex)));

	mutex_enter(&(rseg->mutex));

	ut_a(rseg->last_page_no != FIL_NULL);

	purge_sys->iter.trx_no = rseg->last_trx_no + 1;
	purge_sys->iter.undo_no = 0;
	purge_sys->next_stored = FALSE;

	mtr_start(&mtr);

	undo_page = trx_undo_page_get_s_latched(rseg->space, rseg->zip_size,
						rseg->last_page_no, &mtr);
	log_hdr = undo_page + rseg->last_offset;
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	/* Increase the purge page count by one for every handled log */

	purge_sys->n_pages_handled++;

	prev_log_addr = trx_purge_get_log_from_hist(
		flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));
	if (prev_log_addr.page == FIL_NULL) {
		/* No logs left in the history list */

		rseg->last_page_no = FIL_NULL;

		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);

		rw_lock_s_lock(&trx_sys->lock);

		/* Add debug code to track history list corruption reported
		on the MySQL mailing list on Nov 9, 2004. The fut0lst.c
		file-based list was corrupt. The prev node pointer was
		FIL_NULL, even though the list length was over 8 million nodes!
		We assume that purge truncates the history list in moderate
		size pieces, and if we here reach the head of the list, the
		list cannot be longer than 20 000 undo logs now. */

		if (trx_sys->rseg_history_len > 20000) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Warning: purge reached the"
				" head of the history list,\n"
				"InnoDB: but its length is still"
				" reported as %lu! Make a detailed bug\n"
				"InnoDB: report, and submit it"
				" to http://bugs.mysql.com\n",
				(ulong) trx_sys->rseg_history_len);
		}

		rw_lock_s_unlock(&trx_sys->lock);

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

	mutex_exit(&(rseg->mutex));

	rw_lock_x_lock(&trx_sys->lock);

	ptr = ib_bh_push(trx_sys->ib_bh, &rseg_queue);
	ut_a(ptr != NULL);

	rw_lock_x_unlock(&trx_sys->lock);
}

/***********************************************************************//**
Chooses the rollback segment with the smallest trx_id.
@return zip_size if log is for a compressed table */
static
ulint
trx_purge_get_rseg_with_min_trx_id(
/*===============================*/
	trx_purge_t*	purge_sys)		/*!< in/out: purge instance */

{
	ulint		zip_size = 0;
	trx_id_t	last_trx_no = IB_ULONGLONG_MAX;

	ut_ad(mutex_own(&(purge_sys->mutex)));

	rw_lock_s_lock(&trx_sys->lock);

	if (!ib_bh_is_empty(trx_sys->ib_bh)) {
		rseg_queue_t*	rseg_queue;

		rseg_queue = ib_bh_first(trx_sys->ib_bh);

		purge_sys->rseg = rseg_queue->rseg;
		last_trx_no = rseg_queue->trx_no;

		ib_bh_pop(trx_sys->ib_bh);

	} else {
		purge_sys->rseg = NULL;
	}

	rw_lock_s_unlock(&trx_sys->lock);

	if (purge_sys->rseg != NULL) {
		trx_rseg_t*	rseg = purge_sys->rseg;

		mutex_enter(&rseg->mutex);

		ut_a(rseg->last_page_no != FIL_NULL);

		/* We assume in purge of externally stored fields that space id == 0 */
		ut_a(rseg->space == 0);

		zip_size = rseg->zip_size;

		ut_a(last_trx_no == rseg->last_trx_no);

		purge_sys->iter.trx_no = rseg->last_trx_no;

		purge_sys->hdr_offset = rseg->last_offset;

		purge_sys->hdr_page_no = rseg->last_page_no;

		mutex_exit(&rseg->mutex);
	}

	return(zip_size);
}

/***********************************************************************//**
Position the purge sys "iterator" on the undo record to use for purging. */
static
void
trx_purge_read_undo_rec(
/*====================*/
	trx_purge_t*	purge_sys,		/*!< in, out: purge instance */
	ulint		zip_size)		/*!< in: block size or 0 */
{
	ulint		page_no;
	ulint		offset = 0;
	ib_uint64_t	undo_no = 0;

	if (purge_sys->rseg->last_del_marks) {
		mtr_t		mtr;
		trx_undo_rec_t*	undo_rec = NULL;

		mtr_start(&mtr);

		undo_rec = trx_undo_get_first_rec(
			0 /* System space id */,
			zip_size, purge_sys->hdr_page_no,
			purge_sys->hdr_offset, RW_S_LATCH,
			&mtr);

		if (undo_rec != NULL) {
			offset = page_offset(undo_rec);
			undo_no = trx_undo_rec_get_undo_no(undo_rec);
		       page_no = page_get_page_no(page_align(undo_rec));
		} else {
			page_no = purge_sys->hdr_page_no;
		}

		mtr_commit(&mtr);
	} else {
		page_no = purge_sys->hdr_page_no;
	}

	purge_sys->offset = offset;
	purge_sys->page_no = page_no;
	purge_sys->iter.undo_no = undo_no;

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

	ut_ad(mutex_own(&(purge_sys->mutex)));
	ut_ad(purge_sys->next_stored == FALSE);

	zip_size = trx_purge_get_rseg_with_min_trx_id(purge_sys);

	if (purge_sys->rseg != NULL) {
		trx_purge_read_undo_rec(purge_sys, zip_size);
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
	page_t*		undo_page;
	page_t*		page;
	ulint		offset;
	ulint		page_no;
	ulint		space;
	ulint		zip_size;
	mtr_t		mtr;

	ut_ad(mutex_own(&(purge_sys->mutex)));
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

	undo_page = trx_undo_page_get_s_latched(space, zip_size,
						page_no, &mtr);
	rec = undo_page + offset;

	rec2 = rec;

	for (;;) {
		ulint		type;
		trx_undo_rec_t*	next_rec;
		ulint		cmpl_info;

		/* Try first to find the next record which requires a purge
		operation from the same page of the same undo log */

		next_rec = trx_undo_page_get_next_rec(rec2,
						      purge_sys->hdr_page_no,
						      purge_sys->hdr_offset);
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

		undo_page = trx_undo_page_get_s_latched(
			space, zip_size, page_no, &mtr);

		rec = undo_page + offset;
	} else {
		page = page_align(rec2);

		purge_sys->offset = rec2 - page;
		purge_sys->page_no = page_get_page_no(page);
		purge_sys->iter.undo_no = trx_undo_rec_get_undo_no(rec2);

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
static
trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
	roll_ptr_t*	roll_ptr,	/*!< out: roll pointer to undo record */
	mem_heap_t*	heap)		/*!< in: memory heap where copied */
{
	const read_view_t*	view = purge_sys->view;
	const purge_iter_t	iter = purge_sys->iter;

	ut_ad(mutex_own(&purge_sys->mutex));

	if (!purge_sys->next_stored) {
		trx_purge_choose_next_log();

		if (!purge_sys->next_stored) {

			if (srv_print_thread_releases) {
				fprintf(stderr,
					"Purge: No logs left in the"
					" history list; pages handled %lu\n",
					(ulong) purge_sys->n_pages_handled);
			}

			return(NULL);
		}
	}

	if (iter.trx_no >= view->low_limit_no) {

		return(NULL);
	}

	/* fprintf(stderr, "Thread %lu purging trx %llu undo record %llu\n",
	os_thread_get_curr_id(), iter->trx_no, iter->undo_no); */

	*roll_ptr = trx_undo_build_roll_ptr(
		FALSE, purge_sys->rseg->id,
		purge_sys->page_no, purge_sys->offset);

	ut_ad(iter.trx_no < view->low_limit_no);

	/* The following call will advance the stored values of the
	purge iterator. */

	return(trx_purge_get_next_rec(heap));
}

/***********************************************************//**
Fetches an undo log record into the purge nodes in the query graph. */
static
void
trx_purge_attach_undo_recs(
/*=======================*/
	ulint		n_purge_threads,/*!< number of purge threads */
	trx_purge_t*	purge_sys,	/*!< purge instance */
	purge_iter_t*	limit,		/*!< records read up to */
	ulint		batch_size)	/*!< no. of records to purge */
{
	que_thr_t*	thr;
	ulint		i = 0;
	ulint		n_thrs = UT_LIST_GET_LEN(purge_sys->query->thrs);

	ut_ad(mutex_own(&purge_sys->mutex));

	*limit = purge_sys->iter;

	/* Debug code to validate some pre-requisites. */
	for (thr = UT_LIST_GET_FIRST(purge_sys->query->thrs);
	     thr != NULL;
	     thr = UT_LIST_GET_NEXT(thrs, thr), ++i) {

		purge_node_t*		node;

		/* Get the purge node. */
		node = (purge_node_t*) thr->child;

		ut_a(que_node_get_type(node) == QUE_NODE_PURGE);
		ut_a(node->undo_recs == NULL);
		ut_a(node->done);

		node->done = FALSE;
	}

	ut_a(i == n_purge_threads || (n_purge_threads == 0 && i == 1));

	/* Fetch and parse the UNDO records. The UNDO records are added
	to a per purge node vector. */
	thr = UT_LIST_GET_FIRST(purge_sys->query->thrs);
	ut_a(n_thrs > 0 && thr != NULL);

	ut_ad(trx_purge_check_limit());

	do {
		purge_node_t*		node;
		trx_purge_rec_t*	purge_rec;
		const purge_iter_t	iter = purge_sys->iter;

		ut_a(!thr->is_active);

		/* Get the purge node. */
		node = (purge_node_t*) thr->child;
		ut_a(que_node_get_type(node) == QUE_NODE_PURGE);

		purge_rec = mem_heap_zalloc(node->heap, sizeof(*purge_rec));

		/* Track the max {trx_id, undo_no} for truncating the
		UNDO logs once we have purged the records. */

		if (iter.trx_no > limit->trx_no
		    || (iter.trx_no == limit->trx_no
			&& iter.undo_no >= limit->undo_no)) {

			*limit = iter;
		}

		/* Fetch the next record, and advance the purge_sys->iter. */
		purge_rec->undo_rec = trx_purge_fetch_next_rec(
			&purge_rec->roll_ptr, node->heap);

		if (purge_rec->undo_rec != NULL) {

			if (node->undo_recs == NULL) {
				node->undo_recs = ib_vector_create(
					node->heap, batch_size);
			} else {
				ut_a(!ib_vector_is_empty(node->undo_recs));
			}

			ib_vector_push(node->undo_recs, purge_rec);

			if (ib_vector_size(node->undo_recs) >= batch_size) {
				break;
			}
		} else {
			break;
		}

		thr = UT_LIST_GET_NEXT(thrs, thr);

		if (thr == NULL) {
			thr = UT_LIST_GET_FIRST(purge_sys->query->thrs);
		}
	} while (thr);

	ut_ad(trx_purge_check_limit());
}

/*******************************************************************//**
Calculate the DML delay required.
@return delay in microseconds or ULINT_MAX */
static
ulint
trx_purge_dml_delay(void)
/*=====================*/
{
	/* Determine how much data manipulation language (DML) statements
	need to be delayed in order to reduce the lagging of the purge
	thread. */
	ulint	delay = 0; /* in microseconds; default: no delay */

	/* If we cannot advance the 'purge view' because of an old
	'consistent read view', then the DML statements cannot be delayed.
	Also, srv_max_purge_lag <= 0 means 'infinity'. Note: we do a dirty
	read of the trx_sys_t data structure here. */
	if (srv_max_purge_lag > 0
	    && !UT_LIST_GET_LAST(trx_sys->view_list)) {
		float	ratio = (float) trx_sys->rseg_history_len
			/ srv_max_purge_lag;
		if (ratio > ULINT_MAX / 10000) {
			/* Avoid overflow: maximum delay is 4295 seconds */
			delay = ULINT_MAX;
		} else if (ratio > 1) {
			/* If the history list length exceeds the
			innodb_max_purge_lag, the
			data manipulation statements are delayed
			by at least 5000 microseconds. */
			delay = (ulint) ((ratio - .5) * 10000);
		}

		MONITOR_SET(MONITOR_DML_PURGE_DELAY, srv_dml_needed_delay);
	}

	return(delay);
}

/*******************************************************************//**
Wait for pending purge jobs to complete. */
static
void
trx_purge_wait_for_workers_to_complete(
/*===================================*/
	trx_purge_t*	purge_sys)	/*!< in: purge instance */ 
{
	/* Ensure that the work queue empties out. Note, we are doing
	a dirty read of purge_sys->n_completed and purge_sys->n_executing. */
	while ((purge_sys->n_submitted > purge_sys->n_completed
	       && srv_shutdown_state <= SRV_SHUTDOWN_CLEANUP)
	       || purge_sys->n_executing > 0) {

		srv_release_threads(SRV_WORKER, 1);

		/* This is an arbitrary choice. */
		os_thread_sleep(4000);
	}

	/* If shutdown is signalled then the worker threads can
	simply exit via os_event_wait(). The thread initiating the
	purge should be prepared to handle this case. */
	if (srv_shutdown_state <= SRV_SHUTDOWN_CLEANUP) {

		/* None of the worker threads can be executing work. */
		ut_a(purge_sys->n_executing == 0);

		/* There should be no outstanding tasks as long
		as the worker threads are active. */
		ut_a(srv_get_task_queue_length() == 0);
	}
}

/******************************************************************//**
Remove old historical changes from the rollback segments. */
static
void
trx_purge_truncate(void)
/*====================*/
{
	mutex_enter(&purge_sys->mutex);

	ut_ad(trx_purge_check_limit());

	if (purge_sys->limit.trx_no == 0) {
		trx_purge_truncate_history(&purge_sys->iter, purge_sys->view);
	} else {
		trx_purge_truncate_history(&purge_sys->limit, purge_sys->view);
	}

	mutex_exit(&purge_sys->mutex);
}

/*******************************************************************//**
This function runs a purge batch.
@return	number of undo log pages handled in the batch */
UNIV_INTERN
ulint
trx_purge(
/*======*/
	ulint	n_purge_threads,	/*!< in: number of purge tasks
					to submit to the work queue */
	ulint	batch_size)		/*!< in: the maximum number of records
					to purge in one batch */
{
	que_thr_t*	thr = NULL;
	ulint		n_pages_handled_start;

	srv_dml_needed_delay = trx_purge_dml_delay();

	mutex_enter(&purge_sys->mutex);

	/* The number of tasks submitted should be completed. */
	ut_a(purge_sys->n_submitted == purge_sys->n_completed);

	rw_lock_x_lock(&purge_sys->latch);

	read_view_remove(purge_sys->view);

	purge_sys->view = NULL;

	mem_heap_empty(purge_sys->heap);

	purge_sys->view = read_view_purge_open(purge_sys->heap);

	rw_lock_x_unlock(&purge_sys->latch);

	n_pages_handled_start = purge_sys->n_pages_handled;

	/* Fetch the UNDO recs that need to be purged. */
	trx_purge_attach_undo_recs(
		n_purge_threads, purge_sys, &purge_sys->limit, batch_size);

	mutex_exit(&purge_sys->mutex);

	/* Do we do an asynchronous purge or not ? */
	if (n_purge_threads > 0) {
		ulint	i = 0;

		/* Submit the tasks to the work queue. */
		for (i = 0; i < n_purge_threads - 1; ++i) {
			thr = que_fork_scheduler_round_robin(
				purge_sys->query, thr);

			ut_a(thr != NULL);

			srv_que_task_enqueue_low(thr);
		}

		thr = que_fork_scheduler_round_robin(purge_sys->query, thr);
		ut_a(thr != NULL);

		purge_sys->n_submitted += n_purge_threads - 1;

		goto run_synchronously;

	/* Do it synchronously. */
	} else {
		thr = que_fork_start_command(purge_sys->query);
		ut_ad(thr);

run_synchronously:
		++purge_sys->n_submitted;

		if (srv_print_thread_releases) {
			fputs("Starting purge\n", stderr);
		}

		que_run_threads(thr);

		os_atomic_inc_ulint(
			&purge_sys->mutex, &purge_sys->n_completed, 1);

		if (srv_print_thread_releases) {

			fprintf(stderr,
				"Purge ends; pages handled %lu\n",
				(ulong) purge_sys->n_pages_handled);
		}

		if (n_purge_threads > 1) {
			trx_purge_wait_for_workers_to_complete(purge_sys);
		}
	}

	trx_purge_truncate();

	return(purge_sys->n_pages_handled - n_pages_handled_start);
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
		(ullint) purge_sys->limit.trx_no,
		(ullint) purge_sys->limit.undo_no);
	fprintf(stderr,
		"InnoDB: Purge next stored %lu, page_no %lu, offset %lu,\n"
		"InnoDB: Purge hdr_page_no %lu, hdr_offset %lu\n",
		(ulong) purge_sys->next_stored,
		(ulong) purge_sys->page_no,
		(ulong) purge_sys->offset,
		(ulong) purge_sys->hdr_page_no,
		(ulong) purge_sys->hdr_offset);
}

