/******************************************************
Mini-transaction log routines

(c) 1995 Innobase Oy

Created 12/7/1995 Heikki Tuuri
*******************************************************/

#include "mtr0log.h"

#ifdef UNIV_NONINL
#include "mtr0log.ic"
#endif

#include "buf0buf.h"
#include "dict0boot.h"

/************************************************************
Catenates n bytes to the mtr log. */

void
mlog_catenate_string(
/*=================*/
	mtr_t*	mtr,	/* in: mtr */
	byte*	str,	/* in: string to write */
	ulint	len)	/* in: string length */
{
	dyn_array_t*	mlog;

	if (mtr_get_log_mode(mtr) == MTR_LOG_NONE) {

		return;
	}

	mlog = &(mtr->log);

	dyn_push_string(mlog, str, len);
}

/************************************************************
Writes the initial part of a log record consisting of one-byte item
type and four-byte space and page numbers. Also pushes info
to the mtr memo that a buffer page has been modified. */

void
mlog_write_initial_log_record(
/*==========================*/
	byte*	ptr,	/* in: pointer to (inside) a buffer frame holding the
			file page where modification is made */
	byte	type,	/* in: log item type: MLOG_1BYTE, ... */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	byte*	log_ptr;

	ut_ad(type <= MLOG_BIGGEST_TYPE);

	log_ptr = mlog_open(mtr, 20);

	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, type, log_ptr, mtr);

	mlog_close(mtr, log_ptr);
}	

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
	ulint*	page_no)/* out: page number */
{
	if (end_ptr < ptr + 1) {

		return(NULL);
	}

	*type = (byte)((ulint)*ptr & ~MLOG_SINGLE_REC_FLAG);

	ptr++;

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	ptr = mach_parse_compressed(ptr, end_ptr, space);

	if (ptr == NULL) {

		return(NULL);
	}

	ptr = mach_parse_compressed(ptr, end_ptr, page_no);

	return(ptr);
}

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
	byte*	page)	/* in: page where to apply the log record, or NULL */
{
	ulint	offset;
	ulint	val;
	dulint	dval;

	ut_a(type <= MLOG_8BYTES);

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	ptr += 2;
		
	if (type == MLOG_8BYTES) {
		ptr = mach_dulint_parse_compressed(ptr, end_ptr, &dval);

		if (ptr == NULL) {

			return(NULL);
		}

		if (page) {
			mach_write_to_8(page + offset, dval);
		}

		return(ptr);
	}

	ptr = mach_parse_compressed(ptr, end_ptr, &val);

	if (ptr == NULL) {

		return(NULL);
	}

	if (page) {
		if (type == MLOG_1BYTE) {
			mach_write_to_1(page + offset, val);
		} else if (type == MLOG_2BYTES) {
			mach_write_to_2(page + offset, val);
		} else {
			ut_ad(type == MLOG_4BYTES);
			mach_write_to_4(page + offset, val);
		}
	}

	return(ptr);
}

/************************************************************
Writes 1 - 4 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */

void
mlog_write_ulint(
/*=============*/
	byte*	ptr,	/* in: pointer where to write */
	ulint	val,	/* in: value to write */
	byte	type,	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	byte*	log_ptr;
	
	if (type == MLOG_1BYTE) {
		mach_write_to_1(ptr, val);
	} else if (type == MLOG_2BYTES) {
		mach_write_to_2(ptr, val);
	} else {
		ut_ad(type == MLOG_4BYTES);
		mach_write_to_4(ptr, val);
	}

	log_ptr = mlog_open(mtr, 30);
	
	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, type, log_ptr, mtr);

	mach_write_to_2(log_ptr, ptr - buf_frame_align(ptr));
	log_ptr += 2;
	
	log_ptr += mach_write_compressed(log_ptr, val);

	mlog_close(mtr, log_ptr);
}

/************************************************************
Writes 8 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */

void
mlog_write_dulint(
/*==============*/
	byte*	ptr,	/* in: pointer where to write */
	dulint	val,	/* in: value to write */
	byte	type,	/* in: MLOG_8BYTES */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	byte*	log_ptr;

	ut_ad(ptr && mtr);
	ut_ad(type == MLOG_8BYTES);

	mach_write_to_8(ptr, val);

	log_ptr = mlog_open(mtr, 30);
	
	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, type, log_ptr, mtr);

	mach_write_to_2(log_ptr, ptr - buf_frame_align(ptr));
	log_ptr += 2;
	
	log_ptr += mach_dulint_write_compressed(log_ptr, val);

	mlog_close(mtr, log_ptr);
}

/************************************************************
Writes a string to a file page buffered in the buffer pool. Writes the
corresponding log record to the mini-transaction log. */

void
mlog_write_string(
/*==============*/
	byte*	ptr,	/* in: pointer where to write */
	byte*	str,	/* in: string to write */
	ulint	len,	/* in: string length */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	byte*	log_ptr;

	ut_ad(ptr && mtr);
	ut_ad(len < UNIV_PAGE_SIZE);

	ut_memcpy(ptr, str, len);

	log_ptr = mlog_open(mtr, 30);
	
	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, MLOG_WRITE_STRING,
								log_ptr, mtr);
	mach_write_to_2(log_ptr, ptr - buf_frame_align(ptr));
	log_ptr += 2;
	
	mach_write_to_2(log_ptr, len);
	log_ptr += 2;

	mlog_close(mtr, log_ptr);

	mlog_catenate_string(mtr, str, len);
}

/************************************************************
Parses a log record written by mlog_write_string. */

byte*
mlog_parse_string(
/*==============*/
			/* out: parsed record end, NULL if not a complete
			record */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	byte*	page)	/* in: page where to apply the log record, or NULL */
{
	ulint	offset;
	ulint	len;

	if (end_ptr < ptr + 4) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	ptr += 2;

	len = mach_read_from_2(ptr);
	ptr += 2;

	if (end_ptr < ptr + len) {

		return(NULL);
	}

	if (page) {
		ut_memcpy(page + offset, ptr, len);
	}

	return(ptr + len);
}
