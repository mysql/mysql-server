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
#include "log0recv.h"

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
	ut_ad(type > MLOG_8BYTES);

	if (ptr < buf_pool->frame_zero || ptr >= buf_pool->high_end) {
		fprintf(stderr,
	"InnoDB: Error: trying to write to a stray memory location %p\n", ptr);
		ut_error;
	}

	log_ptr = mlog_open(mtr, 11);

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

/************************************************************
Parses a log record written by mlog_write_ulint or mlog_write_dulint. */

byte*
mlog_parse_nbytes(
/*==============*/
			/* out: parsed record end, NULL if not a complete
			record or a corrupt record */
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
		
	if (offset >= UNIV_PAGE_SIZE) {
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

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

	if (type == MLOG_1BYTE) {
		if (val > 0xFFUL) {
			recv_sys->found_corrupt_log = TRUE;

			return(NULL);
		}
	} else if (type == MLOG_2BYTES) {
		if (val > 0xFFFFUL) {
			recv_sys->found_corrupt_log = TRUE;

			return(NULL);
		}
	} else {
		if (type != MLOG_4BYTES) {
			recv_sys->found_corrupt_log = TRUE;

			return(NULL);
		}
	}

	if (page) {
		if (type == MLOG_1BYTE) {
			mach_write_to_1(page + offset, val);
		} else if (type == MLOG_2BYTES) {
			mach_write_to_2(page + offset, val);
		} else {
			ut_a(type == MLOG_4BYTES);
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
	
	if (ptr < buf_pool->frame_zero || ptr >= buf_pool->high_end) {
		fprintf(stderr,
	"InnoDB: Error: trying to write to a stray memory location %p\n", ptr);
		ut_error;
	}

	if (type == MLOG_1BYTE) {
		mach_write_to_1(ptr, val);
	} else if (type == MLOG_2BYTES) {
		mach_write_to_2(ptr, val);
	} else {
		ut_ad(type == MLOG_4BYTES);
		mach_write_to_4(ptr, val);
	}

	log_ptr = mlog_open(mtr, 11 + 2 + 5);
	
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
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	byte*	log_ptr;

	if (ptr < buf_pool->frame_zero || ptr >= buf_pool->high_end) {
		fprintf(stderr,
	"InnoDB: Error: trying to write to a stray memory location %p\n", ptr);
		ut_error;
	}

	ut_ad(ptr && mtr);

	mach_write_to_8(ptr, val);

	log_ptr = mlog_open(mtr, 11 + 2 + 9);
	
	/* If no logging is requested, we may return now */
	if (log_ptr == NULL) {

		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(ptr, MLOG_8BYTES,
							log_ptr, mtr);

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

	if (ptr < buf_pool->frame_zero || ptr >= buf_pool->high_end) {
		fprintf(stderr,
	"InnoDB: Error: trying to write to a stray memory location %p\n", ptr);
		ut_error;
	}
	ut_ad(ptr && mtr);
	ut_a(len < UNIV_PAGE_SIZE);

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

	if (offset >= UNIV_PAGE_SIZE) {
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	len = mach_read_from_2(ptr);
	ptr += 2;

	ut_a(len + offset < UNIV_PAGE_SIZE);

	if (end_ptr < ptr + len) {

		return(NULL);
	}

	if (page) {
		ut_memcpy(page + offset, ptr, len);
	}

	return(ptr + len);
}

/************************************************************
Opens a buffer for mlog, writes the initial log record and,
if needed, the field lengths of an index. */

byte*
mlog_open_and_write_index(
/*======================*/
				/* out: buffer, NULL if log mode
				MTR_LOG_NONE */
	mtr_t*		mtr,	/* in: mtr */
	byte*		rec,	/* in: index record or page */
	dict_index_t*	index,	/* in: record descriptor */
	byte		type,	/* in: log item type */
	ulint		size)	/* in: requested buffer size in bytes
				(if 0, calls mlog_close() and returns NULL) */
{
	byte*		log_ptr;
	const byte*	log_start;
	const byte*	log_end;

	if (!index->table->comp) {
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
			dict_field_t*	field;
			dtype_t*	type;
			ulint		len;
			field = dict_index_get_nth_field(index, i);
			type = dict_col_get_type(dict_field_get_col(field));
			len = field->fixed_len;
			ut_ad(len < 0x7fff);
			if (len == 0 && dtype_get_len(type) > 255) {
				/* variable-length field
				with maximum length > 255 */
				len = 0x7fff;
			}
			if (dtype_get_prtype(type) & DATA_NOT_NULL) {
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

/************************************************************
Parses a log record written by mlog_open_and_write_index. */

byte*
mlog_parse_index(
/*=============*/
				/* out: parsed record end,
				NULL if not a complete record */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
				/* out: new value of log_ptr */
	ibool		comp,	/* in: TRUE=compact record format */
	dict_index_t**	index)	/* out, own: dummy index */
{
	ulint		i, n, n_uniq;
	dict_table_t*	table;
	dict_index_t*	ind;

	if (comp) {
		if (end_ptr < ptr + 4) {
			return(NULL);
		}
		n = mach_read_from_2(ptr);
		ptr += 2;
		n_uniq = mach_read_from_2(ptr);
		ut_ad(n_uniq <= n);
		if (end_ptr < ptr + (n + 1) * 2) {
			return(NULL);
		}
	} else {
		n = n_uniq = 1;
	}
	table = dict_mem_table_create("LOG_DUMMY", DICT_HDR_SPACE, n, comp);
	ind = dict_mem_index_create("LOG_DUMMY", "LOG_DUMMY",
				DICT_HDR_SPACE, 0, n);
	ind->table = table;
	ind->n_uniq = n_uniq;
	if (n_uniq != n) {
		ind->type = DICT_CLUSTERED;
	}
	/* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
	ind->cached = TRUE;
	if (comp) {
		for (i = 0; i < n; i++) {
			ulint	len = mach_read_from_2(ptr += 2);
			/* The high-order bit of len is the NOT NULL flag;
			the rest is 0 or 0x7fff for variable-length fields,
			and 1..0x7ffe for fixed-length fields. */
			dict_mem_table_add_col(table, "DUMMY",
					((len + 1) & 0x7fff) <= 1
						? DATA_BINARY
						: DATA_FIXBINARY,
					len & 0x8000 ? DATA_NOT_NULL : 0,
					len & 0x7fff, 0);
			dict_index_add_col(ind,
				dict_table_get_nth_col(table, i), 0, 0);
		}
		ptr += 2;
	}
	*index = ind;
	return(ptr);
}
