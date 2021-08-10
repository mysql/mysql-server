/*****************************************************************************

Copyright (c) 1996, 2021, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

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
#include "srv0mon.h"
#include "fsp0sysspace.h"

#include <algorithm>

/** Creates a rollback segment header.
This function is called only when a new rollback segment is created in
the database.
@param[in]	space		space id
@param[in]	page_size	page size
@param[in]	max_size	max size in pages
@param[in]	rseg_slot_no	rseg id == slot number in trx sys
@param[in,out]	mtr		mini-transaction
@return page number of the created segment, FIL_NULL if fail */
ulint
trx_rseg_header_create(
	ulint			space,
	const page_size_t&	page_size,
	ulint			max_size,
	ulint			rseg_slot_no,
	mtr_t*			mtr)
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

	page_no = block->page.id.page_no();

	/* Get the rollback segment file page */
	rsegf = trx_rsegf_get_new(space, page_no, page_size, mtr);

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

	if (!trx_sys_is_noredo_rseg_slot(rseg_slot_no)) {
		/* No-redo rsegs are re-created on restart and no need to
		persist this information in sys-header. Anyway, on restart
		this information is not valid too as there is no space with
		persisted space-id on restart. */

		/* Add the rollback segment info to the free slot in
		the trx system header */

		sys_header = trx_sysf_get(mtr);

		trx_sysf_rseg_set_space(sys_header, rseg_slot_no, space, mtr);

		trx_sysf_rseg_set_page_no(
			sys_header, rseg_slot_no, page_no, mtr);
	}

	return(page_no);
}

/***********************************************************************//**
Free's an instance of the rollback segment in memory. */
void
trx_rseg_mem_free(
/*==============*/
	trx_rseg_t*	rseg,		/* in, own: instance to free */
	trx_rseg_t**	rseg_array)	/*!< out: add rseg reference to this
					central array. */
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

	ut_a(*((trx_rseg_t**) rseg_array + rseg->id) == rseg);
	*((trx_rseg_t**) rseg_array + rseg->id) = NULL;

	ut_free(rseg);
}

/** Creates and initializes a rollback segment object.
The values for the fields are read from the header. The object is inserted to
the rseg list of the trx system object and a pointer is inserted in the rseg
array in the trx system object.
@param[in]	id		rollback segment id
@param[in]	space		space where the segment is placed
@param[in]	page_no		page number of the segment header
@param[in]	page_size	page size
@param[in,out]	purge_queue	rseg queue
@param[out]	rseg_array	add rseg reference to this central array
@param[in,out]	mtr		mini-transaction
@return own: rollback segment object */
static
trx_rseg_t*
trx_rseg_mem_create(
	ulint			id,
	ulint			space,
	ulint			page_no,
	const page_size_t&	page_size,
	purge_pq_t*		purge_queue,
	trx_rseg_t**		rseg_array,
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
	rseg->space = space;
	rseg->page_size.copy_from(page_size);
	rseg->page_no = page_no;
	rseg->trx_ref_count = 0;
	rseg->skip_allocation = false;

	if (fsp_is_system_temporary(space)) {
		mutex_create(LATCH_ID_NOREDO_RSEG, &rseg->mutex);
	} else {
		mutex_create(LATCH_ID_REDO_RSEG, &rseg->mutex);
	}

	UT_LIST_INIT(rseg->update_undo_list, &trx_undo_t::undo_list);
	UT_LIST_INIT(rseg->update_undo_cached, &trx_undo_t::undo_list);
	UT_LIST_INIT(rseg->insert_undo_list, &trx_undo_t::undo_list);
	UT_LIST_INIT(rseg->insert_undo_cached, &trx_undo_t::undo_list);

	*((trx_rseg_t**) rseg_array + rseg->id) = rseg;

	rseg_header = trx_rsegf_get_new(space, page_no, page_size, mtr);

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
			page_id_t(rseg->space, node_addr.page),
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

/* Read information from system header page for a rollback segment.
@param[in]	rseg_id		rollback segment ID
@param[in,out]	mtr		mini transaction for reading
@param[out]	space		space ID for the rollback segment
@param[out]	page_no		page number of the rollback segment
@return	page size for the rollback segment space.  */
static const page_size_t
read_sys_rseg_info(
	ulint	rseg_id,
	mtr_t	*mtr,
	ulint	&space,
	ulint	&page_no)
{
	trx_sysf_t* sys_header = trx_sysf_get(mtr);

	page_no = trx_sysf_rseg_get_page_no(sys_header, rseg_id, mtr);

	if (page_no == FIL_NULL) {
		space = 0;
		return (univ_page_size);
	}

	space = trx_sysf_rseg_get_space(sys_header, rseg_id, mtr);

	bool found = true;

	const page_size_t& page_size
		= is_system_tablespace(space)
		? univ_page_size
		: fil_space_get_page_size(space, &found);

	ut_ad(found);
	return (page_size);
}

/** Initialize a redo rollback segment and add to purge queue if it has anything
left to purge.
@param[in]	rseg_id		rollback segment ID
@param[in,out]	rseg_array	rollback segment array
@param[in,out]	purge_queue	queue for rollback segments that need purging */
static void
trx_rseg_initialize(
	ulint		rseg_id,
	trx_rseg_t**	rseg_array,
	purge_pq_t*	purge_queue)
{
	ut_a(rseg_array[rseg_id] == NULL);

	mtr_t mtr;
	mtr.start();

	ulint space = 0;
	ulint page_no = FIL_NULL;

	const page_size_t& page_size =
		read_sys_rseg_info(rseg_id, &mtr, space, page_no);

	if (page_no == FIL_NULL) {
		mtr.commit();
		return;
	}

	trx_rseg_t* rseg = trx_rseg_mem_create(rseg_id, space, page_no,
		page_size, purge_queue, rseg_array, &mtr);

	ut_a(rseg->id == rseg_id);

	mtr.commit();
}

/* Check if any of the redo rollback segment has same space, page reference
@param[in]	space		space ID for the rollback segment
@param[in]	page_no		page number of the rollback segment
@return	true iff duplicate is found. */
static bool
check_duplicate_rseg(
	ulint	space,
	ulint	page_no)
{
	for (ulint rseg_id = 0; rseg_id < TRX_SYS_N_RSEGS; rseg_id++) {
		/* Skip over no-redo rollback segments. */
		if (trx_sys_is_noredo_rseg_slot(rseg_id)) {
			continue;
		}

		trx_rseg_t* rseg = trx_sys->rseg_array[rseg_id];

		if (rseg != NULL && rseg->space == space &&
		    rseg->page_no == page_no) {
			ib::info() << "Found duplicate reference rseg: "
				   << rseg_id << " space: " << space
				   << " page: " << page_no;
			return (true);
		}
	}
	return (false);
}

/** Check if pre-5.7.2 rollback segment has data to be purged.
@param[in]	rseg_id		rollback segment ID
@param[out]	reset_rseg	true iff need to reset rseg slot
@return true iff there is data to purge.
*/
static bool is_purge_pending(
	ulint	rseg_id,
	bool&	reset_rseg)
{
	ut_a(trx_sys_is_noredo_rseg_slot(rseg_id));

	mtr_t mtr;
	mtr.start();

	ulint space = 0;
	ulint page_no = FIL_NULL;

	const page_size_t& page_size =
		read_sys_rseg_info(rseg_id, &mtr, space, page_no);

	reset_rseg = (page_no != FIL_NULL);

	if (page_no == FIL_NULL || !is_system_or_undo_tablespace(space)) {
		mtr.commit();
		return (false);
	}

	/* There is an issue till 5.7.34, which could cause a pre-5.7.2 rseg to
	point to some redo rseg after undo tablespace truncate. We need to check
	for it and should not add it for purge in such case. */
	if (check_duplicate_rseg(space, page_no)) {
		mtr.commit();
		ib::info() << "Reset pre-5.7.2 rseg: " << rseg_id
			   << " after duplicate is found.";
		return (false);
	}

	trx_rsegf_t* rseg_header =
		trx_rsegf_get_new(space, page_no, page_size, &mtr);

	ulint len = flst_get_len(rseg_header + TRX_RSEG_HISTORY);

	mtr.commit();

	if (len > 0) {
		ib::info() << "pre-5.7.2 rseg: " << rseg_id
			   << " holds data to be purged. History length: "
			   << len << ". Recommend slow shutdown with"
			   << " innodb_fast_shutdown=0 and restart";
		reset_rseg = false;
		return (true);
	}
	return (false);
}

/** Rollback segment IDs that needs to be reset on disk. */
static std::vector<ulint> s_pending_reset_rseg_ids;

/** Creates the memory copies for the rollback segments and initializes the
rseg array in trx_sys at a database startup.
@param[in,out]	purge_queue	queue for rollback segments that need purging
*/
static void trx_rseg_create_instance(
	purge_pq_t*	purge_queue)
{
        /* Initialize redo rollback segments. */
	for (ulint rseg_id = 0; rseg_id < TRX_SYS_N_RSEGS; rseg_id++) {
                /* Skip all no-redo segments. Slot-1....Slot-n are reserved for
		no-redo rsegs. These no-redo rsegs are recreated on server
		re-start and we should avoid initializing them. There could also
		be some leftover redo rollback segments from pre-5.7.2, which we
		handle in next iteration. */
		if (trx_sys_is_noredo_rseg_slot(rseg_id)) {
			continue;
		}
		trx_rseg_initialize(rseg_id, trx_sys->rseg_array, purge_queue);
	}

        /* Check and initialize redo rollback segments carried forward from
	pre-5.7.2. They could occupy the no-redo rseg slots in range from
	slot-1...slot-n. We need to schedule them for purge if there are pending
	purge operations. It is only possible if pre-5.7.2 server is upgraded
	without using slow shutdown (innodb_fast_shutdown = 0). */
	for (ulint rseg_id = 0; rseg_id < TRX_SYS_N_RSEGS; rseg_id++) {
                /* Skip all redo slots which are handled already in previous
		iteration. */
		if (!trx_sys_is_noredo_rseg_slot(rseg_id)) {
			continue;
		}

		bool reset_rseg_slot = false;

		if (is_purge_pending(rseg_id, reset_rseg_slot)) {
			trx_rseg_initialize(rseg_id,
				trx_sys->pending_purge_rseg_array, purge_queue);
			continue;
		}

		if (reset_rseg_slot) {
			/* We need to reset this rollback segment slot on disk.
			Redo recovery is not finished at this point and we don't
			want to start DB modification generating new redo logs.
			The reset is deferred till recovery end and done by
			trx_rseg_reset_pending(). */
			s_pending_reset_rseg_ids.push_back(rseg_id);
		}
		ut_a(trx_sys->pending_purge_rseg_array[rseg_id] == NULL);
	}
}

/** Reset no-redo rollback segment slot on disk.
@param[in]	rseg_id	rollback segment ID */
static void trx_rseg_reset_slot(
	ulint	rseg_id)
{
	ut_a(rseg_id < TRX_SYS_N_RSEGS);
	ut_a(trx_sys_is_noredo_rseg_slot(rseg_id));

	/* Reset rollback segment slot on disk. */
	mtr_t	mtr;
	mtr.start();
	trx_sysf_t* sys_header = trx_sysf_get(&mtr);

	trx_sysf_rseg_set_page_no(
                              sys_header, rseg_id, FIL_NULL, &mtr);
	mtr.commit();
}

void trx_rseg_reset_pending() {
	if (s_pending_reset_rseg_ids.empty()) {
		return;

	} else if (srv_read_only_mode) {
		ib::warn() << "Could not reset pre-5.7.2 rseg slots"
			   << " in read-only mode.";
		return;
	}
        /* Check and reset no-redo rollback segment slots carried forward from
	pre-5.7.2 with no left-over data to purge. This is a deferred action
	from trx_rseg_create_instance(). */
	std::for_each(s_pending_reset_rseg_ids.begin(),
		      s_pending_reset_rseg_ids.end(), trx_rseg_reset_slot);

	ib::info() << "Successfully reset " << s_pending_reset_rseg_ids.size()
		   << " pre-5.7.2 rseg slots.";

	s_pending_reset_rseg_ids.clear();
}

/*********************************************************************
Creates a rollback segment.
@return pointer to new rollback segment if create successful */
trx_rseg_t*
trx_rseg_create(
/*============*/
	ulint	space_id,	/*!< in: id of UNDO tablespace */
	ulint	nth_free_slot)	/*!< in: allocate nth free slot.
				0 means next free slots. */
{
	mtr_t		mtr;
	ulint		slot_no;
	trx_rseg_t*	rseg = NULL;

	mtr_start(&mtr);

	/* To obey the latching order, acquire the file space
	x-latch before the trx_sys->mutex. */
	const fil_space_t*	space = mtr_x_lock_space(space_id, &mtr);

	switch (space->purpose) {
	case FIL_TYPE_LOG:
	case FIL_TYPE_IMPORT:
		ut_ad(0);
	case FIL_TYPE_TEMPORARY:
		mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
		break;
	case FIL_TYPE_TABLESPACE:
		break;
	}

	slot_no = trx_sysf_rseg_find_free(
		&mtr, space->purpose == FIL_TYPE_TEMPORARY, nth_free_slot);

	if (slot_no != ULINT_UNDEFINED) {
		ulint		id;
		ulint		page_no;
		trx_sysf_t*	sys_header;
		page_size_t	page_size(space->flags);

		page_no = trx_rseg_header_create(
			space_id, page_size, ULINT_MAX, slot_no, &mtr);

		if (page_no == FIL_NULL) {
			mtr_commit(&mtr);

			return(rseg);
		}

		sys_header = trx_sysf_get(&mtr);

		id = trx_sysf_rseg_get_space(sys_header, slot_no, &mtr);
		ut_a(id == space_id || trx_sys_is_noredo_rseg_slot(slot_no));

		trx_rseg_t** rseg_array =
			((trx_rseg_t**) trx_sys->rseg_array);

		rseg = trx_rseg_mem_create(
			slot_no, space_id, page_no, page_size,
			purge_sys->purge_queue, rseg_array, &mtr);
	}

	mtr_commit(&mtr);

	return(rseg);
}

/*********************************************************************//**
Creates the memory copies for rollback segments and initializes the
rseg array in trx_sys at a database startup. */
void
trx_rseg_array_init(
/*================*/
	purge_pq_t*	purge_queue)	/*!< in: rseg queue */
{
	trx_sys->rseg_history_len = 0;

	trx_rseg_create_instance(purge_queue);
}

/********************************************************************
Get the number of unique rollback tablespaces in use except space id 0.
The last space id will be the sentinel value ULINT_UNDEFINED. The array
will be sorted on space id. Note: space_ids should have have space for
TRX_SYS_N_RSEGS + 1 elements.
@return number of unique rollback tablespaces in use. */
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
		std::sort(space_ids, space_ids + n_undo_tablespaces);
	}

	return(n_undo_tablespaces);
}
