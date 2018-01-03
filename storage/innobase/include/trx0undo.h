/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file include/trx0undo.h
Transaction undo log

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0undo_h
#define trx0undo_h

#include "univ.i"
#include "trx0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"
#include "page0types.h"
#include "trx0xa.h"
#include "sql/xa.h"

#ifndef UNIV_HOTBACKUP
/***********************************************************************//**
Returns TRUE if the roll pointer is of the insert type.
@return TRUE if insert undo log */
UNIV_INLINE
ibool
trx_undo_roll_ptr_is_insert(
/*========================*/
	roll_ptr_t	roll_ptr);	/*!< in: roll pointer */
/***********************************************************************//**
Returns true if the record is of the insert type.
@return true if the record was freshly inserted (not updated). */
UNIV_INLINE
bool
trx_undo_trx_id_is_insert(
/*======================*/
	const byte*	trx_id)	/*!< in: DB_TRX_ID, followed by DB_ROLL_PTR */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */

/** Writes a roll ptr to an index page. In case that the size changes in
some future version, this function should be used instead of
mach_write_...
@param[in]	ptr		pointer to memory where written
@param[in]	roll_ptr	roll ptr */
UNIV_INLINE
void
trx_write_roll_ptr(
	byte*		ptr,
	roll_ptr_t	roll_ptr);

/*****************************************************************//**
Reads a roll ptr from an index page. In case that the roll ptr size
changes in some future version, this function should be used instead of
mach_read_...
@return roll ptr */
UNIV_INLINE
roll_ptr_t
trx_read_roll_ptr(
/*==============*/
	const byte*	ptr);	/*!< in: pointer to memory from where to read */
#ifndef UNIV_HOTBACKUP

/** Gets an undo log page and x-latches it.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return pointer to page x-latched */
UNIV_INLINE
page_t*
trx_undo_page_get(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	mtr_t*			mtr);

/** Gets an undo log page and s-latches it.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return pointer to page s-latched */
UNIV_INLINE
page_t*
trx_undo_page_get_s_latched(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	mtr_t*			mtr);

/** Returns the previous undo record on the page in the specified log, or
NULL if none exists.
@param[in]	rec		undo log record
@param[in]	page_no		undo log header page number
@param[in]	offset		undo log header offset on page
@return pointer to record, NULL if none */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_prev_rec(
	trx_undo_rec_t*	rec,
	page_no_t	page_no,
	ulint		offset);

/** Returns the next undo log record on the page in the specified log, or
NULL if none exists.
@param[in]	rec		undo log record
@param[in]	page_no		undo log header page number
@param[in]	offset		undo log header offset on page
@return pointer to record, NULL if none */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_next_rec(
	trx_undo_rec_t*	rec,
	page_no_t	page_no,
	ulint		offset);

/** Returns the last undo record on the page in the specified undo log, or
NULL if none exists.
@param[in]	undo_page	undo log page
@param[in]	page_no		undo log header page number
@param[in]	offset		undo log header offset on page
@return pointer to record, NULL if none */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_last_rec(
	page_t*		undo_page,
	page_no_t	page_no,
	ulint		offset);

/** Returns the first undo record on the page in the specified undo log, or
NULL if none exists.
@param[in]	undo_page	undo log page
@param[in]	page_no		undo log header page number
@param[in]	offset		undo log header offset on page
@return pointer to record, NULL if none */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_page_get_first_rec(
	page_t*		undo_page,
	page_no_t	page_no,
	ulint		offset);

/***********************************************************************//**
Gets the previous record in an undo log.
@return undo log record, the page s-latched, NULL if none */
trx_undo_rec_t*
trx_undo_get_prev_rec(
/*==================*/
	trx_undo_rec_t*	rec,	/*!< in: undo record */
	page_no_t	page_no,/*!< in: undo log header page number */
	ulint		offset,	/*!< in: undo log header offset on page */
	bool		shared,	/*!< in: true=S-latch, false=X-latch */
	mtr_t*		mtr);	/*!< in: mtr */
/***********************************************************************//**
Gets the next record in an undo log.
@return undo log record, the page s-latched, NULL if none */
trx_undo_rec_t*
trx_undo_get_next_rec(
/*==================*/
	trx_undo_rec_t*	rec,	/*!< in: undo record */
	page_no_t	page_no,/*!< in: undo log header page number */
	ulint		offset,	/*!< in: undo log header offset on page */
	mtr_t*		mtr);	/*!< in: mtr */

/** Gets the first record in an undo log.
@param[out]	modifier_trx_id	the modifier trx identifier.
@param[in]	space		undo log header space
@param[in]	page_size	page size
@param[in]	page_no		undo log header page number
@param[in]	offset		undo log header offset on page
@param[in]	mode		latching mode: RW_S_LATCH or RW_X_LATCH
@param[in,out]	mtr		mini-transaction
@return undo log record, the page latched, NULL if none */
trx_undo_rec_t*
trx_undo_get_first_rec(
	trx_id_t*		modifier_trx_id,
	space_id_t		space,
	const page_size_t&	page_size,
	page_no_t		page_no,
	ulint			offset,
	ulint			mode,
	mtr_t*			mtr);

/********************************************************************//**
Tries to add a page to the undo log segment where the undo log is placed.
@return X-latched block if success, else NULL */
buf_block_t*
trx_undo_add_page(
/*==============*/
	trx_t*		trx,		/*!< in: transaction */
	trx_undo_t*	undo,		/*!< in: undo log memory object */
	trx_undo_ptr_t*	undo_ptr,	/*!< in: assign undo log from
					referred rollback segment. */
	mtr_t*		mtr)		/*!< in: mtr which does not have
					a latch to any undo log page;
					the caller must have reserved
					the rollback segment mutex */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Frees the last undo log page.
The caller must hold the rollback segment mutex. */
void
trx_undo_free_last_page_func(
/*==========================*/
#ifdef UNIV_DEBUG
	const trx_t*	trx,	/*!< in: transaction */
#endif /* UNIV_DEBUG */
	trx_undo_t*	undo,	/*!< in/out: undo log memory copy */
	mtr_t*		mtr);	/*!< in/out: mini-transaction which does not
				have a latch to any undo log page or which
				has allocated the undo log page */
#ifdef UNIV_DEBUG
# define trx_undo_free_last_page(trx,undo,mtr)	\
	trx_undo_free_last_page_func(trx,undo,mtr)
#else /* UNIV_DEBUG */
# define trx_undo_free_last_page(trx,undo,mtr)	\
	trx_undo_free_last_page_func(undo,mtr)
#endif /* UNIV_DEBUG */

/***********************************************************************//**
Truncates an undo log from the end. This function is used during a rollback
to free space from an undo log. */
void
trx_undo_truncate_end_func(
/*=======================*/
#ifdef UNIV_DEBUG
	const trx_t*	trx,	/*!< in: transaction whose undo log it is */
#endif /* UNIV_DEBUG */
	trx_undo_t*	undo,	/*!< in/out: undo log */
	undo_no_t	limit);	/*!< in: all undo records with undo number
				>= this value should be truncated */
#ifdef UNIV_DEBUG
# define trx_undo_truncate_end(trx,undo,limit)		\
	trx_undo_truncate_end_func(trx,undo,limit)
#else /* UNIV_DEBUG */
# define trx_undo_truncate_end(trx,undo,limit)		\
	trx_undo_truncate_end_func(undo,limit)
#endif /* UNIV_DEBUG */

/** Truncate the head of an undo log.
NOTE that only whole pages are freed; the header page is not
freed, but emptied, if all the records there are below the limit.
@param[in,out]	rseg		rollback segment
@param[in]	hdr_page_no	header page number
@param[in]	hdr_offset	header offset on the page
@param[in]	limit		first undo number to preserve
(everything below the limit will be truncated) */
void
trx_undo_truncate_start(
	trx_rseg_t*	rseg,
	page_no_t	hdr_page_no,
	ulint		hdr_offset,
	undo_no_t	limit);
/********************************************************************//**
Initializes the undo log lists for a rollback segment memory copy.
This function is only called when the database is started or a new
rollback segment created.
@return the combined size of undo log segments in pages */
ulint
trx_undo_lists_init(
/*================*/
	trx_rseg_t*	rseg);	/*!< in: rollback segment memory object */
/**********************************************************************//**
Assigns an undo log for a transaction. A new undo log is created or a cached
undo log reused.
@return DB_SUCCESS if undo log assign successful, possible error codes
are: DB_TOO_MANY_CONCURRENT_TRXS DB_OUT_OF_FILE_SPACE DB_READ_ONLY
DB_OUT_OF_MEMORY */
dberr_t
trx_undo_assign_undo(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	trx_undo_ptr_t*	undo_ptr,	/*!< in: assign undo log from
					referred rollback segment. */
	ulint		type)		/*!< in: TRX_UNDO_INSERT or
					TRX_UNDO_UPDATE */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************************//**
Sets the state of the undo log segment at a transaction finish.
@return undo log segment header page, x-latched */
page_t*
trx_undo_set_state_at_finish(
/*=========================*/
	trx_undo_t*	undo,	/*!< in: undo log memory copy */
	mtr_t*		mtr);	/*!< in: mtr */

/** Set the state of the undo log segment at a XA PREPARE or XA ROLLBACK.
@param[in,out]	trx		transaction
@param[in,out]	undo		insert_undo or update_undo log
@param[in]	rollback	false=XA PREPARE, true=XA ROLLBACK
@param[in,out]	mtr		mini-transaction
@return undo log segment header page, x-latched */
page_t*
trx_undo_set_state_at_prepare(
	trx_t*		trx,
	trx_undo_t*	undo,
	bool		rollback,
	mtr_t*		mtr);

/**********************************************************************//**
Adds the update undo log header as the first in the history list, and
frees the memory object, or puts it to the list of cached update undo log
segments. */
void
trx_undo_update_cleanup(
/*====================*/
	trx_t*		trx,		/*!< in: trx owning the update
					undo log */
	trx_undo_ptr_t*	undo_ptr,	/*!< in: update undo log. */
	page_t*		undo_page,	/*!< in: update undo log header page,
					x-latched */
	bool		update_rseg_history_len,
					/*!< in: if true: update rseg history
					len else skip updating it. */
	ulint		n_added_logs,	/*!< in: number of logs added */
	mtr_t*		mtr);		/*!< in: mtr */

/** Frees an insert undo log after a transaction commit or rollback.
Knowledge of inserts is not needed after a commit or rollback, therefore
the data can be discarded.
@param[in,out]	undo_ptr	undo log to clean up
@param[in]	noredo		whether the undo tablespace is redo logged */
void
trx_undo_insert_cleanup(
	trx_undo_ptr_t*	undo_ptr,
	bool		noredo);

/********************************************************************//**
At shutdown, frees the undo logs of a PREPARED transaction. */
void
trx_undo_free_prepared(
/*===================*/
	trx_t*	trx)	/*!< in/out: PREPARED transaction */
	UNIV_COLD;

/* Forward declaration. */
namespace undo {
	struct Tablespace;
	class Truncate;
}

/** Truncate UNDO tablespace, reinitialize header and rseg.
@param[in]	undo_trunc	UNDO tablespace handler
@return true if success else false. */
bool
trx_undo_truncate_tablespace(
	undo::Truncate*	undo_trunc);

#endif /* !UNIV_HOTBACKUP */
/***********************************************************//**
Parses the redo log entry of an undo log page initialization.
@return end of log record or NULL */
byte*
trx_undo_parse_page_init(
/*=====================*/
	const byte*	ptr,	/*!< in: buffer */
	const byte*	end_ptr,/*!< in: buffer end */
	page_t*		page,	/*!< in: page or NULL */
	mtr_t*		mtr);	/*!< in: mtr or NULL */
/** Parse the redo log entry of an undo log page header create or reuse.
@param[in]	type	MLOG_UNDO_HDR_CREATE or MLOG_UNDO_HDR_REUSE
@param[in]	ptr	redo log record
@param[in]	end_ptr	end of log buffer
@param[in,out]	page	page frame or NULL
@param[in,out]	mtr	mini-transaction or NULL
@return end of log record or NULL */
byte*
trx_undo_parse_page_header(
	mlog_id_t	type,
	const byte*	ptr,
	const byte*	end_ptr,
	page_t*		page,
	mtr_t*		mtr);
/************************************************************************
Frees an undo log memory copy. */
void
trx_undo_mem_free(
/*==============*/
	trx_undo_t*	undo);		/* in: the undo object to be freed */

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
#define	TRX_UNDO_PREPARED	5	/* contains an undo log of an
					prepared transaction */

#ifndef UNIV_HOTBACKUP
/** Transaction undo log memory object; this is protected by the undo_mutex
in the corresponding transaction object */

struct trx_undo_t {
	/*-----------------------------*/
	ulint		id;		/*!< undo log slot number within the
					rollback segment */
	ulint		type;		/*!< TRX_UNDO_INSERT or
					TRX_UNDO_UPDATE */
	ulint		state;		/*!< state of the corresponding undo log
					segment */
	ibool		del_marks;	/*!< relevant only in an update undo
					log: this is TRUE if the transaction may
					have delete marked records, because of
					a delete of a row or an update of an
					indexed field; purge is then
					necessary; also TRUE if the transaction
					has updated an externally stored
					field */
	trx_id_t	trx_id;		/*!< id of the trx assigned to the undo
					log */
	XID		xid;		/*!< X/Open XA transaction
					identification */
	ibool		dict_operation;	/*!< TRUE if a dict operation trx */
	trx_rseg_t*	rseg;		/*!< rseg where the undo log belongs */
	/*-----------------------------*/
	space_id_t	space;		/*!< space id where the undo log
					placed */
	page_size_t	page_size;
	page_no_t	hdr_page_no;	/*!< page number of the header page in
					the undo log */
	ulint		hdr_offset;	/*!< header offset of the undo log on
					the page */
	page_no_t	last_page_no;	/*!< page number of the last page in the
					undo log; this may differ from
					top_page_no during a rollback */
	ulint		size;		/*!< current size in pages */
	/*-----------------------------*/
	ulint		empty;		/*!< TRUE if the stack of undo log
					records is currently empty */
	page_no_t	top_page_no;	/*!< page number where the latest undo
					log record was catenated; during
					rollback the page from which the latest
					undo record was chosen */
	ulint		top_offset;	/*!< offset of the latest undo record,
					i.e., the topmost element in the undo
					log if we think of it as a stack */
	undo_no_t	top_undo_no;	/*!< undo number of the latest record */
	buf_block_t*	guess_block;	/*!< guess for the buffer block where
					the top page might reside */
	ulint		withdraw_clock;	/*!< the withdraw clock value of the
					buffer pool when guess_block was stored */
	/*-----------------------------*/
	UT_LIST_NODE_T(trx_undo_t) undo_list;
					/*!< undo log objects in the rollback
					segment are chained into lists */
};
#endif /* !UNIV_HOTBACKUP */

/** The offset of the undo log page header on pages of the undo log */
#define	TRX_UNDO_PAGE_HDR	FSEG_PAGE_DATA
/*-------------------------------------------------------------*/
/** Transaction undo log page header offsets */
/* @{ */
#define	TRX_UNDO_PAGE_TYPE	0	/*!< TRX_UNDO_INSERT or
					TRX_UNDO_UPDATE */
#define	TRX_UNDO_PAGE_START	2	/*!< Byte offset where the undo log
					records for the LATEST transaction
					start on this page (remember that
					in an update undo log, the first page
					can contain several undo logs) */
#define	TRX_UNDO_PAGE_FREE	4	/*!< On each page of the undo log this
					field contains the byte offset of the
					first free byte on the page */
#define TRX_UNDO_PAGE_NODE	6	/*!< The file list node in the chain
					of undo log pages */
/*-------------------------------------------------------------*/
#define TRX_UNDO_PAGE_HDR_SIZE	(6 + FLST_NODE_SIZE)
					/*!< Size of the transaction undo
					log page header, in bytes */
/* @} */

/** An update undo segment with just one page can be reused if it has
at most this many bytes used; we must leave space at least for one new undo
log header on the page */

#define TRX_UNDO_PAGE_REUSE_LIMIT	(3 * UNIV_PAGE_SIZE / 4)

/* An update undo log segment may contain several undo logs on its first page
if the undo logs took so little space that the segment could be cached and
reused. All the undo log headers are then on the first page, and the last one
owns the undo log records on subsequent pages if the segment is bigger than
one page. If an undo log is stored in a segment, then on the first page it is
allowed to have zero undo records, but if the segment extends to several
pages, then all the rest of the pages must contain at least one undo log
record. */

/** The offset of the undo log segment header on the first page of the undo
log segment */

#define	TRX_UNDO_SEG_HDR	(TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE)
/** Undo log segment header */
/* @{ */
/*-------------------------------------------------------------*/
#define	TRX_UNDO_STATE		0	/*!< TRX_UNDO_ACTIVE, ... */

#define	TRX_UNDO_LAST_LOG	2	/*!< Offset of the last undo log header
					on the segment header page, 0 if
					none */
#define	TRX_UNDO_FSEG_HEADER	4	/*!< Header for the file segment which
					the undo log segment occupies */
#define	TRX_UNDO_PAGE_LIST	(4 + FSEG_HEADER_SIZE)
					/*!< Base node for the list of pages in
					the undo log segment; defined only on
					the undo log segment's first page */
/*-------------------------------------------------------------*/
/** Size of the undo log segment header */
#define TRX_UNDO_SEG_HDR_SIZE	(4 + FSEG_HEADER_SIZE + FLST_BASE_NODE_SIZE)
/* @} */


/** The undo log header. There can be several undo log headers on the first
page of an update undo log segment. */
/* @{ */
/*-------------------------------------------------------------*/
#define	TRX_UNDO_TRX_ID		0	/*!< Transaction id */
#define	TRX_UNDO_TRX_NO		8	/*!< Transaction number of the
					transaction; defined only if the log
					is in a history list */
#define TRX_UNDO_DEL_MARKS	16	/*!< Defined only in an update undo
					log: TRUE if the transaction may have
					done delete markings of records, and
					thus purge is necessary */
#define	TRX_UNDO_LOG_START	18	/*!< Offset of the first undo log record
					of this log on the header page; purge
					may remove undo log record from the
					log start, and therefore this is not
					necessarily the same as this log
					header end offset */
#define	TRX_UNDO_XID_EXISTS	20	/*!< TRUE if undo log header includes
					X/Open XA transaction identification
					XID */
#define	TRX_UNDO_DICT_TRANS	21	/*!< TRUE if the transaction is a table
					create, index create, or drop
					transaction: in recovery
					the transaction cannot be rolled back
					in the usual way: a 'rollback' rather
					means dropping the created or dropped
					table, if it still exists */
#define TRX_UNDO_TABLE_ID	22	/*!< Id of the table if the preceding
					field is TRUE. Note: deprecated */
#define	TRX_UNDO_NEXT_LOG	30	/*!< Offset of the next undo log header
					on this page, 0 if none */
#define	TRX_UNDO_PREV_LOG	32	/*!< Offset of the previous undo log
					header on this page, 0 if none */
#define TRX_UNDO_HISTORY_NODE	34	/*!< If the log is put to the history
					list, the file list node is here */
/*-------------------------------------------------------------*/
/** Size of the undo log header without XID information */
#define TRX_UNDO_LOG_OLD_HDR_SIZE (34 + FLST_NODE_SIZE)

/* Note: the writing of the undo log old header is coded by a log record
MLOG_UNDO_HDR_CREATE or MLOG_UNDO_HDR_REUSE. The appending of an XID to the
header is logged separately. In this sense, the XID is not really a member
of the undo log header. TODO: do not append the XID to the log header if XA
is not needed by the user. The XID wastes about 150 bytes of space in every
undo log. In the history list we may have millions of undo logs, which means
quite a large overhead. */

/** X/Open XA Transaction Identification (XID) */
/* @{ */
/** xid_t::formatID */
#define	TRX_UNDO_XA_FORMAT	(TRX_UNDO_LOG_OLD_HDR_SIZE)
/** xid_t::gtrid_length */
#define	TRX_UNDO_XA_TRID_LEN	(TRX_UNDO_XA_FORMAT + 4)
/** xid_t::bqual_length */
#define	TRX_UNDO_XA_BQUAL_LEN	(TRX_UNDO_XA_TRID_LEN + 4)
/** Distributed transaction identifier data */
#define	TRX_UNDO_XA_XID		(TRX_UNDO_XA_BQUAL_LEN + 4)
/*--------------------------------------------------------------*/
#define TRX_UNDO_LOG_XA_HDR_SIZE (TRX_UNDO_XA_XID + XIDDATASIZE)
					/*!< Total size of the undo log header
					with the XA XID */
/* @} */

#include "trx0undo.ic"
#endif
