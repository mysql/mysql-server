/******************************************************
Transaction undo log

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0undo.h"

#ifdef UNIV_NONINL
#include "trx0undo.ic"
#endif

#include "fsp0fsp.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "srv0srv.h"
#include "trx0rec.h"
#include "trx0purge.h"
#include "trx0xa.h"

/* How should the old versions in the history list be managed?
   ----------------------------------------------------------
If each transaction is given a whole page for its update undo log, file
space consumption can be 10 times higher than necessary. Therefore,
partly filled update undo log pages should be reusable. But then there
is no way individual pages can be ordered so that the ordering agrees
with the serialization numbers of the transactions on the pages. Thus,
the history list must be formed of undo logs, not their header pages as
it was in the old implementation.
	However, on a single header page the transactions are placed in
the order of their serialization numbers. As old versions are purged, we
may free the page when the last transaction on the page has been purged.
	A problem is that the purge has to go through the transactions
in the serialization order. This means that we have to look through all
rollback segments for the one that has the smallest transaction number
in its history list.
	When should we do a purge? A purge is necessary when space is
running out in any of the rollback segments. Then we may have to purge
also old version which might be needed by some consistent read. How do
we trigger the start of a purge? When a transaction writes to an undo log,
it may notice that the space is running out. When a read view is closed,
it may make some history superfluous. The server can have an utility which
periodically checks if it can purge some history.
	In a parallellized purge we have the problem that a query thread
can remove a delete marked clustered index record before another query
thread has processed an earlier version of the record, which cannot then
be done because the row cannot be constructed from the clustered index
record. To avoid this problem, we will store in the update and delete mark
undo record also the columns necessary to construct the secondary index
entries which are modified.
	We can latch the stack of versions of a single clustered index record
by taking a latch on the clustered index page. As long as the latch is held,
no new versions can be added and no versions removed by undo. But, a purge
can still remove old versions from the bottom of the stack. */

/* How to protect rollback segments, undo logs, and history lists with
   -------------------------------------------------------------------
latches?
-------
The contention of the kernel mutex should be minimized. When a transaction
does its first insert or modify in an index, an undo log is assigned for it.
Then we must have an x-latch to the rollback segment header.
	When the transaction does more modifys or rolls back, the undo log is
protected with undo_mutex in the transaction.
	When the transaction commits, its insert undo log is either reset and
cached for a fast reuse, or freed. In these cases we must have an x-latch on
the rollback segment page. The update undo log is put to the history list. If
it is not suitable for reuse, its slot in the rollback segment is reset. In
both cases, an x-latch must be acquired on the rollback segment.
	The purge operation steps through the history list without modifying
it until a truncate operation occurs, which can remove undo logs from the end
of the list and release undo log segments. In stepping through the list,
s-latches on the undo log pages are enough, but in a truncate, x-latches must
be obtained on the rollback segment and individual pages. */

/************************************************************************
Initializes the fields in an undo log segment page. */
static
void
trx_undo_page_init(
/*===============*/
	page_t* undo_page,	/* in: undo log segment page */
	ulint	type,		/* in: undo log segment type */
	mtr_t*	mtr);		/* in: mtr */
/************************************************************************
Creates and initializes an undo log memory object. */
static
trx_undo_t*
trx_undo_mem_create(
/*================*/
				/* out, own: the undo log memory object */
	trx_rseg_t*	rseg,	/* in: rollback segment memory object */
	ulint		id,	/* in: slot index within rseg */
	ulint		type,	/* in: type of the log: TRX_UNDO_INSERT or
				TRX_UNDO_UPDATE */
	dulint		trx_id,	/* in: id of the trx for which the undo log
				is created */
	XID*		xid,	/* in: X/Open XA transaction identification*/
	ulint		page_no,/* in: undo log header page number */
	ulint		offset);/* in: undo log header byte offset on page */
/*******************************************************************
Initializes a cached insert undo log header page for new use. NOTE that this
function has its own log record type MLOG_UNDO_HDR_REUSE. You must NOT change
the operation of this function! */
static
ulint
trx_undo_insert_header_reuse(
/*=========================*/
				/* out: undo log header byte offset on page */
	page_t*	undo_page,	/* in: insert undo log segment header page,
				x-latched */
	dulint	trx_id,		/* in: transaction id */
	mtr_t*	mtr);		/* in: mtr */
/**************************************************************************
If an update undo log can be discarded immediately, this function frees the
space, resetting the page to the proper state for caching. */
static
void
trx_undo_discard_latest_update_undo(
/*================================*/
	page_t*	undo_page,	/* in: header page of an undo log of size 1 */
	mtr_t*	mtr);		/* in: mtr */


/***************************************************************************
Gets the previous record in an undo log from the previous page. */
static
trx_undo_rec_t*
trx_undo_get_prev_rec_from_prev_page(
/*=================================*/
				/* out: undo log record, the page s-latched,
				NULL if none */
	trx_undo_rec_t*	rec,	/* in: undo record */
	ulint		page_no,/* in: undo log header page number */
	ulint		offset,	/* in: undo log header offset on page */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	prev_page_no;
	page_t* prev_page;
	page_t*	undo_page;

	undo_page = buf_frame_align(rec);

	prev_page_no = flst_get_prev_addr(undo_page + TRX_UNDO_PAGE_HDR
					  + TRX_UNDO_PAGE_NODE, mtr)
		.page;

	if (prev_page_no == FIL_NULL) {

		return(NULL);
	}

	prev_page = trx_undo_page_get_s_latched
		(buf_frame_get_space_id(undo_page), prev_page_no, mtr);

	return(trx_undo_page_get_last_rec(prev_page, page_no, offset));
}

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
	mtr_t*		mtr)	/* in: mtr */
{
	trx_undo_rec_t*	prev_rec;

	prev_rec = trx_undo_page_get_prev_rec(rec, page_no, offset);

	if (prev_rec) {

		return(prev_rec);
	}

	/* We have to go to the previous undo log page to look for the
	previous record */

	return(trx_undo_get_prev_rec_from_prev_page(rec, page_no, offset,
						    mtr));
}

/***************************************************************************
Gets the next record in an undo log from the next page. */
static
trx_undo_rec_t*
trx_undo_get_next_rec_from_next_page(
/*=================================*/
			/* out: undo log record, the page latched, NULL if
			none */
	page_t*	undo_page, /* in: undo log page */
	ulint	page_no,/* in: undo log header page number */
	ulint	offset,	/* in: undo log header offset on page */
	ulint	mode,	/* in: latch mode: RW_S_LATCH or RW_X_LATCH */
	mtr_t*	mtr)	/* in: mtr */
{
	trx_ulogf_t*	log_hdr;
	ulint		next_page_no;
	page_t*		next_page;
	ulint		space;
	ulint		next;

	if (page_no == buf_frame_get_page_no(undo_page)) {

		log_hdr = undo_page + offset;
		next = mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG);

		if (next != 0) {

			return(NULL);
		}
	}

	space = buf_frame_get_space_id(undo_page);

	next_page_no = flst_get_next_addr(undo_page + TRX_UNDO_PAGE_HDR
					  + TRX_UNDO_PAGE_NODE, mtr)
		.page;
	if (next_page_no == FIL_NULL) {

		return(NULL);
	}

	if (mode == RW_S_LATCH) {
		next_page = trx_undo_page_get_s_latched(space, next_page_no,
							mtr);
	} else {
		ut_ad(mode == RW_X_LATCH);
		next_page = trx_undo_page_get(space, next_page_no, mtr);
	}

	return(trx_undo_page_get_first_rec(next_page, page_no, offset));
}

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
	mtr_t*		mtr)	/* in: mtr */
{
	trx_undo_rec_t*	next_rec;

	next_rec = trx_undo_page_get_next_rec(rec, page_no, offset);

	if (next_rec) {
		return(next_rec);
	}

	return(trx_undo_get_next_rec_from_next_page(buf_frame_align(rec),
						    page_no, offset,
						    RW_S_LATCH, mtr));
}

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
	mtr_t*	mtr)	/* in: mtr */
{
	page_t*		undo_page;
	trx_undo_rec_t*	rec;

	if (mode == RW_S_LATCH) {
		undo_page = trx_undo_page_get_s_latched(space, page_no, mtr);
	} else {
		undo_page = trx_undo_page_get(space, page_no, mtr);
	}

	rec = trx_undo_page_get_first_rec(undo_page, page_no, offset);

	if (rec) {
		return(rec);
	}

	return(trx_undo_get_next_rec_from_next_page(undo_page, page_no, offset,
						    mode, mtr));
}

/*============== UNDO LOG FILE COPY CREATION AND FREEING ==================*/

/**************************************************************************
Writes the mtr log entry of an undo log page initialization. */
UNIV_INLINE
void
trx_undo_page_init_log(
/*===================*/
	page_t* undo_page,	/* in: undo log page */
	ulint	type,		/* in: undo log type */
	mtr_t*	mtr)		/* in: mtr */
{
	mlog_write_initial_log_record(undo_page, MLOG_UNDO_INIT, mtr);

	mlog_catenate_ulint_compressed(mtr, type);
}

/***************************************************************
Parses the redo log entry of an undo log page initialization. */

byte*
trx_undo_parse_page_init(
/*=====================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ulint	type;

	ptr = mach_parse_compressed(ptr, end_ptr, &type);

	if (ptr == NULL) {

		return(NULL);
	}

	if (page) {
		trx_undo_page_init(page, type, mtr);
	}

	return(ptr);
}

/************************************************************************
Initializes the fields in an undo log segment page. */
static
void
trx_undo_page_init(
/*===============*/
	page_t* undo_page,	/* in: undo log segment page */
	ulint	type,		/* in: undo log segment type */
	mtr_t*	mtr)		/* in: mtr */
{
	trx_upagef_t*	page_hdr;

	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_TYPE, type);

	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START,
			TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);
	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE,
			TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);

	fil_page_set_type(undo_page, FIL_PAGE_UNDO_LOG);

	trx_undo_page_init_log(undo_page, type, mtr);
}

/*******************************************************************
Creates a new undo log segment in file. */
static
page_t*
trx_undo_seg_create(
/*================*/
				/* out: segment header page x-latched, NULL
				if no space left */
	trx_rseg_t*	rseg __attribute__((unused)),/* in: rollback segment */
	trx_rsegf_t*	rseg_hdr,/* in: rollback segment header, page
				x-latched */
	ulint		type,	/* in: type of the segment: TRX_UNDO_INSERT or
				TRX_UNDO_UPDATE */
	ulint*		id,	/* out: slot index within rseg header */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint		slot_no;
	ulint		space;
	page_t*		undo_page;
	trx_upagef_t*	page_hdr;
	trx_usegf_t*	seg_hdr;
	ulint		n_reserved;
	ibool		success;

	ut_ad(mtr && id && rseg_hdr);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	/*	fputs(type == TRX_UNDO_INSERT
	? "Creating insert undo log segment\n"
	: "Creating update undo log segment\n", stderr); */
	slot_no = trx_rsegf_undo_find_free(rseg_hdr, mtr);

	if (slot_no == ULINT_UNDEFINED) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"InnoDB: Warning: cannot find a free slot for"
			" an undo log. Do you have too\n"
			"InnoDB: many active transactions"
			" running concurrently?\n");

		return(NULL);
	}

	space = buf_frame_get_space_id(rseg_hdr);

	success = fsp_reserve_free_extents(&n_reserved, space, 2, FSP_UNDO,
					   mtr);
	if (!success) {

		return(NULL);
	}

	/* Allocate a new file segment for the undo log */
	undo_page = fseg_create_general(space, 0,
					TRX_UNDO_SEG_HDR
					+ TRX_UNDO_FSEG_HEADER, TRUE, mtr);

	fil_space_release_free_extents(space, n_reserved);

	if (undo_page == NULL) {
		/* No space left */

		return(NULL);
	}

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(undo_page, SYNC_TRX_UNDO_PAGE);
#endif /* UNIV_SYNC_DEBUG */

	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	trx_undo_page_init(undo_page, type, mtr);

	mlog_write_ulint(page_hdr + TRX_UNDO_PAGE_FREE,
			 TRX_UNDO_SEG_HDR + TRX_UNDO_SEG_HDR_SIZE,
			 MLOG_2BYTES, mtr);

	mlog_write_ulint(seg_hdr + TRX_UNDO_LAST_LOG, 0, MLOG_2BYTES, mtr);

	flst_init(seg_hdr + TRX_UNDO_PAGE_LIST, mtr);

	flst_add_last(seg_hdr + TRX_UNDO_PAGE_LIST,
		      page_hdr + TRX_UNDO_PAGE_NODE, mtr);

	trx_rsegf_set_nth_undo(rseg_hdr, slot_no,
			       buf_frame_get_page_no(undo_page), mtr);
	*id = slot_no;

	return(undo_page);
}

/**************************************************************************
Writes the mtr log entry of an undo log header initialization. */
UNIV_INLINE
void
trx_undo_header_create_log(
/*=======================*/
	page_t* undo_page,	/* in: undo log header page */
	dulint	trx_id,		/* in: transaction id */
	mtr_t*	mtr)		/* in: mtr */
{
	mlog_write_initial_log_record(undo_page, MLOG_UNDO_HDR_CREATE, mtr);

	mlog_catenate_dulint_compressed(mtr, trx_id);
}

/*******************************************************************
Creates a new undo log header in file. NOTE that this function has its own
log record type MLOG_UNDO_HDR_CREATE. You must NOT change the operation of
this function! */
static
ulint
trx_undo_header_create(
/*===================*/
				/* out: header byte offset on page */
	page_t*	undo_page,	/* in: undo log segment header page,
				x-latched; it is assumed that there are
				TRX_UNDO_LOG_XA_HDR_SIZE bytes free space
				on it */
	dulint	trx_id,		/* in: transaction id */
	mtr_t*	mtr)		/* in: mtr */
{
	trx_upagef_t*	page_hdr;
	trx_usegf_t*	seg_hdr;
	trx_ulogf_t*	log_hdr;
	trx_ulogf_t*	prev_log_hdr;
	ulint		prev_log;
	ulint		free;
	ulint		new_free;

	ut_ad(mtr && undo_page);

	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	free = mach_read_from_2(page_hdr + TRX_UNDO_PAGE_FREE);

	log_hdr = undo_page + free;

	new_free = free + TRX_UNDO_LOG_OLD_HDR_SIZE;

	ut_a(free + TRX_UNDO_LOG_XA_HDR_SIZE < UNIV_PAGE_SIZE - 100);

	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START, new_free);

	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, new_free);

	mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_ACTIVE);

	prev_log = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);

	if (prev_log != 0) {
		prev_log_hdr = undo_page + prev_log;

		mach_write_to_2(prev_log_hdr + TRX_UNDO_NEXT_LOG, free);
	}

	mach_write_to_2(seg_hdr + TRX_UNDO_LAST_LOG, free);

	log_hdr = undo_page + free;

	mach_write_to_2(log_hdr + TRX_UNDO_DEL_MARKS, TRUE);

	mach_write_to_8(log_hdr + TRX_UNDO_TRX_ID, trx_id);
	mach_write_to_2(log_hdr + TRX_UNDO_LOG_START, new_free);

	mach_write_to_1(log_hdr + TRX_UNDO_XID_EXISTS, FALSE);
	mach_write_to_1(log_hdr + TRX_UNDO_DICT_TRANS, FALSE);

	mach_write_to_2(log_hdr + TRX_UNDO_NEXT_LOG, 0);
	mach_write_to_2(log_hdr + TRX_UNDO_PREV_LOG, prev_log);

	/* Write the log record about the header creation */
	trx_undo_header_create_log(undo_page, trx_id, mtr);

	return(free);
}

/************************************************************************
Write X/Open XA Transaction Identification (XID) to undo log header */
static
void
trx_undo_write_xid(
/*===============*/
	trx_ulogf_t*	log_hdr,/* in: undo log header */
	const XID*	xid,	/* in: X/Open XA Transaction Identification */
	mtr_t*		mtr)	/* in: mtr */
{
	mlog_write_ulint(log_hdr + TRX_UNDO_XA_FORMAT,
			 (ulint)xid->formatID, MLOG_4BYTES, mtr);

	mlog_write_ulint(log_hdr + TRX_UNDO_XA_TRID_LEN,
			 (ulint)xid->gtrid_length, MLOG_4BYTES, mtr);

	mlog_write_ulint(log_hdr + TRX_UNDO_XA_BQUAL_LEN,
			 (ulint)xid->bqual_length, MLOG_4BYTES, mtr);

	mlog_write_string(log_hdr + TRX_UNDO_XA_XID, (const byte*) xid->data,
			  XIDDATASIZE, mtr);
}

/************************************************************************
Read X/Open XA Transaction Identification (XID) from undo log header */
static
void
trx_undo_read_xid(
/*==============*/
	trx_ulogf_t*	log_hdr,/* in: undo log header */
	XID*		xid)	/* out: X/Open XA Transaction Identification */
{
	xid->formatID = (long)mach_read_from_4(log_hdr + TRX_UNDO_XA_FORMAT);

	xid->gtrid_length
		= (long) mach_read_from_4(log_hdr + TRX_UNDO_XA_TRID_LEN);
	xid->bqual_length
		= (long) mach_read_from_4(log_hdr + TRX_UNDO_XA_BQUAL_LEN);

	memcpy(xid->data, log_hdr + TRX_UNDO_XA_XID, XIDDATASIZE);
}

/*******************************************************************
Adds space for the XA XID after an undo log old-style header. */
static
void
trx_undo_header_add_space_for_xid(
/*==============================*/
	page_t*		undo_page,/* in: undo log segment header page */
	trx_ulogf_t*	log_hdr,/* in: undo log header */
	mtr_t*		mtr)	/* in: mtr */
{
	trx_upagef_t*	page_hdr;
	ulint		free;
	ulint		new_free;

	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

	free = mach_read_from_2(page_hdr + TRX_UNDO_PAGE_FREE);

	/* free is now the end offset of the old style undo log header */

	ut_a(free == (ulint)(log_hdr - undo_page) + TRX_UNDO_LOG_OLD_HDR_SIZE);

	new_free = free + (TRX_UNDO_LOG_XA_HDR_SIZE
			   - TRX_UNDO_LOG_OLD_HDR_SIZE);

	/* Add space for a XID after the header, update the free offset
	fields on the undo log page and in the undo log header */

	mlog_write_ulint(page_hdr + TRX_UNDO_PAGE_START, new_free,
			 MLOG_2BYTES, mtr);

	mlog_write_ulint(page_hdr + TRX_UNDO_PAGE_FREE, new_free,
			 MLOG_2BYTES, mtr);

	mlog_write_ulint(log_hdr + TRX_UNDO_LOG_START, new_free,
			 MLOG_2BYTES, mtr);
}

/**************************************************************************
Writes the mtr log entry of an undo log header reuse. */
UNIV_INLINE
void
trx_undo_insert_header_reuse_log(
/*=============================*/
	page_t* undo_page,	/* in: undo log header page */
	dulint	trx_id,		/* in: transaction id */
	mtr_t*	mtr)		/* in: mtr */
{
	mlog_write_initial_log_record(undo_page, MLOG_UNDO_HDR_REUSE, mtr);

	mlog_catenate_dulint_compressed(mtr, trx_id);
}

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
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	dulint	trx_id;

	ptr = mach_dulint_parse_compressed(ptr, end_ptr, &trx_id);

	if (ptr == NULL) {

		return(NULL);
	}

	if (page) {
		if (type == MLOG_UNDO_HDR_CREATE) {
			trx_undo_header_create(page, trx_id, mtr);
		} else {
			ut_ad(type == MLOG_UNDO_HDR_REUSE);
			trx_undo_insert_header_reuse(page, trx_id, mtr);
		}
	}

	return(ptr);
}

/*******************************************************************
Initializes a cached insert undo log header page for new use. NOTE that this
function has its own log record type MLOG_UNDO_HDR_REUSE. You must NOT change
the operation of this function! */
static
ulint
trx_undo_insert_header_reuse(
/*=========================*/
				/* out: undo log header byte offset on page */
	page_t*	undo_page,	/* in: insert undo log segment header page,
				x-latched */
	dulint	trx_id,		/* in: transaction id */
	mtr_t*	mtr)		/* in: mtr */
{
	trx_upagef_t*	page_hdr;
	trx_usegf_t*	seg_hdr;
	trx_ulogf_t*	log_hdr;
	ulint		free;
	ulint		new_free;

	ut_ad(mtr && undo_page);

	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;
	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

	free = TRX_UNDO_SEG_HDR + TRX_UNDO_SEG_HDR_SIZE;

	ut_a(free + TRX_UNDO_LOG_XA_HDR_SIZE < UNIV_PAGE_SIZE - 100);

	log_hdr = undo_page + free;

	new_free = free + TRX_UNDO_LOG_OLD_HDR_SIZE;

	/* Insert undo data is not needed after commit: we may free all
	the space on the page */

	ut_a(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
			      + TRX_UNDO_PAGE_TYPE)
	     == TRX_UNDO_INSERT);

	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START, new_free);

	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, new_free);

	mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_ACTIVE);

	log_hdr = undo_page + free;

	mach_write_to_8(log_hdr + TRX_UNDO_TRX_ID, trx_id);
	mach_write_to_2(log_hdr + TRX_UNDO_LOG_START, new_free);

	mach_write_to_1(log_hdr + TRX_UNDO_XID_EXISTS, FALSE);
	mach_write_to_1(log_hdr + TRX_UNDO_DICT_TRANS, FALSE);

	/* Write the log record MLOG_UNDO_HDR_REUSE */
	trx_undo_insert_header_reuse_log(undo_page, trx_id, mtr);

	return(free);
}

/**************************************************************************
Writes the redo log entry of an update undo log header discard. */
UNIV_INLINE
void
trx_undo_discard_latest_log(
/*========================*/
	page_t* undo_page,	/* in: undo log header page */
	mtr_t*	mtr)		/* in: mtr */
{
	mlog_write_initial_log_record(undo_page, MLOG_UNDO_HDR_DISCARD, mtr);
}

/***************************************************************
Parses the redo log entry of an undo log page header discard. */

byte*
trx_undo_parse_discard_latest(
/*==========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr __attribute__((unused)), /* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ut_ad(end_ptr);

	if (page) {
		trx_undo_discard_latest_update_undo(page, mtr);
	}

	return(ptr);
}

/**************************************************************************
If an update undo log can be discarded immediately, this function frees the
space, resetting the page to the proper state for caching. */
static
void
trx_undo_discard_latest_update_undo(
/*================================*/
	page_t*	undo_page,	/* in: header page of an undo log of size 1 */
	mtr_t*	mtr)		/* in: mtr */
{
	trx_usegf_t*	seg_hdr;
	trx_upagef_t*	page_hdr;
	trx_ulogf_t*	log_hdr;
	trx_ulogf_t*	prev_log_hdr;
	ulint		free;
	ulint		prev_hdr_offset;

	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

	free = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);
	log_hdr = undo_page + free;

	prev_hdr_offset = mach_read_from_2(log_hdr + TRX_UNDO_PREV_LOG);

	if (prev_hdr_offset != 0) {
		prev_log_hdr = undo_page + prev_hdr_offset;

		mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START,
				mach_read_from_2(prev_log_hdr
						 + TRX_UNDO_LOG_START));
		mach_write_to_2(prev_log_hdr + TRX_UNDO_NEXT_LOG, 0);
	}

	mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, free);

	mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_CACHED);
	mach_write_to_2(seg_hdr + TRX_UNDO_LAST_LOG, prev_hdr_offset);

	trx_undo_discard_latest_log(undo_page, mtr);
}

/************************************************************************
Tries to add a page to the undo log segment where the undo log is placed. */

ulint
trx_undo_add_page(
/*==============*/
				/* out: page number if success, else
				FIL_NULL */
	trx_t*		trx,	/* in: transaction */
	trx_undo_t*	undo,	/* in: undo log memory object */
	mtr_t*		mtr)	/* in: mtr which does not have a latch to any
				undo log page; the caller must have reserved
				the rollback segment mutex */
{
	page_t*		header_page;
	page_t*		new_page;
	trx_rseg_t*	rseg;
	ulint		page_no;
	ulint		n_reserved;
	ibool		success;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(trx->undo_mutex)));
	ut_ad(!mutex_own(&kernel_mutex));
	ut_ad(mutex_own(&(trx->rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	rseg = trx->rseg;

	if (rseg->curr_size == rseg->max_size) {

		return(FIL_NULL);
	}

	header_page = trx_undo_page_get(undo->space, undo->hdr_page_no, mtr);

	success = fsp_reserve_free_extents(&n_reserved, undo->space, 1,
					   FSP_UNDO, mtr);
	if (!success) {

		return(FIL_NULL);
	}

	page_no = fseg_alloc_free_page_general(header_page + TRX_UNDO_SEG_HDR
					       + TRX_UNDO_FSEG_HEADER,
					       undo->top_page_no + 1, FSP_UP,
					       TRUE, mtr);

	fil_space_release_free_extents(undo->space, n_reserved);

	if (page_no == FIL_NULL) {

		/* No space left */

		return(FIL_NULL);
	}

	undo->last_page_no = page_no;

	new_page = trx_undo_page_get(undo->space, page_no, mtr);

	trx_undo_page_init(new_page, undo->type, mtr);

	flst_add_last(header_page + TRX_UNDO_SEG_HDR + TRX_UNDO_PAGE_LIST,
		      new_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE, mtr);
	undo->size++;
	rseg->curr_size++;

	return(page_no);
}

/************************************************************************
Frees an undo log page that is not the header page. */
static
ulint
trx_undo_free_page(
/*===============*/
				/* out: last page number in remaining log */
	trx_rseg_t* rseg,	/* in: rollback segment */
	ibool	in_history,	/* in: TRUE if the undo log is in the history
				list */
	ulint	space,		/* in: space */
	ulint	hdr_page_no,	/* in: header page number */
	ulint	page_no,	/* in: page number to free: must not be the
				header page */
	mtr_t*	mtr)		/* in: mtr which does not have a latch to any
				undo log page; the caller must have reserved
				the rollback segment mutex */
{
	page_t*		header_page;
	page_t*		undo_page;
	fil_addr_t	last_addr;
	trx_rsegf_t*	rseg_header;
	ulint		hist_size;

	ut_a(hdr_page_no != page_no);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex));
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	undo_page = trx_undo_page_get(space, page_no, mtr);

	header_page = trx_undo_page_get(space, hdr_page_no, mtr);

	flst_remove(header_page + TRX_UNDO_SEG_HDR + TRX_UNDO_PAGE_LIST,
		    undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE, mtr);

	fseg_free_page(header_page + TRX_UNDO_SEG_HDR + TRX_UNDO_FSEG_HEADER,
		       space, page_no, mtr);

	last_addr = flst_get_last(header_page + TRX_UNDO_SEG_HDR
				  + TRX_UNDO_PAGE_LIST, mtr);
	rseg->curr_size--;

	if (in_history) {
		rseg_header = trx_rsegf_get(space, rseg->page_no, mtr);

		hist_size = mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
					   MLOG_4BYTES, mtr);
		ut_ad(hist_size > 0);
		mlog_write_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
				 hist_size - 1, MLOG_4BYTES, mtr);
	}

	return(last_addr.page);
}

/************************************************************************
Frees an undo log page when there is also the memory object for the undo
log. */
static
void
trx_undo_free_page_in_rollback(
/*===========================*/
	trx_t*		trx __attribute__((unused)), /* in: transaction */
	trx_undo_t*	undo,	/* in: undo log memory copy */
	ulint		page_no,/* in: page number to free: must not be the
				header page */
	mtr_t*		mtr)	/* in: mtr which does not have a latch to any
				undo log page; the caller must have reserved
				the rollback segment mutex */
{
	ulint	last_page_no;

	ut_ad(undo->hdr_page_no != page_no);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(trx->undo_mutex)));
#endif /* UNIV_SYNC_DEBUG */

	last_page_no = trx_undo_free_page(undo->rseg, FALSE, undo->space,
					  undo->hdr_page_no, page_no, mtr);

	undo->last_page_no = last_page_no;
	undo->size--;
}

/************************************************************************
Empties an undo log header page of undo records for that undo log. Other
undo logs may still have records on that page, if it is an update undo log. */
static
void
trx_undo_empty_header_page(
/*=======================*/
	ulint	space,		/* in: space */
	ulint	hdr_page_no,	/* in: header page number */
	ulint	hdr_offset,	/* in: header offset */
	mtr_t*	mtr)		/* in: mtr */
{
	page_t*		header_page;
	trx_ulogf_t*	log_hdr;
	ulint		end;

	header_page = trx_undo_page_get(space, hdr_page_no, mtr);

	log_hdr = header_page + hdr_offset;

	end = trx_undo_page_get_end(header_page, hdr_page_no, hdr_offset);

	mlog_write_ulint(log_hdr + TRX_UNDO_LOG_START, end, MLOG_2BYTES, mtr);
}

/***************************************************************************
Truncates an undo log from the end. This function is used during a rollback
to free space from an undo log. */

void
trx_undo_truncate_end(
/*==================*/
	trx_t*		trx,	/* in: transaction whose undo log it is */
	trx_undo_t*	undo,	/* in: undo log */
	dulint		limit)	/* in: all undo records with undo number
				>= this value should be truncated */
{
	page_t*		undo_page;
	ulint		last_page_no;
	trx_undo_rec_t* rec;
	trx_undo_rec_t* trunc_here;
	trx_rseg_t*	rseg;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(trx->undo_mutex)));
	ut_ad(mutex_own(&(trx->rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	rseg = trx->rseg;

	for (;;) {
		mtr_start(&mtr);

		trunc_here = NULL;

		last_page_no = undo->last_page_no;

		undo_page = trx_undo_page_get(undo->space, last_page_no, &mtr);

		rec = trx_undo_page_get_last_rec(undo_page, undo->hdr_page_no,
						 undo->hdr_offset);
		for (;;) {
			if (rec == NULL) {
				if (last_page_no == undo->hdr_page_no) {

					goto function_exit;
				}

				trx_undo_free_page_in_rollback
					(trx, undo, last_page_no, &mtr);
				break;
			}

			if (ut_dulint_cmp(trx_undo_rec_get_undo_no(rec), limit)
			    >= 0) {
				/* Truncate at least this record off, maybe
				more */
				trunc_here = rec;
			} else {
				goto function_exit;
			}

			rec = trx_undo_page_get_prev_rec(rec,
							 undo->hdr_page_no,
							 undo->hdr_offset);
		}

		mtr_commit(&mtr);
	}

function_exit:
	if (trunc_here) {
		mlog_write_ulint(undo_page + TRX_UNDO_PAGE_HDR
				 + TRX_UNDO_PAGE_FREE,
				 trunc_here - undo_page, MLOG_2BYTES, &mtr);
	}

	mtr_commit(&mtr);
}

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
	dulint	limit)		/* in: all undo pages with undo numbers <
				this value should be truncated; NOTE that
				the function only frees whole pages; the
				header page is not freed, but emptied, if
				all the records there are < limit */
{
	page_t*		undo_page;
	trx_undo_rec_t* rec;
	trx_undo_rec_t* last_rec;
	ulint		page_no;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (0 == ut_dulint_cmp(limit, ut_dulint_zero)) {

		return;
	}
loop:
	mtr_start(&mtr);

	rec = trx_undo_get_first_rec(space, hdr_page_no, hdr_offset,
				     RW_X_LATCH, &mtr);
	if (rec == NULL) {
		/* Already empty */

		mtr_commit(&mtr);

		return;
	}

	undo_page = buf_frame_align(rec);

	last_rec = trx_undo_page_get_last_rec(undo_page, hdr_page_no,
					      hdr_offset);
	if (ut_dulint_cmp(trx_undo_rec_get_undo_no(last_rec), limit) >= 0) {

		mtr_commit(&mtr);

		return;
	}

	page_no = buf_frame_get_page_no(undo_page);

	if (page_no == hdr_page_no) {
		trx_undo_empty_header_page(space, hdr_page_no, hdr_offset,
					   &mtr);
	} else {
		trx_undo_free_page(rseg, TRUE, space, hdr_page_no,
				   page_no, &mtr);
	}

	mtr_commit(&mtr);

	goto loop;
}

/**************************************************************************
Frees an undo log segment which is not in the history list. */
static
void
trx_undo_seg_free(
/*==============*/
	trx_undo_t*	undo)	/* in: undo log */
{
	trx_rseg_t*	rseg;
	fseg_header_t*	file_seg;
	trx_rsegf_t*	rseg_header;
	trx_usegf_t*	seg_header;
	ibool		finished;
	mtr_t		mtr;

	finished = FALSE;
	rseg = undo->rseg;

	while (!finished) {

		mtr_start(&mtr);
#ifdef UNIV_SYNC_DEBUG
		ut_ad(!mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
		mutex_enter(&(rseg->mutex));

		seg_header = trx_undo_page_get(undo->space, undo->hdr_page_no,
					       &mtr) + TRX_UNDO_SEG_HDR;

		file_seg = seg_header + TRX_UNDO_FSEG_HEADER;

		finished = fseg_free_step(file_seg, &mtr);

		if (finished) {
			/* Update the rseg header */
			rseg_header = trx_rsegf_get(rseg->space, rseg->page_no,
						    &mtr);
			trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL,
					       &mtr);
		}

		mutex_exit(&(rseg->mutex));
		mtr_commit(&mtr);
	}
}

/*========== UNDO LOG MEMORY COPY INITIALIZATION =====================*/

/************************************************************************
Creates and initializes an undo log memory object according to the values
in the header in file, when the database is started. The memory object is
inserted in the appropriate list of rseg. */
static
trx_undo_t*
trx_undo_mem_create_at_db_start(
/*============================*/
				/* out, own: the undo log memory object */
	trx_rseg_t*	rseg,	/* in: rollback segment memory object */
	ulint		id,	/* in: slot index within rseg */
	ulint		page_no,/* in: undo log segment page number */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*		undo_page;
	trx_upagef_t*	page_header;
	trx_usegf_t*	seg_header;
	trx_ulogf_t*	undo_header;
	trx_undo_t*	undo;
	ulint		type;
	ulint		state;
	dulint		trx_id;
	ulint		offset;
	fil_addr_t	last_addr;
	page_t*		last_page;
	trx_undo_rec_t*	rec;
	XID		xid;
	ibool		xid_exists = FALSE;

	if (id >= TRX_RSEG_N_SLOTS) {
		fprintf(stderr,
			"InnoDB: Error: undo->id is %lu\n", (ulong) id);
		ut_error;
	}

	undo_page = trx_undo_page_get(rseg->space, page_no, mtr);

	page_header = undo_page + TRX_UNDO_PAGE_HDR;

	type = mtr_read_ulint(page_header + TRX_UNDO_PAGE_TYPE, MLOG_2BYTES,
			      mtr);
	seg_header = undo_page + TRX_UNDO_SEG_HDR;

	state = mach_read_from_2(seg_header + TRX_UNDO_STATE);

	offset = mach_read_from_2(seg_header + TRX_UNDO_LAST_LOG);

	undo_header = undo_page + offset;

	trx_id = mtr_read_dulint(undo_header + TRX_UNDO_TRX_ID, mtr);

	xid_exists = mtr_read_ulint(undo_header + TRX_UNDO_XID_EXISTS,
				    MLOG_1BYTE, mtr);

	/* Read X/Open XA transaction identification if it exists, or
	set it to NULL. */

	memset(&xid, 0, sizeof(xid));
	xid.formatID = -1;

	if (xid_exists == TRUE) {
		trx_undo_read_xid(undo_header, &xid);
	}

	mutex_enter(&(rseg->mutex));

	undo = trx_undo_mem_create(rseg, id, type, trx_id, &xid,
				   page_no, offset);
	mutex_exit(&(rseg->mutex));

	undo->dict_operation =	mtr_read_ulint
		(undo_header + TRX_UNDO_DICT_TRANS, MLOG_1BYTE, mtr);

	undo->table_id = mtr_read_dulint(undo_header + TRX_UNDO_TABLE_ID, mtr);
	undo->state = state;
	undo->size = flst_get_len(seg_header + TRX_UNDO_PAGE_LIST, mtr);

	/* If the log segment is being freed, the page list is inconsistent! */
	if (state == TRX_UNDO_TO_FREE) {

		goto add_to_list;
	}

	last_addr = flst_get_last(seg_header + TRX_UNDO_PAGE_LIST, mtr);

	undo->last_page_no = last_addr.page;
	undo->top_page_no = last_addr.page;

	last_page = trx_undo_page_get(rseg->space, undo->last_page_no, mtr);

	rec = trx_undo_page_get_last_rec(last_page, page_no, offset);

	if (rec == NULL) {
		undo->empty = TRUE;
	} else {
		undo->empty = FALSE;
		undo->top_offset = rec - last_page;
		undo->top_undo_no = trx_undo_rec_get_undo_no(rec);
	}
add_to_list:
	if (type == TRX_UNDO_INSERT) {
		if (state != TRX_UNDO_CACHED) {
			UT_LIST_ADD_LAST(undo_list, rseg->insert_undo_list,
					 undo);
		} else {
			UT_LIST_ADD_LAST(undo_list, rseg->insert_undo_cached,
					 undo);
		}
	} else {
		ut_ad(type == TRX_UNDO_UPDATE);
		if (state != TRX_UNDO_CACHED) {
			UT_LIST_ADD_LAST(undo_list, rseg->update_undo_list,
					 undo);
		} else {
			UT_LIST_ADD_LAST(undo_list, rseg->update_undo_cached,
					 undo);
		}
	}

	return(undo);
}

/************************************************************************
Initializes the undo log lists for a rollback segment memory copy. This
function is only called when the database is started or a new rollback
segment is created. */

ulint
trx_undo_lists_init(
/*================*/
				/* out: the combined size of undo log segments
				in pages */
	trx_rseg_t*	rseg)	/* in: rollback segment memory object */
{
	ulint		page_no;
	trx_undo_t*	undo;
	ulint		size	= 0;
	trx_rsegf_t*	rseg_header;
	ulint		i;
	mtr_t		mtr;

	UT_LIST_INIT(rseg->update_undo_list);
	UT_LIST_INIT(rseg->update_undo_cached);
	UT_LIST_INIT(rseg->insert_undo_list);
	UT_LIST_INIT(rseg->insert_undo_cached);

	mtr_start(&mtr);

	rseg_header = trx_rsegf_get_new(rseg->space, rseg->page_no, &mtr);

	for (i = 0; i < TRX_RSEG_N_SLOTS; i++) {
		page_no = trx_rsegf_get_nth_undo(rseg_header, i, &mtr);

		/* In forced recovery: try to avoid operations which look
		at database pages; undo logs are rapidly changing data, and
		the probability that they are in an inconsistent state is
		high */

		if (page_no != FIL_NULL
		    && srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {

			undo = trx_undo_mem_create_at_db_start(rseg, i,
							       page_no, &mtr);
			size += undo->size;

			mtr_commit(&mtr);

			mtr_start(&mtr);

			rseg_header = trx_rsegf_get(rseg->space,
						    rseg->page_no, &mtr);
		}
	}

	mtr_commit(&mtr);

	return(size);
}

/************************************************************************
Creates and initializes an undo log memory object. */
static
trx_undo_t*
trx_undo_mem_create(
/*================*/
				/* out, own: the undo log memory object */
	trx_rseg_t*	rseg,	/* in: rollback segment memory object */
	ulint		id,	/* in: slot index within rseg */
	ulint		type,	/* in: type of the log: TRX_UNDO_INSERT or
				TRX_UNDO_UPDATE */
	dulint		trx_id,	/* in: id of the trx for which the undo log
				is created */
	XID*		xid,	/* in: X/Open transaction identification */
	ulint		page_no,/* in: undo log header page number */
	ulint		offset)	/* in: undo log header byte offset on page */
{
	trx_undo_t*	undo;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (id >= TRX_RSEG_N_SLOTS) {
		fprintf(stderr,
			"InnoDB: Error: undo->id is %lu\n", (ulong) id);
		ut_error;
	}

	undo = mem_alloc(sizeof(trx_undo_t));

	undo->id = id;
	undo->type = type;
	undo->state = TRX_UNDO_ACTIVE;
	undo->del_marks = FALSE;
	undo->trx_id = trx_id;
	undo->xid = *xid;

	undo->dict_operation = FALSE;

	undo->rseg = rseg;

	undo->space = rseg->space;
	undo->hdr_page_no = page_no;
	undo->hdr_offset = offset;
	undo->last_page_no = page_no;
	undo->size = 1;

	undo->empty = TRUE;
	undo->top_page_no = page_no;
	undo->guess_page = NULL;

	return(undo);
}

/************************************************************************
Initializes a cached undo log object for new use. */
static
void
trx_undo_mem_init_for_reuse(
/*========================*/
	trx_undo_t*	undo,	/* in: undo log to init */
	dulint		trx_id,	/* in: id of the trx for which the undo log
				is created */
	XID*		xid,	/* in: X/Open XA transaction identification*/
	ulint		offset)	/* in: undo log header byte offset on page */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&((undo->rseg)->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (undo->id >= TRX_RSEG_N_SLOTS) {
		fprintf(stderr, "InnoDB: Error: undo->id is %lu\n",
			(ulong) undo->id);

		mem_analyze_corruption(undo);
		ut_error;
	}

	undo->state = TRX_UNDO_ACTIVE;
	undo->del_marks = FALSE;
	undo->trx_id = trx_id;
	undo->xid = *xid;

	undo->dict_operation = FALSE;

	undo->hdr_offset = offset;
	undo->empty = TRUE;
}

/************************************************************************
Frees an undo log memory copy. */
static
void
trx_undo_mem_free(
/*==============*/
	trx_undo_t*	undo)	/* in: the undo object to be freed */
{
	if (undo->id >= TRX_RSEG_N_SLOTS) {
		fprintf(stderr,
			"InnoDB: Error: undo->id is %lu\n", (ulong) undo->id);
		ut_error;
	}

	mem_free(undo);
}

/**************************************************************************
Creates a new undo log. */
static
trx_undo_t*
trx_undo_create(
/*============*/
				/* out: undo log object, NULL if did not
				succeed: out of space */
	trx_t*		trx,	/* in: transaction */
	trx_rseg_t*	rseg,	/* in: rollback segment memory copy */
	ulint		type,	/* in: type of the log: TRX_UNDO_INSERT or
				TRX_UNDO_UPDATE */
	dulint		trx_id,	/* in: id of the trx for which the undo log
				is created */
	XID*		xid,	/* in: X/Open transaction identification*/
	mtr_t*		mtr)	/* in: mtr */
{
	trx_rsegf_t*	rseg_header;
	ulint		page_no;
	ulint		offset;
	ulint		id;
	trx_undo_t*	undo;
	page_t*		undo_page;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (rseg->curr_size == rseg->max_size) {

		return(NULL);
	}

	rseg->curr_size++;

	rseg_header = trx_rsegf_get(rseg->space, rseg->page_no, mtr);

	undo_page = trx_undo_seg_create(rseg, rseg_header, type, &id, mtr);

	if (undo_page == NULL) {
		/* Did not succeed */

		rseg->curr_size--;

		return(NULL);
	}

	page_no = buf_frame_get_page_no(undo_page);

	offset = trx_undo_header_create(undo_page, trx_id, mtr);

	if (trx->support_xa) {
		trx_undo_header_add_space_for_xid(undo_page,
						  undo_page + offset, mtr);
	}

	undo = trx_undo_mem_create(rseg, id, type, trx_id, xid,
				   page_no, offset);
	return(undo);
}

/*================ UNDO LOG ASSIGNMENT AND CLEANUP =====================*/

/************************************************************************
Reuses a cached undo log. */
static
trx_undo_t*
trx_undo_reuse_cached(
/*==================*/
				/* out: the undo log memory object, NULL if
				none cached */
	trx_t*		trx,	/* in: transaction */
	trx_rseg_t*	rseg,	/* in: rollback segment memory object */
	ulint		type,	/* in: type of the log: TRX_UNDO_INSERT or
				TRX_UNDO_UPDATE */
	dulint		trx_id,	/* in: id of the trx for which the undo log
				is used */
	XID*		xid,	/* in: X/Open XA transaction identification */
	mtr_t*		mtr)	/* in: mtr */
{
	trx_undo_t*	undo;
	page_t*		undo_page;
	ulint		offset;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (type == TRX_UNDO_INSERT) {

		undo = UT_LIST_GET_FIRST(rseg->insert_undo_cached);
		if (undo == NULL) {

			return(NULL);
		}

		UT_LIST_REMOVE(undo_list, rseg->insert_undo_cached, undo);
	} else {
		ut_ad(type == TRX_UNDO_UPDATE);

		undo = UT_LIST_GET_FIRST(rseg->update_undo_cached);
		if (undo == NULL) {

			return(NULL);
		}

		UT_LIST_REMOVE(undo_list, rseg->update_undo_cached, undo);
	}

	ut_ad(undo->size == 1);

	if (undo->id >= TRX_RSEG_N_SLOTS) {
		fprintf(stderr, "InnoDB: Error: undo->id is %lu\n",
			(ulong) undo->id);
		mem_analyze_corruption(undo);
		ut_error;
	}

	undo_page = trx_undo_page_get(undo->space, undo->hdr_page_no, mtr);

	if (type == TRX_UNDO_INSERT) {
		offset = trx_undo_insert_header_reuse(undo_page, trx_id, mtr);

		if (trx->support_xa) {
			trx_undo_header_add_space_for_xid
				(undo_page, undo_page + offset, mtr);
		}
	} else {
		ut_a(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR
				      + TRX_UNDO_PAGE_TYPE)
		     == TRX_UNDO_UPDATE);

		offset = trx_undo_header_create(undo_page, trx_id, mtr);

		if (trx->support_xa) {
			trx_undo_header_add_space_for_xid
				(undo_page, undo_page + offset, mtr);
		}
	}

	trx_undo_mem_init_for_reuse(undo, trx_id, xid, offset);

	return(undo);
}

/**************************************************************************
Marks an undo log header as a header of a data dictionary operation
transaction. */
static
void
trx_undo_mark_as_dict_operation(
/*============================*/
	trx_t*		trx,	/* in: dict op transaction */
	trx_undo_t*	undo,	/* in: assigned undo log */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*	hdr_page;

	ut_a(trx->dict_operation);

	hdr_page = trx_undo_page_get(undo->space, undo->hdr_page_no, mtr);

	mlog_write_ulint(hdr_page + undo->hdr_offset
			 + TRX_UNDO_DICT_TRANS,
			 trx->dict_operation, MLOG_1BYTE, mtr);

	mlog_write_dulint(hdr_page + undo->hdr_offset + TRX_UNDO_TABLE_ID,
			  trx->table_id, mtr);

	undo->dict_operation = trx->dict_operation;
	undo->table_id = trx->table_id;
}

/**************************************************************************
Assigns an undo log for a transaction. A new undo log is created or a cached
undo log reused. */

trx_undo_t*
trx_undo_assign_undo(
/*=================*/
			/* out: the undo log, NULL if did not succeed: out of
			space */
	trx_t*	trx,	/* in: transaction */
	ulint	type)	/* in: TRX_UNDO_INSERT or TRX_UNDO_UPDATE */
{
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;
	mtr_t		mtr;

	ut_ad(trx);
	ut_ad(trx->rseg);

	rseg = trx->rseg;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(trx->undo_mutex)));
#endif /* UNIV_SYNC_DEBUG */

	mtr_start(&mtr);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	mutex_enter(&(rseg->mutex));

	undo = trx_undo_reuse_cached(trx, rseg, type, trx->id, &trx->xid,
				     &mtr);
	if (undo == NULL) {
		undo = trx_undo_create(trx, rseg, type, trx->id, &trx->xid,
				       &mtr);
		if (undo == NULL) {
			/* Did not succeed */

			mutex_exit(&(rseg->mutex));
			mtr_commit(&mtr);

			return(NULL);
		}
	}

	if (type == TRX_UNDO_INSERT) {
		UT_LIST_ADD_FIRST(undo_list, rseg->insert_undo_list, undo);
		ut_ad(trx->insert_undo == NULL);
		trx->insert_undo = undo;
	} else {
		UT_LIST_ADD_FIRST(undo_list, rseg->update_undo_list, undo);
		ut_ad(trx->update_undo == NULL);
		trx->update_undo = undo;
	}

	if (trx->dict_operation) {
		trx_undo_mark_as_dict_operation(trx, undo, &mtr);
	}

	mutex_exit(&(rseg->mutex));
	mtr_commit(&mtr);

	return(undo);
}

/**********************************************************************
Sets the state of the undo log segment at a transaction finish. */

page_t*
trx_undo_set_state_at_finish(
/*=========================*/
				/* out: undo log segment header page,
				x-latched */
	trx_t*		trx __attribute__((unused)), /* in: transaction */
	trx_undo_t*	undo,	/* in: undo log memory copy */
	mtr_t*		mtr)	/* in: mtr */
{
	trx_usegf_t*	seg_hdr;
	trx_upagef_t*	page_hdr;
	page_t*		undo_page;
	ulint		state;

	ut_ad(trx && undo && mtr);

	if (undo->id >= TRX_RSEG_N_SLOTS) {
		fprintf(stderr, "InnoDB: Error: undo->id is %lu\n",
			(ulong) undo->id);
		mem_analyze_corruption(undo);
		ut_error;
	}

	undo_page = trx_undo_page_get(undo->space, undo->hdr_page_no, mtr);

	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

	if (undo->size == 1 && mach_read_from_2(page_hdr + TRX_UNDO_PAGE_FREE)
	    < TRX_UNDO_PAGE_REUSE_LIMIT) {
		state = TRX_UNDO_CACHED;

	} else if (undo->type == TRX_UNDO_INSERT) {

		state = TRX_UNDO_TO_FREE;
	} else {
		state = TRX_UNDO_TO_PURGE;
	}

	undo->state = state;

	mlog_write_ulint(seg_hdr + TRX_UNDO_STATE, state, MLOG_2BYTES, mtr);

	return(undo_page);
}

/**********************************************************************
Sets the state of the undo log segment at a transaction prepare. */

page_t*
trx_undo_set_state_at_prepare(
/*==========================*/
				/* out: undo log segment header page,
				x-latched */
	trx_t*		trx,	/* in: transaction */
	trx_undo_t*	undo,	/* in: undo log memory copy */
	mtr_t*		mtr)	/* in: mtr */
{
	trx_usegf_t*	seg_hdr;
	trx_upagef_t*	page_hdr;
	trx_ulogf_t*	undo_header;
	page_t*		undo_page;
	ulint		offset;

	ut_ad(trx && undo && mtr);

	if (undo->id >= TRX_RSEG_N_SLOTS) {
		fprintf(stderr, "InnoDB: Error: undo->id is %lu\n",
			(ulong) undo->id);
		mem_analyze_corruption(undo);
		ut_error;
	}

	undo_page = trx_undo_page_get(undo->space, undo->hdr_page_no, mtr);

	seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
	page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

	/*------------------------------*/
	undo->state = TRX_UNDO_PREPARED;
	undo->xid   = trx->xid;
	/*------------------------------*/

	mlog_write_ulint(seg_hdr + TRX_UNDO_STATE, undo->state,
			 MLOG_2BYTES, mtr);

	offset = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);
	undo_header = undo_page + offset;

	mlog_write_ulint(undo_header + TRX_UNDO_XID_EXISTS,
			 TRUE, MLOG_1BYTE, mtr);

	trx_undo_write_xid(undo_header, &undo->xid, mtr);

	return(undo_page);
}

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
	mtr_t*	mtr)		/* in: mtr */
{
	trx_rseg_t*	rseg;
	trx_undo_t*	undo;

	undo = trx->update_undo;
	rseg = trx->rseg;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(rseg->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	trx_purge_add_update_undo_to_history(trx, undo_page, mtr);

	UT_LIST_REMOVE(undo_list, rseg->update_undo_list, undo);

	trx->update_undo = NULL;

	if (undo->state == TRX_UNDO_CACHED) {

		UT_LIST_ADD_FIRST(undo_list, rseg->update_undo_cached, undo);
	} else {
		ut_ad(undo->state == TRX_UNDO_TO_PURGE);

		trx_undo_mem_free(undo);
	}
}

/**********************************************************************
Frees or caches an insert undo log after a transaction commit or rollback.
Knowledge of inserts is not needed after a commit or rollback, therefore
the data can be discarded. */

void
trx_undo_insert_cleanup(
/*====================*/
	trx_t*	trx)	/* in: transaction handle */
{
	trx_undo_t*	undo;
	trx_rseg_t*	rseg;

	undo = trx->insert_undo;
	ut_ad(undo);

	rseg = trx->rseg;

	mutex_enter(&(rseg->mutex));

	UT_LIST_REMOVE(undo_list, rseg->insert_undo_list, undo);
	trx->insert_undo = NULL;

	if (undo->state == TRX_UNDO_CACHED) {

		UT_LIST_ADD_FIRST(undo_list, rseg->insert_undo_cached, undo);
	} else {
		ut_ad(undo->state == TRX_UNDO_TO_FREE);

		/* Delete first the undo log segment in the file */

		mutex_exit(&(rseg->mutex));

		trx_undo_seg_free(undo);

		mutex_enter(&(rseg->mutex));

		ut_ad(rseg->curr_size > undo->size);

		rseg->curr_size -= undo->size;

		trx_undo_mem_free(undo);
	}

	mutex_exit(&(rseg->mutex));
}
