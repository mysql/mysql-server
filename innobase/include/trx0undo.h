/******************************************************
Transaction undo log

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0undo_h
#define trx0undo_h

#include "univ.i"
#include "trx0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"
#include "page0types.h"

/***************************************************************************
Builds a roll pointer dulint. */
UNIV_INLINE
dulint
trx_undo_build_roll_ptr(
/*====================*/
				/* out: roll pointer */
	ibool	is_insert,	/* in: TRUE if insert undo log */
	ulint	rseg_id,	/* in: rollback segment id */
	ulint	page_no,	/* in: page number */
	ulint	offset);		/* in: offset of the undo entry within page */
/***************************************************************************
Decodes a roll pointer dulint. */
UNIV_INLINE
void
trx_undo_decode_roll_ptr(
/*=====================*/
	dulint	roll_ptr,	/* in: roll pointer */
	ibool*	is_insert,	/* out: TRUE if insert undo log */
	ulint*	rseg_id,	/* out: rollback segment id */
	ulint*	page_no,	/* out: page number */
	ulint*	offset);		/* out: offset of the undo entry within page */
/***************************************************************************
Returns TRUE if the roll pointer is of the insert type. */
UNIV_INLINE
ibool
trx_undo_roll_ptr_is_insert(
/*========================*/
				/* out: TRUE if insert undo log */
	dulint	roll_ptr);	/* in: roll pointer */
/*********************************************************************
Writes a roll ptr to an index page. In case that the size changes in
some future version, this function should be used instead of
mach_write_... */
UNIV_INLINE
void
trx_write_roll_ptr(
/*===============*/
	byte*	ptr,		/* in: pointer to memory where written */
	dulint	roll_ptr);	/* in: roll ptr */
/*********************************************************************
Reads a roll ptr from an index page. In case that the roll ptr size
changes in some future version, this function should be used instead of
mach_read_... */
UNIV_INLINE
dulint
trx_read_roll_ptr(
/*==============*/
			/* out: roll ptr */
	byte*	ptr);	/* in: pointer to memory from where to read */
/**********************************************************************
Gets an undo log page and x-latches it. */
UNIV_INLINE
page_t*
trx_undo_page_get(
/*===============*/
				/* out: pointer to page x-latched */
	ulint	space,		/* in: space where placed */
	ulint	page_no,	/* in: page number */
	mtr_t*	mtr);		/* in: mtr */
/**********************************************************************
Gets an undo log page and s-latches it. */
UNIV_INLINE
page_t*
trx_undo_page_get_s_latched(
/*=========================*/
				/* out: pointer to page s-latched */
	ulint	space,		/* in: space where placed */
	ulint	page_no,	/* in: page number */
	mtr_t*	mtr);		/* in: mtr */
/**********************************************************************
Returns the previous undo record on the page in the specified log, or
NULL if none exists. */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_prev_rec(
/*=======================*/
				/* out: pointer to record, NULL if none */
	trx_undo_rec_t*	rec,	/* in: undo log record */
	ulint		page_no,/* in: undo log header page number */
	ulint		offset);	/* in: undo log header offset on page */
/**********************************************************************
Returns the next undo log record on the page in the specified log, or
NULL if none exists. */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_next_rec(
/*=======================*/
				/* out: pointer to record, NULL if none */
	trx_undo_rec_t*	rec,	/* in: undo log record */
	ulint		page_no,/* in: undo log header page number */
	ulint		offset);	/* in: undo log header offset on page */
/**********************************************************************
Returns the last undo record on the page in the specified undo log, or
NULL if none exists. */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_last_rec(
/*=======================*/
			/* out: pointer to record, NULL if none */
	page_t*	undo_page,/* in: undo log page */
	ulint	page_no,/* in: undo log header page number */
	ulint	offset);	/* in: undo log header offset on page */
/**********************************************************************
Returns the first undo record on the page in the specified undo log, or
NULL if none exists. */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_first_rec(
/*========================*/
			/* out: pointer to record, NULL if none */
	page_t*	undo_page,/* in: undo log page */
	ulint	page_no,/* in: undo log header page number */
	ulint	offset);	/* in: undo log header offset on page */
/***************************************************************************
Gets the previous record in an undo log. */

trx_undo_rec_t*
trx_undo_get_prev_rec(
/*==================*/
				/* out: undo log record, the page s-latched,
				NULL if none */
	trx_undo_rec_t*	rec,	/* in: undo record */
	ulint		page_no,/* in: undo log header page number */
	ulint		offset,	/* in: undo log header offset on page */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************************
Gets the next record in an undo log. */

trx_undo_rec_t*
trx_undo_get_next_rec(
/*==================*/
				/* out: undo log record, the page s-latched,
				NULL if none */
	trx_undo_rec_t*	rec,	/* in: undo record */
	ulint		page_no,/* in: undo log header page number */
	ulint		offset,	/* in: undo log header offset on page */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************************
Gets the first record in an undo log. */

trx_undo_rec_t*
trx_undo_get_first_rec(
/*===================*/
			/* out: undo log record, the page latched, NULL if
			none */
	ulint	space,	/* in: undo log header space */	
	ulint	page_no,/* in: undo log header page number */
	ulint	offset,	/* in: undo log header offset on page */
	ulint	mode,	/* in: latching mode: RW_S_LATCH or RW_X_LATCH */
	mtr_t*	mtr);	/* in: mtr */
/************************************************************************
Tries to add a page to the undo log segment where the undo log is placed. */

ulint
trx_undo_add_page(
/*==============*/
				/* out: page number if success, else
				FIL_NULL */
	trx_t*		trx,	/* in: transaction */
	trx_undo_t*	undo,	/* in: undo log memory object */
	mtr_t*		mtr);	/* in: mtr which does not have a latch to any
				undo log page; the caller must have reserved
				the rollback segment mutex */
/***************************************************************************
Truncates an undo log from the end. This function is used during a rollback
to free space from an undo log. */

void
trx_undo_truncate_end(
/*==================*/
	trx_t*		trx,	/* in: transaction whose undo log it is */
	trx_undo_t*	undo,	/* in: undo log */
	dulint		limit);	/* in: all undo records with undo number
				>= this value should be truncated */
/***************************************************************************
Truncates an undo log from the start. This function is used during a purge
operation. */

void
trx_undo_truncate_start(
/*====================*/
	trx_rseg_t* rseg,	/* in: rollback segment */
	ulint	space,		/* in: space id of the log */
	ulint	hdr_page_no,	/* in: header page number */
	ulint	hdr_offset,	/* in: header offset on the page */
	dulint	limit);		/* in: all undo pages with undo numbers <
				this value should be truncated; NOTE that
				the function only frees whole pages; the
				header page is not freed, but emptied, if
				all the records there are < limit */
/************************************************************************
Initializes the undo log lists for a rollback segment memory copy.
This function is only called when the database is started or a new
rollback segment created. */

ulint
trx_undo_lists_init(
/*================*/
				/* out: the combined size of undo log segments
				in pages */
	trx_rseg_t*	rseg);	/* in: rollback segment memory object */	
/**************************************************************************
Assigns an undo log for a transaction. A new undo log is created or a cached
undo log reused. */

trx_undo_t*
trx_undo_assign_undo(
/*=================*/
			/* out: the undo log, NULL if did not succeed: out of
			space */
	trx_t*	trx,	/* in: transaction */
	ulint	type);	/* in: TRX_UNDO_INSERT or TRX_UNDO_UPDATE */
/**********************************************************************
Sets the state of the undo log segment at a transaction finish. */

page_t*
trx_undo_set_state_at_finish(
/*=========================*/
				/* out: undo log segment header page,
				x-latched */
	trx_t*		trx,	/* in: transaction */
	trx_undo_t*	undo,	/* in: undo log memory copy */
	mtr_t*		mtr);	/* in: mtr */
/**************************************************************************
Adds the update undo log header as the first in the history list, and
frees the memory object, or puts it to the list of cached update undo log
segments. */

void
trx_undo_update_cleanup(
/*====================*/
	trx_t*	trx,		/* in: trx owning the update undo log */
	page_t*	undo_page,	/* in: update undo log header page,
				x-latched */
	mtr_t*	mtr);		/* in: mtr */
/**************************************************************************
Discards an undo log and puts the segment to the list of cached update undo
log segments. This optimized function is called if there is no need to
keep the update undo log because there exist no read views and the transaction
made no delete markings, which would make purge necessary. We restrict this
to undo logs of size 1 to make things simpler. */

dulint
trx_undo_update_cleanup_by_discard(
/*===============================*/
			/* out: log sequence number at which mtr is
			committed */	
	trx_t*	trx,	/* in: trx owning the update undo log */
	mtr_t*	mtr);	/* in: mtr */
/**********************************************************************
Frees or caches an insert undo log after a transaction commit or rollback.
Knowledge of inserts is not needed after a commit or rollback, therefore
the data can be discarded. */

void
trx_undo_insert_cleanup(
/*====================*/
	trx_t*	trx);	/* in: transaction handle */
/***************************************************************
Parses the redo log entry of an undo log page initialization. */

byte*
trx_undo_parse_page_init(
/*======================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/***************************************************************
Parses the redo log entry of an undo log page header create or reuse. */

byte*
trx_undo_parse_page_header(
/*=======================*/
			/* out: end of log record or NULL */
	ulint	type,	/* in: MLOG_UNDO_HDR_CREATE or MLOG_UNDO_HDR_REUSE */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/***************************************************************
Parses the redo log entry of an undo log page header discard. */

byte*
trx_undo_parse_discard_latest(
/*==========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */


/* Types of an undo log segment */
#define	TRX_UNDO_INSERT		1	/* contains undo entries for inserts */
#define	TRX_UNDO_UPDATE		2	/* contains undo entries for updates
					and delete markings: in short,
					modifys (the name 'UPDATE' is a
					historical relic) */
/* States of an undo log segment */
#define TRX_UNDO_ACTIVE		1	/* contains an undo log of an active
					transaction */
#define	TRX_UNDO_CACHED		2	/* cached for quick reuse */
#define	TRX_UNDO_TO_FREE	3	/* insert undo segment can be freed */
#define	TRX_UNDO_TO_PURGE	4	/* update undo segment will not be
					reused: it can be freed in purge when
					all undo data in it is removed */

/* Transaction undo log memory object; this is protected by the undo_mutex
in the corresponding transaction object */

struct trx_undo_struct{
	/*-----------------------------*/
	ulint		id;		/* undo log slot number within the
					rollback segment */
	ulint		type;		/* TRX_UNDO_INSERT or
					TRX_UNDO_UPDATE */
	ulint		state;		/* state of the corresponding undo log
					segment */
	ibool		del_marks;	/* relevant only in an update undo log:
					this is TRUE if the transaction may
					have delete marked records, because of
					a delete of a row or an update of an
					indexed field; purge is then
					necessary. */
	dulint		trx_id;		/* id of the trx assigned to the undo
					log */
	ibool		dict_operation;	/* TRUE if a dict operation trx */
	dulint		table_id;	/* if a dict operation, then the table
					id */
	trx_rseg_t*	rseg;		/* rseg where the undo log belongs */
	/*-----------------------------*/
	ulint		space;		/* space id where the undo log
					placed */
	ulint		hdr_page_no;	/* page number of the header page in
					the undo log */
	ulint		hdr_offset;	/* header offset of the undo log on the
					page */
	ulint		last_page_no;	/* page number of the last page in the
					undo log; this may differ from
					top_page_no during a rollback */
	ulint		size;		/* current size in pages */
	/*-----------------------------*/
	ulint		empty;		/* TRUE if the stack of undo log
					records is currently empty */
	ulint		top_page_no;	/* page number where the latest undo
					log record was catenated; during
					rollback the page from which the latest
					undo record was chosen */
	ulint		top_offset;	/* offset of the latest undo record,
					i.e., the topmost element in the undo
					log if we think of it as a stack */
	dulint		top_undo_no;	/* undo number of the latest record */
	page_t*		guess_page;	/* guess for the buffer frame where
					the top page might reside */
	/*-----------------------------*/
	UT_LIST_NODE_T(trx_undo_t) undo_list;
					/* undo log objects in the rollback
					segment are chained into lists */
};

/* The offset of the undo log page header on pages of the undo log */
#define	TRX_UNDO_PAGE_HDR	FSEG_PAGE_DATA
/*-------------------------------------------------------------*/
/* Transaction undo log page header offsets */
#define	TRX_UNDO_PAGE_TYPE	0	/* TRX_UNDO_INSERT or
					TRX_UNDO_UPDATE */
#define	TRX_UNDO_PAGE_START	2	/* Byte offset where the undo log
					records for the LATEST transaction
					start on this page (remember that
					in an update undo log, the first page
					can contain several undo logs) */
#define	TRX_UNDO_PAGE_FREE	4	/* On each page of the undo log this
					field contains the byte offset of the
					first free byte on the page */
#define TRX_UNDO_PAGE_NODE	6	/* The file list node in the chain
					of undo log pages */
/*-------------------------------------------------------------*/
#define TRX_UNDO_PAGE_HDR_SIZE	(6 + FLST_NODE_SIZE)

/* An update undo segment with just one page can be reused if it has
< this number bytes used */

#define TRX_UNDO_PAGE_REUSE_LIMIT	(3 * UNIV_PAGE_SIZE / 4)

/* An update undo log segment may contain several undo logs on its first page
if the undo logs took so little space that the segment could be cached and
reused. All the undo log headers are then on the first page, and the last one
owns the undo log records on subsequent pages if the segment is bigger than
one page. If an undo log is stored in a segment, then on the first page it is
allowed to have zero undo records, but if the segment extends to several
pages, then all the rest of the pages must contain at least one undo log
record. */

/* The offset of the undo log segment header on the first page of the undo
log segment */

#define	TRX_UNDO_SEG_HDR	(TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE)
/*-------------------------------------------------------------*/
#define	TRX_UNDO_STATE		0	/* TRX_UNDO_ACTIVE, ... */
#define	TRX_UNDO_LAST_LOG	2	/* Offset of the last undo log header
					on the segment header page, 0 if
					none */
#define	TRX_UNDO_FSEG_HEADER	4	/* Header for the file segment which
					the undo log segment occupies */
#define	TRX_UNDO_PAGE_LIST	(4 + FSEG_HEADER_SIZE)
					/* Base node for the list of pages in
					the undo log segment; defined only on
					the undo log segment's first page */
/*-------------------------------------------------------------*/
/* Size of the undo log segment header */
#define TRX_UNDO_SEG_HDR_SIZE	(4 + FSEG_HEADER_SIZE + FLST_BASE_NODE_SIZE)


/* The undo log header. There can be several undo log headers on the first
page of an update undo log segment. */
/*-------------------------------------------------------------*/
#define	TRX_UNDO_TRX_ID		0	/* Transaction id */
#define	TRX_UNDO_TRX_NO		8	/* Transaction number of the
					transaction; defined only if the log
					is in a history list */
#define TRX_UNDO_DEL_MARKS	16	/* Defined only in an update undo
					log: TRUE if the transaction may have
					done delete markings of records, and
					thus purge is necessary */
#define	TRX_UNDO_LOG_START	18	/* Offset of the first undo log record
					of this log on the header page; purge
					may remove undo log record from the
					log start, and therefore this is not
					necessarily the same as this log
					header end offset */
#define	TRX_UNDO_DICT_OPERATION	20	/* TRUE if the transaction is a table
					create, index create, or drop
					transaction: in recovery
					the transaction cannot be rolled back
					in the usual way: a 'rollback' rather
					means dropping the created or dropped
					table, if it still exists */
#define TRX_UNDO_TABLE_ID	22	/* Id of the table if the preceding
					field is TRUE */
#define	TRX_UNDO_NEXT_LOG	30	/* Offset of the next undo log header
					on this page, 0 if none */
#define	TRX_UNDO_PREV_LOG	32	/* Offset of the previous undo log
					header on this page, 0 if none */
#define TRX_UNDO_HISTORY_NODE	34	/* If the log is put to the history
					list, the file list node is here */
/*-------------------------------------------------------------*/
#define TRX_UNDO_LOG_HDR_SIZE	(34 + FLST_NODE_SIZE)

#ifndef UNIV_NONINL
#include "trx0undo.ic"
#endif

#endif 
