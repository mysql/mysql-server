/******************************************************
Rollback segment

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0rseg_h
#define trx0rseg_h

#include "univ.i"
#include "trx0types.h"
#include "trx0sys.h"

/**********************************************************************
Gets a rollback segment header. */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get(
/*==========*/
				/* out: rollback segment header, page
				x-latched */
	ulint	space,		/* in: space where placed */
	ulint	page_no,	/* in: page number of the header */
	mtr_t*	mtr);		/* in: mtr */
/**********************************************************************
Gets a newly created rollback segment header. */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get_new(
/*==============*/
				/* out: rollback segment header, page
				x-latched */
	ulint	space,		/* in: space where placed */
	ulint	page_no,	/* in: page number of the header */
	mtr_t*	mtr);		/* in: mtr */
/*******************************************************************
Gets the file page number of the nth undo log slot. */
UNIV_INLINE
ulint
trx_rsegf_get_nth_undo(
/*===================*/
				/* out: page number of the undo log segment */
	trx_rsegf_t*	rsegf,	/* in: rollback segment header */
	ulint		n,	/* in: index of slot */
	mtr_t*		mtr);	/* in: mtr */
/*******************************************************************
Sets the file page number of the nth undo log slot. */
UNIV_INLINE
void
trx_rsegf_set_nth_undo(
/*===================*/
	trx_rsegf_t*	rsegf,	/* in: rollback segment header */
	ulint		n,	/* in: index of slot */
	ulint		page_no,/* in: page number of the undo log segment */
	mtr_t*		mtr);	/* in: mtr */
/********************************************************************
Looks for a free slot for an undo log segment. */
UNIV_INLINE
ulint
trx_rsegf_undo_find_free(
/*=====================*/
				/* out: slot index or ULINT_UNDEFINED if not
				found */
	trx_rsegf_t*	rsegf,	/* in: rollback segment header */
	mtr_t*		mtr);	/* in: mtr */
/**********************************************************************
Looks for a rollback segment, based on the rollback segment id. */

trx_rseg_t*
trx_rseg_get_on_id(
/*===============*/
			/* out: rollback segment */
	ulint	id);	/* in: rollback segment id */
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
	mtr_t*	mtr);		/* in: mtr */
/*************************************************************************
Creates the memory copies for rollback segments and initializes the
rseg list and array in trx_sys at a database startup. */

void
trx_rseg_list_and_array_init(
/*=========================*/
	trx_sysf_t*	sys_header,	/* in: trx system header */
	mtr_t*		mtr);		/* in: mtr */
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
	mtr_t*	mtr);		/* in: mtr */


/* Number of undo log slots in a rollback segment file copy */
#define TRX_RSEG_N_SLOTS	1024

/* Maximum number of transactions supported by a single rollback segment */
#define TRX_RSEG_MAX_N_TRXS	(TRX_RSEG_N_SLOTS / 2)

/* The rollback segment memory object */
struct trx_rseg_struct{
	/*--------------------------------------------------------*/
	ulint		id;	/* rollback segment id == the index of 
				its slot in the trx system file copy */
	mutex_t		mutex;	/* mutex protecting the fields in this
				struct except id; NOTE that the latching
				order must always be kernel mutex ->
				rseg mutex */
	ulint		space;	/* space where the rollback segment is 
				header is placed */
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
	ulint		last_page_no;	/* Page number of the last not yet
					purged log header in the history list;
					FIL_NULL if all list purged */
	ulint		last_offset;	/* Byte offset of the last not yet
					purged log header */
	dulint		last_trx_no;	/* Transaction number of the last not
					yet purged log */
	ibool		last_del_marks;	/* TRUE if the last not yet purged log
					needs purging */
	/*--------------------------------------------------------*/
	UT_LIST_NODE_T(trx_rseg_t) rseg_list;
					/* the list of the rollback segment
					memory objects */
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
