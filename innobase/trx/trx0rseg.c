/******************************************************
Rollback segment

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0rseg.h"

#ifdef UNIV_NONINL
#include "trx0rseg.ic"
#endif

#include "trx0undo.h"
#include "fut0lst.h"
#include "srv0srv.h"
#include "trx0purge.h"

/**********************************************************************
Looks for a rollback segment, based on the rollback segment id. */

trx_rseg_t*
trx_rseg_get_on_id(
/*===============*/
			/* out: rollback segment */
	ulint	id)	/* in: rollback segment id */
{
	trx_rseg_t*	rseg;

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);
	ut_ad(rseg);

	while (rseg->id != id) {
		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
		ut_ad(rseg);
	}

	return(rseg);
}

/********************************************************************
Creates a rollback segment header. This function is called only when
a new rollback segment is created in the database. */

ulint
trx_rseg_header_create(
/*===================*/
				/* out: page number of the created segment,
				FIL_NULL if fail */
	ulint	space,		/* in: space id */
	ulint	max_size,	/* in: max size in pages */
	ulint*	slot_no,	/* out: rseg id == slot number in trx sys */
	mtr_t*	mtr)		/* in: mtr */
{
	ulint		page_no;
	trx_rsegf_t*	rsegf;
	trx_sysf_t*	sys_header;
	ulint		i;
	page_t*		page;
	
	ut_ad(mtr);
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space),
							MTR_MEMO_X_LOCK));
	sys_header = trx_sysf_get(mtr);

	*slot_no = trx_sysf_rseg_find_free(mtr);

	if (*slot_no == ULINT_UNDEFINED) {

		return(FIL_NULL);
	}

	/* Allocate a new file segment for the rollback segment */
	page = fseg_create(space, 0, TRX_RSEG + TRX_RSEG_FSEG_HEADER, mtr);

	if (page == NULL) {
		/* No space left */

		return(FIL_NULL);
	}

	buf_page_dbg_add_level(page, SYNC_RSEG_HEADER_NEW);

	page_no = buf_frame_get_page_no(page);

	/* Get the rollback segment file page */
	rsegf = trx_rsegf_get_new(space, page_no, mtr);
					    
	/* Initialize max size field */
	mlog_write_ulint(rsegf + TRX_RSEG_MAX_SIZE, max_size, MLOG_4BYTES, mtr);
	
	/* Initialize the history list */

	mlog_write_ulint(rsegf + TRX_RSEG_HISTORY_SIZE, 0, MLOG_4BYTES, mtr);
	flst_init(rsegf + TRX_RSEG_HISTORY, mtr);

	/* Reset the undo log slots */
	for (i = 0; i < TRX_RSEG_N_SLOTS; i++) {

		trx_rsegf_set_nth_undo(rsegf, i, FIL_NULL, mtr);
	}

	/* Add the rollback segment info to the free slot in the trx system
	header */

	trx_sysf_rseg_set_space(sys_header, *slot_no, space, mtr);	
	trx_sysf_rseg_set_page_no(sys_header, *slot_no, page_no, mtr);

	return(page_no);
}

/***************************************************************************
Creates and initializes a rollback segment object. The values for the
fields are read from the header. The object is inserted to the rseg
list of the trx system object and a pointer is inserted in the rseg
array in the trx system object. */
static
trx_rseg_t*
trx_rseg_mem_create(
/*================*/
				/* out, own: rollback segment object */
	ulint	id,		/* in: rollback segment id */
	ulint	space,		/* in: space where the segment placed */
	ulint	page_no,	/* in: page number of the segment header */
	mtr_t*	mtr)		/* in: mtr */
{
	trx_rsegf_t*	rseg_header;
	trx_rseg_t*	rseg;
	trx_ulogf_t*	undo_log_hdr;
	fil_addr_t	node_addr;
	ulint		sum_of_undo_sizes;

	ut_ad(mutex_own(&kernel_mutex));	

	rseg = mem_alloc(sizeof(trx_rseg_t));

	rseg->id = id;
	rseg->space = space;
	rseg->page_no = page_no;
	
	mutex_create(&(rseg->mutex));
	mutex_set_level(&(rseg->mutex), SYNC_RSEG);

	UT_LIST_ADD_LAST(rseg_list, trx_sys->rseg_list, rseg);

	trx_sys_set_nth_rseg(trx_sys, id, rseg);

	rseg_header = trx_rsegf_get_new(space, page_no, mtr);	

	rseg->max_size = mtr_read_ulint(rseg_header + TRX_RSEG_MAX_SIZE,
							MLOG_4BYTES, mtr);

	/* Initialize the undo log lists according to the rseg header */

	sum_of_undo_sizes = trx_undo_lists_init(rseg);

	rseg->curr_size = mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
							MLOG_4BYTES, mtr)
			  + 1 + sum_of_undo_sizes;

	if (flst_get_len(rseg_header + TRX_RSEG_HISTORY, mtr) > 0) {

		node_addr = trx_purge_get_log_from_hist(
			      flst_get_last(rseg_header + TRX_RSEG_HISTORY,
									mtr));
		rseg->last_page_no = node_addr.page;
		rseg->last_offset = node_addr.boffset;

		undo_log_hdr = trx_undo_page_get(rseg->space, node_addr.page,
									mtr)
			       + node_addr.boffset;
			       
		rseg->last_trx_no = mtr_read_dulint(
					undo_log_hdr + TRX_UNDO_TRX_NO,
					MLOG_8BYTES, mtr);
		rseg->last_del_marks = mtr_read_ulint(
					undo_log_hdr + TRX_UNDO_DEL_MARKS,
					MLOG_2BYTES, mtr);
	} else {
		rseg->last_page_no = FIL_NULL;
	}

	return(rseg);
}

/*************************************************************************
Creates the memory copies for rollback segments and initializes the
rseg list and array in trx_sys at a database startup. */

void
trx_rseg_list_and_array_init(
/*=========================*/
	trx_sysf_t*	sys_header,	/* in: trx system header */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	i;
	ulint	page_no;
	ulint	space;

	UT_LIST_INIT(trx_sys->rseg_list);

	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, mtr);

		if (page_no == FIL_NULL) {

			trx_sys_set_nth_rseg(trx_sys, i, NULL);
		} else {
			space = trx_sysf_rseg_get_space(sys_header, i, mtr);

			trx_rseg_mem_create(i, space, page_no, mtr);
		}
	}
}

/********************************************************************
Creates a new rollback segment to the database. */

trx_rseg_t*
trx_rseg_create(
/*============*/
				/* out: the created segment object, NULL if
				fail */
	ulint	space,		/* in: space id */
	ulint	max_size,	/* in: max size in pages */
	ulint*	id,		/* out: rseg id */
	mtr_t*	mtr)		/* in: mtr */
{
	ulint		page_no;
	trx_rseg_t*	rseg;

	mtr_x_lock(fil_space_get_latch(space), mtr);	
	mutex_enter(&kernel_mutex);

	page_no = trx_rseg_header_create(space, max_size, id, mtr);

	if (page_no == FIL_NULL) {

		mutex_exit(&kernel_mutex);
		return(NULL);
	}

	rseg = trx_rseg_mem_create(*id, space, page_no, mtr);

	mutex_exit(&kernel_mutex);

	return(rseg);
}
