/*****************************************************************************

Copyright (c) 1994, 2013, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/********************************************************************//**
@file page/page0cur.cc
The page cursor

Created 10/4/1994 Heikki Tuuri
*************************************************************************/

#include "page0cur.h"
#ifdef UNIV_NONINL
#include "page0cur.ic"
#endif

#include "page0zip.h"
#include "row0upd.h"
#include "btr0btr.h"
#include "ibuf0ibuf.h"
#include "btr0sea.h"
#include "mtr0log.h"
#include "lock0lock.h"
#include "log0recv.h"
#include "ut0ut.h"
#ifndef UNIV_HOTBACKUP
#include "rem0cmp.h"

#include <algorithm>

using std::min;

#ifdef PAGE_CUR_ADAPT
# ifdef UNIV_SEARCH_PERF_STAT
static ulint	page_cur_short_succ	= 0;
# endif /* UNIV_SEARCH_PERF_STAT */

/*******************************************************************//**
This is a linear congruential generator PRNG. Returns a pseudo random
number between 0 and 2^64-1 inclusive. The formula and the constants
being used are:
X[n+1] = (a * X[n] + c) mod m
where:
X[0] = ut_time_us(NULL)
a = 1103515245 (3^5 * 5 * 7 * 129749)
c = 12345 (3 * 5 * 823)
m = 18446744073709551616 (2^64)

@return	number between 0 and 2^64-1 */
static
ib_uint64_t
page_cur_lcg_prng(void)
/*===================*/
{
#define LCG_a	1103515245
#define LCG_c	12345
	static ib_uint64_t	lcg_current = 0;
	static ibool		initialized = FALSE;

	if (!initialized) {
		lcg_current = (ib_uint64_t) ut_time_us(NULL);
		initialized = TRUE;
	}

	/* no need to "% 2^64" explicitly because lcg_current is
	64 bit and this will be done anyway */
	lcg_current = LCG_a * lcg_current + LCG_c;

	return(lcg_current);
}

/****************************************************************//**
Tries a search shortcut based on the last insert.
@return	TRUE on success */
UNIV_INLINE
ibool
page_cur_try_search_shortcut(
/*=========================*/
	const buf_block_t*	block,	/*!< in: index page */
	const dict_index_t*	index,	/*!< in: record descriptor */
	const dtuple_t*		tuple,	/*!< in: data tuple */
	ulint*			iup_matched_fields,
					/*!< in/out: already matched
					fields in upper limit record */
	ulint*			ilow_matched_fields,
					/*!< in/out: already matched
					fields in lower limit record */
	page_cur_t*		cursor) /*!< out: page cursor */
{
	const rec_t*	rec;
	const rec_t*	next_rec;
	ulint		low_match;
	ulint		up_match;
	ibool		success		= FALSE;
	const page_t*	page		= buf_block_get_frame(block);
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	ut_ad(dtuple_check_typed(tuple));

	rec = page_header_get_ptr(page, PAGE_LAST_INSERT);
	offsets = rec_get_offsets(rec, index, offsets,
				  dtuple_get_n_fields(tuple), &heap);

	ut_ad(rec);
	ut_ad(page_rec_is_user_rec(rec));

	low_match = up_match = min(*ilow_matched_fields, *iup_matched_fields);

	if (cmp_dtuple_rec_with_match(tuple, rec, offsets,
				      &low_match) < 0) {
		goto exit_func;
	}

	next_rec = page_rec_get_next_const(rec);

	if (!page_rec_is_supremum(next_rec)) {
		offsets = rec_get_offsets(next_rec, index, offsets,
					  dtuple_get_n_fields(tuple), &heap);

		if (cmp_dtuple_rec_with_match(tuple, next_rec, offsets,
					      &up_match) >= 0) {
			goto exit_func;
		}

		*iup_matched_fields = up_match;
	}

	page_cur_position(rec, block, cursor);

	*ilow_matched_fields = low_match;

#ifdef UNIV_SEARCH_PERF_STAT
	page_cur_short_succ++;
#endif
	success = TRUE;
exit_func:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(success);
}

#endif

/****************************************************************//**
Searches the right position for a page cursor. */

void
page_cur_search_with_match(
/*=======================*/
	const buf_block_t*	block,	/*!< in: buffer block */
	const dict_index_t*	index,	/*!< in: record descriptor */
	const dtuple_t*		tuple,	/*!< in: data tuple */
	ulint			mode,	/*!< in: PAGE_CUR_L,
					PAGE_CUR_LE, PAGE_CUR_G, or
					PAGE_CUR_GE */
	ulint*			iup_matched_fields,
					/*!< in/out: already matched
					fields in upper limit record */
	ulint*			ilow_matched_fields,
					/*!< in/out: already matched
					fields in lower limit record */
	page_cur_t*		cursor)	/*!< out: page cursor */
{
	ulint		up;
	ulint		low;
	ulint		mid;
	const page_t*	page;
	const page_dir_slot_t* slot;
	const rec_t*	up_rec;
	const rec_t*	low_rec;
	const rec_t*	mid_rec;
	ulint		up_matched_fields;
	ulint		low_matched_fields;
	ulint		cur_matched_fields;
	int		cmp;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	ut_ad(dtuple_validate(tuple));
#ifdef UNIV_DEBUG
	switch (mode) {
	case PAGE_CUR_L:
	case PAGE_CUR_LE:
	case PAGE_CUR_G:
	case PAGE_CUR_GE:
		goto mode_ok;
	}
	ut_ad(0);
mode_ok:
#endif /* UNIV_DEBUG */
	page = buf_block_get_frame(block);
	page_zip_validate_if_zip(buf_block_get_page_zip(block), page, index);

	page_check_dir(page);

#ifdef PAGE_CUR_ADAPT
	if (page_is_leaf(page)
	    && (mode == PAGE_CUR_LE)
	    && (page_header_get_field(page, PAGE_N_DIRECTION) > 3)
	    && (page_header_get_ptr(page, PAGE_LAST_INSERT))
	    && (page_header_get_field(page, PAGE_DIRECTION) == PAGE_RIGHT)) {

		if (page_cur_try_search_shortcut(
			    block, index, tuple,
			    iup_matched_fields,
			    ilow_matched_fields,
			    cursor)) {
			return;
		}
	}
#endif

	/* If mode PAGE_CUR_G is specified, we are trying to position the
	cursor to answer a query of the form "tuple < X", where tuple is
	the input parameter, and X denotes an arbitrary physical record on
	the page. We want to position the cursor on the first X which
	satisfies the condition. */

	up_matched_fields  = *iup_matched_fields;
	low_matched_fields = *ilow_matched_fields;

	/* Perform binary search. First the search is done through the page
	directory, after that as a linear search in the list of records
	owned by the upper limit directory slot. */

	low = 0;
	up = page_dir_get_n_slots(page) - 1;

	/* Perform binary search until the lower and upper limit directory
	slots come to the distance 1 of each other */

	while (up - low > 1) {
		mid = (low + up) / 2;
		slot = page_dir_get_nth_slot(page, mid);
		mid_rec = page_dir_slot_get_rec(slot);

		cur_matched_fields = min(low_matched_fields,
					 up_matched_fields);

		offsets = rec_get_offsets(mid_rec, index, offsets,
					  dtuple_get_n_fields_cmp(tuple),
					  &heap);

		cmp = cmp_dtuple_rec_with_match(tuple, mid_rec, offsets,
						&cur_matched_fields);
		if (cmp > 0) {
low_slot_match:
			low = mid;
			low_matched_fields = cur_matched_fields;

		} else if (cmp) {
up_slot_match:
			up = mid;
			up_matched_fields = cur_matched_fields;

		} else if (mode == PAGE_CUR_G || mode == PAGE_CUR_LE) {

			goto low_slot_match;
		} else {

			goto up_slot_match;
		}
	}

	slot = page_dir_get_nth_slot(page, low);
	low_rec = page_dir_slot_get_rec(slot);
	slot = page_dir_get_nth_slot(page, up);
	up_rec = page_dir_slot_get_rec(slot);

	/* Perform linear search until the upper and lower records come to
	distance 1 of each other. */

	while (page_rec_get_next_const(low_rec) != up_rec) {

		mid_rec = page_rec_get_next_const(low_rec);

		cur_matched_fields = min(low_matched_fields,
					 up_matched_fields);

		offsets = rec_get_offsets(mid_rec, index, offsets,
					  dtuple_get_n_fields_cmp(tuple),
					  &heap);

		cmp = cmp_dtuple_rec_with_match(tuple, mid_rec, offsets,
						&cur_matched_fields);
		if (cmp > 0) {
low_rec_match:
			low_rec = mid_rec;
			low_matched_fields = cur_matched_fields;
		} else if (cmp) {
up_rec_match:
			up_rec = mid_rec;
			up_matched_fields = cur_matched_fields;
		} else if (mode == PAGE_CUR_G || mode == PAGE_CUR_LE) {

			goto low_rec_match;
		} else {

			goto up_rec_match;
		}
	}

	if (mode <= PAGE_CUR_GE) {
		page_cur_position(up_rec, block, cursor);
	} else {
		page_cur_position(low_rec, block, cursor);
	}

	*iup_matched_fields  = up_matched_fields;
	*ilow_matched_fields = low_matched_fields;
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/***********************************************************//**
Positions a page cursor on a randomly chosen user record on a page. If there
are no user records, sets the cursor on the infimum record. */

void
page_cur_open_on_rnd_user_rec(
/*==========================*/
	buf_block_t*	block,	/*!< in: page */
	page_cur_t*	cursor)	/*!< out: page cursor */
{
	const page_t*	page = buf_block_get_frame(block);

	if (page_is_empty(page)) {
		page_cur_set_before_first(block, cursor);
		return;
	}

	ulint 	rnd = (ulint) (page_cur_lcg_prng() % page_get_n_recs(page));
	page_cur_position(page_rec_get_nth_const(page, 1 + rnd),
			  block, cursor);
}

/***********************************************************//**
Writes the log record of a record insert on a page. */
static
void
page_cur_insert_rec_write_log(
/*==========================*/
	const rec_t*		insert_rec,	/*!< in: inserted record */
	ulint			rec_size,	/*!< in: insert_rec size */
	const rec_t*		cursor_rec,	/*!< in: record the
						cursor is pointing to */
	const dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*			mtr)		/*!< in/out: mini-transaction */
{
	ulint	cur_rec_size;
	ulint	extra_size;
	ulint	cur_extra_size;
	const byte* ins_ptr;
	byte*	log_ptr;
	const byte* log_end;
	ulint	i;

	/* Avoid REDO logging to save on costly IO because
	temporary tables are not recovered during crash recovery. */
	if (dict_table_is_temporary(index->table)) {
		mtr->modifications = TRUE;
		return;
	}

	ut_a(rec_size < UNIV_PAGE_SIZE);
	ut_ad(page_align(insert_rec) == page_align(cursor_rec));
	ut_ad(!page_rec_is_comp(insert_rec)
	      == !dict_table_is_comp(index->table));

	{
		mem_heap_t*	heap		= NULL;
		ulint		cur_offs_[REC_OFFS_NORMAL_SIZE];
		ulint		ins_offs_[REC_OFFS_NORMAL_SIZE];

		ulint*		cur_offs;
		ulint*		ins_offs;

		rec_offs_init(cur_offs_);
		rec_offs_init(ins_offs_);

		cur_offs = rec_get_offsets(cursor_rec, index, cur_offs_,
					   ULINT_UNDEFINED, &heap);
		ins_offs = rec_get_offsets(insert_rec, index, ins_offs_,
					   ULINT_UNDEFINED, &heap);

		extra_size = rec_offs_extra_size(ins_offs);
		cur_extra_size = rec_offs_extra_size(cur_offs);
		ut_ad(rec_size == rec_offs_size(ins_offs));
		cur_rec_size = rec_offs_size(cur_offs);

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}

	ins_ptr = insert_rec - extra_size;

	i = 0;

	if (cur_extra_size == extra_size) {
		ulint		min_rec_size = ut_min(cur_rec_size, rec_size);

		const byte*	cur_ptr = cursor_rec - cur_extra_size;

		/* Find out the first byte in insert_rec which differs from
		cursor_rec; skip the bytes in the record info */

		do {
			if (*ins_ptr == *cur_ptr) {
				i++;
				ins_ptr++;
				cur_ptr++;
			} else if ((i < extra_size)
				   && (i >= extra_size
				       - page_rec_get_base_extra_size
				       (insert_rec))) {
				i = extra_size;
				ins_ptr = insert_rec;
				cur_ptr = cursor_rec;
			} else {
				break;
			}
		} while (i < min_rec_size);
	}

	if (mtr_get_log_mode(mtr) != MTR_LOG_SHORT_INSERTS) {

		if (page_rec_is_comp(insert_rec)) {
			log_ptr = mlog_open_and_write_index(
				mtr, insert_rec, index, MLOG_COMP_REC_INSERT,
				2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN);
			if (UNIV_UNLIKELY(!log_ptr)) {
				/* Logging in mtr is switched off
				during crash recovery: in that case
				mlog_open returns NULL */
				return;
			}
		} else {
			log_ptr = mlog_open(mtr, 11
					    + 2 + 5 + 1 + 5 + 5
					    + MLOG_BUF_MARGIN);
			if (UNIV_UNLIKELY(!log_ptr)) {
				/* Logging in mtr is switched off
				during crash recovery: in that case
				mlog_open returns NULL */
				return;
			}

			log_ptr = mlog_write_initial_log_record_fast(
				insert_rec, MLOG_REC_INSERT, log_ptr, mtr);
		}

		log_end = &log_ptr[2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN];
		/* Write the cursor rec offset as a 2-byte ulint */
		mach_write_to_2(log_ptr, page_offset(cursor_rec));
		log_ptr += 2;
	} else {
		log_ptr = mlog_open(mtr, 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN);
		if (!log_ptr) {
			/* Logging in mtr is switched off during crash
			recovery: in that case mlog_open returns NULL */
			return;
		}
		log_end = &log_ptr[5 + 1 + 5 + 5 + MLOG_BUF_MARGIN];
	}

	if (page_rec_is_comp(insert_rec)) {
		if (UNIV_UNLIKELY
		    (rec_get_info_and_status_bits(insert_rec, TRUE)
		     != rec_get_info_and_status_bits(cursor_rec, TRUE))) {

			goto need_extra_info;
		}
	} else {
		if (UNIV_UNLIKELY
		    (rec_get_info_and_status_bits(insert_rec, FALSE)
		     != rec_get_info_and_status_bits(cursor_rec, FALSE))) {

			goto need_extra_info;
		}
	}

	if (extra_size != cur_extra_size || rec_size != cur_rec_size) {
need_extra_info:
		/* Write the record end segment length
		and the extra info storage flag */
		log_ptr += mach_write_compressed(log_ptr,
						 2 * (rec_size - i) + 1);

		/* Write the info bits */
		mach_write_to_1(log_ptr,
				rec_get_info_and_status_bits(
					insert_rec,
					page_rec_is_comp(insert_rec)));
		log_ptr++;

		/* Write the record origin offset */
		log_ptr += mach_write_compressed(log_ptr, extra_size);

		/* Write the mismatch index */
		log_ptr += mach_write_compressed(log_ptr, i);

		ut_a(i < UNIV_PAGE_SIZE);
		ut_a(extra_size < UNIV_PAGE_SIZE);
	} else {
		/* Write the record end segment length
		and the extra info storage flag */
		log_ptr += mach_write_compressed(log_ptr, 2 * (rec_size - i));
	}

	/* Write to the log the inserted index record end segment which
	differs from the cursor record */

	rec_size -= i;

	if (log_ptr + rec_size <= log_end) {
		memcpy(log_ptr, ins_ptr, rec_size);
		mlog_close(mtr, log_ptr + rec_size);
	} else {
		mlog_close(mtr, log_ptr);
		ut_a(rec_size < UNIV_PAGE_SIZE);
		mlog_catenate_string(mtr, ins_ptr, rec_size);
	}
}
#else /* !UNIV_HOTBACKUP */
# define page_cur_insert_rec_write_log(ins_rec,size,cur,index,mtr) ((void) 0)
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Parses a log record of a record insert on a page.
@return	end of log record or NULL */

byte*
page_cur_parse_insert_rec(
/*======================*/
	ibool		is_short,/*!< in: TRUE if short inserts */
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in: page or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
{
	ulint	origin_offset;
	ulint	end_seg_len;
	ulint	mismatch_index;
	ulint	rec_offset;
	byte	buf1[1024];
	byte*	buf;
	const byte* const	ptr2	= ptr;
	ulint	info_and_status_bits = 0; /* remove warning */

	if (is_short) {
		rec_offset = 0;
	} else {
		/* Read the cursor rec offset as a 2-byte ulint */

		if (UNIV_UNLIKELY(end_ptr < ptr + 2)) {

			return(NULL);
		}

		rec_offset = mach_read_from_2(ptr);
		ptr += 2;

		if (UNIV_UNLIKELY(rec_offset >= UNIV_PAGE_SIZE)) {

			recv_sys->found_corrupt_log = TRUE;

			return(NULL);
		}
	}

	ptr = mach_parse_compressed(ptr, end_ptr, &end_seg_len);

	if (ptr == NULL) {

		return(NULL);
	}

	if (UNIV_UNLIKELY(end_seg_len >= UNIV_PAGE_SIZE << 1)) {
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	if (end_seg_len & 0x1UL) {
		/* Read the info bits */

		if (end_ptr < ptr + 1) {

			return(NULL);
		}

		info_and_status_bits = mach_read_from_1(ptr);
		ptr++;

		ptr = mach_parse_compressed(ptr, end_ptr, &origin_offset);

		if (ptr == NULL) {

			return(NULL);
		}

		ut_ad(origin_offset >= REC_N_NEW_EXTRA_BYTES);
		ut_a(origin_offset < UNIV_PAGE_SIZE);

		ptr = mach_parse_compressed(ptr, end_ptr, &mismatch_index);

		if (ptr == NULL) {

			return(NULL);
		}

		ut_a(mismatch_index < UNIV_PAGE_SIZE);
	}

	ptr += (end_seg_len >> 1);

	if (UNIV_UNLIKELY(end_ptr < ptr)) {

		return(NULL);
	}

	if (!block) {

		return(ptr);
	}

	PageCur cursor(mtr, index, block);

	if (rec_offset) {
		cursor.setRec(buf_block_get_frame(block) + rec_offset);
	} else {
		cursor.setAfterLast();
		cursor.prev();
	}

	ulint		cur_extra_size;
	const ulint	cur_data_size = cursor.getRecSize(cur_extra_size);

	/* Read from the log the inserted index record end segment which
	differs from the cursor record */

	if (!(end_seg_len & 0x1UL)) {
		info_and_status_bits = rec_get_info_and_status_bits(
			cursor.getRec(), cursor.isComp());
		origin_offset = cur_extra_size;
		mismatch_index = cur_data_size + cur_extra_size
			- (end_seg_len >> 1);
	}

	end_seg_len >>= 1;

	/* Build the inserted record to buf */

        if (UNIV_UNLIKELY(mismatch_index >= UNIV_PAGE_SIZE)) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Is short %lu, info_and_status_bits %lu, offset %lu, "
			"o_offset %lu, mismatch index %lu, end_seg_len %lu"
			"parsed len %lu",
			(ulong) is_short, (ulong) info_and_status_bits,
			(ulong) page_offset(cursor.getRec()),
			(ulong) origin_offset,
			(ulong) mismatch_index, (ulong) end_seg_len,
			(ulong) (ptr - ptr2));

		fputs("Dump of 300 bytes of log:\n", stderr);
		ut_print_buf(stderr, ptr2, 300);
		putc('\n', stderr);

		buf_page_print(buf_block_get_frame(block), 0, 0);

		ut_error;
	}

	if (mismatch_index + end_seg_len < sizeof buf1) {
		buf = buf1;
	} else {
		buf = new byte[mismatch_index + end_seg_len];
	}

	memcpy(buf, cursor.getRec() - cur_extra_size, mismatch_index);
	memcpy(buf + mismatch_index, ptr - end_seg_len, end_seg_len);

	if (cursor.isComp()) {
		rec_set_info_and_status_bits(buf + origin_offset,
					     info_and_status_bits);
	} else {
		rec_set_info_bits_old(buf + origin_offset,
				      info_and_status_bits);
	}

	if (!cursor.insert(buf + origin_offset, origin_offset,
			   end_seg_len + mismatch_index - origin_offset)) {
		/* The redo log record should only have been written
		after the write was successful. */
		ut_error;
	}

	if (buf != buf1) {
		delete[] buf;
	}

	return(ptr);
}

/** Inserts a record next to page cursor on an uncompressed page.
@param[in/out]	current_rec	record after which the new record is inserted
@param[in]	index		B-tree index
@param[in]	rec		physical record
@param[in]	extra		size of rec header, in bytes
@param[in]	data		size of rec data, in bytes
@param[in/out]	mtr		mini-transaction, NULL=no redo logging
@return	pointer to record if succeed, NULL otherwise */

rec_t*
page_cur_insert_rec_low(
	rec_t*			current_rec,
	const dict_index_t*	index,
	const rec_t*		rec,
	ulint			extra,
	ulint			data,
	mtr_t*			mtr)
{
	byte*		insert_buf;
	const ulint	rec_size	= extra + data;
	page_t*		page;		/*!< the relevant page */
	rec_t*		last_insert;	/*!< cursor position at previous
					insert */
	rec_t*		insert_rec;	/*!< inserted record */
	ulint		heap_no;	/*!< heap number of the inserted
					record */

	page = page_align(current_rec);
	ut_ad(dict_table_is_comp(index->table)
	      == (ibool) !!page_is_comp(page));
	ut_ad(fil_page_get_type(page) == FIL_PAGE_INDEX);
	ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID)
	      == index->id || recv_recovery_is_on() || mtr->inside_ibuf);

	ut_ad(!page_rec_is_supremum(current_rec));

	/* All data bytes of the record must be valid. */
	UNIV_MEM_ASSERT_RW(rec, data);
	/* The variable-length header must be valid. */
	UNIV_MEM_ASSERT_RW(rec - extra,
			   extra - (page_is_comp(page)
				    ? REC_N_NEW_EXTRA_BYTES
				    : REC_N_OLD_EXTRA_BYTES));

	if (rec_t* free_rec = page_header_get_ptr(page, PAGE_FREE)) {
		ulint	free_extra_size;
		ulint	free_data_size = page_is_comp(page)
			? rec_get_size_comp(free_rec, index, free_extra_size)
			: rec_get_size_old(free_rec, free_extra_size);

		if (free_extra_size + free_data_size < rec_size) {
			goto use_heap;
		}

		insert_buf = free_rec - free_extra_size;

		if (page_is_comp(page)) {
			heap_no = rec_get_heap_no_new(free_rec);
			page_mem_alloc_free(page, NULL,
					rec_get_next_ptr(free_rec, TRUE),
					rec_size);
		} else {
			heap_no = rec_get_heap_no_old(free_rec);
			page_mem_alloc_free(page, NULL,
					rec_get_next_ptr(free_rec, FALSE),
					rec_size);
		}
	} else {
use_heap:
		insert_buf = page_mem_alloc_heap(page, NULL,
						 rec_size, &heap_no);

		if (UNIV_UNLIKELY(insert_buf == NULL)) {
			return(NULL);
		}
	}

	/* Create the record */
	memcpy(insert_buf, rec - extra, rec_size);
	insert_rec = insert_buf + extra;

	/* Insert the record in the linked list of records */
	ut_ad(current_rec != insert_rec);

	{
		/* next record after current before the insertion */
		rec_t*	next_rec = page_rec_get_next(current_rec);
#ifdef UNIV_DEBUG
		if (page_is_comp(page)) {
			ut_ad(rec_get_status(current_rec)
				<= REC_STATUS_INFIMUM);
			ut_ad(rec_get_status(insert_rec) < REC_STATUS_INFIMUM);
			ut_ad(rec_get_status(next_rec) != REC_STATUS_INFIMUM);
		}
#endif
		page_rec_set_next(insert_rec, next_rec);
		page_rec_set_next(current_rec, insert_rec);
	}

	page_header_set_field(page, NULL, PAGE_N_RECS,
			      1 + page_get_n_recs(page));

	/* Set the n_owned field in the inserted record to zero,
	and set the heap_no field */
	if (page_is_comp(page)) {
		rec_set_n_owned_new(insert_rec, NULL, 0);
		rec_set_heap_no_new(insert_rec, heap_no);
	} else {
		rec_set_n_owned_old(insert_rec, 0);
		rec_set_heap_no_old(insert_rec, heap_no);
	}

	UNIV_MEM_ASSERT_RW(insert_buf, rec_size);
	/* Update the last insertion info in page header */

	last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
	ut_ad(!last_insert || !page_is_comp(page)
	      || rec_get_node_ptr_flag(last_insert)
	      == rec_get_node_ptr_flag(insert_rec));

	if (UNIV_UNLIKELY(last_insert == NULL)) {
		page_header_set_field(page, NULL, PAGE_DIRECTION,
				      PAGE_NO_DIRECTION);
		page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);

	} else if ((last_insert == current_rec)
		   && (page_header_get_field(page, PAGE_DIRECTION)
		       != PAGE_LEFT)) {

		page_header_set_field(page, NULL, PAGE_DIRECTION,
							PAGE_RIGHT);
		page_header_set_field(page, NULL, PAGE_N_DIRECTION,
				      page_header_get_field(
					      page, PAGE_N_DIRECTION) + 1);

	} else if ((page_rec_get_next(insert_rec) == last_insert)
		   && (page_header_get_field(page, PAGE_DIRECTION)
		       != PAGE_RIGHT)) {

		page_header_set_field(page, NULL, PAGE_DIRECTION,
							PAGE_LEFT);
		page_header_set_field(page, NULL, PAGE_N_DIRECTION,
				      page_header_get_field(
					      page, PAGE_N_DIRECTION) + 1);
	} else {
		page_header_set_field(page, NULL, PAGE_DIRECTION,
							PAGE_NO_DIRECTION);
		page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);
	}

	page_header_set_ptr(page, NULL, PAGE_LAST_INSERT, insert_rec);

	/* 7. It remains to update the owner record. */
	{
		rec_t*	owner_rec	= page_rec_find_owner_rec(insert_rec);
		ulint	n_owned;
		if (page_is_comp(page)) {
			n_owned = rec_get_n_owned_new(owner_rec);
			rec_set_n_owned_new(owner_rec, NULL, n_owned + 1);
		} else {
			n_owned = rec_get_n_owned_old(owner_rec);
			rec_set_n_owned_old(owner_rec, n_owned + 1);
		}

		/* 8. Now we have incremented the n_owned field of the owner
		record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
		we have to split the corresponding directory slot in two. */

		if (UNIV_UNLIKELY(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED)) {
			page_dir_split_slot(
				page, NULL,
				page_dir_find_owner_slot(owner_rec));
		}
	}

	/* 9. Write log record of the insert */
	if (UNIV_LIKELY(mtr != NULL)) {
		page_cur_insert_rec_write_log(insert_rec, rec_size,
					      current_rec, index, mtr);
	}

	return(insert_rec);
}

/***********************************************************//**
Inserts a record next to page cursor on a compressed and uncompressed
page. Returns pointer to inserted record if succeed, i.e.,
enough space available, NULL otherwise.
The cursor stays at the same position.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@return	pointer to record if succeed, NULL otherwise */

rec_t*
page_cur_insert_rec_zip(
/*====================*/
	page_cur_t*	cursor,	/*!< in/out: page cursor */
	dict_index_t*	index,	/*!< in: record descriptor */
	const rec_t*	rec,	/*!< in: pointer to a physical record */
	ulint*		offsets,/*!< in/out: rec_get_offsets(rec, index) */
	mtr_t*		mtr)	/*!< in: mini-transaction handle, or NULL */
{
	byte*		insert_buf;
	ulint		rec_size;
	page_t*		page;		/*!< the relevant page */
	rec_t*		last_insert;	/*!< cursor position at previous
					insert */
	rec_t*		free_rec;	/*!< a free record that was reused,
					or NULL */
	rec_t*		insert_rec;	/*!< inserted record */
	ulint		heap_no;	/*!< heap number of the inserted
					record */
	page_zip_des_t*	page_zip;

	page_zip = page_cur_get_page_zip(cursor);
	ut_ad(page_zip);

	ut_ad(rec_offs_validate(rec, index, offsets));

	page = page_cur_get_page(cursor);
	ut_ad(dict_table_is_comp(index->table));
	ut_ad(page_is_comp(page));
	ut_ad(fil_page_get_type(page) == FIL_PAGE_INDEX);
	ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID)
	      == index->id || mtr->inside_ibuf || recv_recovery_is_on());

	ut_ad(!page_cur_is_after_last(cursor));
	page_zip_validate_if_zip(page_zip, page, index);

	/* 1. Get the size of the physical record in the page */
	rec_size = rec_offs_size(offsets);

#ifdef UNIV_DEBUG_VALGRIND
	{
		const void*	rec_start
			= rec - rec_offs_extra_size(offsets);
		ulint		extra_size
			= rec_offs_extra_size(offsets)
			- (rec_offs_comp(offsets)
			   ? REC_N_NEW_EXTRA_BYTES
			   : REC_N_OLD_EXTRA_BYTES);

		/* All data bytes of the record must be valid. */
		UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
		/* The variable-length header must be valid. */
		UNIV_MEM_ASSERT_RW(rec_start, extra_size);
	}
#endif /* UNIV_DEBUG_VALGRIND */

	const bool reorg_before_insert = page_has_garbage(page)
		&& rec_size > page_get_max_insert_size(page, 1)
		&& rec_size <= page_get_max_insert_size_after_reorganize(
			page, 1);

	/* 2. Try to find suitable space from page memory management */
	if (!page_zip_available(page_zip, dict_index_is_clust(index),
				rec_size, 1)
	    || reorg_before_insert) {
		/* The values can change dynamically. */
		bool	log_compressed	= page_zip_log_pages;
		ulint	level		= page_zip_level;
#ifdef UNIV_DEBUG
		rec_t*	cursor_rec	= page_cur_get_rec(cursor);
#endif /* UNIV_DEBUG */

		/* If we are not writing compressed page images, we
		must reorganize the page before attempting the
		insert. */
		if (recv_recovery_is_on()) {
			/* Insert into the uncompressed page only.
			The page reorganization or creation that we
			would attempt outside crash recovery would
			have been covered by a previous redo log record. */
		} else if (page_is_empty(page)) {
			ut_ad(page_cur_is_before_first(cursor));

			/* This is an empty page. Recreate it to
			get rid of the modification log. */
			page_create_zip(page_cur_get_block(cursor), index,
					page_header_get_field(page, PAGE_LEVEL),
					level, 0, mtr);
			ut_ad(!page_header_get_ptr(page, PAGE_FREE));

			if (page_zip_available(
				    page_zip, dict_index_is_clust(index),
				    rec_size, 1)) {
				goto use_heap;
			}

			/* The cursor should remain on the page infimum. */
			return(NULL);
		} else if (!page_zip->m_nonempty && !page_has_garbage(page)) {
			/* The page has been freshly compressed, so
			reorganizing it will not help. */
		} else if (log_compressed && !reorg_before_insert) {
			/* Insert into uncompressed page only, and
			try page_zip_reorganize() afterwards. */
		} else if (btr_page_reorganize_low(
				   recv_recovery_is_on(), level,
				   cursor, index, mtr)) {
			ut_ad(!page_header_get_ptr(page, PAGE_FREE));

			if (page_zip_available(
				    page_zip, dict_index_is_clust(index),
				    rec_size, 1)) {
				/* After reorganizing, there is space
				available. */
				goto use_heap;
			}
		} else {
			ut_ad(cursor->rec == cursor_rec);
			return(NULL);
		}

		/* Try compressing the whole page afterwards. */
		insert_rec = page_cur_insert_rec_low(
			cursor->rec, index, rec,
			rec_offs_extra_size(offsets),
			rec_offs_data_size(offsets), NULL);
		ut_d(if (insert_rec) rec_offs_make_valid(
			     insert_rec, index, offsets));

		/* If recovery is on, this implies that the compression
		of the page was successful during runtime. Had that not
		been the case or had the redo logging of compressed
		pages been enabled during runtime then we'd have seen
		a MLOG_ZIP_PAGE_COMPRESS redo record. Therefore, we
		know that we don't need to reorganize the page. We,
		however, do need to recompress the page. That will
		happen when the next redo record is read which must
		be of type MLOG_ZIP_PAGE_COMPRESS_NO_DATA and it must
		contain a valid compression level value.
		This implies that during recovery from this point till
		the next redo is applied the uncompressed and
		compressed versions are not identical and
		page_zip_validate will fail but that is OK because
		we call page_zip_validate only after processing
		all changes to a page under a single mtr during
		recovery. */
		if (insert_rec == NULL) {
			/* Out of space.
			This should never occur during crash recovery,
			because the MLOG_COMP_REC_INSERT should only
			be logged after a successful operation. */
			ut_ad(!recv_recovery_is_on());
		} else if (recv_recovery_is_on()) {
			/* This should be followed by
			MLOG_ZIP_PAGE_COMPRESS_NO_DATA,
			which should succeed. */
			rec_offs_make_valid(insert_rec, index, offsets);
		} else {
			ulint	pos = page_rec_get_n_recs_before(insert_rec);
			ut_ad(pos > 0);

			if (!log_compressed) {
				if (page_zip_compress(
					    page_zip, page, index,
					    level, NULL)) {
					page_cur_insert_rec_write_log(
						insert_rec, rec_size,
						cursor->rec, index, mtr);
					page_zip_compress_write_log_no_data(
						level, page, index, mtr);

					rec_offs_make_valid(
						insert_rec, index, offsets);
					return(insert_rec);
				}

				ut_ad(cursor->rec
				      == page_rec_get_nth(page, pos - 1));
			} else {
				/* We are writing entire page images
				to the log. Reduce the redo log volume
				by reorganizing the page at the same time. */
				if (page_zip_reorganize(
					    cursor->block, index, mtr)) {
					/* The page was reorganized:
					Seek to pos. */
					cursor->rec = page_rec_get_nth(
						page, pos - 1);

					insert_rec = page + rec_get_next_offs(
						cursor->rec, TRUE);
					rec_offs_make_valid(
						insert_rec, index, offsets);
					return(insert_rec);
				}

				/* Theoretically, we could try one
				last resort of btr_page_reorganize_low()
				followed by page_zip_available(), but
				that would be very unlikely to
				succeed. (If the full reorganized page
				failed to compress, why would it
				succeed to compress the page, plus log
				the insert of this record? */
			}

			/* Out of space: restore the page */
			if (!page_zip_decompress(page_zip, page, FALSE)) {
				ut_error; /* Memory corrupted? */
			}
			ut_ad(page_validate(cursor->block, index));
			insert_rec = NULL;
		}

		return(insert_rec);
	}

	free_rec = page_header_get_ptr(page, PAGE_FREE);
	if (UNIV_LIKELY_NULL(free_rec)) {
		/* Try to allocate from the head of the free list. */
		lint	extra_size_diff;
		ulint	free_extra_size;
		ulint	free_data_size = rec_get_size_comp(
			free_rec, index, free_extra_size);

		if (free_extra_size + free_data_size < rec_size) {
			goto use_heap;
		}

		insert_buf = free_rec - free_extra_size;

		/* On compressed pages, do not relocate records from
		the free list.  If extra_size would grow, use the heap. */
		extra_size_diff = free_extra_size
			- rec_offs_extra_size(offsets);

		if (extra_size_diff > 0) {
			/* Add an offset to the extra_size. */
			if (free_data_size + free_extra_size
			    < rec_size + extra_size_diff) {
				goto use_heap;
			}

			insert_buf += extra_size_diff;
		} else if (extra_size_diff) {
			/* Do not allow extra_size to grow */
			goto use_heap;
		}

		heap_no = rec_get_heap_no_new(free_rec);
		page_mem_alloc_free(page, page_zip,
				    rec_get_next_ptr(free_rec, TRUE),
				    rec_size);
	} else {
use_heap:
		free_rec = NULL;
		insert_buf = page_mem_alloc_heap(page, page_zip,
						 rec_size, &heap_no);

		if (UNIV_UNLIKELY(insert_buf == NULL)) {
			return(NULL);
		}

		page_zip_dir_add_slot(page_zip, dict_index_is_clust(index));
	}

	/* 3. Create the record */
	insert_rec = rec_copy(insert_buf, rec, offsets);
	rec_offs_make_valid(insert_rec, index, offsets);

	/* 4. Insert the record in the linked list of records */
	ut_ad(cursor->rec != insert_rec);

	{
		/* next record after current before the insertion */
		const rec_t*	next_rec = page_rec_get_next_low(
			cursor->rec, TRUE);
		ut_ad(rec_get_status(cursor->rec)
		      <= REC_STATUS_INFIMUM);
		ut_ad(rec_get_status(insert_rec) < REC_STATUS_INFIMUM);
		ut_ad(rec_get_status(next_rec) != REC_STATUS_INFIMUM);

		page_rec_set_next(insert_rec, next_rec);
		page_rec_set_next(cursor->rec, insert_rec);
	}

	page_header_set_field(page, page_zip, PAGE_N_RECS,
			      1 + page_get_n_recs(page));

	/* 5. Set the n_owned field in the inserted record to zero,
	and set the heap_no field */
	rec_set_n_owned_new(insert_rec, NULL, 0);
	rec_set_heap_no_new(insert_rec, heap_no);

	UNIV_MEM_ASSERT_RW(rec_get_start(insert_rec, offsets),
			   rec_offs_size(offsets));

	page_zip_dir_insert(page_zip, cursor->rec, free_rec, insert_rec);

	/* 6. Update the last insertion info in page header */

	last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
	ut_ad(!last_insert
	      || rec_get_node_ptr_flag(last_insert)
	      == rec_get_node_ptr_flag(insert_rec));

	if (UNIV_UNLIKELY(last_insert == NULL)) {
		page_header_set_field(page, page_zip, PAGE_DIRECTION,
							PAGE_NO_DIRECTION);
		page_header_set_field(page, page_zip, PAGE_N_DIRECTION, 0);

	} else if ((last_insert == cursor->rec)
		   && (page_header_get_field(page, PAGE_DIRECTION)
		       != PAGE_LEFT)) {

		page_header_set_field(page, page_zip, PAGE_DIRECTION,
							PAGE_RIGHT);
		page_header_set_field(page, page_zip, PAGE_N_DIRECTION,
				      page_header_get_field(
					      page, PAGE_N_DIRECTION) + 1);

	} else if ((page_rec_get_next(insert_rec) == last_insert)
		   && (page_header_get_field(page, PAGE_DIRECTION)
		       != PAGE_RIGHT)) {

		page_header_set_field(page, page_zip, PAGE_DIRECTION,
							PAGE_LEFT);
		page_header_set_field(page, page_zip, PAGE_N_DIRECTION,
				      page_header_get_field(
					      page, PAGE_N_DIRECTION) + 1);
	} else {
		page_header_set_field(page, page_zip, PAGE_DIRECTION,
							PAGE_NO_DIRECTION);
		page_header_set_field(page, page_zip, PAGE_N_DIRECTION, 0);
	}

	page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, insert_rec);

	/* 7. It remains to update the owner record. */
	{
		rec_t*	owner_rec	= page_rec_find_owner_rec(insert_rec);
		ulint	n_owned;

		n_owned = rec_get_n_owned_new(owner_rec);
		rec_set_n_owned_new(owner_rec, page_zip, n_owned + 1);

		/* 8. Now we have incremented the n_owned field of the owner
		record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
		we have to split the corresponding directory slot in two. */

		if (UNIV_UNLIKELY(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED)) {
			page_dir_split_slot(
				page, page_zip,
				page_dir_find_owner_slot(owner_rec));
		}
	}

	page_zip_write_rec(page_zip, insert_rec, index, offsets, 1);

	/* 9. Write log record of the insert */
	if (UNIV_LIKELY(mtr != NULL)) {
		page_cur_insert_rec_write_log(insert_rec, rec_size,
					      cursor->rec, index, mtr);
	}

	return(insert_rec);
}

/**********************************************************//**
Parses a log record of copying a record list end to a new created page.
@return	end of log record or NULL */

byte*
page_parse_copy_rec_list_to_created_page(
/*=====================================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in: page or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
{
	byte*		rec_end;
	ulint		log_data_len;
	page_t*		page;
	page_zip_des_t*	page_zip;

	if (ptr + 4 > end_ptr) {

		return(NULL);
	}

	log_data_len = mach_read_from_4(ptr);
	ptr += 4;

	rec_end = ptr + log_data_len;

	if (rec_end > end_ptr) {

		return(NULL);
	}

	if (!block) {

		return(rec_end);
	}

	while (ptr < rec_end) {
		ptr = page_cur_parse_insert_rec(TRUE, ptr, end_ptr,
						block, index, mtr);
	}

	ut_a(ptr == rec_end);

	page = buf_block_get_frame(block);
	page_zip = buf_block_get_page_zip(block);

	page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, NULL);
	page_header_set_field(page, page_zip, PAGE_DIRECTION,
							PAGE_NO_DIRECTION);
	page_header_set_field(page, page_zip, PAGE_N_DIRECTION, 0);

	return(rec_end);
}

#ifndef UNIV_HOTBACKUP
/***********************************************************//**
Writes log record of a record delete on a page. */
UNIV_INLINE
void
page_cur_delete_rec_write_log(
/*==========================*/
	const rec_t*		rec,	/*!< in: record to be deleted */
	const dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*			mtr)	/*!< in: mini-transaction handle */
{
	byte*	log_ptr;

	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));

	log_ptr = mlog_open_and_write_index(mtr, rec, index,
					    page_rec_is_comp(rec)
					    ? MLOG_COMP_REC_DELETE
					    : MLOG_REC_DELETE, 2);

	if (!log_ptr) {
		/* Logging in mtr is switched off during crash recovery:
		in that case mlog_open returns NULL */
		return;
	}

	/* Write the cursor rec offset as a 2-byte ulint */
	mach_write_to_2(log_ptr, page_offset(rec));

	mlog_close(mtr, log_ptr + 2);
}
#else /* !UNIV_HOTBACKUP */
# define page_cur_delete_rec_write_log(rec,index,mtr) ((void) 0)
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Parses log record of a record delete on a page.
@return	pointer to record end or NULL */

byte*
page_cur_parse_delete_rec(
/*======================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in: page or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
{
	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	/* Read the cursor rec offset as a 2-byte ulint */
	ulint	offset = mach_read_from_2(ptr);
	ptr += 2;

	ut_a(offset <= UNIV_PAGE_SIZE);

	if (block) {
		PageCur(mtr, index, block, block->frame + offset).purge();
	}

	return(ptr);
}

/***********************************************************//**
Deletes a record at the page cursor. The cursor is moved to the next
record after the deleted one. */

void
page_cur_delete_rec(
/*================*/
	page_cur_t*		cursor,	/*!< in/out: a page cursor */
	const dict_index_t*	index,	/*!< in: record descriptor */
	const ulint*		offsets,/*!< in: rec_get_offsets(
					cursor->rec, index) */
	mtr_t*			mtr)	/*!< in: mini-transaction handle */
{
	page_dir_slot_t* cur_dir_slot;
	page_dir_slot_t* prev_slot;
	page_t*		page;
	page_zip_des_t*	page_zip;
	rec_t*		current_rec;
	rec_t*		prev_rec	= NULL;
	rec_t*		next_rec;
	ulint		cur_slot_no;
	ulint		cur_n_owned;
	rec_t*		rec;

	page = page_cur_get_page(cursor);
	page_zip = page_cur_get_page_zip(cursor);

	/* page_zip_validate() will fail here when
	btr_cur_pessimistic_delete() invokes btr_set_min_rec_mark().
	Then, both "page_zip" and "page" would have the min-rec-mark
	set on the smallest user record, but "page" would additionally
	have it set on the smallest-but-one record.  Because sloppy
	page_zip_validate_low() only ignores min-rec-flag differences
	in the smallest user record, it cannot be used here either. */

	current_rec = cursor->rec;
	ut_ad(rec_offs_validate(current_rec, index, offsets));
	ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));
	ut_ad(fil_page_get_type(page) == FIL_PAGE_INDEX);
	ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID)
	      == index->id || mtr->inside_ibuf || recv_recovery_is_on());

	/* The record must not be the supremum or infimum record. */
	ut_ad(page_rec_is_user_rec(current_rec));

	if (page_get_n_recs(page) == 1) {
		/* Empty the page. */
		ut_ad(page_is_leaf(page));
		/* Usually, this should be the root page,
		and the whole index tree should become empty.
		However, this could also be a call in
		btr_cur_pessimistic_update() to delete the only
		record in the page and to insert another one. */
		page_cur_move_to_next(cursor);
		ut_ad(page_cur_is_after_last(cursor));
		page_create_empty(page_cur_get_block(cursor),
				  const_cast<dict_index_t*>(index), mtr);
		return;
	}

	/* Save to local variables some data associated with current_rec */
	cur_slot_no = page_dir_find_owner_slot(current_rec);
	ut_ad(cur_slot_no > 0);
	cur_dir_slot = page_dir_get_nth_slot(page, cur_slot_no);
	cur_n_owned = page_dir_slot_get_n_owned(cur_dir_slot);

	/* 0. Write the log record */
	if (mtr != 0) {
		page_cur_delete_rec_write_log(current_rec, index, mtr);
	}

	/* 1. Reset the last insert info in the page header and increment
	the modify clock for the frame */

	page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, NULL);

	/* The page gets invalid for optimistic searches: increment the
	frame modify clock only if there is an mini-transaction covering
	the change. During IMPORT we allocate local blocks that are not
	part of the buffer pool. */

	if (mtr != 0) {
		buf_block_modify_clock_inc(page_cur_get_block(cursor));
	}

	/* 2. Find the next and the previous record. Note that the cursor is
	left at the next record. */

	ut_ad(cur_slot_no > 0);
	prev_slot = page_dir_get_nth_slot(page, cur_slot_no - 1);

	rec = (rec_t*) page_dir_slot_get_rec(prev_slot);

	/* rec now points to the record of the previous directory slot. Look
	for the immediate predecessor of current_rec in a loop. */

	while(current_rec != rec) {
		prev_rec = rec;
		rec = page_rec_get_next(rec);
	}

	page_cur_move_to_next(cursor);
	next_rec = cursor->rec;

	/* 3. Remove the record from the linked list of records */

	page_rec_set_next(prev_rec, next_rec);

	/* 4. If the deleted record is pointed to by a dir slot, update the
	record pointer in slot. In the following if-clause we assume that
	prev_rec is owned by the same slot, i.e., PAGE_DIR_SLOT_MIN_N_OWNED
	>= 2. */

#if PAGE_DIR_SLOT_MIN_N_OWNED < 2
# error "PAGE_DIR_SLOT_MIN_N_OWNED < 2"
#endif
	ut_ad(cur_n_owned > 1);

	if (current_rec == page_dir_slot_get_rec(cur_dir_slot)) {
		page_dir_slot_set_rec(cur_dir_slot, prev_rec);
	}

	/* 5. Update the number of owned records of the slot */

	page_dir_slot_set_n_owned(cur_dir_slot, page_zip, cur_n_owned - 1);

	/* 6. Free the memory occupied by the record */
	page_mem_free(page, page_zip, current_rec, index,
		      rec_offs_data_size(offsets),
		      rec_offs_extra_size(offsets),
		      rec_offs_n_extern(offsets));

	/* 7. Now we have decremented the number of owned records of the slot.
	If the number drops below PAGE_DIR_SLOT_MIN_N_OWNED, we balance the
	slots. */

	if (cur_n_owned <= PAGE_DIR_SLOT_MIN_N_OWNED) {
		page_dir_balance_slot(page, page_zip, cur_slot_no);
	}

	page_zip_validate_if_zip(page_zip, page, index);
}

/** Initialize a page cursor, either to rec or the page infimum.
Allocates and initializes offsets[]. */

void
PageCur::init(void)
{
	ut_ad(dict_table_is_comp(m_index->table));

	const ulint	n	= getNumFields();
	const ulint	size	= n + (1 + REC_OFFS_HEADER_SIZE);

	m_offsets = new ulint[size];
	rec_offs_set_n_alloc(m_offsets, size);
	rec_offs_set_n_fields(m_offsets, n);

	if (!m_rec) {
		/* Initialize to the page infimum. */
		m_rec = getPage() + page_get_infimum_offset(getPage());
	} else {
		ut_ad(page_align(m_rec) == getPage());
		if (isUser()) {
			rec_init_offsets(m_rec, m_index, m_offsets);
			return;
		}
	}

	adjustSentinelOffsets();
}

/** Write a redo log record about an insert.
@param[in]	rec		inserted record
@param[in]	extra_size	size of insert_rec header, in bytes
@param[in]	data_size	size of insert_rec payload, in bytes */
inline
void
PageCur::logInsert(
	const rec_t*	rec,
	ulint		extra_size,
	ulint		data_size)
{
	ut_ad(isMutable());
	ut_ad(extra_size < UNIV_PAGE_SIZE);
	ut_ad(data_size < UNIV_PAGE_SIZE);
	ut_ad(rec != m_rec);

	if (!m_mtr) {
		return;
	}

	m_mtr->modifications = TRUE;

	switch (mtr_get_log_mode(m_mtr)) {
	case MTR_LOG_NONE:
	case MTR_LOG_NO_REDO:
		return;
	}

	if (dict_table_is_temporary(m_index->table)) {
		return;
	}

	ulint		cur_extra_size;
	ulint		cur_data_size = getRecSize(cur_extra_size);

	const byte*	ins_ptr = rec - extra_size;

	if (extra_size == cur_extra_size) {
		/* Find out the first byte in the inserted record
		which differs from the cursor record; skip the
		fixed-length header. */

		const byte*	cur_ptr
			= m_rec - cur_extra_size;
		const byte*	cur_extra_end
			= m_rec - getRecHeaderFixed();

		while (cur_ptr < cur_extra_end && *cur_ptr == *ins_ptr) {
			cur_ptr++;
			ins_ptr++;
		}

		if (cur_ptr == cur_extra_end) {
			const byte*	ins_end
				= rec + ut_min(cur_data_size, data_size);

			for (cur_ptr = getRec(), ins_ptr = rec;
			     ins_ptr < ins_end && *cur_ptr == *ins_ptr; ) {
				cur_ptr++;
				ins_ptr++;
			}
		}
	}

	byte*		log_ptr;
	const byte*	log_end;

	if (mtr_get_log_mode(m_mtr) != MTR_LOG_SHORT_INSERTS) {
		if (isComp()) {
			log_ptr = mlog_open_and_write_index(
				m_mtr, rec, m_index, MLOG_COMP_REC_INSERT,
				2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN);
		} else {
			log_ptr = mlog_open(m_mtr, 11
					    + 2 + 5 + 1 + 5 + 5
					    + MLOG_BUF_MARGIN);
			log_ptr = mlog_write_initial_log_record_fast(
				rec, MLOG_REC_INSERT, log_ptr, m_mtr);
		}

		mach_write_to_2(log_ptr, page_offset(m_rec));
		log_ptr += 2;
		log_end = &log_ptr[2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN];
	} else {
		log_ptr = mlog_open(m_mtr, 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN);
		log_end = &log_ptr[5 + 1 + 5 + 5 + MLOG_BUF_MARGIN];
	}

	const ulint	i		= ins_ptr - (rec - extra_size);
	ulint		length_info	= (extra_size + data_size - i) << 1;

	/* The adjacent records should differ at least in the last byte.
	Inserting fully duplicate records does not make any sense.
	In the internal ordering, records have to differ at least in
	the last fields (child page number in node pointers, or primary
	key values in secondary index leaf page records). */
	ut_ad(i < extra_size + data_size);

	if (extra_size != cur_extra_size || data_size != cur_data_size) {
		length_info |= 1;
	} else if (isComp()) {
		if (UNIV_UNLIKELY
		    (rec_get_info_and_status_bits(rec, TRUE)
		     != rec_get_info_and_status_bits(m_rec, TRUE))) {
			length_info |= 1;
		}
	} else {
		if (UNIV_UNLIKELY
		    (rec_get_info_and_status_bits(rec, FALSE)
		     != rec_get_info_and_status_bits(m_rec, FALSE))) {
			length_info |= 1;
		}
	}

	log_ptr += mach_write_compressed(log_ptr, length_info);

	if (length_info & 1) {
		/* Write the info bits */
		mach_write_to_1(log_ptr++,
				rec_get_info_and_status_bits(rec, isComp()));

		/* Write the record origin offset */
		log_ptr += mach_write_compressed(log_ptr, extra_size);

		/* Write the mismatch index */
		log_ptr += mach_write_compressed(log_ptr, i);
	}

	/* Write to the log the inserted index record end segment which
	differs from the cursor record */

	length_info >>= 1;
	ut_ad(length_info < UNIV_PAGE_SIZE);

	if (log_ptr + length_info <= log_end) {
		memcpy(log_ptr, ins_ptr, length_info);
		mlog_close(m_mtr, log_ptr + length_info);
	} else {
		mlog_close(m_mtr, log_ptr);
		mlog_catenate_string(m_mtr, ins_ptr, length_info);
	}
}

/** Insert an entry after the current cursor position
into a compressed page, recompressing the page.
The cursor stays at the same position.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@param[in]	rec		record to insert
@param[in]	extra_size	size of record header, in bytes
@param[in]	data_size	size of record payload, in bytes
@param[in]	reorg_first	whether to reorganize before inserting
@return	inserted record; NULL if out of space */
inline
const rec_t*
PageCur::insertZip(
	const rec_t*	rec,
	ulint		extra_size,
	ulint		data_size,
	bool		reorg_first)
{
	const ulint	rec_size	= extra_size + data_size;
	page_t*		page		= getPage();
	page_zip_des_t*	page_zip	= getPageZip();
#ifdef UNIV_DEBUG
	const rec_t*	cursor_rec	= m_rec;
#endif /* UNIV_DEBUG */

	ut_ad(isMutable());
	ut_ad(isComp());
	ut_ad(page_zip);
	ut_ad(!isAfterLast());
	ut_ad(fil_page_get_type(getPage()) == FIL_PAGE_INDEX);
	ut_ad(mach_read_from_8(getPage() + PAGE_HEADER + PAGE_INDEX_ID)
	      == m_index->id || recv_recovery_is_on()
	      || (m_mtr && m_mtr->inside_ibuf));
	ut_ad(getPage() != page_align(rec));

	/* The values can change dynamically. */
	bool		log_compressed	= page_zip_log_pages;
	ulint		level		= page_zip_level;

	/* If we are not writing compressed page images, we
	must reorganize the page before attempting the
	insert. */
	if (recv_recovery_is_on()) {
		/* Insert into the uncompressed page only.
		The page reorganization or creation that we
		would attempt outside crash recovery would
		have been covered by a previous redo log record. */
	} else if (page_is_empty(page)) {
		ut_ad(isBeforeFirst());

		/* This is an empty page. Recreate it to
		get rid of the modification log. */
		page_create_zip(m_block, m_index,
				page_header_get_field(page, PAGE_LEVEL),
				level, 0, m_mtr);
		ut_ad(!page_header_get_ptr(page, PAGE_FREE));

		if (page_zip_available(
			    page_zip, dict_index_is_clust(m_index),
			    rec_size, 1)) {
try_insert:
			return(insertNoReorganize(
				       rec, extra_size, data_size, page_zip));
		}

		/* The cursor should remain on the page infimum. */
		return(NULL);
	} else if (!page_zip->m_nonempty && !page_has_garbage(page)) {
		/* The page has been freshly compressed, so
		reorganizing it will not help. */
	} else if (log_compressed && !reorg_first) {
		/* Insert into uncompressed page only, and
		try reorganize() afterwards. */
	} else if (reorganize(recv_recovery_is_on(), true, level)) {
		ut_ad(!page_header_get_ptr(page, PAGE_FREE));
		if (page_zip_available(
			    page_zip, dict_index_is_clust(m_index),
			    rec_size, 1)) {
			/* After reorganizing, there is space available. */
			goto try_insert;
		}
	} else {
		ut_ad(m_rec == cursor_rec);
		return(NULL);
	}

	/* Try compressing the whole page afterwards. */
	const rec_t*	insert_rec = page_cur_insert_rec_low(
		m_rec, m_index, rec, extra_size, data_size, NULL);

	/* If recovery is on, this implies that the compression
	of the page was successful during runtime. Had that not
	been the case or had the redo logging of compressed
	pages been enabled during runtime then we'd have seen
	a MLOG_ZIP_PAGE_COMPRESS redo record. Therefore, we
	know that we don't need to reorganize the page. We,
	however, do need to recompress the page. That will
	happen when the next redo record is read which must
	be of type MLOG_ZIP_PAGE_COMPRESS_NO_DATA and it must
	contain a valid compression level value.
	This implies that during recovery from this point till
	the next redo is applied the uncompressed and
	compressed versions are not identical and
	page_zip_validate will fail but that is OK because
	we call page_zip_validate only after processing
	all changes to a page under a single mtr during
	recovery. */
	if (insert_rec == NULL) {
		/* Out of space.
		This should never occur during crash recovery,
		because the MLOG_COMP_REC_INSERT should only
		be logged after a successful operation. */
		ut_ad(!recv_recovery_is_on());
	} else if (recv_recovery_is_on()) {
		/* This should be followed by
		MLOG_ZIP_PAGE_COMPRESS_NO_DATA,
		which should succeed. */
	} else {
		if (!log_compressed) {
			if (page_zip_compress(
				    page_zip, page, m_index,
				    level, NULL)) {
				logInsert(insert_rec, extra_size, data_size);
				page_zip_compress_write_log_no_data(
					level, page, m_index, m_mtr);
				return(insert_rec);
			}

			/* Out of space: restore the page */
			if (!page_zip_decompress(page_zip, page, FALSE)) {
				ut_error; /* Memory corrupted? */
			}
			ut_ad(page_validate(m_block, m_index));
		} else {
			/* We are writing entire page images
			to the log. Reduce the redo log volume
			by reorganizing the page at the same time. */
			if (reorganize(false, false, level)) {
				return(page + rec_get_next_offs(m_rec, TRUE));
			}

			/* Theoretically, we could try one
			last resort of btr_page_reorganize_low()
			followed by page_zip_available(), but
			that would be very unlikely to
			succeed. (If the full reorganized page
			failed to compress, why would it
			succeed to compress the page, plus log
			the insert of this record? */
		}

		return(NULL);
	}

	goto try_insert;
}

/** Insert an entry after the current cursor position
without reorganizing the page.
The cursor stays at the same position.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@param[in]	rec		record to insert
@param[in]	extra_size	size of record header, in bytes
@param[in]	data_size	size of record payload, in bytes
@param[in/out]	page_zip	getPageZip(), or NULL to not
update the compressed page
@return	inserted record; NULL if out of space */

const rec_t*
PageCur::insertNoReorganize(
	const rec_t*	rec,
	ulint		extra_size,
	ulint		data_size,
	page_zip_des_t*	page_zip)
{
	ut_ad(!isAfterLast());
	ut_ad(fil_page_get_type(getPage()) == FIL_PAGE_INDEX);
	ut_ad(mach_read_from_8(getPage() + PAGE_HEADER + PAGE_INDEX_ID)
	      == m_index->id || recv_recovery_is_on()
	      || (m_mtr && m_mtr->inside_ibuf));
	ut_ad(getPage() != page_align(rec));
	ut_ad(isMutable());
	ut_ad(!page_zip || page_zip == getPageZip());

	page_zip_validate_if_zip(page_zip, getPage(), m_index);

	byte*		insert_buf;
	ulint		heap_no;
	page_t*		page		= getPage();
	rec_t*		free_rec = page_header_get_ptr(page, PAGE_FREE);

	if (free_rec) {
		rec_t*	next_free;

		/* Try to allocate from the head of the free list. */
		if (isComp()) {
			ulint	free_extra_size;
			ulint	free_data_size	= rec_get_size_comp(
				free_rec, m_index, free_extra_size);

			if (free_extra_size + free_data_size
			    < extra_size + data_size) {
				goto use_heap;
			}

			insert_buf = free_rec - free_extra_size;

			if (page_zip) {
				/* On compressed pages, do not relocate
				records from the free list. If extra_size
				would grow, use the heap. */
				lint	extra_size_diff
					= free_extra_size - extra_size;

				if (extra_size_diff > 0) {
					/* Add an offset to the extra_size. */
					if (free_data_size + free_extra_size
					    < extra_size
					    + data_size + extra_size_diff) {
						goto use_heap;
					}

					insert_buf += extra_size_diff;
				} else if (extra_size_diff) {
					/* Do not allow extra_size to grow */
					goto use_heap;
				}
			}

			heap_no = rec_get_heap_no_new(free_rec);
			next_free = rec_get_next_ptr(free_rec, TRUE);
		} else {
			ut_ad(!page_zip);

			ulint	free_extra_size;
			ulint	free_data_size
				= rec_get_size_old(free_rec, free_extra_size);

			if (free_extra_size + free_data_size
			    < extra_size + data_size) {
				goto use_heap;
			}

			insert_buf = free_rec - free_extra_size;
			heap_no = rec_get_heap_no_old(free_rec);
			next_free = rec_get_next_ptr(free_rec, FALSE);
		}

		page_mem_alloc_free(page, page_zip, next_free,
				    extra_size + data_size);
	} else {
use_heap:
		free_rec = NULL;

		insert_buf = page_mem_alloc_heap(
			page, page_zip, extra_size + data_size, &heap_no);

		if (!insert_buf) {
			return(NULL);
		}

		if (page_zip) {
			page_zip_dir_add_slot(
				page_zip, dict_index_is_clust(m_index));
		}
	}

	return(insertBuf(rec, extra_size, data_size, page_zip,
			 insert_buf, free_rec, heap_no));
}

/** Insert all records from a cursor to an empty page,
not updating the compressed page.

NOTE: The cursor must be positioned on the page infimum,
and at the end it will be positioned on the page supremum.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@param[in/out]	cursor		cursor pointing to first record to insert */

void
PageCur::appendEmptyNoZip(PageCur& cursor)
{
	ut_ad(isMutable());
	ut_ad(isBeforeFirst());
	ut_ad(cursor.isUser());
	ut_ad(cursor.m_index == m_index);
	ut_ad(cursor.m_block != m_block);
	ut_ad(!getOffsetsIfExist());
	ut_ad(!cursor.getOffsetsIfExist());
	ut_ad(isComp() == cursor.isComp());
	ut_ad(page_is_leaf(getPage()) == page_is_leaf(cursor.getPage()));

	page_t*	page	= getPage();

	ut_ad(page_is_empty(page));

	byte*	log_ptr;

	if (!m_mtr) {
		log_ptr = NULL;
	} else if (dict_table_is_temporary(m_index->table)) {
		m_mtr->modifications = TRUE;
		log_ptr = NULL;
	} else {
		log_ptr = mlog_open_and_write_index(
			m_mtr, page, m_index, isComp()
			? MLOG_COMP_LIST_END_COPY_CREATED
			: MLOG_LIST_END_COPY_CREATED, 4);
	}

	ulint	log_mode;

	if (log_ptr) {
		mlog_close(m_mtr, log_ptr + 4);
		mach_write_to_4(log_ptr, dyn_array_get_data_size(&m_mtr->log));
		log_mode = mtr_set_log_mode(m_mtr, MTR_LOG_SHORT_INSERTS);
	} else {
		log_mode = 0; /* remove warning */
	}

	byte*			heap_top= page
		+ (isComp() ? PAGE_NEW_SUPREMUM_END : PAGE_OLD_SUPREMUM_END);
	ulint			n_owned	= 0;
	page_dir_slot_t*	slot	= page_dir_get_nth_slot(page, 0);
	ulint			heap_no	= PAGE_HEAP_NO_USER_LOW;

	ut_ad(heap_top == page + page_header_get_field(page, PAGE_HEAP_TOP));
	ut_ad(heap_no == page_dir_get_n_heap(page));

	do {
		ulint	extra_size;
		ulint	data_size;

		if (isComp()) {
			data_size	= rec_get_size_comp(
				cursor.getRec(), m_index, extra_size);
			memcpy(heap_top, cursor.getRec() - extra_size,
			       extra_size + data_size);

			heap_top += extra_size;
			rec_set_next_offs_new(m_rec, page_offset(heap_top));

			rec_set_n_owned_new(heap_top, NULL, 0);
			rec_set_heap_no_new(heap_top, heap_no++);
		} else {
			data_size = rec_get_size_old(
				cursor.getRec(), extra_size);
			memcpy(heap_top, cursor.getRec() - extra_size,
			       extra_size + data_size);

			heap_top += extra_size;
			rec_set_next_offs_old(m_rec, page_offset(heap_top));

			rec_set_n_owned_old(heap_top, 0);
			rec_set_heap_no_old(heap_top, heap_no++);
		}

		ut_ad(page_align(heap_top) == page);

		if (++n_owned == (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2) {
			slot -= PAGE_DIR_SLOT_SIZE;
			mach_write_to_2(slot, page_offset(heap_top));

			if (isComp()) {
				rec_set_n_owned_new(heap_top, NULL, n_owned);
			} else {
				rec_set_n_owned_old(heap_top, n_owned);
			}

			n_owned = 0;
		}

		if (log_ptr) {
			logInsert(heap_top, extra_size, data_size);
		}

		m_rec = heap_top;
		heap_top += data_size;
	} while (cursor.next());

	if (++n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED / 2
	    && slot != page_dir_get_nth_slot(page, 0)) {
		/* Merge the last two page directory slots in order
		to imitate exactly page_cur_parse_insert_rec() in
		page_parse_copy_rec_list_to_created_page(). */
		n_owned += (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;
		page_dir_slot_set_n_owned(slot, NULL, 0);
	} else {
		slot -= PAGE_DIR_SLOT_SIZE;
	}

	if (log_ptr) {
		ulint	log_data_len = dyn_array_get_data_size(&m_mtr->log)
			- mach_read_from_4(log_ptr);
		/* The MTR_LOG_SHORT_INSERTS should save redo log
		volume by omitting common bytes between successive
		records.  Ideally, we should never write more log than
		the total size of the copied records (even if the
		first byte is always different, but there is some
		overhead in the format. */
		ut_ad(log_data_len < 3 * UNIV_PAGE_SIZE);
		mach_write_to_4(log_ptr, log_data_len);
		mtr_set_log_mode(m_mtr, log_mode);
	}

	if (isComp()) {
		/* The next-record offset is relative. It must be adjusted,
		because the last user record offset (m_rec) ought to differ
		between this and cursor. */
		rec_set_next_offs_new(m_rec, PAGE_NEW_SUPREMUM);
		mach_write_to_2(slot, PAGE_NEW_SUPREMUM);
		m_rec = page + PAGE_NEW_SUPREMUM;
		rec_set_n_owned_new(m_rec, NULL, n_owned);
	} else {
		/* The next-record offset is absolute in ROW_FORMAT=REDUNDANT;
		it must stay at the same offset. */
		ut_ad(rec_get_next_offs(m_rec, FALSE) == PAGE_OLD_SUPREMUM);
		mach_write_to_2(slot, PAGE_OLD_SUPREMUM);
		m_rec = page + PAGE_OLD_SUPREMUM;
		rec_set_n_owned_old(m_rec, n_owned);
	}

	ut_ad(heap_top > page + page_header_get_field(page, PAGE_HEAP_TOP));
	ut_ad(heap_no > PAGE_HEAP_NO_USER_LOW);

	page_dir_set_n_slots(page, NULL, 1
			     + (page_dir_get_nth_slot(page, 0) - slot)
			     / PAGE_DIR_SLOT_SIZE);
	page_header_set_ptr(page, NULL, PAGE_HEAP_TOP, heap_top);
	page_dir_set_n_heap(page, NULL, heap_no);
	page_header_set_field(page, NULL, PAGE_N_RECS,
			      heap_no - PAGE_HEAP_NO_USER_LOW);

	page_header_set_ptr(page, NULL, PAGE_LAST_INSERT, NULL);
	page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);
}

/** Insert an entry after the current cursor position.
The cursor stays at the same position.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@param[in]	rec		record to insert
@param[in]	extra_size	size of record header, in bytes
@param[in]	data_size	size of record payload, in bytes
@param[in/out]	page_zip	getPageZip(), or NULL to not
update the compressed page
@param[out]	insert_buf	storage for the inserted record
@param[in]	free_rec	record from which insert_buf was
allocated; NULL if not from the PAGE_FREE list
@param[in]	heap_no		heap_no of the inserted record
@return	insert_buf+extra_size */
inline
const rec_t*
PageCur::insertBuf(
	const rec_t*	rec,
	ulint		extra_size,
	ulint		data_size,
	page_zip_des_t*	page_zip,
	byte*		insert_buf,
	const byte*	free_rec,
	ulint		heap_no)
{
	ut_ad(page_align(insert_buf) == getPage());
	ut_ad(isMutable());

	page_t*		page		= getPage();
	rec_t*		insert_rec	= insert_buf + extra_size;

	ut_ad(getRec() != insert_rec);
	ut_ad(!page_zip || page_zip == getPageZip());

	memcpy(insert_buf, rec - extra_size, extra_size + data_size);

	/* Insert the record in the linked list of records. */

	/* next record after current before the insertion */
	const rec_t*	next_rec = page_rec_get_next_const(getRec());

	mach_write_to_2(PAGE_N_RECS + PAGE_HEADER + page,
			1 + page_get_n_recs(page));

	rec_t*		owner_rec;
	ulint		n_owned;

	if (isComp()) {
		ut_ad(rec_get_status(insert_rec) < REC_STATUS_INFIMUM);
		ut_ad(rec_get_status(next_rec) != REC_STATUS_INFIMUM);
		rec_set_next_offs_new(insert_rec, page_offset(next_rec));
		rec_set_next_offs_new(m_rec, page_offset(insert_rec));
		rec_set_n_owned_new(insert_rec, NULL, 0);
		rec_set_heap_no_new(insert_rec, heap_no);

		if (page_zip) {
			page_zip_write_header(page_zip,
					      PAGE_N_RECS + PAGE_HEADER + page,
					      2, NULL);
			page_zip_dir_insert(
				page_zip, m_rec, free_rec, insert_rec);
		}

		owner_rec = page_rec_find_owner_rec_comp(insert_rec);
		n_owned = rec_get_n_owned_new(owner_rec);
		rec_set_n_owned_new(owner_rec, page_zip, n_owned + 1);
	} else {
		ut_ad(!page_zip);
		rec_set_next_offs_old(insert_rec, page_offset(next_rec));
		rec_set_next_offs_old(m_rec, page_offset(insert_rec));
		rec_set_n_owned_old(insert_rec, 0);
		rec_set_heap_no_old(insert_rec, heap_no);

		owner_rec = page_rec_find_owner_rec_old(insert_rec);
		n_owned = rec_get_n_owned_old(owner_rec);
		rec_set_n_owned_old(owner_rec, n_owned + 1);
	}

	UNIV_MEM_ASSERT_RW(insert_buf, extra_size + data_size);

	if (UNIV_UNLIKELY(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED)) {
		page_dir_split_slot(page, page_zip,
				    page_dir_find_owner_slot(owner_rec));
	}

	/* Update the last insertion info in page header */

	const rec_t* last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
	page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, insert_rec);

	if (UNIV_UNLIKELY(last_insert == NULL)) {
		page_header_set_field(
			page, page_zip, PAGE_DIRECTION, PAGE_NO_DIRECTION);
		page_header_set_field(
			page, page_zip, PAGE_N_DIRECTION, 0);

	} else if (last_insert == m_rec
		   && page_header_get_field(page, PAGE_DIRECTION)
		   != PAGE_LEFT) {
		page_header_set_field(
			page, page_zip, PAGE_DIRECTION, PAGE_RIGHT);
		page_header_set_field(
			page, page_zip, PAGE_N_DIRECTION,
			page_header_get_field(page, PAGE_N_DIRECTION) + 1);

	} else if (page_rec_get_next(insert_rec) == last_insert
		   && page_header_get_field(page, PAGE_DIRECTION)
		   != PAGE_RIGHT) {
		page_header_set_field(
			page, page_zip, PAGE_DIRECTION, PAGE_LEFT);
		page_header_set_field(
			page, page_zip, PAGE_N_DIRECTION,
			page_header_get_field(page, PAGE_N_DIRECTION) + 1);
	} else {
		page_header_set_field(
			page, page_zip, PAGE_DIRECTION, PAGE_NO_DIRECTION);
		page_header_set_field(page, page_zip, PAGE_N_DIRECTION, 0);
	}

	if (page_zip) {
		PageCur cur_ins(*this);
		cur_ins.setRec(insert_rec);
		page_zip_write_rec(page_zip, insert_rec, m_index,
				   cur_ins.getOffsets(), 1);
	}

	logInsert(insert_rec, extra_size, data_size);
	return(insert_rec);
}

/** Insert an entry after the current cursor position.
The compressed page is updated in sync.
The cursor stays at the same position.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@param[in]	rec		record to insert
@param[in]	extra_size	size of record header, in bytes
@param[in]	data_size	size of record payload, in bytes
@return	pointer to record if enough space available, NULL otherwise */

const rec_t*
PageCur::insert(
	const rec_t*	rec,
	ulint		extra_size,
	ulint		data_size)
{
	page_zip_des_t*	page_zip	= getPageZip();

	if (page_zip) {
		const ulint	size = extra_size + data_size;
		const bool	reorg_before_insert
			= page_has_garbage(getPage())
			&& size > page_get_max_insert_size(getPage(), 1)
			&& size <= page_get_max_insert_size_after_reorganize(
				getPage(), 1);

		/* 2. Try to find suitable space from page memory management */
		if (reorg_before_insert
		    || !page_zip_available(
			    page_zip, dict_index_is_clust(m_index),
			    size, 1)) {
			return(insertZip(rec, extra_size, data_size,
					 reorg_before_insert));
		}
	}

	return(insertNoReorganize(rec, extra_size, data_size, page_zip));
}

/** Insert an entry after the current cursor position.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if this is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@param[in] tuple	record to insert
@param[in] n_ext	number of externally stored columns
@return	pointer to record if enough space available, NULL otherwise */

const rec_t*
PageCur::insert(const dtuple_t* tuple, ulint n_ext)
{
	ulint		extra_size;
	ulint		data_size;

	if (isComp()) {
		data_size = rec_get_converted_size_comp(
			m_index,
			dtuple_get_info_bits(tuple) & REC_NEW_STATUS_MASK,
			tuple->fields, tuple->n_fields, &extra_size);
		data_size -= extra_size;
	} else {
		data_size = dtuple_get_data_size(tuple, 0);
		extra_size = rec_get_converted_extra_size(
			data_size, dtuple_get_n_fields(tuple), n_ext);
	}

	byte*		buf	= new byte[data_size + extra_size];
	const rec_t*	rec	= rec_convert_dtuple_to_rec(
		buf, m_index, tuple, n_ext);
	const rec_t*	ins_rec	= insert(rec, extra_size, data_size);

	if (!ins_rec) {
		/* Page reorganization or recompression should already
		have been attempted by PageCur::insertZip(). */
		ut_ad(!getPageZip());

		if (reorganize(false, false, 0)) {
			ins_rec = insertNoZip(rec, extra_size, data_size);
		}
	}

	delete[] buf;
	return(ins_rec);
}

/** @brief Check the free space in the page modification log
for updating or inserting the current record in a compressed page.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE if
this is a secondary index leaf page. This has to be done
either within m_mtr, or by invoking ibuf_reset_free_bits()
before mtr_commit(m_mtr).

@param[in]	create		true=delete-and-insert,
false=update-in-place
@return true if enough space is available; if not,
IBUF_BITMAP_FREE will be reset outside m_mtr
if the page was recompressed */

bool
PageCur::zipAlloc(bool create)
{
	page_zip_des_t*	page_zip	= getPageZip();
	const page_t*	page		= getPage();

	ut_ad(isMutable());
	ut_ad(page_zip == buf_block_get_page_zip(m_block));
	ut_ad(!dict_index_is_ibuf(m_index));
	ut_ad(isUser());
	ut_ad(getPageZip());
	ut_ad(isComp());
	ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID)
	      == m_index->id);

	ulint	extra;
	ulint	length = getRecSize(extra);
	length += extra;

	if (page_zip_available(page_zip, dict_index_is_clust(m_index),
			       length, create)) {
		return(true);
	}

	if (!page_zip->m_nonempty && !page_has_garbage(page)) {
		/* The page has been freshly compressed, so
		reorganizing it will not help. */
		return(false);
	}

	/* After recompressing a page, we must make sure that the free
	bits in the insert buffer bitmap will not exceed the free
	space on the page. Because we will not attempt recompression
	unless page_zip_available() fails above, it is safe to reset
	the free bits if page_zip_available() fails again, below. The
	free bits can safely be reset in a separate mini-transaction.
	If page_zip_available() succeeds below, we can be sure that
	the reorganize() below did not reduce the free space
	available on the page. */

	if (!create
	    || !page_is_leaf(page)
	    || (length + page_get_data_size(page)
		< dict_index_zip_pad_optimal_page_size(
			const_cast<dict_index_t*>(m_index)))) {

		if (reorganize(false, true, page_zip_level)
		    && page_zip_available(
			    page_zip, dict_index_is_clust(m_index),
			    length, create)) {
			return(true);
		}
	}

	/* Out of space: reset the free bits. */
	if (!dict_index_is_clust(m_index)
	    && page_is_leaf(page)
	    && !dict_table_is_temporary(m_index->table)) {
		ibuf_reset_free_bits(m_block);
	}

	return(false);
}

/** Update the current record in place.
NOTE: on compressed page, updateAlloc() must have been called first.
@param[in]	update		the update vector */

void
PageCur::update(const upd_t* update)
{
	ut_ad(isMutable());
	ut_ad(isUser());

	if (const ulint* offsets = getOffsets()) {
		ut_ad(isComp());
		rec_set_info_bits_new(m_rec, update->info_bits);

		for (ulint i = 0; i < upd_get_n_fields(update); i++) {
			const upd_field_t*	upd_field
				= upd_get_nth_field(update, i);
			const dfield_t*		new_val
				= &upd_field->new_val;
			ulint			len;
			byte*			field
				= rec_get_nth_field(
					m_rec, offsets,
					upd_field->field_no, &len);
			ut_ad(!dfield_is_ext(new_val) ==
			      !rec_offs_nth_extern(
				      offsets, upd_field->field_no));
			ut_ad(len == dfield_get_len(new_val));
			ut_ad(len != UNIV_SQL_NULL);

			memcpy(field, dfield_get_data(new_val),
			       dfield_get_len(new_val));
			UNIV_MEM_ASSERT_RW(field, dfield_get_len(new_val));
		}

		if (page_zip_des_t* page_zip = getPageZip()) {
			page_zip_write_rec(
				page_zip, m_rec, m_index, offsets, 0);
		}
	} else {
		ut_ad(!isComp());
		ut_ad(!getPageZip());
		rec_set_info_bits_old(m_rec, update->info_bits);

		for (ulint i = 0; i < upd_get_n_fields(update); i++) {
			const upd_field_t*	upd_field
				= upd_get_nth_field(update, i);
			const ulint		field_no
				= upd_field->field_no;
			const dfield_t*		new_val
				= &upd_field->new_val;
			ulint			len;
			byte*			field
				= rec_get_nth_field_old(m_rec, field_no, &len);

			ut_ad(!dfield_is_ext(new_val)
			      == rec_get_1byte_offs_flag(m_rec)
			      || !rec_2_is_field_extern(m_rec, field_no));

			if (dfield_is_null(new_val)) {
				/* An update from NULL to NULL should
				not be included in the update vector. */
				ut_ad(len != UNIV_SQL_NULL);
				rec_set_nth_field_sql_null(m_rec, field_no);
			} else {
				ut_ad(rec_get_nth_field_size(m_rec, field_no)
				      == dfield_get_len(new_val));
				/* The field should be different. */
				ut_ad(memcmp(field, dfield_get_data(new_val),
					     dfield_get_len(new_val))
				      || len == UNIV_SQL_NULL);

				if (len == UNIV_SQL_NULL) {
					rec_set_nth_field_null_bit(
						m_rec, field_no, FALSE);
				}

				memcpy(field, dfield_get_data(new_val),
				       dfield_get_len(new_val));
			}
		}
	}
}

#ifdef PAGE_CUR_ADAPT
/** Try a search shortcut.
@param[in]	tuple		entry to search for
@param[in/out]	up_fields	matched fields in upper limit record
@param[in/out]	low_fields	matched fields in lower limit record
@return	whether tuple matches the current record */
inline
bool
PageCur::searchShortcut(
	const dtuple_t*	tuple,
	ulint&		up_fields,
	ulint&		low_fields)
{
	ulint		low_match;
	ulint		up_match;

	ut_ad(dtuple_check_typed(tuple));
	ut_ad(dtuple_get_n_fields(tuple) <= getNumFields());
	ut_ad(isUser());

	low_match = up_match = min(low_fields, up_fields);

	const rec_t*	rec = getRec();

	if (cmp_dtuple_rec_with_match(tuple, rec, getOffsets(), &low_match)
	    < 0) {
		return(false);
	}

	if (next()) {
		bool match = cmp_dtuple_rec_with_match(
			tuple, getRec(), getOffsets(), &up_match) >= 0;
		setUserRec(rec);
		if (match) {
			return(false);
		} else {
			up_fields = up_match;
		}
	} else {
		setUserRec(rec);
		low_fields = low_match;
	}

# ifdef UNIV_SEARCH_PERF_STAT
	page_cur_short_succ++;
# endif
	return(true);
}
#endif /* PAGE_CUR_ADAPT */

/** Search for an entry in the index page.
@param entry data tuple to search for
@return true if a full match was found */

bool
PageCur::search(const dtuple_t* tuple)
{
	const ulint	mode	= PAGE_CUR_LE;//TODO: take as parameter
	const page_t*	page	= getPage();
	/* TODO: if mode PAGE_CUR_G is specified, we are trying to position the
	cursor to answer a query of the form "tuple < X", where tuple is
	the input parameter, and X denotes an arbitrary physical record on
	the page. We want to position the cursor on the first X which
	satisfies the condition. */
	ulint		up_match	= 0;
	ulint		low_match	= 0;

	ut_ad(dtuple_validate(tuple));
	ut_ad(dtuple_check_typed(tuple));
	ut_ad(dtuple_get_n_fields(tuple) <= getNumFields());
	ut_ad(!m_mtr
	      || mtr_memo_contains(m_mtr, m_block, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains(m_mtr, m_block, MTR_MEMO_PAGE_S_FIX));

	page_zip_validate_if_zip(getPageZip(), page, m_index);

	page_check_dir(page);

#ifdef PAGE_CUR_ADAPT
	if (page_is_leaf(page)
	    && mode == PAGE_CUR_LE
	    && page_header_get_field(page, PAGE_N_DIRECTION) > 3
	    && page_header_get_field(page, PAGE_DIRECTION) == PAGE_RIGHT) {
		if (const rec_t* rec
		    = page_header_get_ptr(page, PAGE_LAST_INSERT)) {
			setUserRec(rec);

			if (searchShortcut(tuple, up_match, low_match)) {
				return(low_match
				       == dtuple_get_n_fields(tuple));
			}
		}
	}
#endif

	/* Perform binary search until the lower and upper limit directory
	slots come to the distance 1 of each other */
	ulint	low	= 0;
	ulint	up	= page_dir_get_n_slots(page) - 1;

	while (up - low > 1) {
		ulint			mid	= (low + up) / 2;
		const page_dir_slot_t*	slot	= page_dir_get_nth_slot(
			page, mid);
		ulint			match	= min(low_match, up_match);

		setRec(page_dir_slot_get_rec(slot));
		int cmp = cmp_dtuple_rec_with_match(
			tuple, getRec(), getOffsets(), &match);
		if (cmp > 0) {
low_slot_match:
			low = mid;
			low_match = match;
		} else if (cmp) {
up_slot_match:
			up = mid;
			up_match = match;
		} else if (mode == PAGE_CUR_G || mode == PAGE_CUR_LE) {
			goto low_slot_match;
		} else {
			goto up_slot_match;
		}
	}

	/* Perform linear search until the upper and lower records come to
	distance 1 of each other. */

	const rec_t*	low_rec	= page_dir_slot_get_rec(
		page_dir_get_nth_slot(page, low));
	const rec_t*	up_rec	= page_dir_slot_get_rec(
		page_dir_get_nth_slot(page, up));

	setRec(low_rec);

	for (;;) {
		if (!next() || getRec() == up_rec) {
			ut_ad(getRec() == up_rec);
			setRec(low_rec);
			break;
		}

		ulint	match = min(low_match, up_match);

		int cmp = cmp_dtuple_rec_with_match(
			tuple, getRec(), getOffsets(), &match);
		if (cmp > 0) {
low_rec_match:
			low_rec = getRec();
			low_match = match;
		} else if (cmp) {
up_rec_match:
			up_match = match;
			if (mode > PAGE_CUR_GE) {
				setRec(low_rec);
			}
			break;
		} else if (mode == PAGE_CUR_G || mode == PAGE_CUR_LE) {
			goto low_rec_match;
		} else {
			goto up_rec_match;
		}
	}

	return(low_match == dtuple_get_n_fields(tuple));
}

/** Delete the current record.
The cursor is moved to the next record after the deleted one.
@return true if the next record is a user record */

bool
PageCur::purge()
{
	page_t*		page	= getPage();
	page_zip_des_t*	page_zip= getPageZip();

	ut_ad(isMutable());
	ut_ad(isUser());
	ut_ad(fil_page_get_type(page) == FIL_PAGE_INDEX);
	ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID)
	      == m_index->id || recv_recovery_is_on()
	      || (m_mtr && m_mtr->inside_ibuf));

	/* page_zip_validate() will fail here when
	btr_cur_pessimistic_delete() invokes btr_set_min_rec_mark().
	Then, both "page_zip" and "page" would have the min-rec-mark
	set on the smallest user record, but "page" would additionally
	have it set on the smallest-but-one record.  Because sloppy
	page_zip_validate_low() only ignores min-rec-flag differences
	in the smallest user record, it cannot be used here either. */

	if (page_get_n_recs(page) == 1) {
		/* Empty the page. */
		ut_ad(page_is_leaf(page));
		ut_ad(m_mtr);
		/* Usually, this should be the root page,
		and the whole index tree should become empty.
		However, this could also be a call in
		btr_cur_pessimistic_update() to delete the only
		record in the page and to insert another one. */
		ut_ad(!next());
		setAfterLast();
		page_create_empty(m_block,
				  const_cast<dict_index_t*>(m_index), m_mtr);
		return(false);
	}

	ulint		slot_no	= page_dir_find_owner_slot(m_rec);
	page_dir_slot_t*slot	= page_dir_get_nth_slot(page, slot_no);
	page_dir_slot_t*prev_sl	= page_dir_get_nth_slot(page, slot_no - 1);
	ulint		n_owned	= page_dir_slot_get_n_owned(slot);

	ut_ad(n_owned > 1);
	ut_ad(slot_no > 0);

	/* 0. Write the log record */
	if (m_mtr) {
		page_cur_delete_rec_write_log(m_rec, m_index, m_mtr);
	}

	/* 1. Reset the last insert info in the page header and increment
	the modify clock for the frame */

	page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, NULL);

	/* The page gets invalid for optimistic searches: increment the
	frame modify clock only if there is an mini-transaction covering
	the change. During IMPORT we allocate local blocks that are not
	part of the buffer pool. */

	if (m_mtr) {
		buf_block_modify_clock_inc(m_block);
	}

	/* 2. Find the next and the previous record. Note that the cursor is
	left at the next record. */

	rec_t*	prev_rec	= NULL;

	/* Find the immediate predecessor of m_rec. */
	for (rec_t* rec = page_dir_slot_get_rec(prev_sl); rec != m_rec;
	     rec = page_rec_get_next(rec)) {
		prev_rec = rec;
	}

	rec_t*	del_rec	= m_rec;
	ulint	extra_size;
	ulint	n_ext;
	ulint	data_size = getRecSize(extra_size, &n_ext);

	next();

	/* 3. Remove the record from the linked list of records */

	page_rec_set_next(prev_rec, m_rec);

	/* 4. If the deleted record is pointed to by a dir slot, update the
	record pointer in slot. In the following if-clause we assume that
	prev_rec is owned by the same slot, i.e., PAGE_DIR_SLOT_MIN_N_OWNED
	>= 2. */

#if PAGE_DIR_SLOT_MIN_N_OWNED < 2
# error "PAGE_DIR_SLOT_MIN_N_OWNED < 2"
#endif
	if (del_rec == page_dir_slot_get_rec(slot)) {
		page_dir_slot_set_rec(slot, prev_rec);
	}

	/* 5. Update the number of owned records of the slot */

	page_dir_slot_set_n_owned(slot, page_zip, n_owned - 1);

	/* 6. Free the memory occupied by the record */
	page_mem_free(page, page_zip, del_rec, m_index,
		      data_size, extra_size, n_ext);

	/* 7. Now we have decremented the number of owned records of the slot.
	If the number drops below PAGE_DIR_SLOT_MIN_N_OWNED, we balance the
	slots. */

	if (n_owned <= PAGE_DIR_SLOT_MIN_N_OWNED) {
		page_dir_balance_slot(page, page_zip, slot_no);
	}

	page_zip_validate_if_zip(page_zip, page, m_index);

	return(!isAfterLast());
}

/** Reorganize the page. The cursor position will be adjusted.

NOTE: m_mtr must not be NULL.

NOTE: m_rec must be positioned on a record that exists on both
the uncompressed page and the uncompressed copy of the
compressed page.

IMPORTANT: On success, the caller will have to update
IBUF_BITMAP_FREE if this is a compressed leaf page in a
secondary index. This has to be done either within m_mtr, or
by invoking ibuf_reset_free_bits() before
mtr_commit(m_mtr). On uncompressed pages, IBUF_BITMAP_FREE is
unaffected by reorganization.

@param[in]	recovery	true if called in recovery:
locks and adaptive hash index will not be updated on recovery
@param[in]	zip_valid	true if the compressed page corresponds
to the uncompressed page
@param[in]	z_level		compression level, for compressed pages
@retval true if the operation was successful
@retval false if it is a compressed page, and recompression failed
(the page will be restored to correspond to the compressed page,
and the cursor will be positioned on the page infimum) */

bool
PageCur::reorganize(bool recovery, bool zip_valid, ulint z_level)
{
#ifndef UNIV_HOTBACKUP
	buf_pool_t*	buf_pool	= buf_pool_from_bpage(&m_block->page);
#endif /* !UNIV_HOTBACKUP */
	page_t*		page		= getPage();
	page_zip_des_t*	page_zip	= getPageZip();
	buf_block_t*	temp_block;
	page_t*		temp_page;
	ulint		log_mode;
	ulint		data_size1;
	ulint		data_size2;
	ulint		max_ins_size1;
	ulint		max_ins_size2;
	bool		success		= false;
	ulint		pos;
	bool		log_compressed;

	ut_ad(m_mtr);
	ut_ad(isMutable());
	page_zip_validate_if_zip(zip_valid ? page_zip : 0, page, m_index);
	data_size1 = page_get_data_size(page);
	max_ins_size1 = page_get_max_insert_size_after_reorganize(page, 1);

	/* Turn logging off */
	log_mode = mtr_set_log_mode(m_mtr, MTR_LOG_NONE);

#ifndef UNIV_HOTBACKUP
	temp_block = buf_block_alloc(buf_pool);
#else /* !UNIV_HOTBACKUP */
	ut_ad(block == back_block1);
	temp_block = back_block2;
#endif /* !UNIV_HOTBACKUP */
	temp_page = temp_block->frame;

	/* Copy the old page to temporary space */
	buf_frame_copy(temp_page, page);

#ifndef UNIV_HOTBACKUP
	if (!recovery) {
		btr_search_drop_page_hash_index(m_block);
	}

	m_block->check_index_page_at_flush = TRUE;
#endif /* !UNIV_HOTBACKUP */

	/* Save the cursor position. */
	pos = page_rec_get_n_recs_before(getRec());

	/* Recreate the page: note that global data on page (possible
	segment headers, next page-field, etc.) is preserved intact */

	page_create(m_block, m_mtr, isComp());

	/* Copy the records from the temporary space to the recreated page;
	do not copy the lock bits yet */

	setBeforeFirst();

	for (PageCur temp_cur(m_mtr, m_index, temp_block); temp_cur.next(); ) {
		if (!insertNoZip(temp_cur) || !next()) {
			ut_error;
		}
	}

	/* Multiple transactions cannot simultaneously operate on the
	same temp-table in parallel.
	max_trx_id is ignored for temp tables because it not required
	for MVCC. */
	if (dict_index_is_sec_or_ibuf(m_index)
	    && page_is_leaf(page)
	    && !dict_table_is_temporary(m_index->table)) {
		/* Copy max trx id to recreated page */
		trx_id_t	max_trx_id = page_get_max_trx_id(temp_page);
		page_set_max_trx_id(m_block, NULL, max_trx_id, m_mtr);
		/* In crash recovery, dict_index_is_sec_or_ibuf() always
		holds, even for clustered indexes.  max_trx_id is
		unused in clustered index pages. */
		ut_ad(max_trx_id != 0 || recovery);
	}

	/* If innodb_log_compressed_pages is ON, page reorganize should log the
	compressed page image.*/
	log_compressed = page_zip && page_zip_log_pages;

	if (log_compressed) {
		mtr_set_log_mode(m_mtr, log_mode);
	}

	data_size2 = page_get_data_size(page);
	max_ins_size2 = page_get_max_insert_size_after_reorganize(page, 1);

	if (data_size1 != data_size2 || max_ins_size1 != max_ins_size2) {
		buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);
		buf_page_print(temp_page, 0, BUF_PAGE_PRINT_NO_CRASH);

		fprintf(stderr,
			"InnoDB: Error: page old data size %lu"
			" new data size %lu\n"
			"InnoDB: Error: page old max ins size %lu"
			" new max ins size %lu\n"
			"InnoDB: Submit a detailed bug report"
			" to http://bugs.mysql.com\n",
			(unsigned long) data_size1, (unsigned long) data_size2,
			(unsigned long) max_ins_size1,
			(unsigned long) max_ins_size2);
		ut_ad(0);
	} else if (page_zip && !page_zip_compress(
			   page_zip, page, m_index, z_level, m_mtr)) {
		/* Failed to compress. Restore the old page. */
		if (zip_valid) {
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
			/* Check that the bytes that we skip are identical. */
			ut_a(!memcmp(page, temp_page, PAGE_HEADER));
			ut_a(!memcmp(PAGE_HEADER + PAGE_N_RECS + page,
				     PAGE_HEADER + PAGE_N_RECS + temp_page,
				     PAGE_DATA - (PAGE_HEADER + PAGE_N_RECS)));
			ut_a(!memcmp(UNIV_PAGE_SIZE - FIL_PAGE_DATA_END + page,
				     UNIV_PAGE_SIZE - FIL_PAGE_DATA_END
				     + temp_page, FIL_PAGE_DATA_END));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */

			memcpy(PAGE_HEADER + page, PAGE_HEADER + temp_page,
			       PAGE_N_RECS - PAGE_N_DIR_SLOTS);
			memcpy(PAGE_DATA + page, PAGE_DATA + temp_page,
			       UNIV_PAGE_SIZE - PAGE_DATA - FIL_PAGE_DATA_END);

#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
			ut_a(!memcmp(page, temp_page, UNIV_PAGE_SIZE));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
		} else {
			if (!page_zip_decompress(page_zip, page, FALSE)) {
				ut_error;
			}
			ut_ad(page_validate(m_block, m_index));
		}
	} else {
		success = true;

#ifndef UNIV_HOTBACKUP
		if (!recovery) {
			/* Update the record lock bitmaps */
			lock_move_reorganize_page(m_block, temp_block);
		}
#endif /* !UNIV_HOTBACKUP */
	}

	/* Restore the cursor position. */
	setRec(page_rec_get_nth(page, pos));
	page_zip_validate_if_zip(page_zip, page, m_index);
	mtr_set_log_mode(m_mtr, log_mode);

#ifndef UNIV_HOTBACKUP
	buf_block_free(temp_block);

	if (success) {
		byte	type;
		byte*	log_ptr;

		/* Write the log record */
		if (page_zip) {
			ut_ad(page_is_comp(page));
			type = MLOG_ZIP_PAGE_REORGANIZE;
		} else if (page_is_comp(page)) {
			type = MLOG_COMP_PAGE_REORGANIZE;
		} else {
			type = MLOG_PAGE_REORGANIZE;
		}

		log_ptr = log_compressed
			? NULL
			: mlog_open_and_write_index(
				m_mtr, page, m_index, type,
				page_zip ? 1 : 0);

		/* For compressed pages write the compression level. */
		if (log_ptr && page_zip) {
			mach_write_to_1(log_ptr, z_level);
			mlog_close(m_mtr, log_ptr + 1);
		}
	}
#endif /* !UNIV_HOTBACKUP */

	return(success);
}

/** Compress the page, reorganizing it if needed.

NOTE: m_mtr must not be NULL.

NOTE: m_rec must be positioned on a record that exists on both
the uncompressed page and the uncompressed copy of the
compressed page.

IMPORTANT: On success, the caller will have to update
IBUF_BITMAP_FREE if this is a compressed leaf page in a
secondary index. This has to be done either within m_mtr, or
by invoking ibuf_reset_free_bits() before
mtr_commit(m_mtr). On uncompressed pages, IBUF_BITMAP_FREE is
unaffected by reorganization.

@param[in]	z_level		compression level, for compressed pages
@retval true if the operation was successful
@retval false if the recompression failed
(the page will be restored to correspond to the compressed page,
and the cursor will be positioned on the page infimum) */

bool
PageCur::compress(ulint z_level)
{
	page_t*		page		= getPage();
	page_zip_des_t*	page_zip	= getPageZip();

	ut_ad(m_mtr);
	ut_ad(isMutable());
	ut_ad(page_zip);

	return(page_zip_compress(page_zip, page, m_index, z_level, m_mtr)
	       || reorganize(false, false, z_level));
}

#ifdef UNIV_COMPILE_TEST_FUNCS

/*******************************************************************//**
Print the first n numbers, generated by page_cur_lcg_prng() to make sure
(visually) that it works properly. */
void
test_page_cur_lcg_prng(
/*===================*/
	int	n)	/*!< in: print first n numbers */
{
	int			i;
	unsigned long long	rnd;

	for (i = 0; i < n; i++) {
		rnd = page_cur_lcg_prng();
		printf("%llu\t%%2=%llu %%3=%llu %%5=%llu %%7=%llu %%11=%llu\n",
		       rnd,
		       rnd % 2,
		       rnd % 3,
		       rnd % 5,
		       rnd % 7,
		       rnd % 11);
	}
}

#endif /* UNIV_COMPILE_TEST_FUNCS */
