/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file trx/trx0purge.cc
Purge old versions

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0purge.h"

#ifdef UNIV_NONINL
#include "trx0purge.ic"
#endif

#include "fsp0fsp.h"
#include "mach0data.h"
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
#include "mtr0log.h"

/** Maximum allowable purge history length.  <=0 means 'infinite'. */
UNIV_INTERN ulong		srv_max_purge_lag = 0;

/** Max DML user threads delay in micro-seconds. */
UNIV_INTERN ulong		srv_max_purge_lag_delay = 0;

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
	ulint		n_purge_threads,	/*!< in: number of purge
						threads */
	ib_bh_t*	ib_bh)			/*!< in, own: UNDO log min
						binary heap */
{
	purge_sys = static_cast<trx_purge_t*>(mem_zalloc(sizeof(*purge_sys)));

	purge_sys->state = PURGE_STATE_INIT;
	purge_sys->event = os_event_create("purge");

	/* Take ownership of ib_bh, we are responsible for freeing it. */
	purge_sys->ib_bh = ib_bh;

	rw_lock_create(trx_purge_latch_key,
		       &purge_sys->latch, SYNC_PURGE_LATCH);

	mutex_create("purge_sys_bh", &purge_sys->bh_mutex);

	purge_sys->heap = mem_heap_create(256);

	ut_a(n_purge_threads > 0);

	purge_sys->sess = sess_open();

	purge_sys->trx = purge_sys->sess->trx;

	ut_a(purge_sys->trx->sess == purge_sys->sess);

	/* A purge transaction is not a real transaction, we use a transaction
	here only because the query threads code requires it. It is otherwise
	quite unnecessary. We should get rid of it eventually. */
	purge_sys->trx->id = 0;
	purge_sys->trx->start_time = ut_time();
	purge_sys->trx->state = TRX_STATE_ACTIVE;
	purge_sys->trx->op_info = "purge trx";

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
	ut_a(purge_sys->sess->trx == purge_sys->trx);

	purge_sys->trx->state = TRX_STATE_NOT_STARTED;

	sess_close(purge_sys->sess);

	purge_sys->sess = NULL;

	purge_sys->view = NULL;

	rw_lock_free(&purge_sys->latch);
	mutex_free(&purge_sys->bh_mutex);

	mem_heap_free(purge_sys->heap);

	ib_bh_free(purge_sys->ib_bh);

	os_event_destroy(purge_sys->event);

	purge_sys->event = NULL;

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
	trx_ulogf_t*	undo_header;

	undo = trx->update_undo;
	rseg = undo->rseg;

	rseg_header = trx_rsegf_get(
		undo->rseg->space, undo->rseg->zip_size, undo->rseg->page_no,
		mtr);

	undo_header = undo_page + undo->hdr_offset;

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

		MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_USED);

		hist_size = mtr_read_ulint(
			rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr);

		ut_ad(undo->size == flst_get_len(
			      seg_header + TRX_UNDO_PAGE_LIST, mtr));

		mlog_write_ulint(
			rseg_header + TRX_RSEG_HISTORY_SIZE,
			hist_size + undo->size, MLOG_4BYTES, mtr);
	}

	/* Add the log as the first in the history list */
	flst_add_first(rseg_header + TRX_RSEG_HISTORY,
		       undo_header + TRX_UNDO_HISTORY_NODE, mtr);

#ifdef HAVE_ATOMIC_BUILTINS
	os_atomic_increment_ulint(&trx_sys->rseg_history_len, 1);
#else
	trx_sys_mutex_enter();
	++trx_sys->rseg_history_len;
	trx_sys_mutex_exit();
#endif /* HAVE_ATOMIC_BUILTINS */

	srv_wake_purge_thread_if_not_active();

	/* Write the trx number to the undo log header */
	mlog_write_ull(undo_header + TRX_UNDO_TRX_NO, trx->no, mtr);

	/* Write information about delete markings to the undo log header */

	if (!undo->del_marks) {
		mlog_write_ulint(undo_header + TRX_UNDO_DEL_MARKS, FALSE,
				 MLOG_2BYTES, mtr);
	}

	if (rseg->last_page_no == FIL_NULL) {
		rseg->last_page_no = undo->hdr_page_no;
		rseg->last_offset = undo->hdr_offset;
		rseg->last_trx_no = trx->no;
		rseg->last_del_marks = undo->del_marks;
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
	trx_sys_mutex_enter();
	trx_sys->rseg_history_len -= n_removed_logs;
	trx_sys_mutex_exit();
#endif /* HAVE_ATOMIC_BUILTINS */

	do {

		/* Here we assume that a file segment with just the header
		page can be freed in a few steps, so that the buffer pool
		is not flooded with bufferfixed pages: see the note in
		fsp0fsp.cc. */

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
		trx_sys_mutex_enter();
		trx_sys->rseg_history_len -= n_removed_logs;
		trx_sys_mutex_exit();
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
	trx_rseg_t*	rseg,		/*!< in: rollback segment */
	ulint*		n_pages_handled)/*!< in/out: number of UNDO pages
					handled */
{
	const void*	ptr;
	page_t*		undo_page;
	trx_ulogf_t*	log_hdr;
	fil_addr_t	prev_log_addr;
	trx_id_t	trx_no;
	ibool		del_marks;
	mtr_t		mtr;
	rseg_queue_t	rseg_queue;

	mutex_enter(&(rseg->mutex));

	ut_a(rseg->last_page_no != FIL_NULL);

	purge_sys->iter.trx_no = rseg->last_trx_no + 1;
	purge_sys->iter.undo_no = 0;
	purge_sys->next_stored = FALSE;

	mtr_start(&mtr);

	undo_page = trx_undo_page_get_s_latched(
		rseg->space, rseg->zip_size, rseg->last_page_no, &mtr);

	log_hdr = undo_page + rseg->last_offset;

	/* Increase the purge page count by one for every handled log */

	(*n_pages_handled)++;

	prev_log_addr = trx_purge_get_log_from_hist(
		flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));

	if (prev_log_addr.page == FIL_NULL) {
		/* No logs left in the history list */

		rseg->last_page_no = FIL_NULL;

		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);

		trx_sys_mutex_enter();

		/* Add debug code to track history list corruption reported
		on the MySQL mailing list on Nov 9, 2004. The fut0lst.cc
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

		trx_sys_mutex_exit();

		return;
	}

	mutex_exit(&rseg->mutex);

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

	mutex_exit(&rseg->mutex);
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

	/* We assume in purge of externally stored fields that space id is
	in the range of UNDO tablespace space ids */
	ut_a(purge_sys->rseg->space <= srv_undo_tablespaces_open);

	zip_size = purge_sys->rseg->zip_size;

	ut_a(purge_sys->iter.trx_no <= purge_sys->rseg->last_trx_no);

	purge_sys->iter.trx_no = purge_sys->rseg->last_trx_no;
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
	ulint		offset;
	ulint		page_no;
	ib_uint64_t	undo_no;

	purge_sys->hdr_offset = purge_sys->rseg->last_offset;
	page_no = purge_sys->hdr_page_no = purge_sys->rseg->last_page_no;

	if (purge_sys->rseg->last_del_marks) {
		mtr_t		mtr;
		trx_undo_rec_t*	undo_rec = NULL;

		mtr_start(&mtr);

		undo_rec = trx_undo_get_first_rec(
			purge_sys->rseg->space,
			zip_size,
			purge_sys->hdr_page_no,
			purge_sys->hdr_offset, RW_S_LATCH, &mtr);

		if (undo_rec != NULL) {
			offset = page_offset(undo_rec);
			undo_no = trx_undo_rec_get_undo_no(undo_rec);
			page_no = page_get_page_no(page_align(undo_rec));
		} else {
			offset = 0;
			undo_no = 0;
		}

		mtr_commit(&mtr);
	} else {
		offset = 0;
		undo_no = 0;
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
	ulint*		n_pages_handled,/*!< in/out: number of UNDO pages
					handled */
	mem_heap_t*	heap)		/*!< in: memory heap where copied */
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

	ut_ad(purge_sys->next_stored);
	ut_ad(purge_sys->iter.trx_no < purge_sys->view->low_limit_no);

	space = purge_sys->rseg->space;
	zip_size = purge_sys->rseg->zip_size;
	page_no = purge_sys->page_no;
	offset = purge_sys->offset;

	if (offset == 0) {
		/* It is the dummy undo log record, which means that there is
		no need to purge this undo log */

		trx_purge_rseg_get_next_history_log(
			purge_sys->rseg, n_pages_handled);

		/* Look for the next undo log and record to purge */

		trx_purge_choose_next_log();

		return(&trx_purge_dummy_rec);
	}

	mtr_start(&mtr);

	undo_page = trx_undo_page_get_s_latched(space, zip_size, page_no, &mtr);

	rec = undo_page + offset;

	rec2 = rec;

	for (;;) {
		ulint		type;
		trx_undo_rec_t*	next_rec;
		ulint		cmpl_info;

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

		trx_purge_rseg_get_next_history_log(
			purge_sys->rseg, n_pages_handled);

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
			(*n_pages_handled)++;
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
static __attribute__((warn_unused_result, nonnull))
trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
	roll_ptr_t*	roll_ptr,	/*!< out: roll pointer to undo record */
	ulint*		n_pages_handled,/*!< in/out: number of UNDO log pages
					handled */
	mem_heap_t*	heap)		/*!< in: memory heap where copied */
{
	if (!purge_sys->next_stored) {
		trx_purge_choose_next_log();

		if (!purge_sys->next_stored) {

			if (srv_print_thread_releases) {
				fprintf(stderr,
					"Purge: No logs left in the"
					" history list\n");
			}

			return(NULL);
		}
	}

	if (purge_sys->iter.trx_no >= purge_sys->view->low_limit_no) {

		return(NULL);
	}

	/* fprintf(stderr, "Thread %lu purging trx %llu undo record %llu\n",
	os_thread_get_curr_id(), iter->trx_no, iter->undo_no); */

	*roll_ptr = trx_undo_build_roll_ptr(
		FALSE, purge_sys->rseg->id,
		purge_sys->page_no, purge_sys->offset);

	/* The following call will advance the stored values of the
	purge iterator. */

	return(trx_purge_get_next_rec(n_pages_handled, heap));
}

/*******************************************************************//**
This function runs a purge batch.
@return	number of undo log pages handled in the batch */
static
ulint
trx_purge_attach_undo_recs(
/*=======================*/
	ulint		n_purge_threads,/*!< in: number of purge threads */
	trx_purge_t*	purge_sys,	/*!< in/out: purge instance */
	purge_iter_t*	limit,		/*!< out: records read up to */
	ulint		batch_size)	/*!< in: no. of pages to purge */
{
	que_thr_t*	thr;
	ulint		i = 0;
	ulint		n_pages_handled = 0;
	ulint		n_thrs = UT_LIST_GET_LEN(purge_sys->query->thrs);

	ut_a(n_purge_threads > 0);

	*limit = purge_sys->iter;

	/* Debug code to validate some pre-requisites and reset done flag. */
	for (thr = UT_LIST_GET_FIRST(purge_sys->query->thrs);
	     thr != NULL && i < n_purge_threads;
	     thr = UT_LIST_GET_NEXT(thrs, thr), ++i) {

		purge_node_t*		node;

		/* Get the purge node. */
		node = (purge_node_t*) thr->child;

		ut_a(que_node_get_type(node) == QUE_NODE_PURGE);
		ut_a(node->undo_recs == NULL);
		ut_a(node->done);

		node->done = FALSE;
	}

	/* There should never be fewer nodes than threads, the inverse
	however is allowed because we only use purge threads as needed. */
	ut_a(i == n_purge_threads);

	/* Fetch and parse the UNDO records. The UNDO records are added
	to a per purge node vector. */
	thr = UT_LIST_GET_FIRST(purge_sys->query->thrs);
	ut_a(n_thrs > 0 && thr != NULL);

	ut_ad(trx_purge_check_limit());

	i = 0;

	for (;;) {
		purge_node_t*		node;
		trx_purge_rec_t*	purge_rec;

		ut_a(!thr->is_active);

		/* Get the purge node. */
		node = (purge_node_t*) thr->child;
		ut_a(que_node_get_type(node) == QUE_NODE_PURGE);

		purge_rec = static_cast<trx_purge_rec_t*>(
			mem_heap_zalloc(node->heap, sizeof(*purge_rec)));

		/* Track the max {trx_id, undo_no} for truncating the
		UNDO logs once we have purged the records. */

		if (purge_sys->iter.trx_no > limit->trx_no
		    || (purge_sys->iter.trx_no == limit->trx_no
			&& purge_sys->iter.undo_no >= limit->undo_no)) {

			*limit = purge_sys->iter;
		}

		/* Fetch the next record, and advance the purge_sys->iter. */
		purge_rec->undo_rec = trx_purge_fetch_next_rec(
			&purge_rec->roll_ptr, &n_pages_handled, node->heap);

		if (purge_rec->undo_rec != NULL) {

			if (node->undo_recs == NULL) {
				node->undo_recs = ib_vector_create(
					ib_heap_allocator_create(node->heap),
					sizeof(trx_purge_rec_t),
					batch_size);
			} else {
				ut_a(!ib_vector_is_empty(node->undo_recs));
			}

			ib_vector_push(node->undo_recs, purge_rec);

			if (n_pages_handled >= batch_size) {

				break;
			}
		} else {
			break;
		}

		thr = UT_LIST_GET_NEXT(thrs, thr);

		if (!(++i % n_purge_threads)) {
			thr = UT_LIST_GET_FIRST(purge_sys->query->thrs);
		}

		ut_a(thr != NULL);
	}

	ut_ad(trx_purge_check_limit());

	return(n_pages_handled);
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

	/* If purge lag is set (ie. > 0) then calculate the new DML delay.
	Note: we do a dirty read of the trx_sys_t data structure here,
	without holding trx_sys->mutex. */

	if (srv_max_purge_lag > 0) {
		float	ratio;

		ratio = float(trx_sys->rseg_history_len) / srv_max_purge_lag;

		if (ratio > 1.0) {
			/* If the history list length exceeds the
			srv_max_purge_lag, the data manipulation
			statements are delayed by at least 5000
			microseconds. */
			delay = (ulint) ((ratio - .5) * 10000);
		}

		if (delay > srv_max_purge_lag_delay) {
			delay = srv_max_purge_lag_delay;
		}

		MONITOR_SET(MONITOR_DML_PURGE_DELAY, delay);
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
	ulint		n_submitted = purge_sys->n_submitted;

#ifdef HAVE_ATOMIC_BUILTINS
	/* Ensure that the work queue empties out. */
	while (!os_compare_and_swap_ulint(
			&purge_sys->n_completed, n_submitted, n_submitted)) {
#else
	mutex_enter(&purge_sys->bh_mutex);

	while (purge_sys->n_completed < n_submitted) {
#endif /* HAVE_ATOMIC_BUILTINS */

#ifndef HAVE_ATOMIC_BUILTINS
		mutex_exit(&purge_sys->bh_mutex);
#endif /* !HAVE_ATOMIC_BUILTINS */

		if (srv_get_task_queue_length() > 0) {
			srv_release_threads(SRV_WORKER, 1);
		}

		os_thread_yield();

#ifndef HAVE_ATOMIC_BUILTINS
		mutex_enter(&purge_sys->bh_mutex);
#endif /* !HAVE_ATOMIC_BUILTINS */
	}

#ifndef HAVE_ATOMIC_BUILTINS
	mutex_exit(&purge_sys->bh_mutex);
#endif /* !HAVE_ATOMIC_BUILTINS */

	/* None of the worker threads should be doing any work. */
	ut_a(purge_sys->n_submitted == purge_sys->n_completed);

	/* There should be no outstanding tasks as long
	as the worker threads are active. */
	ut_a(srv_get_task_queue_length() == 0);
}

/******************************************************************//**
Remove old historical changes from the rollback segments. */
static
void
trx_purge_truncate(void)
/*====================*/
{
	ut_ad(trx_purge_check_limit());

	if (purge_sys->limit.trx_no == 0) {
		trx_purge_truncate_history(&purge_sys->iter, purge_sys->view);
	} else {
		trx_purge_truncate_history(&purge_sys->limit, purge_sys->view);
	}
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
	ulint	batch_size,		/*!< in: the maximum number of records
					to purge in one batch */
	bool	truncate)		/*!< in: truncate history if true */
{
	que_thr_t*	thr = NULL;
	ulint		n_pages_handled;

	ut_a(n_purge_threads > 0);

	srv_dml_needed_delay = trx_purge_dml_delay();

	/* The number of tasks submitted should be completed. */
	ut_a(purge_sys->n_submitted == purge_sys->n_completed);

	rw_lock_x_lock(&purge_sys->latch);

	purge_sys->view = NULL;

	mem_heap_empty(purge_sys->heap);

	purge_sys->view = read_view_purge_open(purge_sys->heap);

	rw_lock_x_unlock(&purge_sys->latch);

	/* Fetch the UNDO recs that need to be purged. */
	n_pages_handled = trx_purge_attach_undo_recs(
		n_purge_threads, purge_sys, &purge_sys->limit, batch_size);

	/* Do we do an asynchronous purge or not ? */
	if (n_purge_threads > 1) {
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
		thr = que_fork_scheduler_round_robin(purge_sys->query, NULL);
		ut_ad(thr);

run_synchronously:
		++purge_sys->n_submitted;

		que_run_threads(thr);

		os_atomic_inc_ulint(
			&purge_sys->bh_mutex, &purge_sys->n_completed, 1);

		if (n_purge_threads > 1) {
			trx_purge_wait_for_workers_to_complete(purge_sys);
		}
	}

	ut_a(purge_sys->n_submitted == purge_sys->n_completed);

#ifdef UNIV_DEBUG
	if (purge_sys->limit.trx_no == 0) {
		purge_sys->done = purge_sys->iter;
	} else {
		purge_sys->done = purge_sys->limit;
	}
#endif /* UNIV_DEBUG */

	if (truncate) {
		trx_purge_truncate();
	}

	MONITOR_INC_VALUE(MONITOR_PURGE_INVOKED, 1);
	MONITOR_INC_VALUE(MONITOR_PURGE_N_PAGE_HANDLED, n_pages_handled);

	return(n_pages_handled);
}

/*******************************************************************//**
Get the purge state.
@return purge state. */
UNIV_INTERN
purge_state_t
trx_purge_state(void)
/*=================*/
{
	purge_state_t	state;

	rw_lock_x_lock(&purge_sys->latch);

	state = purge_sys->state;

	rw_lock_x_unlock(&purge_sys->latch);

	return(state);
}

/*******************************************************************//**
Stop purge and wait for it to stop, move to PURGE_STATE_STOP. */
UNIV_INTERN
void
trx_purge_stop(void)
/*================*/
{
	purge_state_t	state;
	ib_int64_t	sig_count = os_event_reset(purge_sys->event);

	ut_a(srv_n_purge_threads > 0);

	rw_lock_x_lock(&purge_sys->latch);

	ut_a(purge_sys->state != PURGE_STATE_INIT);
	ut_a(purge_sys->state != PURGE_STATE_EXIT);
	ut_a(purge_sys->state != PURGE_STATE_DISABLED);

	++purge_sys->n_stop;

	state = purge_sys->state;

	if (state == PURGE_STATE_RUN) {
		ib_logf(IB_LOG_LEVEL_INFO, "Stopping purge");

		/* We need to wakeup the purge thread in case it is suspended,
		so that it can acknowledge the state change. */

		srv_wake_purge_thread_if_not_active();
	}

	purge_sys->state = PURGE_STATE_STOP;

	rw_lock_x_unlock(&purge_sys->latch);

	if (state != PURGE_STATE_STOP) {

		/* Wait for purge coordinator to signal that it
		is suspended. */
		os_event_wait_low(purge_sys->event, sig_count);
	}

	MONITOR_INC_VALUE(MONITOR_PURGE_STOP_COUNT, 1);
}

/*******************************************************************//**
Resume purge, move to PURGE_STATE_RUN. */
UNIV_INTERN
void
trx_purge_run(void)
/*===============*/
{
	rw_lock_x_lock(&purge_sys->latch);

	switch(purge_sys->state) {
	case PURGE_STATE_INIT:
	case PURGE_STATE_EXIT:
	case PURGE_STATE_DISABLED:
		ut_error;

	case PURGE_STATE_RUN:
	case PURGE_STATE_STOP:
		break;
	}

	if (purge_sys->n_stop > 0) {

		ut_a(purge_sys->state == PURGE_STATE_STOP);

		--purge_sys->n_stop;

		if (purge_sys->n_stop == 0) {

			ib_logf(IB_LOG_LEVEL_INFO, "Resuming purge");

			purge_sys->state = PURGE_STATE_RUN;
		}

		MONITOR_INC_VALUE(MONITOR_PURGE_RESUME_COUNT, 1);
	} else {
		ut_a(purge_sys->state == PURGE_STATE_RUN);
	}

	rw_lock_x_unlock(&purge_sys->latch);

	srv_wake_purge_thread_if_not_active();
}
