/******************************************************
Purge old versions

(c) 1996 Innobase Oy

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
#include "srv0que.h"
#include "os0thread.h"

/* The global data structure coordinating a purge */
trx_purge_t*	purge_sys = NULL;

/* A dummy undo record used as a return value when we have a whole undo log
which needs no purge */
trx_undo_rec_t	trx_purge_dummy_rec;

/*********************************************************************
Checks if trx_id is >= purge_view: then it is guaranteed that its update
undo log still exists in the system. */

ibool
trx_purge_update_undo_must_exist(
/*=============================*/
			/* out: TRUE if is sure that it is preserved, also
			if the function returns FALSE, it is possible that
			the undo log still exists in the system */
	dulint	trx_id)	/* in: transaction id */
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

/***********************************************************************
Stores info of an undo log record during a purge. */
static
trx_undo_inf_t*
trx_purge_arr_store_info(
/*=====================*/
			/* out: pointer to the storage cell */
	dulint	trx_no,	/* in: transaction number */
	dulint	undo_no)/* in: undo number */
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

/***********************************************************************
Removes info of an undo log record during a purge. */
UNIV_INLINE
void
trx_purge_arr_remove_info(
/*======================*/
	trx_undo_inf_t*	cell)	/* in: pointer to the storage cell */
{
	trx_undo_arr_t*	arr;

	arr = purge_sys->arr;	

	cell->in_use = FALSE;
				
	ut_ad(arr->n_used > 0);

	arr->n_used--;
}

/***********************************************************************
Gets the biggest pair of a trx number and an undo number in a purge array. */
static
void
trx_purge_arr_get_biggest(
/*======================*/
	trx_undo_arr_t*	arr,	/* in: purge array */
	dulint*		trx_no,	/* out: transaction number: ut_dulint_zero
				if array is empty */
	dulint*		undo_no)/* out: undo number */
{
	trx_undo_inf_t*	cell;
	dulint		pair_trx_no;
	dulint		pair_undo_no;
	int		trx_cmp;
	ulint		n_used;
	ulint		i;
	ulint		n;
	
	n = 0;
	n_used = arr->n_used;
	pair_trx_no = ut_dulint_zero;
	pair_undo_no = ut_dulint_zero;
	
	for (i = 0;; i++) {
		cell = trx_undo_arr_get_nth_info(arr, i);

		if (cell->in_use) {
			n++;
 			trx_cmp = ut_dulint_cmp(cell->trx_no, pair_trx_no);

			if ((trx_cmp > 0)
			    || ((trx_cmp == 0)
			        && (ut_dulint_cmp(cell->undo_no,
						pair_undo_no) >= 0))) {
						
				pair_trx_no = cell->trx_no;
				pair_undo_no = cell->undo_no;
			}
		}

		if (n == n_used) {
			*trx_no = pair_trx_no;
			*undo_no = pair_undo_no;

			return;
		}
	}
}

/********************************************************************
Builds a purge 'query' graph. The actual purge is performed by executing
this query graph. */
static
que_t*
trx_purge_graph_build(void)
/*=======================*/
				/* out, own: the query graph */
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

	thr2->child = row_purge_node_create(fork, thr2, heap);   */

	return(fork);
}

/************************************************************************
Creates the global purge system control structure and inits the history
mutex. */

void
trx_purge_sys_create(void)
/*======================*/
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	purge_sys = mem_alloc(sizeof(trx_purge_t));

	purge_sys->state = TRX_STOP_PURGE;

	purge_sys->n_pages_handled = 0;

	purge_sys->purge_trx_no = ut_dulint_zero;
	purge_sys->purge_undo_no = ut_dulint_zero;
	purge_sys->next_stored = FALSE;
	
	rw_lock_create(&(purge_sys->latch));
	rw_lock_set_level(&(purge_sys->latch), SYNC_PURGE_LATCH);

	mutex_create(&(purge_sys->mutex));
	mutex_set_level(&(purge_sys->mutex), SYNC_PURGE_SYS);

	purge_sys->heap = mem_heap_create(256);

	purge_sys->arr = trx_undo_arr_create();

	purge_sys->sess = sess_open();

	purge_sys->trx = purge_sys->sess->trx;

	purge_sys->trx->type = TRX_PURGE;

	ut_a(trx_start_low(purge_sys->trx, ULINT_UNDEFINED));

	purge_sys->query = trx_purge_graph_build();
				
	purge_sys->view = read_view_oldest_copy_or_open_new(NULL,
							purge_sys->heap);
}

/*================ UNDO LOG HISTORY LIST =============================*/

/************************************************************************
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. */

void
trx_purge_add_update_undo_to_history(
/*=================================*/
	trx_t*	trx,		/* in: transaction */
	page_t*	undo_page,	/* in: update undo log header page,
				x-latched */
	mtr_t*	mtr)		/* in: mtr */
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
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	rseg_header = trx_rsegf_get(rseg->space, rseg->page_no, mtr);

	undo_header = undo_page + undo->hdr_offset;
	seg_header  = undo_page + TRX_UNDO_SEG_HDR;
	page_header = undo_page + TRX_UNDO_PAGE_HDR;
	
	if (undo->state != TRX_UNDO_CACHED) {
		/* The undo log segment will not be reused */

		if (undo->id >= TRX_RSEG_N_SLOTS) {
			fprintf(stderr,
			"InnoDB: Error: undo->id is %lu\n", (ulong) undo->id);
			ut_error;
		}

		trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL, mtr);

		hist_size = mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
							MLOG_4BYTES, mtr);
		ut_ad(undo->size ==
			flst_get_len(seg_header + TRX_UNDO_PAGE_LIST, mtr));

		mlog_write_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
				hist_size + undo->size, MLOG_4BYTES, mtr);
	}

	/* Add the log as the first in the history list */
	flst_add_first(rseg_header + TRX_RSEG_HISTORY,
				undo_header + TRX_UNDO_HISTORY_NODE, mtr);
	mutex_enter(&kernel_mutex);
	trx_sys->rseg_history_len++;
	mutex_exit(&kernel_mutex);

	/* Write the trx number to the undo log header */
	mlog_write_dulint(undo_header + TRX_UNDO_TRX_NO, trx->no, mtr);
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

/**************************************************************************
Frees an undo log segment which is in the history list. Cuts the end of the
history list at the youngest undo log in this segment. */
static
void
trx_purge_free_segment(
/*===================*/
	trx_rseg_t*	rseg,		/* in: rollback segment */
	fil_addr_t	hdr_addr,	/* in: the file address of log_hdr */
	ulint		n_removed_logs)	/* in: count of how many undo logs we
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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(purge_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
loop:	
	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));	
	
	rseg_hdr = trx_rsegf_get(rseg->space, rseg->page_no, &mtr);

	undo_page = trx_undo_page_get(rseg->space, hdr_addr.page, &mtr);
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

/************************************************************************
Removes unnecessary history data from a rollback segment. */
static
void
trx_purge_truncate_rseg_history(
/*============================*/
	trx_rseg_t*	rseg,		/* in: rollback segment */
	dulint		limit_trx_no,	/* in: remove update undo logs whose
					trx number is < limit_trx_no */
	dulint		limit_undo_no)	/* in: if transaction number is equal
					to limit_trx_no, truncate undo records
					with undo number < limit_undo_no */
{
	fil_addr_t	hdr_addr;
	fil_addr_t	prev_hdr_addr;
	trx_rsegf_t*	rseg_hdr;
	page_t*		undo_page;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	int		cmp;
	ulint		n_removed_logs	= 0;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(purge_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	mtr_start(&mtr);
	mutex_enter(&(rseg->mutex));	
	
	rseg_hdr = trx_rsegf_get(rseg->space, rseg->page_no, &mtr);

	hdr_addr = trx_purge_get_log_from_hist(
			flst_get_last(rseg_hdr + TRX_RSEG_HISTORY, &mtr));
loop:
	if (hdr_addr.page == FIL_NULL) {

		mutex_exit(&(rseg->mutex));	

		mtr_commit(&mtr);

		return;
	}

	undo_page = trx_undo_page_get(rseg->space, hdr_addr.page, &mtr);

	log_hdr = undo_page + hdr_addr.boffset;

 	cmp = ut_dulint_cmp(mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO),
 			  					limit_trx_no);
	if (cmp == 0) {
		trx_undo_truncate_start(rseg, rseg->space, hdr_addr.page,
					hdr_addr.boffset, limit_undo_no);
	}

	if (cmp >= 0) {
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
			  flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE,
									&mtr));
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

	rseg_hdr = trx_rsegf_get(rseg->space, rseg->page_no, &mtr);

	hdr_addr = prev_hdr_addr;
	
	goto loop;
}

/************************************************************************
Removes unnecessary history data from rollback segments. NOTE that when this
function is called, the caller must not have any latches on undo log pages! */
static
void
trx_purge_truncate_history(void)
/*============================*/
{
	trx_rseg_t*	rseg;
	dulint		limit_trx_no;
	dulint		limit_undo_no;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(purge_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	trx_purge_arr_get_biggest(purge_sys->arr, &limit_trx_no,
							&limit_undo_no);
	
	if (ut_dulint_cmp(limit_trx_no, ut_dulint_zero) == 0) {
		
		limit_trx_no = purge_sys->purge_trx_no;
		limit_undo_no = purge_sys->purge_undo_no;
	}

	/* We play safe and set the truncate limit at most to the purge view
	low_limit number, though this is not necessary */

	if (ut_dulint_cmp(limit_trx_no, purge_sys->view->low_limit_no) >= 0) {
		limit_trx_no = purge_sys->view->low_limit_no;
		limit_undo_no = ut_dulint_zero;
	}

	ut_ad((ut_dulint_cmp(limit_trx_no,
				purge_sys->view->low_limit_no) <= 0));

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	while (rseg) {
		trx_purge_truncate_rseg_history(rseg, limit_trx_no,
							limit_undo_no);
		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}
}

/************************************************************************
Does a truncate if the purge array is empty. NOTE that when this function is
called, the caller must not have any latches on undo log pages! */
UNIV_INLINE
ibool
trx_purge_truncate_if_arr_empty(void)
/*=================================*/
			/* out: TRUE if array empty */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(purge_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (purge_sys->arr->n_used == 0) {

		trx_purge_truncate_history();

		return(TRUE);
	}

	return(FALSE);
}

/***************************************************************************
Updates the last not yet purged history log info in rseg when we have purged
a whole undo log. Advances also purge_sys->purge_trx_no past the purged log. */
static 
void
trx_purge_rseg_get_next_history_log(
/*================================*/
	trx_rseg_t*	rseg)	/* in: rollback segment */
{
	page_t* 	undo_page;
	trx_ulogf_t*	log_hdr;
	trx_usegf_t*	seg_hdr;
	fil_addr_t	prev_log_addr;
	dulint		trx_no;
	ibool		del_marks;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(purge_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	mutex_enter(&(rseg->mutex));

	ut_a(rseg->last_page_no != FIL_NULL);

	purge_sys->purge_trx_no = ut_dulint_add(rseg->last_trx_no, 1);
	purge_sys->purge_undo_no = ut_dulint_zero;
	purge_sys->next_stored = FALSE;
	
	mtr_start(&mtr);
	
	undo_page = trx_undo_page_get_s_latched(rseg->space,
						rseg->last_page_no, &mtr);
	log_hdr = undo_page + rseg->last_offset;
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	/* Increase the purge page count by one for every handled log */

	purge_sys->n_pages_handled++;

	prev_log_addr = trx_purge_get_log_from_hist(
			 flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE,
									&mtr));
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
		We assume that purge truncates the history list in moderate
		size pieces, and if we here reach the head of the list, the
		list cannot be longer than 20 000 undo logs now. */
	
		if (trx_sys->rseg_history_len > 20000) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
"  InnoDB: Warning: purge reached the head of the history list,\n"
"InnoDB: but its length is still reported as %lu! Make a detailed bug\n"
"InnoDB: report, and post it to bugs.mysql.com\n",
					(ulong)trx_sys->rseg_history_len);
		}

		mutex_exit(&kernel_mutex);

		return;
	}

	mutex_exit(&(rseg->mutex));
	mtr_commit(&mtr);

	/* Read the trx number and del marks from the previous log header */
	mtr_start(&mtr);

	log_hdr = trx_undo_page_get_s_latched(rseg->space,
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

	mutex_exit(&(rseg->mutex));
}
	
/***************************************************************************
Chooses the next undo log to purge and updates the info in purge_sys. This
function is used to initialize purge_sys when the next record to purge is
not known, and also to update the purge system info on the next record when
purge has handled the whole undo log for a transaction. */
static 
void
trx_purge_choose_next_log(void)
/*===========================*/
{
	trx_undo_rec_t*	rec;
	trx_rseg_t*	rseg;
	trx_rseg_t*	min_rseg;
	dulint		min_trx_no;
	ulint		space = 0;   /* remove warning (??? bug ???) */
	ulint		page_no = 0; /* remove warning (??? bug ???) */
	ulint		offset = 0;  /* remove warning (??? bug ???) */
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(purge_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(purge_sys->next_stored == FALSE);

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	min_trx_no = ut_dulint_max;

	min_rseg = NULL;
	
	while (rseg) {
		mutex_enter(&(rseg->mutex));
		
		if (rseg->last_page_no != FIL_NULL) {

			if ((min_rseg == NULL)
			    || (ut_dulint_cmp(min_trx_no, rseg->last_trx_no)
			    	> 0)) {

				min_rseg = rseg;
				min_trx_no = rseg->last_trx_no;
				space = rseg->space;
				ut_a(space == 0); /* We assume in purge of
						externally stored fields
						that space id == 0 */
				page_no = rseg->last_page_no;
				offset = rseg->last_offset;
			}
		}

		mutex_exit(&(rseg->mutex));

		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}
	
	if (min_rseg == NULL) {

		return;
	}

	mtr_start(&mtr);

	if (!min_rseg->last_del_marks) {
		/* No need to purge this log */

		rec = &trx_purge_dummy_rec;
	} else {
		rec = trx_undo_get_first_rec(space, page_no, offset,
							RW_S_LATCH, &mtr);
		if (rec == NULL) {
			/* Undo log empty */

			rec = &trx_purge_dummy_rec;
		}
	}
	
	purge_sys->next_stored = TRUE;
	purge_sys->rseg = min_rseg;

	purge_sys->hdr_page_no = page_no;
	purge_sys->hdr_offset = offset;

	purge_sys->purge_trx_no = min_trx_no;

	if (rec == &trx_purge_dummy_rec) {

		purge_sys->purge_undo_no = ut_dulint_zero;
		purge_sys->page_no = page_no;
		purge_sys->offset = 0;
	} else {
		purge_sys->purge_undo_no = trx_undo_rec_get_undo_no(rec);

		purge_sys->page_no = buf_frame_get_page_no(rec);
		purge_sys->offset = rec - buf_frame_align(rec);
	}

	mtr_commit(&mtr);
}

/***************************************************************************
Gets the next record to purge and updates the info in the purge system. */
static
trx_undo_rec_t*
trx_purge_get_next_rec(
/*===================*/
				/* out: copy of an undo log record or
				pointer to the dummy undo log record */
	mem_heap_t*	heap)	/* in: memory heap where copied */
{
	trx_undo_rec_t*	rec;
	trx_undo_rec_t*	rec_copy;
	trx_undo_rec_t*	rec2;
	trx_undo_rec_t*	next_rec;
	page_t* 	undo_page;
	page_t* 	page;
	ulint		offset;
	ulint		page_no;
	ulint		space;
	ulint		type;
	ulint		cmpl_info;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(purge_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(purge_sys->next_stored);

	space = purge_sys->rseg->space;
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

	undo_page = trx_undo_page_get_s_latched(space, page_no, &mtr);
	rec = undo_page + offset;

	rec2 = rec;

	for (;;) {
		/* Try first to find the next record which requires a purge
		operation from the same page of the same undo log */
	
		next_rec = trx_undo_page_get_next_rec(rec2,
						purge_sys->hdr_page_no,
						purge_sys->hdr_offset);
		if (next_rec == NULL) {
			rec2 = trx_undo_get_next_rec(rec2,
						purge_sys->hdr_page_no,
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

		undo_page = trx_undo_page_get_s_latched(space, page_no, &mtr);

		rec = undo_page + offset;
	} else {
		page = buf_frame_align(rec2);
		
		purge_sys->purge_undo_no = trx_undo_rec_get_undo_no(rec2);
		purge_sys->page_no = buf_frame_get_page_no(page);
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

/************************************************************************
Fetches the next undo log record from the history list to purge. It must be
released with the corresponding release function. */

trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
				/* out: copy of an undo log record or
				pointer to the dummy undo log record
				&trx_purge_dummy_rec, if the whole undo log
				can skipped in purge; NULL if none left */
	dulint*		roll_ptr,/* out: roll pointer to undo record */
	trx_undo_inf_t** cell,	/* out: storage cell for the record in the
				purge array */
	mem_heap_t*	heap)	/* in: memory heap where copied */
{
	trx_undo_rec_t*	undo_rec;
	
	mutex_enter(&(purge_sys->mutex));

	if (purge_sys->state == TRX_STOP_PURGE) {
		trx_purge_truncate_if_arr_empty();

		mutex_exit(&(purge_sys->mutex));

		return(NULL);
	}

	if (!purge_sys->next_stored) {
		trx_purge_choose_next_log();

		if (!purge_sys->next_stored) {
			purge_sys->state = TRX_STOP_PURGE;
	
			trx_purge_truncate_if_arr_empty();

			if (srv_print_thread_releases) {
				fprintf(stderr,
	"Purge: No logs left in the history list; pages handled %lu\n",
					(ulong) purge_sys->n_pages_handled);
			}

			mutex_exit(&(purge_sys->mutex));

			return(NULL);
		}			
	}	

	if (purge_sys->n_pages_handled >= purge_sys->handle_limit) {

		purge_sys->state = TRX_STOP_PURGE;
	
		trx_purge_truncate_if_arr_empty();

		mutex_exit(&(purge_sys->mutex));

		return(NULL);
	}		

	if (ut_dulint_cmp(purge_sys->purge_trx_no,
				purge_sys->view->low_limit_no) >= 0) {
		purge_sys->state = TRX_STOP_PURGE;
	
		trx_purge_truncate_if_arr_empty();

		mutex_exit(&(purge_sys->mutex));

		return(NULL);
	}
		
/*	fprintf(stderr, "Thread %lu purging trx %lu undo record %lu\n",
		os_thread_get_curr_id(),
		ut_dulint_get_low(purge_sys->purge_trx_no),
		ut_dulint_get_low(purge_sys->purge_undo_no)); */

	*roll_ptr = trx_undo_build_roll_ptr(FALSE, (purge_sys->rseg)->id,
							purge_sys->page_no,
							purge_sys->offset);

	*cell = trx_purge_arr_store_info(purge_sys->purge_trx_no,
					 purge_sys->purge_undo_no);

	ut_ad(ut_dulint_cmp(purge_sys->purge_trx_no,
			    (purge_sys->view)->low_limit_no) < 0);
	
	/* The following call will advance the stored values of purge_trx_no
	and purge_undo_no, therefore we had to store them first */
	
	undo_rec = trx_purge_get_next_rec(heap);

	mutex_exit(&(purge_sys->mutex));

	return(undo_rec);
}

/***********************************************************************
Releases a reserved purge undo record. */

void
trx_purge_rec_release(
/*==================*/
	trx_undo_inf_t*	cell)	/* in: storage cell */
{
	trx_undo_arr_t*	arr;
	
	mutex_enter(&(purge_sys->mutex));

	arr = purge_sys->arr;

	trx_purge_arr_remove_info(cell);

	mutex_exit(&(purge_sys->mutex));
}

/***********************************************************************
This function runs a purge batch. */

ulint
trx_purge(void)
/*===========*/
				/* out: number of undo log pages handled in
				the batch */
{
	que_thr_t*	thr;
/*	que_thr_t*	thr2; */
	ulint		old_pages_handled;

	mutex_enter(&(purge_sys->mutex));

	if (purge_sys->trx->n_active_thrs > 0) {
	
		mutex_exit(&(purge_sys->mutex));

		/* Should not happen */

		ut_error;
		
		return(0);
	}		

	rw_lock_x_lock(&(purge_sys->latch));

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

	purge_sys->view = read_view_oldest_copy_or_open_new(NULL,
							purge_sys->heap);
	mutex_exit(&kernel_mutex);	

	rw_lock_x_unlock(&(purge_sys->latch));

	purge_sys->state = TRX_PURGE_ON;	
	
	/* Handle at most 20 undo log pages in one purge batch */

	purge_sys->handle_limit = purge_sys->n_pages_handled + 20;

	old_pages_handled = purge_sys->n_pages_handled;

	mutex_exit(&(purge_sys->mutex));

	mutex_enter(&kernel_mutex);

	thr = que_fork_start_command(purge_sys->query);

	ut_ad(thr);
	
/*	thr2 = que_fork_start_command(purge_sys->query);
	
	ut_ad(thr2); */
	

	mutex_exit(&kernel_mutex);

/*	srv_que_task_enqueue(thr2); */

	if (srv_print_thread_releases) {
	
		fputs("Starting purge\n", stderr);
	}

	que_run_threads(thr);

	if (srv_print_thread_releases) {

		fprintf(stderr,
		"Purge ends; pages handled %lu\n",
		(ulong) purge_sys->n_pages_handled);
	}

	return(purge_sys->n_pages_handled - old_pages_handled);
}

/**********************************************************************
Prints information of the purge system to stderr. */

void
trx_purge_sys_print(void)
/*=====================*/
{
	fprintf(stderr, "InnoDB: Purge system view:\n");
	read_view_print(purge_sys->view);

	fprintf(stderr, "InnoDB: Purge trx n:o %lu %lu, undo n_o %lu %lu\n",
			(ulong) ut_dulint_get_high(purge_sys->purge_trx_no),
			(ulong) ut_dulint_get_low(purge_sys->purge_trx_no),
			(ulong) ut_dulint_get_high(purge_sys->purge_undo_no),
			(ulong) ut_dulint_get_low(purge_sys->purge_undo_no));
	fprintf(stderr,
	"InnoDB: Purge next stored %lu, page_no %lu, offset %lu,\n"
	"InnoDB: Purge hdr_page_no %lu, hdr_offset %lu\n",
		(ulong) purge_sys->next_stored,
		(ulong) purge_sys->page_no,
		(ulong) purge_sys->offset,
		(ulong) purge_sys->hdr_page_no,
		(ulong) purge_sys->hdr_offset);
}
