/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#include <stddef.h>
#include <algorithm>

#include "fsp0sysspace.h"
#include "fut0lst.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "trx0undo.h"

/** Creates a rollback segment header.
This function is called only when a new rollback segment is created in
the database.
@param[in]	space_id	space id
@param[in]	page_size	page size
@param[in]	max_size	max size in pages
@param[in]	rseg_slot	rseg id == slot number in trx sys
@param[in,out]	mtr		mini-transaction
@return page number of the created segment, FIL_NULL if fail */
page_no_t
trx_rseg_header_create(
	space_id_t		space_id,
	const page_size_t&	page_size,
	page_no_t		max_size,
	ulint			rseg_slot,
	mtr_t*			mtr)
{
	page_no_t	page_no;
	trx_rsegf_t*	rsegf;
	trx_sysf_t*	sys_header;
	ulint		i;
	buf_block_t*	block;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space_id, NULL),
				MTR_MEMO_X_LOCK));

	/* Allocate a new file segment for the rollback segment */
	block = fseg_create(space_id, 0, TRX_RSEG + TRX_RSEG_FSEG_HEADER, mtr);

	if (block == NULL) {
		return(FIL_NULL);	/* No space left */
	}

	buf_block_dbg_add_level(block, SYNC_RSEG_HEADER_NEW);

	page_no = block->page.id.page_no();

	/* Get the rollback segment file page */
	rsegf = trx_rsegf_get_new(space_id, page_no, page_size, mtr);

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

	if (!fsp_is_system_temporary(space_id)) {
		/* All rollback segments in the system tablespace or
		any undo tablespace need to be found in the TRX_SYS
		page in the rseg_id slot.
		Add the rollback segment info to the free slot in the
		trx system header in the TRX_SYS page. */

		sys_header = trx_sysf_get(mtr);

		trx_sysf_rseg_set_space(
			sys_header, rseg_slot, space_id, mtr);

		trx_sysf_rseg_set_page_no(
			sys_header, rseg_slot, page_no, mtr);
	}

	return(page_no);
}

/** Free an instance of the rollback segment in memory.
@param[in]	rseg	pointer to an rseg to free */
void
trx_rseg_mem_free(
	trx_rseg_t*	rseg)
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

		UT_LIST_REMOVE(rseg->update_undo_cached, undo);

		MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);

		trx_undo_mem_free(undo);
	}

	for (undo = UT_LIST_GET_FIRST(rseg->insert_undo_cached);
	     undo != NULL;
	     undo = next_undo) {

		next_undo = UT_LIST_GET_NEXT(undo_list, undo);

		UT_LIST_REMOVE(rseg->insert_undo_cached, undo);

		MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);

		trx_undo_mem_free(undo);
	}

	ut_free(rseg);
}

/** Create and initialize a rollback segment object.  Some of
the values for the fields are read from the segment header page.
The caller must insert it into the correct list.
@param[in]	id		rollback segment id
@param[in]	space_id	space where the segment is placed
@param[in]	page_no		page number of the segment header
@param[in]	page_size	page size
@param[in,out]	purge_queue	rseg queue
@param[in,out]	mtr		mini-transaction
@return own: rollback segment object */
trx_rseg_t*
trx_rseg_mem_create(
	ulint			id,
	space_id_t		space_id,
	page_no_t		page_no,
	const page_size_t&	page_size,
	purge_pq_t*		purge_queue,
	mtr_t*			mtr)
{
	ulint		len;
	trx_rseg_t*	rseg;
	fil_addr_t	node_addr;
	trx_rsegf_t*	rseg_header;
	trx_ulogf_t*	undo_log_hdr;
	ulint		sum_of_undo_sizes;

	rseg = static_cast<trx_rseg_t*>(ut_zalloc_nokey(sizeof(trx_rseg_t)));

	rseg->id = id;
	rseg->space_id = space_id;
	rseg->page_size.copy_from(page_size);
	rseg->page_no = page_no;
	rseg->trx_ref_count = 0;
	rseg->skip_allocation = false;

	if (fsp_is_system_temporary(space_id)) {
		mutex_create(LATCH_ID_TEMP_SPACE_RSEG, &rseg->mutex);
	} else {
		mutex_create(LATCH_ID_TRX_SYS_RSEG, &rseg->mutex);
	}

	UT_LIST_INIT(rseg->update_undo_list, &trx_undo_t::undo_list);
	UT_LIST_INIT(rseg->update_undo_cached, &trx_undo_t::undo_list);
	UT_LIST_INIT(rseg->insert_undo_list, &trx_undo_t::undo_list);
	UT_LIST_INIT(rseg->insert_undo_cached, &trx_undo_t::undo_list);

	rseg_header = trx_rsegf_get_new(space_id, page_no, page_size, mtr);

	rseg->max_size = mtr_read_ulint(
		rseg_header + TRX_RSEG_MAX_SIZE, MLOG_4BYTES, mtr);

	/* Initialize the undo log lists according to the rseg header */

	sum_of_undo_sizes = trx_undo_lists_init(rseg);

	rseg->curr_size = mtr_read_ulint(
		rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr)
		+ 1 + sum_of_undo_sizes;

	len = flst_get_len(rseg_header + TRX_RSEG_HISTORY);

	if (len > 0) {
		trx_sys->rseg_history_len += len;

		node_addr = trx_purge_get_log_from_hist(
			flst_get_last(rseg_header + TRX_RSEG_HISTORY, mtr));

		rseg->last_page_no = node_addr.page;
		rseg->last_offset = node_addr.boffset;

		undo_log_hdr = trx_undo_page_get(
			page_id_t(rseg->space_id, node_addr.page),
			rseg->page_size, mtr) + node_addr.boffset;

		rseg->last_trx_no = mach_read_from_8(
			undo_log_hdr + TRX_UNDO_TRX_NO);

		rseg->last_del_marks = mtr_read_ulint(
			undo_log_hdr + TRX_UNDO_DEL_MARKS, MLOG_2BYTES, mtr);

		TrxUndoRsegs elem(rseg->last_trx_no);
		elem.push_back(rseg);

		if (rseg->last_page_no != FIL_NULL) {

			/* There is no need to cover this operation by the purge
			mutex because we are still bootstrapping. */

			purge_queue->push(elem);
		}
	} else {
		rseg->last_page_no = FIL_NULL;
	}

	return(rseg);
}

/** Read each rollback segment slot in the TRX_SYS page and create
trx_rseg_t objects for all rollback segments found.  Initialize the
trx_sys->rsegs at a database startup.  We need to look at all TRX_SYS
slots in order to collect any undo logs for purge.  No latch is needed
since this is still single-threaded startup.
@param[in]	purge_queue	queue of rsegs to purge */
void
trx_sys_rsegs_init(
	purge_pq_t*	purge_queue)
{
	trx_sys->rseg_history_len = 0;

	ulint			slot;
	mtr_t			mtr;
	space_id_t		space_id;
	page_no_t		page_no;
	trx_rseg_t*		rseg = nullptr;

	for (slot = 0; slot < TRX_SYS_N_RSEGS; slot++) {

		mtr.start();
		trx_sysf_t* sys_header = trx_sysf_get(&mtr);

		page_no = trx_sysf_rseg_get_page_no(sys_header, slot, &mtr);

		if (page_no != FIL_NULL) {

			space_id = trx_sysf_rseg_get_space(
				sys_header, slot, &mtr);

			/* Create the trx_rseg_t object.
			Note that all tablespaces with rollback segments
			use univ_page_size. (system, temp & undo) */
			rseg = trx_rseg_mem_create(
				slot, space_id, page_no, univ_page_size,
				purge_queue, &mtr);

			ut_a(rseg->id == slot);

			trx_sys->rsegs.push_back(rseg);
		}
		mtr.commit();
	}
}

/** Create a rollback segment in the given tablespace. This could be either
the system tablespace, the temporary tablespace, or an undo tablespace.
@param[in]	space_id	tablespace to get the rollback segment
@param[in]	rseg_id		slot number of the rseg within this tablespace
@return pointer to new rollback segment if create was successful */
trx_rseg_t*
trx_rseg_create(
	space_id_t	space_id,
	ulint		rseg_id)
{
	mtr_t		mtr;

	/* To obey the latching order, acquire the file space
	x-latch before the trx_sys->mutex. */
	fil_space_t*	space = fil_space_get(space_id);

	mtr_start(&mtr);

	mtr_x_lock(&space->latch, &mtr);

	ut_ad(!fsp_is_system_temporary(space_id)
		|| rseg_id < srv_tmp_rollback_segments);
	ut_ad(fsp_is_system_temporary(space_id)
		|| rseg_id < srv_rollback_segments);
	ut_ad(to_int(space->purpose) == fsp_is_system_temporary(space_id)
				? FIL_TYPE_TEMPORARY
				: FIL_TYPE_TABLESPACE);
	ut_ad(univ_page_size.equals_to(page_size_t(space->flags)));

	if (fsp_is_system_temporary(space_id)) {
		mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
	} else {
		/* We will modify TRX_SYS_RSEGS in TRX_SYS page. */
	}

	page_no_t	page_no = trx_rseg_header_create(
		space_id, univ_page_size, PAGE_NO_MAX, rseg_id, &mtr);

	if (page_no == FIL_NULL) {
		mtr_commit(&mtr);

		return(nullptr);
	}

	ut_ad(fsp_is_system_temporary(space_id)
	      || space_id == trx_sysf_rseg_get_space(
		      trx_sysf_get(&mtr), rseg_id, &mtr));

	trx_rseg_t*	rseg = trx_rseg_mem_create(
		rseg_id, space_id,
		page_no, univ_page_size,
		purge_sys->purge_queue, &mtr);

	ut_ad(rseg->id == rseg_id);

	mtr_commit(&mtr);

	return(rseg);
}

/** Creates a rollback segment in the system temporary tablespace.
@return pointer to new rollback segment if create was successful */
trx_rseg_t*
trx_rseg_create_in_temp_space(ulint rseg_id)
{
	space_id_t	space_id = srv_tmp_space.space_id();
	trx_rseg_t*	rseg;
	mtr_t		mtr;

	rseg = trx_rseg_create(space_id, rseg_id);

	/* Push it onto the trx_sys->tmp_rsegs list. */
	if (rseg != nullptr) {
		trx_sys->tmp_rsegs.push_back(rseg);
	}

	return(rseg);
}

/** Build a list of unique undo tablespaces found in the TRX_SYS page.
Do not count the system tablespace. The vector will be sorted on space id.
@param[in,out]	spaces_to_open		list of undo tablespaces found.  */
void
trx_rseg_get_n_undo_tablespaces(Space_Ids& spaces_to_open)
{
	ulint		i;
	mtr_t		mtr;
	trx_sysf_t*	sys_header;

	ut_ad(spaces_to_open.size() == 0);

	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {
		page_no_t	page_no;
		space_id_t	space_id;

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, &mtr);

		if (page_no == FIL_NULL) {
			continue;
		}

		space_id = trx_sysf_rseg_get_space(sys_header, i, &mtr);

		/* The system space id should not be in this array. */
		if (space_id != 0
		    && !spaces_to_open.contains(space_id)) {

			spaces_to_open.push_back(space_id);
		}
	}

	mtr_commit(&mtr);

	ut_a(spaces_to_open.size() <= TRX_SYS_N_RSEGS);
}
