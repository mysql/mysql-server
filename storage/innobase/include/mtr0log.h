/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/mtr0log.h
Mini-transaction logging routines

Created 12/7/1995 Heikki Tuuri
*******************************************************/

#ifndef mtr0log_h
#define mtr0log_h

#include "univ.i"
#include "mtr0mtr.h"
#include "dyn0buf.h"

// Forward declaration
struct dict_index_t;

/********************************************************//**
Writes 1, 2 or 4 bytes to a file page. Writes the corresponding log
record to the mini-transaction log if mtr is not NULL. */
void
mlog_write_ulint(
/*=============*/
	byte*		ptr,	/*!< in: pointer where to write */
	ulint		val,	/*!< in: value to write */
	mlog_id_t	type,	/*!< in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*		mtr);	/*!< in: mini-transaction handle */

/********************************************************//**
Writes 8 bytes to a file page. Writes the corresponding log
record to the mini-transaction log, only if mtr is not NULL */
void
mlog_write_ull(
/*===========*/
	byte*		ptr,	/*!< in: pointer where to write */
	ib_uint64_t	val,	/*!< in: value to write */
	mtr_t*		mtr);	/*!< in: mini-transaction handle */
/********************************************************//**
Writes a string to a file page buffered in the buffer pool. Writes the
corresponding log record to the mini-transaction log. */
void
mlog_write_string(
/*==============*/
	byte*		ptr,	/*!< in: pointer where to write */
	const byte*	str,	/*!< in: string to write */
	ulint		len,	/*!< in: string length */
	mtr_t*		mtr);	/*!< in: mini-transaction handle */
/********************************************************//**
Logs a write of a string to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */
void
mlog_log_string(
/*============*/
	byte*	ptr,	/*!< in: pointer written to */
	ulint	len,	/*!< in: string length */
	mtr_t*	mtr);	/*!< in: mini-transaction handle */
/********************************************************//**
Writes initial part of a log record consisting of one-byte item
type and four-byte space and page numbers. */
void
mlog_write_initial_log_record(
/*==========================*/
	const byte*	ptr,	/*!< in: pointer to (inside) a buffer
				frame holding the file page where
				modification is made */
	mlog_id_t	type,	/*!< in: log item type: MLOG_1BYTE, ... */
	mtr_t*		mtr);	/*!< in: mini-transaction handle */

/** Catenates 1 - 4 bytes to the mtr log. The value is not compressed.
@param[in,out]	dyn_buf	buffer to write
@param[in]	val	value to write
@param[in]	type	type of value to write */
UNIV_INLINE
void
mlog_catenate_ulint(
	mtr_buf_t*	dyn_buf,
	ulint		val,
	mlog_id_t	type);

/** Catenates 1 - 4 bytes to the mtr log.
@param[in]	mtr	mtr
@param[in]	val	value to write
@param[in]	type	MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
UNIV_INLINE
void
mlog_catenate_ulint(
	mtr_t*		mtr,
	ulint		val,
	mlog_id_t	type);

/********************************************************//**
Catenates n bytes to the mtr log. */
void
mlog_catenate_string(
/*=================*/
	mtr_t*		mtr,	/*!< in: mtr */
	const byte*	str,	/*!< in: string to write */
	ulint		len);	/*!< in: string length */

/** Catenates a compressed ulint to mlog.
@param[in]	mtr	mtr
@param[in]	val	value to write */
UNIV_INLINE
void
mlog_catenate_ulint_compressed(
	mtr_t*		mtr,
	ulint		val);

/** Catenates a compressed 64-bit integer to mlog.
@param[in]	mtr	mtr
@param[in]	val	value to write */
UNIV_INLINE
void
mlog_catenate_ull_compressed(
	mtr_t*		mtr,
	ib_uint64_t	val);

/** Opens a buffer to mlog. It must be closed with mlog_close.
@param[in,out]	mtr	mtr
@param[in]	size	buffer size in bytes; MUST be smaller than
			DYN_ARRAY_DATA_SIZE!
@return buffer, NULL if log mode MTR_LOG_NONE */
UNIV_INLINE
byte*
mlog_open(
	mtr_t*		mtr,
	ulint		size);

/** Opens a buffer to mlog. It must be closed with mlog_close.
This is used for writing log for metadata changes
@param[in,out]	mtr	mtr
@param[in]	size	buffer size in bytes; MUST be smaller than
			DYN_ARRAY_DATA_SIZE!
@return buffer */
UNIV_INLINE
byte*
mlog_open_metadata(
	mtr_t*		mtr,
	ulint		size);

/** Closes a buffer opened to mlog.
@param[in]	mtr	mtr
@param[in]	ptr	buffer space from ptr up was not used */
UNIV_INLINE
void
mlog_close(
	mtr_t*		mtr,
	byte*		ptr);

/** Writes a log record about a dictionary operation.
@param[in]	type		redo log record type
@param[in]	id		table id
@param[in]	version		table dynamic metadata version
@param[in,out]	log_ptr		current end of mini-transaction log
@param[in,out]	mtr		mini-transaction
@return end of mini-transaction log */
UNIV_INLINE
byte*
mlog_write_initial_dict_log_record(
	mlog_id_t	type,
	table_id_t	id,
	uint64_t	version,
	byte*		log_ptr,
	mtr_t*		mtr);

/** Writes a log record about an operation.
@param[in]	type		redo log record type
@param[in]	space_id	tablespace identifier
@param[in]	page_no		page number
@param[in,out]	log_ptr		current end of mini-transaction log
@param[in,out]	mtr		mini-transaction
@return	end of mini-transaction log */
UNIV_INLINE
byte*
mlog_write_initial_log_record_low(
	mlog_id_t	type,
	space_id_t	space_id,
	page_no_t	page_no,
	byte*		log_ptr,
	mtr_t*		mtr);

#ifndef UNIV_HOTBACKUP
/** Writes the initial part of a log record (3..11 bytes).
If the implementation of this function is changed, all size parameters to
mlog_open() should be adjusted accordingly!
@param[in]	ptr	pointer to (inside) a buffer frame holding the file
			page where modification is made
@param[in]	type	log item type: MLOG_1BYTE, ...
@param[in]	log_ptr	pointer to mtr log which has been opened
@param[in]	mtr	mtr
@return new value of log_ptr */
UNIV_INLINE
byte*
mlog_write_initial_log_record_fast(
	const byte*	ptr,
	mlog_id_t	type,
	byte*		log_ptr,
	mtr_t*		mtr);
#else /* !UNIV_HOTBACKUP */
# define mlog_write_initial_log_record(ptr,type,mtr) ((void) 0)
# define mlog_write_initial_log_record_fast(ptr,type,log_ptr,mtr) ((byte*) 0)
#endif /* !UNIV_HOTBACKUP */

/** Parses an initial log record written by mlog_write_initial_dict_log_record.
@param[in]	ptr		buffer
@param[in]	end_ptr		buffer end
@param[out]	type		log record type, should be
				MLOG_TABLE_DYNAMIC_META
@param[out]	id		table id
@param[out]	version		table dynamic metadata version
@return parsed record end, NULL if not a complete record */
byte*
mlog_parse_initial_dict_log_record(
	const byte*	ptr,
	const byte*	end_ptr,
	mlog_id_t*	type,
	table_id_t*	id,
	uint64*		version);

/********************************************************//**
Parses an initial log record written by mlog_write_initial_log_record.
@return parsed record end, NULL if not a complete record */
byte*
mlog_parse_initial_log_record(
/*==========================*/
	const byte*	ptr,	/*!< in: buffer */
	const byte*	end_ptr,/*!< in: buffer end */
	mlog_id_t*	type,	/*!< out: log record type: MLOG_1BYTE, ... */
	space_id_t*	space,	/*!< out: space id */
	page_no_t*	page_no);/*!< out: page number */
/********************************************************//**
Parses a log record written by mlog_write_ulint or mlog_write_ull.
@return parsed record end, NULL if not a complete record */
byte*
mlog_parse_nbytes(
/*==============*/
	mlog_id_t	type,	/*!< in: log record type: MLOG_1BYTE, ... */
	const byte*	ptr,	/*!< in: buffer */
	const byte*	end_ptr,/*!< in: buffer end */
	byte*		page,	/*!< in: page where to apply the log record,
				or NULL */
	void*		page_zip);/*!< in/out: compressed page, or NULL */
/********************************************************//**
Parses a log record written by mlog_write_string.
@return parsed record end, NULL if not a complete record */
byte*
mlog_parse_string(
/*==============*/
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	byte*	page,	/*!< in: page where to apply the log record, or NULL */
	void*	page_zip);/*!< in/out: compressed page, or NULL */

/********************************************************//**
Opens a buffer for mlog, writes the initial log record and,
if needed, the field lengths of an index.  Reserves space
for further log entries.  The log entry must be closed with
mtr_close().
@return buffer, NULL if log mode MTR_LOG_NONE */
byte*
mlog_open_and_write_index(
/*======================*/
	mtr_t*			mtr,	/*!< in: mtr */
	const byte*		rec,	/*!< in: index record or page */
	const dict_index_t*	index,	/*!< in: record descriptor */
	mlog_id_t		type,	/*!< in: log item type */
	ulint			size);	/*!< in: requested buffer size in bytes
					(if 0, calls mlog_close() and
					returns NULL) */

/********************************************************//**
Parses a log record written by mlog_open_and_write_index.
@return parsed record end, NULL if not a complete record */
byte*
mlog_parse_index(
/*=============*/
	byte*		ptr,	/*!< in: buffer */
	const byte*	end_ptr,/*!< in: buffer end */
	ibool		comp,	/*!< in: TRUE=compact record format */
	dict_index_t**	index);	/*!< out, own: dummy index */

/** Insert, update, and maybe other functions may use this value to define an
extra mlog buffer size for variable size data */
#define MLOG_BUF_MARGIN	256

#include "mtr0log.ic"

#endif /* mtr0log_h */
