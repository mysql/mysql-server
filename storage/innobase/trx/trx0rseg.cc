/*****************************************************************************

Copyright (c) 1996, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file trx/trx0rseg.cc
Rollback segment

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
#include "ut0bh.h"
#include "srv0mon.h"

#ifdef UNIV_PFS_MUTEX
/* Key to register rseg_mutex_key with performance schema */
UNIV_INTERN mysql_pfs_key_t	rseg_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/****************************************************************//**
Creates a rollback segment header. This function is called only when
a new rollback segment is created in the database.
@return	page number of the created segment, FIL_NULL if fail */
UNIV_INTERN
ulint
trx_rseg_header_create(
/*===================*/
	ulint	space,		/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	max_size,	/*!< in: max size in pages */
	ulint	rseg_slot_no,	/*!< in: rseg id == slot number in trx sys */
	mtr_t*	mtr)		/*!< in: mtr */
{
	ulint		page_no;
	trx_rsegf_t*	rsegf;
	trx_sysf_t*	sys_header;
	ulint		i;
	buf_block_t*	block;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space, NULL),
				MTR_MEMO_X_LOCK));

	/* Allocate a new file segment for the rollback segment */
	block = fseg_create(space, 0, TRX_RSEG + TRX_RSEG_FSEG_HEADER, mtr);

	if (block == NULL) {
		/* No space left */

		return(FIL_NULL);
	}

	buf_block_dbg_add_level(block, SYNC_RSEG_HEADER_NEW);

	page_no = buf_block_get_page_no(block);

	/* Get the rollback segment file page */
	rsegf = trx_rsegf_get_new(space, zip_size, page_no, mtr);

	/* Initialize max size field */
	mlog_write_ulint(rsegf + TRX_RSEG_MAX_SIZE, max_size,
			 MLOG_4BYTES, mtr);

	/* Initialize the history list */

	mlog_write_ulint(rsegf + TRX_RSEG_HISTORY_SIZE, 0, MLOG_4BYTES, mtr);
	flst_init(rsegf + TRX_RSEG_HISTORY, mtr);

	/* Reset the undo log slots */
	for (i = 0; i < TRX_RSEG_N_SLOTS; i++) {

		trx_rsegf_set_nth_undo(rsegf, i, FIL_NULL, mtr);
	}

	/* Add the rollback segment info to the free slot in
	the trx system header */

	sys_header = trx_sysf_get(mtr);

	trx_sysf_rseg_set_space(sys_header, rseg_slot_no, space, mtr);
	trx_sysf_rseg_set_page_no(sys_header, rseg_slot_no, page_no, mtr);

	return(page_no);
}

/***********************************************************************//**
Free's an instance of the rollback segment in memory. */
UNIV_INTERN
void
trx_rseg_mem_free(
/*==============*/
	trx_rseg_t*	rseg)	/* in, own: instance to free */
{
	trx_undo_t*	undo;
	trx_undo_t*	next_undo;

	mutex_free(&rseg->mutex);

	/* There can't be any active transactions. */
	ut_a(UT_LIST_GET_LEN(rseg->update_undo_list) == 0);
	ut_a(UT_LIST_GET_LEN(rseg->insert_undo_list) == 0);

	for (undo = UT_LIST_GET_FIRST(rseg->update_undo_cached);
	     undo != NULL;
	     undo = next_undo) {

		next_undo = UT_LIST_GET_NEXT(undo_list, undo);

		UT_LIST_REMOVE(undo_list, rseg->update_undo_cached, undo);

		MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);

		trx_undo_mem_free(undo);
	}

	for (undo = UT_LIST_GET_FIRST(rseg->insert_undo_cached);
	     undo != NULL;
	     undo = next_undo) {

		next_undo = UT_LIST_GET_NEXT(undo_list, undo);

		UT_LIST_REMOVE(undo_list, rseg->insert_undo_cached, undo);

		MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);

		trx_undo_mem_free(undo);
	}

	/* const_cast<trx_rseg_t*>() because this function is
	like a destructor.  */

	*((trx_rseg_t**) trx_sys->rseg_array + rseg->id) = NULL;

	mem_free(rseg);
}

/***************************************************************************
Creates and initializes a rollback segment object. The values for the
fields are read from the header. The object is inserted to the rseg
list of the trx system object and a pointer is inserted in the rseg
array in the trx system object.
@return	own: rollback segment object */
static
trx_rseg_t*
trx_rseg_mem_create(
/*================*/
	ulint		id,		/*!< in: rollback segment id */
	ulint		space,		/*!< in: space where the segment
					placed */
	ulint		zip_size,	/*!< in: compressed page size in bytes
					or 0 for uncompressed pages */
	ulint		page_no,	/*!< in: page number of the segment
					header */
	ib_bh_t*	ib_bh,		/*!< in/out: rseg queue */
	mtr_t*		mtr)		/*!< in: mtr */
{
	ulint		len;
	trx_rseg_t*	rseg;
	fil_addr_t	node_addr;
	trx_rsegf_t*	rseg_header;
	trx_ulogf_t*	undo_log_hdr;
	ulint		sum_of_undo_sizes;

	rseg = static_cast<trx_rseg_t*>(mem_zalloc(sizeof(trx_rseg_t)));

	rseg->id = id;
	rseg->space = space;
	rseg->zip_size = zip_size;
	rseg->page_no = page_no;

	mutex_create(rseg_mutex_key, &rseg->mutex, SYNC_RSEG);

	/* const_cast<trx_rseg_t*>() because this function is
	like a constructor.  */
	*((trx_rseg_t**) trx_sys->rseg_array + rseg->id) = rseg;

	rseg_header = trx_rsegf_get_new(space, zip_size, page_no, mtr);

	rseg->max_size = mtr_read_ulint(
		rseg_header + TRX_RSEG_MAX_SIZE, MLOG_4BYTES, mtr);

	/* Initialize the undo log lists according to the rseg header */

	sum_of_undo_sizes = trx_undo_lists_init(rseg);

	rseg->curr_size = mtr_read_ulint(
		rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr)
		+ 1 + sum_of_undo_sizes;

	len = flst_get_len(rseg_header + TRX_RSEG_HISTORY, mtr);

	if (len > 0) {
		rseg_queue_t	rseg_queue;

		trx_sys->rseg_history_len += len;

		node_addr = trx_purge_get_log_from_hist(
			flst_get_last(rseg_header + TRX_RSEG_HISTORY, mtr));

		rseg->last_page_no = node_addr.page;
		rseg->last_offset = node_addr.boffset;

		undo_log_hdr = trx_undo_page_get(
			rseg->space, rseg->zip_size, node_addr.page,
			mtr) + node_addr.boffset;

		rseg->last_trx_no = mach_read_from_8(
			undo_log_hdr + TRX_UNDO_TRX_NO);

		rseg->last_del_marks = mtr_read_ulint(
			undo_log_hdr + TRX_UNDO_DEL_MARKS, MLOG_2BYTES, mtr);

		rseg_queue.rseg = rseg;
		rseg_queue.trx_no = rseg->last_trx_no;

		if (rseg->last_page_no != FIL_NULL) {
			const void*	ptr;

			/* There is no need to cover this operation by the purge
			mutex because we are still bootstrapping. */

			ptr = ib_bh_push(ib_bh, &rseg_queue);
			ut_a(ptr != NULL);
		}
	} else {
		rseg->last_page_no = FIL_NULL;
	}

	return(rseg);
}

/********************************************************************
Creates the memory copies for the rollback segments and initializes the
rseg array in trx_sys at a database startup. */
static
void
trx_rseg_create_instance(
/*=====================*/
	trx_sysf_t*	sys_header,	/*!< in: trx system header */
	ib_bh_t*	ib_bh,		/*!< in/out: rseg queue */
	mtr_t*		mtr)		/*!< in: mtr */
{
	ulint		i;

	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {
		ulint	page_no;

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, mtr);

		if (page_no != FIL_NULL) {
			ulint		space;
			ulint		zip_size;
			trx_rseg_t*	rseg = NULL;

			ut_a(!trx_rseg_get_on_id(i));

			space = trx_sysf_rseg_get_space(sys_header, i, mtr);

			zip_size = space ? fil_space_get_zip_size(space) : 0;

			rseg = trx_rseg_mem_create(
				i, space, zip_size, page_no, ib_bh, mtr);

			ut_a(rseg->id == i);
		} else {
			ut_a(trx_sys->rseg_array[i] == NULL);
		}
	}
}

/*********************************************************************
Creates a rollback segment.
@return pointer to new rollback segment if create successful */
UNIV_INTERN
trx_rseg_t*
trx_rseg_create(
/*============*/
	ulint		space)		/*!< in: id of UNDO tablespace */
{
	mtr_t		mtr;
	ulint		slot_no;
	trx_rseg_t*	rseg = NULL;

	mtr_start(&mtr);

	/* To obey the latching order, acquire the file space
	x-latch before the trx_sys->mutex. */
	mtr_x_lock(fil_space_get_latch(space, NULL), &mtr);

	slot_no = trx_sysf_rseg_find_free(&mtr);

	if (slot_no != ULINT_UNDEFINED) {
		ulint		id;
		ulint		page_no;
		ulint		zip_size;
		trx_sysf_t*	sys_header;

		page_no = trx_rseg_header_create(
			space, 0, ULINT_MAX, slot_no, &mtr);

		ut_a(page_no != FIL_NULL);

		sys_header = trx_sysf_get(&mtr);

		id = trx_sysf_rseg_get_space(sys_header, slot_no, &mtr);
		ut_a(id == space);

		zip_size = space ? fil_space_get_zip_size(space) : 0;

		rseg = trx_rseg_mem_create(
			slot_no, space, zip_size, page_no,
			purge_sys->ib_bh, &mtr);
	}

	mtr_commit(&mtr);

	return(rseg);
}

/*********************************************************************//**
Creates the memory copies for rollback segments and initializes the
rseg array in trx_sys at a database startup. */
UNIV_INTERN
void
trx_rseg_array_init(
/*================*/
	trx_sysf_t*	sys_header,	/* in/out: trx system header */
	ib_bh_t*	ib_bh,		/*!< in: rseg queue */
	mtr_t*		mtr)		/*!< in: mtr */
{
	trx_sys->rseg_history_len = 0;

	trx_rseg_create_instance(sys_header, ib_bh, mtr);
}

/********************************************************************
Get the number of unique rollback tablespaces in use except space id 0.
The last space id will be the sentinel value ULINT_UNDEFINED. The array
will be sorted on space id. Note: space_ids should have have space for
TRX_SYS_N_RSEGS + 1 elements.
@return number of unique rollback tablespaces in use. */
UNIV_INTERN
ulint
trx_rseg_get_n_undo_tablespaces(
/*============================*/
	ulint*		space_ids)	/*!< out: array of space ids of
					UNDO tablespaces */
{
	ulint		i;
	mtr_t		mtr;
	trx_sysf_t*	sys_header;
	ulint		n_undo_tablespaces = 0;
	ulint		space_ids_aux[TRX_SYS_N_RSEGS + 1];

	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {
		ulint	page_no;
		ulint	space;

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, &mtr);

		if (page_no == FIL_NULL) {
			continue;
		}

		space = trx_sysf_rseg_get_space(sys_header, i, &mtr);

		if (space != 0) {
			ulint	j;
			ibool	found = FALSE;

			for (j = 0; j < n_undo_tablespaces; ++j) {
				if (space_ids[j] == space) {
					found = TRUE;
					break;
				}
			}

			if (!found) {
				ut_a(n_undo_tablespaces <= i);
				space_ids[n_undo_tablespaces++] = space;
			}
		}
	}

	mtr_commit(&mtr);

	ut_a(n_undo_tablespaces <= TRX_SYS_N_RSEGS);

	space_ids[n_undo_tablespaces] = ULINT_UNDEFINED;

	if (n_undo_tablespaces > 0) {
		ut_ulint_sort(space_ids, space_ids_aux, 0, n_undo_tablespaces);
	}

	return(n_undo_tablespaces);
}
