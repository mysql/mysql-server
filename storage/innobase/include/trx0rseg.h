/*****************************************************************************

Copyright (c) 1996, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
	ulint			space,
	ulint			page_no,
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
	ulint			space,
	ulint			page_no,
	const page_size_t&	page_size,
	mtr_t*			mtr);

/***************************************************************//**
Gets the file page number of the nth undo log slot.
@return page number of the undo log segment */
UNIV_INLINE
ulint
trx_rsegf_get_nth_undo(
/*===================*/
	trx_rsegf_t*	rsegf,	/*!< in: rollback segment header */
	ulint		n,	/*!< in: index of slot */
	mtr_t*		mtr);	/*!< in: mtr */
/***************************************************************//**
Sets the file page number of the nth undo log slot. */
UNIV_INLINE
void
trx_rsegf_set_nth_undo(
/*===================*/
	trx_rsegf_t*	rsegf,	/*!< in: rollback segment header */
	ulint		n,	/*!< in: index of slot */
	ulint		page_no,/*!< in: page number of the undo log segment */
	mtr_t*		mtr);	/*!< in: mtr */
/****************************************************************//**
Looks for a free slot for an undo log segment.
@return slot index or ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint
trx_rsegf_undo_find_free(
/*=====================*/
	trx_rsegf_t*	rsegf,	/*!< in: rollback segment header */
	mtr_t*		mtr);	/*!< in: mtr */
/******************************************************************//**
Looks for a rollback segment, based on the rollback segment id.
@return rollback segment */
UNIV_INLINE
trx_rseg_t*
trx_rseg_get_on_id(
/*===============*/
	ulint	id,		/*!< in: rollback segment id */
	bool	is_redo_rseg);	/*!< in: true if redo rseg else false. */

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
	mtr_t*			mtr);

/*********************************************************************//**
Creates the memory copies for rollback segments and initializes the
rseg array in trx_sys at a database startup. */
void
trx_rseg_array_init(
/*================*/
	purge_pq_t*	purge_queue);	/*!< in: rseg queue */

/***************************************************************************
Free's an instance of the rollback segment in memory. */
void
trx_rseg_mem_free(
/*==============*/
	trx_rseg_t*	rseg,		/*!< in, own: instance to free */
	trx_rseg_t**	rseg_array);	/*!< out: add rseg reference to this
					central array. */
/*********************************************************************
Creates a rollback segment. */
trx_rseg_t*
trx_rseg_create(
/*============*/
	ulint	space_id,	/*!< in: id of UNDO tablespace */
	ulint   nth_free_slot);	/*!< in: allocate nth free slot.
				0 means next free slots. */

/********************************************************************
Get the number of unique rollback tablespaces in use except space id 0.
The last space id will be the sentinel value ULINT_UNDEFINED. The array
will be sorted on space id. Note: space_ids should have have space for
TRX_SYS_N_RSEGS + 1 elements.
@return number of unique rollback tablespaces in use. */
ulint
trx_rseg_get_n_undo_tablespaces(
/*============================*/
	ulint*		space_ids);	/*!< out: array of space ids of
					UNDO tablespaces */
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

	/** space where the rollback segment header is placed */
	ulint				space;

	/** page number of the rollback segment header */
	ulint				page_no;

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
	ulint				last_page_no;

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

#ifndef UNIV_NONINL
#include "trx0rseg.ic"
#endif

#endif
