/*****************************************************************************

Copyright (c) 1996, 2011, Innobase Oy. All Rights Reserved.

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
@file include/trx0rseg.h
Rollback segment

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0rseg_h
#define trx0rseg_h

#include "univ.i"
#include "trx0types.h"
#include "trx0sys.h"

/******************************************************************//**
Gets a rollback segment header.
@return	rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get(
/*==========*/
	ulint	space,		/*!< in: space where placed */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	page_no,	/*!< in: page number of the header */
	mtr_t*	mtr);		/*!< in: mtr */
/******************************************************************//**
Gets a newly created rollback segment header.
@return	rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get_new(
/*==============*/
	ulint	space,		/*!< in: space where placed */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	page_no,	/*!< in: page number of the header */
	mtr_t*	mtr);		/*!< in: mtr */
/***************************************************************//**
Gets the file page number of the nth undo log slot.
@return	page number of the undo log segment */
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
@return	slot index or ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint
trx_rsegf_undo_find_free(
/*=====================*/
	trx_rsegf_t*	rsegf,	/*!< in: rollback segment header */
	mtr_t*		mtr);	/*!< in: mtr */
/******************************************************************//**
Looks for a rollback segment, based on the rollback segment id.
@return	rollback segment */
UNIV_INTERN
trx_rseg_t*
trx_rseg_get_on_id(
/*===============*/
	ulint	id);	/*!< in: rollback segment id */
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
	mtr_t*	mtr);		/*!< in: mtr */
/*********************************************************************//**
Creates the memory copies for rollback segments and initializes the
rseg list and array in trx_sys at a database startup. */
UNIV_INTERN
void
trx_rseg_list_and_array_init(
/*=========================*/
	trx_sysf_t*	sys_header,	/*!< in: trx system header */
	ib_bh_t*	ib_bh,		/*!< in: rseg queue */
	mtr_t*		mtr);		/*!< in: mtr */

/***************************************************************************
Free's an instance of the rollback segment in memory. */
UNIV_INTERN
void
trx_rseg_mem_free(
/*==============*/
	trx_rseg_t*	rseg);		/* in, own: instance to free */

/*********************************************************************
Creates a rollback segment. */
UNIV_INTERN
trx_rseg_t*
trx_rseg_create(void);
/*==================*/

/* Number of undo log slots in a rollback segment file copy */
#define TRX_RSEG_N_SLOTS	(UNIV_PAGE_SIZE / 16)

/* Maximum number of transactions supported by a single rollback segment */
#define TRX_RSEG_MAX_N_TRXS	(TRX_RSEG_N_SLOTS / 2)

/* The rollback segment memory object */
struct trx_rseg_struct{
	/*--------------------------------------------------------*/
	ulint		id;	/*!< rollback segment id == the index of
				its slot in the trx system file copy */
	mutex_t		mutex;	/*!< mutex protecting the fields in this
				struct except id, which is constant */
	ulint		space;	/*!< space where the rollback segment is
				header is placed */
	ulint		zip_size;/* compressed page size of space
				in bytes, or 0 for uncompressed spaces */
	ulint		page_no;/* page number of the rollback segment
				header */
	ulint		max_size;/* maximum allowed size in pages */
	ulint		curr_size;/* current size in pages */
	/*--------------------------------------------------------*/
	/* Fields for update undo logs */
	UT_LIST_BASE_NODE_T(trx_undo_t) update_undo_list;
					/* List of update undo logs */
	UT_LIST_BASE_NODE_T(trx_undo_t) update_undo_cached;
					/* List of update undo log segments
					cached for fast reuse */
	/*--------------------------------------------------------*/
	/* Fields for insert undo logs */
	UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_list;
					/* List of insert undo logs */
	UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_cached;
					/* List of insert undo log segments
					cached for fast reuse */
	/*--------------------------------------------------------*/
	ulint		last_page_no;	/*!< Page number of the last not yet
					purged log header in the history list;
					FIL_NULL if all list purged */
	ulint		last_offset;	/*!< Byte offset of the last not yet
					purged log header */
	trx_id_t	last_trx_no;	/*!< Transaction number of the last not
					yet purged log */
	ibool		last_del_marks;	/*!< TRUE if the last not yet purged log
					needs purging */
	/*--------------------------------------------------------*/
	UT_LIST_NODE_T(trx_rseg_t) rseg_list;
					/* the list of the rollback segment
					memory objects */
};

/** For prioritising the rollback segments for purge. */
struct rseg_queue_struct {
	trx_id_t	trx_no;		/*!< trx_rseg_t::last_trx_no */
	trx_rseg_t*	rseg;		/*!< Rollback segment */
};

typedef struct rseg_queue_struct rseg_queue_t;

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
