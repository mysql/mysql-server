/******************************************************
Mini-transaction logging routines

(c) 1995 Innobase Oy

Created 12/7/1995 Heikki Tuuri
*******************************************************/

#ifndef mtr0log_h
#define mtr0log_h

#include "univ.i"
#include "mtr0mtr.h"

/************************************************************
Writes 1 - 4 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */

void
mlog_write_ulint(
/*=============*/
	byte*	ptr,	/* in: pointer where to write */
	ulint	val,	/* in: value to write */
	byte	type,	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Writes 8 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */

void
mlog_write_dulint(
/*==============*/
	byte*	ptr,	/* in: pointer where to write */
	dulint	val,	/* in: value to write */
	byte	type,	/* in: MLOG_8BYTES */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Writes a string to a file page buffered in the buffer pool. Writes the
corresponding log record to the mini-transaction log. */

void
mlog_write_string(
/*==============*/
	byte*	ptr,	/* in: pointer where to write */
	byte*	str,	/* in: string to write */
	ulint	len,	/* in: string length */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Writes initial part of a log record consisting of one-byte item
type and four-byte space and page numbers. */

void
mlog_write_initial_log_record(
/*==========================*/
	byte*	ptr,	/* in: pointer to (inside) a buffer frame
			holding the file page where modification
			is made */
	byte	type,	/* in: log item type: MLOG_1BYTE, ... */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Catenates 1 - 4 bytes to the mtr log. */
UNIV_INLINE
void
mlog_catenate_ulint(
/*================*/
	mtr_t*	mtr,	/* in: mtr */
	ulint	val,	/* in: value to write */
	ulint	type);	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
/************************************************************
Catenates n bytes to the mtr log. */

void
mlog_catenate_string(
/*=================*/
	mtr_t*	mtr,	/* in: mtr */
	byte*	str,	/* in: string to write */
	ulint	len);	/* in: string length */
/************************************************************
Catenates a compressed ulint to mlog. */
UNIV_INLINE
void
mlog_catenate_ulint_compressed(
/*===========================*/
	mtr_t*	mtr,	/* in: mtr */
	ulint	val);	/* in: value to write */
/************************************************************
Catenates a compressed dulint to mlog. */
UNIV_INLINE
void
mlog_catenate_dulint_compressed(
/*============================*/
	mtr_t*	mtr,	/* in: mtr */
	dulint	val);	/* in: value to write */
/************************************************************
Opens a buffer to mlog. It must be closed with mlog_close. */
UNIV_INLINE
byte*
mlog_open(
/*======*/
			/* out: buffer, NULL if log mode MTR_LOG_NONE */
	mtr_t*	mtr,	/* in: mtr */
	ulint	size);	/* in: buffer size in bytes */
/************************************************************
Closes a buffer opened to mlog. */
UNIV_INLINE
void
mlog_close(
/*=======*/
	mtr_t*	mtr,	/* in: mtr */
	byte*	ptr);	/* in: buffer space from ptr up was not used */
/************************************************************
Writes the initial part of a log record. */
UNIV_INLINE
byte*
mlog_write_initial_log_record_fast(
/*===============================*/
			/* out: new value of log_ptr */
	byte*	ptr,	/* in: pointer to (inside) a buffer frame holding the
			file page where modification is made */
	byte	type,	/* in: log item type: MLOG_1BYTE, ... */
	byte*	log_ptr,/* in: pointer to mtr log which has been opened */
	mtr_t*	mtr);	/* in: mtr */
/****************************************************************
Writes the contents of a mini-transaction log, if any, to the database log. */

dulint
mlog_write(
/*=======*/
	dyn_array_t*	mlog,		/* in: mlog */
	ibool*		modifications);	/* out: TRUE if there were 
					log items to write */
/************************************************************
Parses an initial log record written by mlog_write_initial_log_record. */

byte*
mlog_parse_initial_log_record(
/*==========================*/
			/* out: parsed record end, NULL if not a complete
			record */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	byte*	type,	/* out: log record type: MLOG_1BYTE, ... */
	ulint*	space,	/* out: space id */
	ulint*	page_no);/* out: page number */
/************************************************************
Parses a log record written by mlog_write_ulint or mlog_write_dulint. */

byte*
mlog_parse_nbytes(
/*==============*/
			/* out: parsed record end, NULL if not a complete
			record */
	ulint	type,	/* in: log record type: MLOG_1BYTE, ... */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	byte*	page);	/* in: page where to apply the log record, or NULL */
/************************************************************
Parses a log record written by mlog_write_string. */

byte*
mlog_parse_string(
/*==============*/
			/* out: parsed record end, NULL if not a complete
			record */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	byte*	page);	/* in: page where to apply the log record, or NULL */


/* Insert, update, and maybe other functions may use this value to define an
extra mlog buffer size for variable size data */
#define MLOG_BUF_MARGIN	256

#ifndef UNIV_NONINL
#include "mtr0log.ic"
#endif

#endif
