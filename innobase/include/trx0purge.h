/******************************************************
Purge old versions

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0purge_h
#define trx0purge_h

#include "univ.i"
#include "trx0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"
#include "que0types.h"
#include "page0page.h"
#include "usr0sess.h"
#include "fil0fil.h"

/* The global data structure coordinating a purge */
extern trx_purge_t*	purge_sys;

/* A dummy undo record used as a return value when we have a whole undo log
which needs no purge */
extern trx_undo_rec_t	trx_purge_dummy_rec;

/************************************************************************
Calculates the file address of an undo log header when we have the file
address of its history list node. */
UNIV_INLINE
fil_addr_t
trx_purge_get_log_from_hist(
/*========================*/
					/* out: file address of the log */
	fil_addr_t	node_addr);	/* in: file address of the history
					list node of the log */
/*********************************************************************
Checks if trx_id is >= purge_view: then it is guaranteed that its update
undo log still exists in the system. */

ibool
trx_purge_update_undo_must_exist(
/*=============================*/
			/* out: TRUE if is sure that it is preserved, also
			if the function returns FALSE, it is possible that
			the undo log still exists in the system */
	dulint	trx_id);/* in: transaction id */
/************************************************************************
Creates the global purge system control structure and inits the history
mutex. */

void
trx_purge_sys_create(void);
/*======================*/
/************************************************************************
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. */

void
trx_purge_add_update_undo_to_history(
/*=================================*/
	trx_t*	trx,		/* in: transaction */
	page_t*	undo_page,	/* in: update undo log header page,
				x-latched */
	mtr_t*	mtr);		/* in: mtr */
/************************************************************************
Fetches the next undo log record from the history list to purge. It must be
released with the corresponding release function. */

trx_undo_rec_t*
trx_purge_fetch_next_rec(
/*=====================*/
				/* out: copy of an undo log record, or
				pointer to the dummy undo log record
				&trx_purge_dummy_rec if the whole undo log
				can skipped in purge; NULL if none left */
	dulint*		roll_ptr,/* out: roll pointer to undo record */
	trx_undo_inf_t** cell,	/* out: storage cell for the record in the
				purge array */
	mem_heap_t*	heap);	/* in: memory heap where copied */
/***********************************************************************
Releases a reserved purge undo record. */

void
trx_purge_rec_release(
/*==================*/
	trx_undo_inf_t*	cell);	/* in: storage cell */
/***********************************************************************
This function runs a purge batch. */

ulint
trx_purge(void);
/*===========*/
				/* out: number of undo log pages handled in
				the batch */
/**********************************************************************
Prints information of the purge system to stderr. */

void
trx_purge_sys_print(void);
/*======================*/

/* The control structure used in the purge operation */
struct trx_purge_struct{
	ulint		state;		/* Purge system state */
	sess_t*		sess;		/* System session running the purge
					query */
	trx_t*		trx;		/* System transaction running the purge
					query: this trx is not in the trx list
					of the trx system and it never ends */
	que_t*		query;		/* The query graph which will do the
					parallelized purge operation */
	rw_lock_t	latch;		/* The latch protecting the purge view.
					A purge operation must acquire an
					x-latch here for the instant at which
					it changes the purge view: an undo
					log operation can prevent this by
					obtaining an s-latch here. */
	read_view_t*	view;		/* The purge will not remove undo logs
					which are >= this view (purge view) */
	mutex_t		mutex;		/* Mutex protecting the fields below */
	ulint		n_pages_handled;/* Approximate number of undo log
					pages processed in purge */
	ulint		handle_limit;	/* Target of how many pages to get
					processed in the current purge */
	/*------------------------------*/
	/* The following two fields form the 'purge pointer' which advances
	during a purge, and which is used in history list truncation */

	dulint		purge_trx_no;	/* Purge has advanced past all
					transactions whose number is less
					than this */
	dulint		purge_undo_no;	/* Purge has advanced past all records
					whose undo number is less than this */
	/*-----------------------------*/
	ibool		next_stored;	/* TRUE if the info of the next record
					to purge is stored below: if yes, then
					the transaction number and the undo
					number of the record are stored in
					purge_trx_no and purge_undo_no above */
	trx_rseg_t*	rseg;		/* Rollback segment for the next undo
					record to purge */
	ulint		page_no;	/* Page number for the next undo
					record to purge, page number of the
					log header, if dummy record */
	ulint		offset;		/* Page offset for the next undo
					record to purge, 0 if the dummy
					record */
	ulint		hdr_page_no;	/* Header page of the undo log where
					the next record to purge belongs */
	ulint		hdr_offset;	/* Header byte offset on the page */
	/*-----------------------------*/
	trx_undo_arr_t*	arr;		/* Array of transaction numbers and
					undo numbers of the undo records
					currently under processing in purge */
	mem_heap_t*	heap;		/* Temporary storage used during a
					purge: can be emptied after purge
					completes */
};

#define TRX_PURGE_ON		1	/* purge operation is running */
#define TRX_STOP_PURGE		2	/* purge operation is stopped, or
					it should be stopped */
#ifndef UNIV_NONINL
#include "trx0purge.ic"
#endif

#endif 
