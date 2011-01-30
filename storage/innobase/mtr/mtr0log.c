/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

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
@file mtr/mtr0log.c
Mini-transaction log routines

Created 12/7/1995 Heikki Tuuri
*******************************************************/

#include "mtr0log.h"

#ifdef UNIV_NONINL
#include "mtr0log.ic"
#endif

#include "buf0buf.h"
#include "dict0dict.h"
#include "log0recv.h"
#include "page0page.h"

#ifndef UNIV_HOTBACKUP
# include "dict0boot.h"

/********************************************************//**
Catenates n bytes to the mtr log. */
UNIV_INTERN
void
mlog_catenate_string(
/*=================*/
	mtr_t*		mtr,	/*!< in: mtr */
	const byte*	str,	/*!< in: string to write */
	ulint		len)	/*!< in: string length */
{
	dyn_array_t*	mlog;

	if (mtr_get_log_mode(mtr) == MTR_LOG_NONE) {

		return;
	}

	mlog = &(mtr->log);

	dyn_push_string(mlog, str, len);
}

/********************************************************//**
Writes the initial part of a log record consisting of one-byte item
type and four-byte space and page numbers. Also pushes info
to the mtr memo that a buffer page has been modified. */
UNIV_INTERN
void
mlog_write_initial_log_record(
/*==========================*/
	const byte*	ptr,	/*!< in: pointer to (inside) a buffer
				frame holding the file page where
				modification is made */
	byte		type,	/*!< in: log item type: MLOG_1BYTE, ... */
	mtr_t*		mtr)	/*!< in: mini-transaction handle */
{
	byte*	log_ptr;

	ut_ad(type <= MLOG_BIGGEST_TYPE);
	ut_ad(type > MLOG_8BYTES);

	log_ptr = mlog_open(mtr, 11);

	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, type, log_ptr, mtr);

	mlog_close(mtr, log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************//**
Parses an initial log record written by mlog_write_initial_log_record.
@return	parsed record end, NULL if not a complete record */
UNIV_INTERN
byte*
mlog_parse_initial_log_record(
/*==========================*/
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	byte*	type,	/*!< out: log record type: MLOG_1BYTE, ... */
	ulint*	space,	/*!< out: space id */
	ulint*	page_no)/*!< out: page number */
{
	if (end_ptr < ptr + 1) {

		return(NULL);
	}

	*type = (byte)((ulint)*ptr & ~MLOG_SINGLE_REC_FLAG);
	ut_ad(*type <= MLOG_BIGGEST_TYPE);

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

/********************************************************//**
Parses a log record written by mlog_write_ulint or mlog_write_ull.
@return	parsed record end, NULL if not a complete record or a corrupt record */
UNIV_INTERN
byte*
mlog_parse_nbytes(
/*==============*/
	ulint	type,	/*!< in: log record type: MLOG_1BYTE, ... */
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	byte*	page,	/*!< in: page where to apply the log record, or NULL */
	void*	page_zip)/*!< in/out: compressed page, or NULL */
{
	ulint		offset;
	ulint		val;
	ib_uint64_t	dval;

	ut_a(type <= MLOG_8BYTES);
	ut_a(!page || !page_zip || fil_page_get_type(page) != FIL_PAGE_INDEX);

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	ptr += 2;

	if (offset >= UNIV_PAGE_SIZE) {
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	if (type == MLOG_8BYTES) {
		ptr = mach_ull_parse_compressed(ptr, end_ptr, &dval);

		if (ptr == NULL) {

			return(NULL);
		}

		if (page) {
			if (UNIV_LIKELY_NULL(page_zip)) {
				mach_write_to_8
					(((page_zip_des_t*) page_zip)->data
					 + offset, dval);
			}
			mach_write_to_8(page + offset, dval);
		}

		return(ptr);
	}

	ptr = mach_parse_compressed(ptr, end_ptr, &val);

	if (ptr == NULL) {

		return(NULL);
	}

	switch (type) {
	case MLOG_1BYTE:
		if (UNIV_UNLIKELY(val > 0xFFUL)) {
			goto corrupt;
		}
		if (page) {
			if (UNIV_LIKELY_NULL(page_zip)) {
				mach_write_to_1
					(((page_zip_des_t*) page_zip)->data
					 + offset, val);
			}
			mach_write_to_1(page + offset, val);
		}
		break;
	case MLOG_2BYTES:
		if (UNIV_UNLIKELY(val > 0xFFFFUL)) {
			goto corrupt;
		}
		if (page) {
			if (UNIV_LIKELY_NULL(page_zip)) {
				mach_write_to_2
					(((page_zip_des_t*) page_zip)->data
					 + offset, val);
			}
			mach_write_to_2(page + offset, val);
		}
		break;
	case MLOG_4BYTES:
		if (page) {
			if (UNIV_LIKELY_NULL(page_zip)) {
				mach_write_to_4
					(((page_zip_des_t*) page_zip)->data
					 + offset, val);
			}
			mach_write_to_4(page + offset, val);
		}
		break;
	default:
	corrupt:
		recv_sys->found_corrupt_log = TRUE;
		ptr = NULL;
	}

	return(ptr);
}

/********************************************************//**
Writes 1 - 4 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */
UNIV_INTERN
void
mlog_write_ulint(
/*=============*/
	byte*	ptr,	/*!< in: pointer where to write */
	ulint	val,	/*!< in: value to write */
	byte	type,	/*!< in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*	mtr)	/*!< in: mini-transaction handle */
{
	byte*	log_ptr;

	switch (type) {
	case MLOG_1BYTE:
		mach_write_to_1(ptr, val);
		break;
	case MLOG_2BYTES:
		mach_write_to_2(ptr, val);
		break;
	case MLOG_4BYTES:
		mach_write_to_4(ptr, val);
		break;
	default:
		ut_error;
	}

	log_ptr = mlog_open(mtr, 11 + 2 + 5);

	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, type, log_ptr, mtr);

	mach_write_to_2(log_ptr, page_offset(ptr));
	log_ptr += 2;

	log_ptr += mach_write_compressed(log_ptr, val);

	mlog_close(mtr, log_ptr);
}

/********************************************************//**
Writes 8 bytes to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */
UNIV_INTERN
void
mlog_write_ull(
/*===========*/
	byte*		ptr,	/*!< in: pointer where to write */
	ib_uint64_t	val,	/*!< in: value to write */
	mtr_t*		mtr)	/*!< in: mini-transaction handle */
{
	byte*	log_ptr;

	ut_ad(ptr && mtr);

	mach_write_to_8(ptr, val);

	log_ptr = mlog_open(mtr, 11 + 2 + 9);

	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, MLOG_8BYTES,
						     log_ptr, mtr);

	mach_write_to_2(log_ptr, page_offset(ptr));
	log_ptr += 2;

	log_ptr += mach_ull_write_compressed(log_ptr, val);

	mlog_close(mtr, log_ptr);
}

#ifndef UNIV_HOTBACKUP
/********************************************************//**
Writes a string to a file page buffered in the buffer pool. Writes the
corresponding log record to the mini-transaction log. */
UNIV_INTERN
void
mlog_write_string(
/*==============*/
	byte*		ptr,	/*!< in: pointer where to write */
	const byte*	str,	/*!< in: string to write */
	ulint		len,	/*!< in: string length */
	mtr_t*		mtr)	/*!< in: mini-transaction handle */
{
	ut_ad(ptr && mtr);
	ut_a(len < UNIV_PAGE_SIZE);

	memcpy(ptr, str, len);

	mlog_log_string(ptr, len, mtr);
}

/********************************************************//**
Logs a write of a string to a file page buffered in the buffer pool.
Writes the corresponding log record to the mini-transaction log. */
UNIV_INTERN
void
mlog_log_string(
/*============*/
	byte*	ptr,	/*!< in: pointer written to */
	ulint	len,	/*!< in: string length */
	mtr_t*	mtr)	/*!< in: mini-transaction handle */
{
	byte*	log_ptr;

	ut_ad(ptr && mtr);
	ut_ad(len <= UNIV_PAGE_SIZE);

	log_ptr = mlog_open(mtr, 30);

	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, MLOG_WRITE_STRING,
						     log_ptr, mtr);
	mach_write_to_2(log_ptr, page_offset(ptr));
	log_ptr += 2;

	mach_write_to_2(log_ptr, len);
	log_ptr += 2;

	mlog_close(mtr, log_ptr);

	mlog_catenate_string(mtr, ptr, len);
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************//**
Parses a log record written by mlog_write_string.
@return	parsed record end, NULL if not a complete record */
UNIV_INTERN
byte*
mlog_parse_string(
/*==============*/
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	byte*	page,	/*!< in: page where to apply the log record, or NULL */
	void*	page_zip)/*!< in/out: compressed page, or NULL */
{
	ulint	offset;
	ulint	len;

	ut_a(!page || !page_zip || fil_page_get_type(page) != FIL_PAGE_INDEX);

	if (end_ptr < ptr + 4) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	ptr += 2;
	len = mach_read_from_2(ptr);
	ptr += 2;

	if (UNIV_UNLIKELY(offset >= UNIV_PAGE_SIZE)
	    || UNIV_UNLIKELY(len + offset > UNIV_PAGE_SIZE)) {
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	if (end_ptr < ptr + len) {

		return(NULL);
	}

	if (page) {
		if (UNIV_LIKELY_NULL(page_zip)) {
			memcpy(((page_zip_des_t*) page_zip)->data
				+ offset, ptr, len);
		}
		memcpy(page + offset, ptr, len);
	}

	return(ptr + len);
}

#ifndef UNIV_HOTBACKUP
/********************************************************//**
Opens a buffer for mlog, writes the initial log record and,
if needed, the field lengths of an index.
@return	buffer, NULL if log mode MTR_LOG_NONE */
UNIV_INTERN
byte*
mlog_open_and_write_index(
/*======================*/
	mtr_t*		mtr,	/*!< in: mtr */
	const byte*	rec,	/*!< in: index record or page */
	dict_index_t*	index,	/*!< in: record descriptor */
	byte		type,	/*!< in: log item type */
	ulint		size)	/*!< in: requested buffer size in bytes
				(if 0, calls mlog_close() and returns NULL) */
{
	byte*		log_ptr;
	const byte*	log_start;
	const byte*	log_end;

	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));

	if (!page_rec_is_comp(rec)) {
		log_start = log_ptr = mlog_open(mtr, 11 + size);
		if (!log_ptr) {
			return(NULL); /* logging is disabled */
		}
		log_ptr = mlog_write_initial_log_record_fast(rec, type,
							     log_ptr, mtr);
		log_end = log_ptr + 11 + size;
	} else {
		ulint	i;
		ulint	n	= dict_index_get_n_fields(index);
		/* total size needed */
		ulint	total	= 11 + size + (n + 2) * 2;
		ulint	alloc	= total;
		/* allocate at most DYN_ARRAY_DATA_SIZE at a time */
		if (alloc > DYN_ARRAY_DATA_SIZE) {
			alloc = DYN_ARRAY_DATA_SIZE;
		}
		log_start = log_ptr = mlog_open(mtr, alloc);
		if (!log_ptr) {
			return(NULL); /* logging is disabled */
		}
		log_end = log_ptr + alloc;
		log_ptr = mlog_write_initial_log_record_fast(rec, type,
							     log_ptr, mtr);
		mach_write_to_2(log_ptr, n);
		log_ptr += 2;
		mach_write_to_2(log_ptr,
				dict_index_get_n_unique_in_tree(index));
		log_ptr += 2;
		for (i = 0; i < n; i++) {
			dict_field_t*		field;
			const dict_col_t*	col;
			ulint			len;

			field = dict_index_get_nth_field(index, i);
			col = dict_field_get_col(field);
			len = field->fixed_len;
			ut_ad(len < 0x7fff);
			if (len == 0
			    && (col->len > 255 || col->mtype == DATA_BLOB)) {
				/* variable-length field
				with maximum length > 255 */
				len = 0x7fff;
			}
			if (col->prtype & DATA_NOT_NULL) {
				len |= 0x8000;
			}
			if (log_ptr + 2 > log_end) {
				mlog_close(mtr, log_ptr);
				ut_a(total > (ulint) (log_ptr - log_start));
				total -= log_ptr - log_start;
				alloc = total;
				if (alloc > DYN_ARRAY_DATA_SIZE) {
					alloc = DYN_ARRAY_DATA_SIZE;
				}
				log_start = log_ptr = mlog_open(mtr, alloc);
				if (!log_ptr) {
					return(NULL); /* logging is disabled */
				}
				log_end = log_ptr + alloc;
			}
			mach_write_to_2(log_ptr, len);
			log_ptr += 2;
		}
	}
	if (size == 0) {
		mlog_close(mtr, log_ptr);
		log_ptr = NULL;
	} else if (log_ptr + size > log_end) {
		mlog_close(mtr, log_ptr);
		log_ptr = mlog_open(mtr, size);
	}
	return(log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************//**
Parses a log record written by mlog_open_and_write_index.
@return	parsed record end, NULL if not a complete record */
UNIV_INTERN
byte*
mlog_parse_index(
/*=============*/
	byte*		ptr,	/*!< in: buffer */
	const byte*	end_ptr,/*!< in: buffer end */
	ibool		comp,	/*!< in: TRUE=compact record format */
	dict_index_t**	index)	/*!< out, own: dummy index */
{
	ulint		i, n, n_uniq;
	dict_table_t*	table;
	dict_index_t*	ind;

	ut_ad(comp == FALSE || comp == TRUE);

	if (comp) {
		if (end_ptr < ptr + 4) {
			return(NULL);
		}
		n = mach_read_from_2(ptr);
		ptr += 2;
		n_uniq = mach_read_from_2(ptr);
		ptr += 2;
		ut_ad(n_uniq <= n);
		if (end_ptr < ptr + n * 2) {
			return(NULL);
		}
	} else {
		n = n_uniq = 1;
	}
	table = dict_mem_table_create("LOG_DUMMY", DICT_HDR_SPACE, n,
				      comp ? DICT_TF_COMPACT : 0);
	ind = dict_mem_index_create("LOG_DUMMY", "LOG_DUMMY",
				    DICT_HDR_SPACE, 0, n);
	ind->table = table;
	ind->n_uniq = (unsigned int) n_uniq;
	if (n_uniq != n) {
		ut_a(n_uniq + DATA_ROLL_PTR <= n);
		ind->type = DICT_CLUSTERED;
	}
	if (comp) {
		for (i = 0; i < n; i++) {
			ulint	len = mach_read_from_2(ptr);
			ptr += 2;
			/* The high-order bit of len is the NOT NULL flag;
			the rest is 0 or 0x7fff for variable-length fields,
			and 1..0x7ffe for fixed-length fields. */
			dict_mem_table_add_col(
				table, NULL, NULL,
				((len + 1) & 0x7fff) <= 1
				? DATA_BINARY : DATA_FIXBINARY,
				len & 0x8000 ? DATA_NOT_NULL : 0,
				len & 0x7fff);

			dict_index_add_col(ind, table,
					   dict_table_get_nth_col(table, i),
					   0);
		}
		dict_table_add_system_columns(table, table->heap);
		if (n_uniq != n) {
			/* Identify DB_TRX_ID and DB_ROLL_PTR in the index. */
			ut_a(DATA_TRX_ID_LEN
			     == dict_index_get_nth_col(ind, DATA_TRX_ID - 1
						       + n_uniq)->len);
			ut_a(DATA_ROLL_PTR_LEN
			     == dict_index_get_nth_col(ind, DATA_ROLL_PTR - 1
						       + n_uniq)->len);
			ind->fields[DATA_TRX_ID - 1 + n_uniq].col
				= &table->cols[n + DATA_TRX_ID];
			ind->fields[DATA_ROLL_PTR - 1 + n_uniq].col
				= &table->cols[n + DATA_ROLL_PTR];
		}
	}
	/* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
	ind->cached = TRUE;
	*index = ind;
	return(ptr);
}
