/******************************************************
Transaction system

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0sys.h"

#ifdef UNIV_NONINL
#include "trx0sys.ic"
#endif

#include "fsp0fsp.h"
#include "mtr0mtr.h"
#include "trx0trx.h"
#include "trx0rseg.h"
#include "trx0undo.h"
#include "srv0srv.h"
#include "trx0purge.h"

/* The transaction system */
trx_sys_t*	trx_sys 	= NULL;

/********************************************************************
Checks that trx is in the trx list. */

ibool
trx_in_trx_list(
/*============*/
			/* out: TRUE if is in */
	trx_t*	in_trx)	/* in: trx */
{
	trx_t*	trx;

	ut_ad(mutex_own(&(kernel_mutex)));

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx != NULL) {

		if (trx == in_trx) {

			return(TRUE);
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	return(FALSE);
}

/*********************************************************************
Writes the value of max_trx_id to the file based trx system header. */

void
trx_sys_flush_max_trx_id(void)
/*==========================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;

	ut_ad(mutex_own(&kernel_mutex));

	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	mlog_write_dulint(sys_header + TRX_SYS_TRX_ID_STORE,
				trx_sys->max_trx_id, MLOG_8BYTES, &mtr);
	mtr_commit(&mtr);
}

/********************************************************************
Looks for a free slot for a rollback segment in the trx system file copy. */

ulint
trx_sysf_rseg_find_free(
/*====================*/
			/* out: slot index or ULINT_UNDEFINED if not found */
	mtr_t*	mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		page_no;
	ulint		i;
	
	ut_ad(mutex_own(&(kernel_mutex)));

	sys_header = trx_sysf_get(mtr);

	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, mtr);

		if (page_no == FIL_NULL) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}
	
/*********************************************************************
Creates the file page for the transaction system. This function is called only
at the database creation, before trx_sys_init. */
static
void
trx_sysf_create(
/*============*/
	mtr_t*	mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		slot_no;
	page_t*		page;
	ulint		page_no;
	ulint		i;
	
	ut_ad(mtr);

	/* Note that below we first reserve the file space x-latch, and
	then enter the kernel: we must do it in this order to conform
	to the latching order rules. */

	mtr_x_lock(fil_space_get_latch(TRX_SYS_SPACE), mtr);
	mutex_enter(&kernel_mutex);

	/* Create the trx sys file block in a new allocated file segment */
	page = fseg_create(TRX_SYS_SPACE, 0, TRX_SYS + TRX_SYS_FSEG_HEADER,
					    				mtr);
	ut_a(buf_frame_get_page_no(page) == TRX_SYS_PAGE_NO);

	buf_page_dbg_add_level(page, SYNC_TRX_SYS_HEADER);

	sys_header = trx_sysf_get(mtr);

	/* Start counting transaction ids from number 1 up */
	mlog_write_dulint(sys_header + TRX_SYS_TRX_ID_STORE,
				ut_dulint_create(0, 1), MLOG_8BYTES, mtr);

	/* Reset the rollback segment slots */
	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {

		trx_sysf_rseg_set_page_no(sys_header, i, FIL_NULL, mtr);
	}

	/* Create the first rollback segment in the SYSTEM tablespace */
	page_no = trx_rseg_header_create(TRX_SYS_SPACE, ULINT_MAX, &slot_no,
									mtr);
	ut_a(slot_no == TRX_SYS_SYSTEM_RSEG_ID);
	ut_a(page_no != FIL_NULL);

	mutex_exit(&kernel_mutex);
}

/*********************************************************************
Creates and initializes the central memory structures for the transaction
system. This is called when the database is started. */

void
trx_sys_init_at_db_start(void)
/*==========================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;

	mtr_start(&mtr);
	
	ut_ad(trx_sys == NULL);

	mutex_enter(&kernel_mutex);

	trx_sys = mem_alloc(sizeof(trx_sys_t));
	
	sys_header = trx_sysf_get(&mtr);

	trx_rseg_list_and_array_init(sys_header, &mtr);
	
	trx_sys->latest_rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	/* VERY important: after the database is started, max_trx_id value is
	divisible by TRX_SYS_TRX_ID_WRITE_MARGIN, and the 'if' in
	trx_sys_get_new_trx_id will evaluate to TRUE when the function
	is first time called, and the value for trx id will be written
	to the disk-based header! Thus trx id values will not overlap when
	the database is repeatedly started! */

	trx_sys->max_trx_id = ut_dulint_add(
			      	ut_dulint_align_up(
					mtr_read_dulint(sys_header
						+ TRX_SYS_TRX_ID_STORE,
						MLOG_8BYTES, &mtr),
					TRX_SYS_TRX_ID_WRITE_MARGIN),
				2 * TRX_SYS_TRX_ID_WRITE_MARGIN);
				
	trx_lists_init_at_db_start();

	if (UT_LIST_GET_LEN(trx_sys->trx_list) > 0) {
		fprintf(stderr,
	"Innobase: %lu uncommitted transaction(s) which must be rolled back\n",
				UT_LIST_GET_LEN(trx_sys->trx_list));
	}

	UT_LIST_INIT(trx_sys->view_list);

	trx_purge_sys_create();

	mutex_exit(&kernel_mutex);

	mtr_commit(&mtr);
}

/*********************************************************************
Creates and initializes the transaction system at the database creation. */

void
trx_sys_create(void)
/*================*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	trx_sysf_create(&mtr);

	mtr_commit(&mtr);

	trx_sys_init_at_db_start();
}
