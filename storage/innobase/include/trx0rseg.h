/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/trx0rseg.h
Rollback segment

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0rseg_h
#define trx0rseg_h

#include "univ.i"
#include "trx0types.h"
#include "trx0sys.h"
#include "fut0lst.h"
#include <vector>

/** Gets a rollback segment header.
@param[in]	space		space where placed
@param[in]	page_no		page number of the header
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get(
	space_id_t		space,
	page_no_t		page_no,
	const page_size_t&	page_size,
	mtr_t*			mtr);

/** Gets a newly created rollback segment header.
@param[in]	space		space where placed
@param[in]	page_no		page number of the header
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get_new(
	space_id_t		space,
	page_no_t		page_no,
	const page_size_t&	page_size,
	mtr_t*			mtr);

/** Gets the file page number of the nth undo log slot.
@param[in]	rsegf	rollback segment header
@param[in]	n	index of slot
@param[in]	mtr	mtr
@return page number of the undo log segment */
UNIV_INLINE
page_no_t
trx_rsegf_get_nth_undo(
	trx_rsegf_t*	rsegf,
	ulint		n,
	mtr_t*		mtr);

/** Sets the file page number of the nth undo log slot.
@param[in]	rsegf	rollback segment header
@param[in]	n	index of slot
@param[in]	page_no	page number of the undo log segment
@param[in]	mtr	mtr */
UNIV_INLINE
void
trx_rsegf_set_nth_undo(
	trx_rsegf_t*	rsegf,
	ulint		n,
	page_no_t	page_no,
	mtr_t*		mtr);

/** Looks for a free slot for an undo log segment.
@param[in]	rsegf	rollback segment header
@param[in]	mtr	mtr
@return slot index or ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint
trx_rsegf_undo_find_free(
	trx_rsegf_t*	rsegf,
	mtr_t*		mtr);

/** Look for a rollback segment, based on the rollback segment id.
@param[in]	id		rollback segment id
@param[in]	is_temp		true if rseg from Temp Tablespace else false.
@return rollback segment */
UNIV_INLINE
trx_rseg_t*
trx_rseg_get_on_id(
	ulint	id,
	bool	is_temp);

/** Creates a rollback segment header.
This function is called only when a new rollback segment is created in
the database.
@param[in]	space_id		space id
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
	mtr_t*			mtr);

/** Create the memory copies for rollback segments and initialize the
rseg array in trx_sys at a database startup.
@param[in]	purge_queue	queue of rsegs to purge */
void
trx_sys_rsegs_init(
	purge_pq_t*	purge_queue);

/** Free an instance of the rollback segment in memory.
@param[in]	rseg	pointer to an rseg to free */
void
trx_rseg_mem_free(
	trx_rseg_t*	rseg);

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
	mtr_t*			mtr);
	
/** Create a rollback segment in the given tablespace. This could be either
the system tablespace, the temporary tablespace, or an undo tablespace.
@param[in]	space_id	tablespace to get the rollback segment
@param[in]	rseg_id		slot number of the rseg within this tablespace
@return pointer to new rollback segment if create was successful */
trx_rseg_t*
trx_rseg_create(
	space_id_t	space_id,
	ulint		rseg_id);

/** Creates a rollback segment in the system temporary tablespace.
@return pointer to new rollback segment if create was successful */
trx_rseg_t*
trx_rseg_create_in_temp_space(ulint slot_no);

/** Build a list of unique undo tablespaces found in the TRX_SYS page.
Do not count the system tablespace. The vector will be sorted on space id.
@param[in,out]	spaces_to_open		list of undo tablespaces found. */
void
trx_rseg_get_n_undo_tablespaces(Space_Ids& spaces_to_open);

/* Number of undo log slots in a rollback segment file copy */
#define TRX_RSEG_N_SLOTS	(UNIV_PAGE_SIZE / 16)

/* Maximum number of transactions supported by a single rollback segment */
#define TRX_RSEG_MAX_N_TRXS	(TRX_RSEG_N_SLOTS / 2)

/** The rollback segment memory object */
struct trx_rseg_t {
	/*--------------------------------------------------------*/
	/** rollback segment id == the index of its slot in the trx
	system file copy */
	ulint				id;

	/** mutex protecting the fields in this struct except id,space,page_no
	which are constant */
	RsegMutex			mutex;

	/** space ID where the rollback segment header is placed */
	space_id_t			space_id;

	/** page number of the rollback segment header */
	page_no_t			page_no;

	/** page size of the relevant tablespace */
	page_size_t			page_size;

	/** maximum allowed size in pages */
	ulint				max_size;

	/** current size in pages */
	ulint				curr_size;

	/*--------------------------------------------------------*/
	/* Fields for update undo logs */
	/** List of update undo logs */
	UT_LIST_BASE_NODE_T(trx_undo_t)	update_undo_list;

	/** List of update undo log segments cached for fast reuse */
	UT_LIST_BASE_NODE_T(trx_undo_t)	update_undo_cached;

	/*--------------------------------------------------------*/
	/* Fields for insert undo logs */
	/** List of insert undo logs */
	UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_list;

	/** List of insert undo log segments cached for fast reuse */
	UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_cached;

	/*--------------------------------------------------------*/

	/** Page number of the last not yet purged log header in the history
	list; FIL_NULL if all list purged */
	page_no_t			last_page_no;

	/** Byte offset of the last not yet purged log header */
	ulint				last_offset;

	/** Transaction number of the last not yet purged log */
	trx_id_t			last_trx_no;

	/** TRUE if the last not yet purged log needs purging */
	ibool				last_del_marks;

	/** Reference counter to track rseg allocated transactions. */
	ulint				trx_ref_count;

	/** If true, then skip allocating this rseg as it reside in
	UNDO-tablespace marked for truncate. */
	bool				skip_allocation;
};

/* Undo log segment slot in a rollback segment header */
/*-------------------------------------------------------------*/
#define	TRX_RSEG_SLOT_PAGE_NO	0	/* Page number of the header page of
					an undo log segment */
/*-------------------------------------------------------------*/
/* Slot size */
#define TRX_RSEG_SLOT_SIZE	4

/* The offset of the rollback segment header on its page */
#define	TRX_RSEG		FSEG_PAGE_DATA

/* Transaction rollback segment header */
/*-------------------------------------------------------------*/
#define	TRX_RSEG_MAX_SIZE	0	/* Maximum allowed size for rollback
					segment in pages */
#define	TRX_RSEG_HISTORY_SIZE	4	/* Number of file pages occupied
					by the logs in the history list */
#define	TRX_RSEG_HISTORY	8	/* The update undo logs for committed
					transactions */
#define	TRX_RSEG_FSEG_HEADER	(8 + FLST_BASE_NODE_SIZE)
					/* Header for the file segment where
					this page is placed */
#define TRX_RSEG_UNDO_SLOTS	(8 + FLST_BASE_NODE_SIZE + FSEG_HEADER_SIZE)
					/* Undo log segment slots */
/*-------------------------------------------------------------*/

#include "trx0rseg.ic"

#endif
