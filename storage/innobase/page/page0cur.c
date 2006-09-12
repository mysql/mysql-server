/************************************************************************
The page cursor

(c) 1994-1996 Innobase Oy

Created 10/4/1994 Heikki Tuuri
*************************************************************************/

#include "page0cur.h"
#ifdef UNIV_NONINL
#include "page0cur.ic"
#endif

#include "rem0cmp.h"
#include "mtr0log.h"
#include "log0recv.h"
#include "rem0cmp.h"

static ulint	page_rnd	= 976722341;

#ifdef PAGE_CUR_ADAPT
# ifdef UNIV_SEARCH_PERF_STAT
ulint	page_cur_short_succ	= 0;
# endif /* UNIV_SEARCH_PERF_STAT */

/********************************************************************
Tries a search shortcut based on the last insert. */
UNIV_INLINE
ibool
page_cur_try_search_shortcut(
/*=========================*/
				/* out: TRUE on success */
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint*		iup_matched_fields,
				/* in/out: already matched fields in upper
				limit record */
	ulint*		iup_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	ulint*		ilow_matched_fields,
				/* in/out: already matched fields in lower
				limit record */
	ulint*		ilow_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	page_cur_t*	cursor) /* out: page cursor */
{
	rec_t*	rec;
	rec_t*	next_rec;
	ulint	low_match;
	ulint	low_bytes;
	ulint	up_match;
	ulint	up_bytes;
#ifdef UNIV_SEARCH_DEBUG
	page_cur_t cursor2;
#endif
	ibool		success		= FALSE;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(dtuple_check_typed(tuple));

	rec = page_header_get_ptr(page, PAGE_LAST_INSERT);
	offsets = rec_get_offsets(rec, index, offsets,
				  dtuple_get_n_fields(tuple), &heap);

	ut_ad(rec);
	ut_ad(page_rec_is_user_rec(rec));

	ut_pair_min(&low_match, &low_bytes,
		    *ilow_matched_fields, *ilow_matched_bytes,
		    *iup_matched_fields, *iup_matched_bytes);

	up_match = low_match;
	up_bytes = low_bytes;

	if (page_cmp_dtuple_rec_with_match(tuple, rec, offsets,
					   &low_match, &low_bytes) < 0) {
		goto exit_func;
	}

	next_rec = page_rec_get_next(rec);
	offsets = rec_get_offsets(next_rec, index, offsets,
				  dtuple_get_n_fields(tuple), &heap);

	if (page_cmp_dtuple_rec_with_match(tuple, next_rec, offsets,
					   &up_match, &up_bytes) >= 0) {
		goto exit_func;
	}

	cursor->rec = rec;

#ifdef UNIV_SEARCH_DEBUG
	page_cur_search_with_match(page, index, tuple, PAGE_CUR_DBG,
				   iup_matched_fields,
				   iup_matched_bytes,
				   ilow_matched_fields,
				   ilow_matched_bytes,
				   &cursor2);
	ut_a(cursor2.rec == cursor->rec);

	if (next_rec != page_get_supremum_rec(page)) {

		ut_a(*iup_matched_fields == up_match);
		ut_a(*iup_matched_bytes == up_bytes);
	}

	ut_a(*ilow_matched_fields == low_match);
	ut_a(*ilow_matched_bytes == low_bytes);
#endif
	if (!page_rec_is_supremum(next_rec)) {

		*iup_matched_fields = up_match;
		*iup_matched_bytes = up_bytes;
	}

	*ilow_matched_fields = low_match;
	*ilow_matched_bytes = low_bytes;

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

#ifdef PAGE_CUR_LE_OR_EXTENDS
/********************************************************************
Checks if the nth field in a record is a character type field which extends
the nth field in tuple, i.e., the field is longer or equal in length and has
common first characters. */
static
ibool
page_cur_rec_field_extends(
/*=======================*/
				/* out: TRUE if rec field
				extends tuple field */
	dtuple_t*	tuple,	/* in: data tuple */
	rec_t*		rec,	/* in: record */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		n)	/* in: compare nth field */
{
	dtype_t* type;
	dfield_t* dfield;
	byte*	  rec_f;
	ulint	  rec_f_len;

	ut_ad(rec_offs_validate(rec, NULL, offsets));
	dfield = dtuple_get_nth_field(tuple, n);

	type = dfield_get_type(dfield);

	rec_f = rec_get_nth_field(rec, offsets, n, &rec_f_len);

	if (type->mtype == DATA_VARCHAR
	    || type->mtype == DATA_CHAR
	    || type->mtype == DATA_FIXBINARY
	    || type->mtype == DATA_BINARY
	    || type->mtype == DATA_BLOB
	    || type->mtype == DATA_VARMYSQL
	    || type->mtype == DATA_MYSQL) {

		if (dfield_get_len(dfield) != UNIV_SQL_NULL
		    && rec_f_len != UNIV_SQL_NULL
		    && rec_f_len >= dfield_get_len(dfield)
		    && !cmp_data_data_slow(type,
					   dfield_get_data(dfield),
					   dfield_get_len(dfield),
					   rec_f, dfield_get_len(dfield))) {

			return(TRUE);
		}
	}

	return(FALSE);
}
#endif /* PAGE_CUR_LE_OR_EXTENDS */

/********************************************************************
Searches the right position for a page cursor. */

void
page_cur_search_with_match(
/*=======================*/
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		mode,	/* in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	ulint*		iup_matched_fields,
				/* in/out: already matched fields in upper
				limit record */
	ulint*		iup_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	ulint*		ilow_matched_fields,
				/* in/out: already matched fields in lower
				limit record */
	ulint*		ilow_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	page_cur_t*	cursor) /* out: page cursor */
{
	ulint	up;
	ulint	low;
	ulint	mid;
	page_dir_slot_t* slot;
	rec_t*	up_rec;
	rec_t*	low_rec;
	rec_t*	mid_rec;
	ulint	up_matched_fields;
	ulint	up_matched_bytes;
	ulint	low_matched_fields;
	ulint	low_matched_bytes;
	ulint	cur_matched_fields;
	ulint	cur_matched_bytes;
	int	cmp;
#ifdef UNIV_SEARCH_DEBUG
	int	dbg_cmp;
	ulint	dbg_matched_fields;
	ulint	dbg_matched_bytes;
#endif
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(page && tuple && iup_matched_fields && iup_matched_bytes
	      && ilow_matched_fields && ilow_matched_bytes && cursor);
	ut_ad(dtuple_validate(tuple));
	ut_ad(dtuple_check_typed(tuple));
#ifdef UNIV_DEBUG
# ifdef PAGE_CUR_DBG
	if (mode != PAGE_CUR_DBG)
# endif /* PAGE_CUR_DBG */
# ifdef PAGE_CUR_LE_OR_EXTENDS
		if (mode != PAGE_CUR_LE_OR_EXTENDS)
# endif /* PAGE_CUR_LE_OR_EXTENDS */
			ut_ad(mode == PAGE_CUR_L || mode == PAGE_CUR_LE
			      || mode == PAGE_CUR_G || mode == PAGE_CUR_GE);
#endif /* UNIV_DEBUG */

	page_check_dir(page);

#ifdef PAGE_CUR_ADAPT
	if ((page_header_get_field(page, PAGE_LEVEL) == 0)
	    && (mode == PAGE_CUR_LE)
	    && (page_header_get_field(page, PAGE_N_DIRECTION) > 3)
	    && (page_header_get_ptr(page, PAGE_LAST_INSERT))
	    && (page_header_get_field(page, PAGE_DIRECTION) == PAGE_RIGHT)) {

		if (page_cur_try_search_shortcut
		    (page, index, tuple,
		     iup_matched_fields, iup_matched_bytes,
		     ilow_matched_fields, ilow_matched_bytes,
		     cursor)) {
			return;
		}
	}
# ifdef PAGE_CUR_DBG
	if (mode == PAGE_CUR_DBG) {
		mode = PAGE_CUR_LE;
	}
# endif
#endif

	/* The following flag does not work for non-latin1 char sets because
	cmp_full_field does not tell how many bytes matched */
#ifdef PAGE_CUR_LE_OR_EXTENDS
	ut_a(mode != PAGE_CUR_LE_OR_EXTENDS);
#endif /* PAGE_CUR_LE_OR_EXTENDS */

	/* If mode PAGE_CUR_G is specified, we are trying to position the
	cursor to answer a query of the form "tuple < X", where tuple is
	the input parameter, and X denotes an arbitrary physical record on
	the page. We want to position the cursor on the first X which
	satisfies the condition. */

	up_matched_fields  = *iup_matched_fields;
	up_matched_bytes   = *iup_matched_bytes;
	low_matched_fields = *ilow_matched_fields;
	low_matched_bytes  = *ilow_matched_bytes;

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

		ut_pair_min(&cur_matched_fields, &cur_matched_bytes,
			    low_matched_fields, low_matched_bytes,
			    up_matched_fields, up_matched_bytes);

		offsets = rec_get_offsets(mid_rec, index, offsets,
					  dtuple_get_n_fields_cmp(tuple),
					  &heap);

		cmp = cmp_dtuple_rec_with_match(tuple, mid_rec, offsets,
						&cur_matched_fields,
						&cur_matched_bytes);
		if (UNIV_LIKELY(cmp > 0)) {
low_slot_match:
			low = mid;
			low_matched_fields = cur_matched_fields;
			low_matched_bytes = cur_matched_bytes;

		} else if (UNIV_EXPECT(cmp, -1)) {
#ifdef PAGE_CUR_LE_OR_EXTENDS
			if (mode == PAGE_CUR_LE_OR_EXTENDS
			    && page_cur_rec_field_extends
			    (tuple, mid_rec, offsets, cur_matched_fields)) {

				goto low_slot_match;
			}
#endif /* PAGE_CUR_LE_OR_EXTENDS */
up_slot_match:
			up = mid;
			up_matched_fields = cur_matched_fields;
			up_matched_bytes = cur_matched_bytes;

		} else if (mode == PAGE_CUR_G || mode == PAGE_CUR_LE
#ifdef PAGE_CUR_LE_OR_EXTENDS
			   || mode == PAGE_CUR_LE_OR_EXTENDS
#endif /* PAGE_CUR_LE_OR_EXTENDS */
			   ) {

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

	while (page_rec_get_next(low_rec) != up_rec) {

		mid_rec = page_rec_get_next(low_rec);

		ut_pair_min(&cur_matched_fields, &cur_matched_bytes,
			    low_matched_fields, low_matched_bytes,
			    up_matched_fields, up_matched_bytes);

		offsets = rec_get_offsets(mid_rec, index, offsets,
					  dtuple_get_n_fields_cmp(tuple),
					  &heap);

		cmp = cmp_dtuple_rec_with_match(tuple, mid_rec, offsets,
						&cur_matched_fields,
						&cur_matched_bytes);
		if (UNIV_LIKELY(cmp > 0)) {
low_rec_match:
			low_rec = mid_rec;
			low_matched_fields = cur_matched_fields;
			low_matched_bytes = cur_matched_bytes;

		} else if (UNIV_EXPECT(cmp, -1)) {
#ifdef PAGE_CUR_LE_OR_EXTENDS
			if (mode == PAGE_CUR_LE_OR_EXTENDS
			    && page_cur_rec_field_extends
			    (tuple, mid_rec, offsets, cur_matched_fields)) {

				goto low_rec_match;
			}
#endif /* PAGE_CUR_LE_OR_EXTENDS */
up_rec_match:
			up_rec = mid_rec;
			up_matched_fields = cur_matched_fields;
			up_matched_bytes = cur_matched_bytes;
		} else if (mode == PAGE_CUR_G || mode == PAGE_CUR_LE
#ifdef PAGE_CUR_LE_OR_EXTENDS
			   || mode == PAGE_CUR_LE_OR_EXTENDS
#endif /* PAGE_CUR_LE_OR_EXTENDS */
			   ) {

			goto low_rec_match;
		} else {

			goto up_rec_match;
		}
	}

#ifdef UNIV_SEARCH_DEBUG

	/* Check that the lower and upper limit records have the
	right alphabetical order compared to tuple. */
	dbg_matched_fields = 0;
	dbg_matched_bytes = 0;

	offsets = rec_get_offsets(low_rec, index, offsets,
				  ULINT_UNDEFINED, &heap);
	dbg_cmp = page_cmp_dtuple_rec_with_match(tuple, low_rec, offsets,
						 &dbg_matched_fields,
						 &dbg_matched_bytes);
	if (mode == PAGE_CUR_G) {
		ut_a(dbg_cmp >= 0);
	} else if (mode == PAGE_CUR_GE) {
		ut_a(dbg_cmp == 1);
	} else if (mode == PAGE_CUR_L) {
		ut_a(dbg_cmp == 1);
	} else if (mode == PAGE_CUR_LE) {
		ut_a(dbg_cmp >= 0);
	}

	if (low_rec != page_get_infimum_rec(page)) {

		ut_a(low_matched_fields == dbg_matched_fields);
		ut_a(low_matched_bytes == dbg_matched_bytes);
	}

	dbg_matched_fields = 0;
	dbg_matched_bytes = 0;

	offsets = rec_get_offsets(up_rec, index, offsets,
				  ULINT_UNDEFINED, &heap);
	dbg_cmp = page_cmp_dtuple_rec_with_match(tuple, up_rec, offsets,
						 &dbg_matched_fields,
						 &dbg_matched_bytes);
	if (mode == PAGE_CUR_G) {
		ut_a(dbg_cmp == -1);
	} else if (mode == PAGE_CUR_GE) {
		ut_a(dbg_cmp <= 0);
	} else if (mode == PAGE_CUR_L) {
		ut_a(dbg_cmp <= 0);
	} else if (mode == PAGE_CUR_LE) {
		ut_a(dbg_cmp == -1);
	}

	if (up_rec != page_get_supremum_rec(page)) {

		ut_a(up_matched_fields == dbg_matched_fields);
		ut_a(up_matched_bytes == dbg_matched_bytes);
	}
#endif
	if (mode <= PAGE_CUR_GE) {
		cursor->rec = up_rec;
	} else {
		cursor->rec = low_rec;
	}

	*iup_matched_fields  = up_matched_fields;
	*iup_matched_bytes   = up_matched_bytes;
	*ilow_matched_fields = low_matched_fields;
	*ilow_matched_bytes  = low_matched_bytes;
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/***************************************************************
Positions a page cursor on a randomly chosen user record on a page. If there
are no user records, sets the cursor on the infimum record. */

void
page_cur_open_on_rnd_user_rec(
/*==========================*/
	page_t*		page,	/* in: page */
	page_cur_t*	cursor)	/* in/out: page cursor */
{
	ulint	rnd;
	rec_t*	rec;

	if (page_get_n_recs(page) == 0) {
		page_cur_position(page_get_infimum_rec(page), cursor);

		return;
	}

	page_rnd += 87584577;

	rnd = page_rnd % page_get_n_recs(page);

	rec = page_get_infimum_rec(page);

	rec = page_rec_get_next(rec);

	while (rnd > 0) {
		rec = page_rec_get_next(rec);

		rnd--;
	}

	page_cur_position(rec, cursor);
}

/***************************************************************
Writes the log record of a record insert on a page. */
static
void
page_cur_insert_rec_write_log(
/*==========================*/
	rec_t*		insert_rec,	/* in: inserted physical record */
	ulint		rec_size,	/* in: insert_rec size */
	rec_t*		cursor_rec,	/* in: record the
					cursor is pointing to */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr)		/* in: mini-transaction handle */
{
	ulint	cur_rec_size;
	ulint	extra_size;
	ulint	cur_extra_size;
	ulint	min_rec_size;
	byte*	ins_ptr;
	byte*	cur_ptr;
	ulint	extra_info_yes;
	byte*	log_ptr;
	byte*	log_end;
	ulint	i;
	ulint	comp;

	ut_a(rec_size < UNIV_PAGE_SIZE);
	ut_ad(buf_frame_align(insert_rec) == buf_frame_align(cursor_rec));
	ut_ad(!page_rec_is_comp(insert_rec)
	      == !dict_table_is_comp(index->table));
	comp = page_rec_is_comp(insert_rec);

	{
		mem_heap_t*	heap		= NULL;
		ulint		cur_offs_[REC_OFFS_NORMAL_SIZE];
		ulint		ins_offs_[REC_OFFS_NORMAL_SIZE];

		ulint*		cur_offs;
		ulint*		ins_offs;

		*cur_offs_ = (sizeof cur_offs_) / sizeof *cur_offs_;
		*ins_offs_ = (sizeof ins_offs_) / sizeof *ins_offs_;

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
		min_rec_size = ut_min(cur_rec_size, rec_size);

		cur_ptr = cursor_rec - cur_extra_size;

		/* Find out the first byte in insert_rec which differs from
		cursor_rec; skip the bytes in the record info */

		for (;;) {
			if (i >= min_rec_size) {

				break;
			} else if (*ins_ptr == *cur_ptr) {
				i++;
				ins_ptr++;
				cur_ptr++;
			} else if ((i < extra_size)
				   && (i >= extra_size
				       - (comp
					  ? REC_N_NEW_EXTRA_BYTES
					  : REC_N_OLD_EXTRA_BYTES))) {
				i = extra_size;
				ins_ptr = insert_rec;
				cur_ptr = cursor_rec;
			} else {
				break;
			}
		}
	}

	if (mtr_get_log_mode(mtr) != MTR_LOG_SHORT_INSERTS) {

		log_ptr = mlog_open_and_write_index(mtr, insert_rec, index,
						    comp
						    ? MLOG_COMP_REC_INSERT
						    : MLOG_REC_INSERT,
						    2 + 5 + 1 + 5 + 5
						    + MLOG_BUF_MARGIN);

		if (!log_ptr) {
			/* Logging in mtr is switched off during crash
			recovery: in that case mlog_open returns NULL */
			return;
		}

		log_end = &log_ptr[2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN];
		/* Write the cursor rec offset as a 2-byte ulint */
		mach_write_to_2(log_ptr, cursor_rec
				- buf_frame_align(cursor_rec));
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

	if ((rec_get_info_and_status_bits(insert_rec, comp)
	     != rec_get_info_and_status_bits(cursor_rec, comp))
	    || (extra_size != cur_extra_size)
	    || (rec_size != cur_rec_size)) {

		extra_info_yes = 1;
	} else {
		extra_info_yes = 0;
	}

	/* Write the record end segment length and the extra info storage
	flag */
	log_ptr += mach_write_compressed(log_ptr, 2 * (rec_size - i)
					 + extra_info_yes);
	if (extra_info_yes) {
		/* Write the info bits */
		mach_write_to_1
			(log_ptr,
			 rec_get_info_and_status_bits(insert_rec, comp));
		log_ptr++;

		/* Write the record origin offset */
		log_ptr += mach_write_compressed(log_ptr, extra_size);

		/* Write the mismatch index */
		log_ptr += mach_write_compressed(log_ptr, i);

		ut_a(i < UNIV_PAGE_SIZE);
		ut_a(extra_size < UNIV_PAGE_SIZE);
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

/***************************************************************
Parses a log record of a record insert on a page. */

byte*
page_cur_parse_insert_rec(
/*======================*/
				/* out: end of log record or NULL */
	ibool		is_short,/* in: TRUE if short inserts */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in: page or NULL */
	mtr_t*		mtr)	/* in: mtr or NULL */
{
	ulint	extra_info_yes;
	ulint	offset = 0; /* remove warning */
	ulint	origin_offset;
	ulint	end_seg_len;
	ulint	mismatch_index;
	rec_t*	cursor_rec;
	byte	buf1[1024];
	byte*	buf;
	byte*	ptr2 = ptr;
	ulint	info_and_status_bits = 0; /* remove warning */
	page_cur_t cursor;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	if (!is_short) {
		/* Read the cursor rec offset as a 2-byte ulint */

		if (end_ptr < ptr + 2) {

			return(NULL);
		}

		offset = mach_read_from_2(ptr);

		if (offset >= UNIV_PAGE_SIZE) {

			recv_sys->found_corrupt_log = TRUE;

			return(NULL);
		}

		ptr += 2;
	}

	ptr = mach_parse_compressed(ptr, end_ptr, &end_seg_len);

	if (ptr == NULL) {

		return(NULL);
	}

	extra_info_yes = end_seg_len & 0x1UL;
	end_seg_len >>= 1;

	if (end_seg_len >= UNIV_PAGE_SIZE) {
		recv_sys->found_corrupt_log = TRUE;

		return(NULL);
	}

	if (extra_info_yes) {
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

		ut_a(origin_offset < UNIV_PAGE_SIZE);

		ptr = mach_parse_compressed(ptr, end_ptr, &mismatch_index);

		if (ptr == NULL) {

			return(NULL);
		}

		ut_a(mismatch_index < UNIV_PAGE_SIZE);
	}

	if (end_ptr < ptr + end_seg_len) {

		return(NULL);
	}

	if (page == NULL) {

		return(ptr + end_seg_len);
	}

	ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

	/* Read from the log the inserted index record end segment which
	differs from the cursor record */

	if (is_short) {
		cursor_rec = page_rec_get_prev(page_get_supremum_rec(page));
	} else {
		cursor_rec = page + offset;
	}

	offsets = rec_get_offsets(cursor_rec, index, offsets,
				  ULINT_UNDEFINED, &heap);

	if (extra_info_yes == 0) {
		info_and_status_bits = rec_get_info_and_status_bits
			(cursor_rec, page_is_comp(page));
		origin_offset = rec_offs_extra_size(offsets);
		mismatch_index = rec_offs_size(offsets) - end_seg_len;
	}

	if (mismatch_index + end_seg_len < sizeof buf1) {
		buf = buf1;
	} else {
		buf = mem_alloc(mismatch_index + end_seg_len);
	}

	/* Build the inserted record to buf */

	if (mismatch_index >= UNIV_PAGE_SIZE) {
		fprintf(stderr,
			"Is short %lu, info_and_status_bits %lu, offset %lu, "
			"o_offset %lu\n"
			"mismatch index %lu, end_seg_len %lu\n"
			"parsed len %lu\n",
			(ulong) is_short, (ulong) info_and_status_bits,
			(ulong) offset,
			(ulong) origin_offset,
			(ulong) mismatch_index, (ulong) end_seg_len,
			(ulong) (ptr - ptr2));

		fputs("Dump of 300 bytes of log:\n", stderr);
		ut_print_buf(stderr, ptr2, 300);

		buf_page_print(page);

		ut_error;
	}

	ut_memcpy(buf, rec_get_start(cursor_rec, offsets), mismatch_index);
	ut_memcpy(buf + mismatch_index, ptr, end_seg_len);

	rec_set_info_and_status_bits(buf + origin_offset, page_is_comp(page),
				     info_and_status_bits);

	page_cur_position(cursor_rec, &cursor);

	offsets = rec_get_offsets(buf + origin_offset, index, offsets,
				  ULINT_UNDEFINED, &heap);
	page_cur_rec_insert(&cursor, buf + origin_offset, index, offsets, mtr);

	if (buf != buf1) {

		mem_free(buf);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	return(ptr + end_seg_len);
}

/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The record to be
inserted can be in a data tuple or as a physical record. The other parameter
must then be NULL. The cursor stays at the same position. */

rec_t*
page_cur_insert_rec_low(
/*====================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	dtuple_t*	tuple,	/* in: pointer to a data tuple or NULL */
	dict_index_t*	index,	/* in: record descriptor */
	rec_t*		rec,	/* in: pointer to a physical record or NULL */
	ulint*		offsets,/* in: rec_get_offsets(rec, index) or NULL */
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	byte*		insert_buf	= NULL;
	ulint		rec_size;
	byte*		page;		/* the relevant page */
	rec_t*		last_insert;	/* cursor position at previous
					insert */
	rec_t*		insert_rec;	/* inserted record */
	ulint		heap_no;	/* heap number of the inserted
					record */
	rec_t*		current_rec;	/* current record after which the
					new record is inserted */
	rec_t*		next_rec;	/* next record after current before
					the insertion */
	ulint		owner_slot;	/* the slot which owns the
					inserted record */
	rec_t*		owner_rec;
	ulint		n_owned;
	mem_heap_t*	heap		= NULL;
	ulint		comp;

	ut_ad(cursor && mtr);
	ut_ad(tuple || rec);
	ut_ad(!(tuple && rec));
	ut_ad(rec || dtuple_check_typed(tuple));

	page = page_cur_get_page(cursor);
	comp = page_is_comp(page);
	ut_ad(dict_table_is_comp(index->table) == !!comp);

	ut_ad(cursor->rec != page_get_supremum_rec(page));

	/* 1. Get the size of the physical record in the page */
	if (tuple != NULL) {
		rec_size = rec_get_converted_size(index, tuple);
	} else {
		if (!offsets) {
			offsets = rec_get_offsets(rec, index, offsets,
						  ULINT_UNDEFINED, &heap);
		}
		ut_ad(rec_offs_validate(rec, index, offsets));
		rec_size = rec_offs_size(offsets);
	}

	/* 2. Try to find suitable space from page memory management */
	insert_buf = page_mem_alloc(page, rec_size, index, &heap_no);

	if (insert_buf == NULL) {
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		return(NULL);
	}

	/* 3. Create the record */
	if (tuple != NULL) {
		insert_rec = rec_convert_dtuple_to_rec(insert_buf,
						       index, tuple);
		offsets = rec_get_offsets(insert_rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
	} else {
		insert_rec = rec_copy(insert_buf, rec, offsets);
		ut_ad(rec_offs_validate(rec, index, offsets));
		rec_offs_make_valid(insert_rec, index, offsets);
	}

	ut_ad(insert_rec);
	ut_ad(rec_size == rec_offs_size(offsets));

	/* 4. Insert the record in the linked list of records */
	current_rec = cursor->rec;

	ut_ad(!comp || rec_get_status(current_rec) <= REC_STATUS_INFIMUM);
	ut_ad(!comp || rec_get_status(insert_rec) < REC_STATUS_INFIMUM);

	next_rec = page_rec_get_next(current_rec);
	ut_ad(!comp || rec_get_status(next_rec) != REC_STATUS_INFIMUM);
	page_rec_set_next(insert_rec, next_rec);
	page_rec_set_next(current_rec, insert_rec);

	page_header_set_field(page, PAGE_N_RECS, 1 + page_get_n_recs(page));

	/* 5. Set the n_owned field in the inserted record to zero,
	and set the heap_no field */

	rec_set_n_owned(insert_rec, comp, 0);
	rec_set_heap_no(insert_rec, comp, heap_no);

	/* 6. Update the last insertion info in page header */

	last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
	ut_ad(!last_insert || !comp
	      || rec_get_node_ptr_flag(last_insert)
	      == rec_get_node_ptr_flag(insert_rec));

	if (last_insert == NULL) {
		page_header_set_field(page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
		page_header_set_field(page, PAGE_N_DIRECTION, 0);

	} else if ((last_insert == current_rec)
		   && (page_header_get_field(page, PAGE_DIRECTION)
		       != PAGE_LEFT)) {

		page_header_set_field(page, PAGE_DIRECTION, PAGE_RIGHT);
		page_header_set_field(page, PAGE_N_DIRECTION,
				      page_header_get_field
				      (page, PAGE_N_DIRECTION) + 1);

	} else if ((page_rec_get_next(insert_rec) == last_insert)
		   && (page_header_get_field(page, PAGE_DIRECTION)
		       != PAGE_RIGHT)) {

		page_header_set_field(page, PAGE_DIRECTION, PAGE_LEFT);
		page_header_set_field(page, PAGE_N_DIRECTION,
				      page_header_get_field
				      (page, PAGE_N_DIRECTION) + 1);
	} else {
		page_header_set_field(page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
		page_header_set_field(page, PAGE_N_DIRECTION, 0);
	}

	page_header_set_ptr(page, PAGE_LAST_INSERT, insert_rec);

	/* 7. It remains to update the owner record. */

	owner_rec = page_rec_find_owner_rec(insert_rec);
	n_owned = rec_get_n_owned(owner_rec, comp);
	rec_set_n_owned(owner_rec, comp, n_owned + 1);

	/* 8. Now we have incremented the n_owned field of the owner
	record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
	we have to split the corresponding directory slot in two. */

	if (n_owned == PAGE_DIR_SLOT_MAX_N_OWNED) {
		owner_slot = page_dir_find_owner_slot(owner_rec);
		page_dir_split_slot(page, owner_slot);
	}

	/* 9. Write log record of the insert */
	page_cur_insert_rec_write_log(insert_rec, rec_size, current_rec,
				      index, mtr);

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(insert_rec);
}

/**************************************************************
Writes a log record of copying a record list end to a new created page. */
UNIV_INLINE
byte*
page_copy_rec_list_to_created_page_write_log(
/*=========================================*/
				/* out: 4-byte field where to
				write the log data length */
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: record descriptor */
	mtr_t*		mtr)	/* in: mtr */
{
	byte*	log_ptr;

	ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

	log_ptr = mlog_open_and_write_index(mtr, page, index,
					    page_is_comp(page)
					    ? MLOG_COMP_LIST_END_COPY_CREATED
					    : MLOG_LIST_END_COPY_CREATED, 4);
	ut_a(log_ptr);
	mlog_close(mtr, log_ptr + 4);

	return(log_ptr);
}

/**************************************************************
Parses a log record of copying a record list end to a new created page. */

byte*
page_parse_copy_rec_list_to_created_page(
/*=====================================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in: page or NULL */
	mtr_t*		mtr)	/* in: mtr or NULL */
{
	byte*	rec_end;
	ulint	log_data_len;

	if (ptr + 4 > end_ptr) {

		return(NULL);
	}

	log_data_len = mach_read_from_4(ptr);
	ptr += 4;

	rec_end = ptr + log_data_len;

	if (rec_end > end_ptr) {

		return(NULL);
	}

	if (!page) {

		return(rec_end);
	}

	while (ptr < rec_end) {
		ptr = page_cur_parse_insert_rec(TRUE, ptr, end_ptr,
						index, page, mtr);
	}

	ut_a(ptr == rec_end);

	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);
	page_header_set_field(page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	page_header_set_field(page, PAGE_N_DIRECTION, 0);

	return(rec_end);
}

/*****************************************************************
Copies records from page to a newly created page, from a given record onward,
including that record. Infimum and supremum records are not copied. */

void
page_copy_rec_list_end_to_created_page(
/*===================================*/
	page_t*		new_page,	/* in: index page to copy to */
	page_t*		page,		/* in: index page */
	rec_t*		rec,		/* in: first record to copy */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr)		/* in: mtr */
{
	page_dir_slot_t* slot = 0; /* remove warning */
	byte*	heap_top;
	rec_t*	insert_rec = 0; /* remove warning */
	rec_t*	prev_rec;
	ulint	count;
	ulint	n_recs;
	ulint	slot_index;
	ulint	rec_size;
	ulint	log_mode;
	byte*	log_ptr;
	ulint	log_data_len;
	ulint		comp		= page_is_comp(page);
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(page_dir_get_n_heap(new_page) == 2);
	ut_ad(page != new_page);
	ut_ad(comp == page_is_comp(new_page));

	if (rec == page_get_infimum_rec(page)) {

		rec = page_rec_get_next(rec);
	}

	if (rec == page_get_supremum_rec(page)) {

		return;
	}

#ifdef UNIV_DEBUG
	/* To pass the debug tests we have to set these dummy values
	in the debug version */
	page_dir_set_n_slots(new_page, UNIV_PAGE_SIZE / 2);
	page_header_set_ptr(new_page, PAGE_HEAP_TOP,
			    new_page + UNIV_PAGE_SIZE - 1);
#endif

	log_ptr = page_copy_rec_list_to_created_page_write_log(new_page,
							       index, mtr);

	log_data_len = dyn_array_get_data_size(&(mtr->log));

	/* Individual inserts are logged in a shorter form */

	log_mode = mtr_set_log_mode(mtr, MTR_LOG_SHORT_INSERTS);

	prev_rec = page_get_infimum_rec(new_page);
	if (comp) {
		heap_top = new_page + PAGE_NEW_SUPREMUM_END;
	} else {
		heap_top = new_page + PAGE_OLD_SUPREMUM_END;
	}
	count = 0;
	slot_index = 0;
	n_recs = 0;

	/* should be do ... until, comment by Jani */
	while (rec != page_get_supremum_rec(page)) {
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		insert_rec = rec_copy(heap_top, rec, offsets);

		rec_set_next_offs(prev_rec, comp, insert_rec - new_page);

		rec_set_n_owned(insert_rec, comp, 0);
		rec_set_heap_no(insert_rec, comp, 2 + n_recs);

		rec_size = rec_offs_size(offsets);

		heap_top = heap_top + rec_size;

		ut_ad(heap_top < new_page + UNIV_PAGE_SIZE);

		count++;
		n_recs++;

		if (count == (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2) {

			slot_index++;

			slot = page_dir_get_nth_slot(new_page, slot_index);

			page_dir_slot_set_rec(slot, insert_rec);
			page_dir_slot_set_n_owned(slot, count);

			count = 0;
		}

		page_cur_insert_rec_write_log(insert_rec, rec_size, prev_rec,
					      index, mtr);
		prev_rec = insert_rec;
		rec = page_rec_get_next(rec);
	}

	if ((slot_index > 0) && (count + 1
				 + (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2
				 <= PAGE_DIR_SLOT_MAX_N_OWNED)) {
		/* We can merge the two last dir slots. This operation is
		here to make this function imitate exactly the equivalent
		task made using page_cur_insert_rec, which we use in database
		recovery to reproduce the task performed by this function.
		To be able to check the correctness of recovery, it is good
		that it imitates exactly. */

		count += (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;

		page_dir_slot_set_n_owned(slot, 0);

		slot_index--;
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	log_data_len = dyn_array_get_data_size(&(mtr->log)) - log_data_len;

	ut_a(log_data_len < 100 * UNIV_PAGE_SIZE);

	mach_write_to_4(log_ptr, log_data_len);

	rec_set_next_offs(insert_rec, comp,
			  comp ? PAGE_NEW_SUPREMUM : PAGE_OLD_SUPREMUM);

	slot = page_dir_get_nth_slot(new_page, 1 + slot_index);

	page_dir_slot_set_rec(slot, page_get_supremum_rec(new_page));
	page_dir_slot_set_n_owned(slot, count + 1);

	page_dir_set_n_slots(new_page, 2 + slot_index);
	page_header_set_ptr(new_page, PAGE_HEAP_TOP, heap_top);
	page_dir_set_n_heap(new_page, 2 + n_recs);
	page_header_set_field(new_page, PAGE_N_RECS, n_recs);

	page_header_set_ptr(new_page, PAGE_LAST_INSERT, NULL);
	page_header_set_field(new_page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	page_header_set_field(new_page, PAGE_N_DIRECTION, 0);

	/* Restore the log mode */

	mtr_set_log_mode(mtr, log_mode);
}

/***************************************************************
Writes log record of a record delete on a page. */
UNIV_INLINE
void
page_cur_delete_rec_write_log(
/*==========================*/
	rec_t*		rec,	/* in: record to be deleted */
	dict_index_t*	index,	/* in: record descriptor */
	mtr_t*		mtr)	/* in: mini-transaction handle */
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
	mach_write_to_2(log_ptr, ut_align_offset(rec, UNIV_PAGE_SIZE));

	mlog_close(mtr, log_ptr + 2);
}

/***************************************************************
Parses log record of a record delete on a page. */

byte*
page_cur_parse_delete_rec(
/*======================*/
				/* out: pointer to record end or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in: page or NULL */
	mtr_t*		mtr)	/* in: mtr or NULL */
{
	ulint		offset;
	page_cur_t	cursor;

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	/* Read the cursor rec offset as a 2-byte ulint */
	offset = mach_read_from_2(ptr);
	ptr += 2;

	ut_a(offset <= UNIV_PAGE_SIZE);

	if (page) {
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		rec_t*		rec		= page + offset;
		*offsets_ = (sizeof offsets_) / sizeof *offsets_;

		page_cur_position(rec, &cursor);

		page_cur_delete_rec(&cursor, index,
				    rec_get_offsets(rec, index, offsets_,
						    ULINT_UNDEFINED, &heap),
				    mtr);
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}

	return(ptr);
}

/***************************************************************
Deletes a record at the page cursor. The cursor is moved to the next
record after the deleted one. */

void
page_cur_delete_rec(
/*================*/
	page_cur_t*	cursor,	/* in: a page cursor */
	dict_index_t*	index,	/* in: record descriptor */
	const ulint*	offsets,/* in: rec_get_offsets(cursor->rec, index) */
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	page_dir_slot_t* cur_dir_slot;
	page_dir_slot_t* prev_slot;
	page_t*		page;
	rec_t*		current_rec;
	rec_t*		prev_rec	= NULL;
	rec_t*		next_rec;
	ulint		cur_slot_no;
	ulint		cur_n_owned;
	rec_t*		rec;

	ut_ad(cursor && mtr);

	page = page_cur_get_page(cursor);
	current_rec = cursor->rec;
	ut_ad(rec_offs_validate(current_rec, index, offsets));
	ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

	/* The record must not be the supremum or infimum record. */
	ut_ad(current_rec != page_get_supremum_rec(page));
	ut_ad(current_rec != page_get_infimum_rec(page));

	/* Save to local variables some data associated with current_rec */
	cur_slot_no = page_dir_find_owner_slot(current_rec);
	cur_dir_slot = page_dir_get_nth_slot(page, cur_slot_no);
	cur_n_owned = page_dir_slot_get_n_owned(cur_dir_slot);

	/* 0. Write the log record */
	page_cur_delete_rec_write_log(current_rec, index, mtr);

	/* 1. Reset the last insert info in the page header and increment
	the modify clock for the frame */

	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);

	/* The page gets invalid for optimistic searches: increment the
	frame modify clock */

	buf_frame_modify_clock_inc(page);

	/* 2. Find the next and the previous record. Note that the cursor is
	left at the next record. */

	ut_ad(cur_slot_no > 0);
	prev_slot = page_dir_get_nth_slot(page, cur_slot_no - 1);

	rec = page_dir_slot_get_rec(prev_slot);

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
	page_header_set_field(page, PAGE_N_RECS,
			      (ulint)(page_get_n_recs(page) - 1));

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

	page_dir_slot_set_n_owned(cur_dir_slot, cur_n_owned - 1);

	/* 6. Free the memory occupied by the record */
	page_mem_free(page, current_rec, offsets);

	/* 7. Now we have decremented the number of owned records of the slot.
	If the number drops below PAGE_DIR_SLOT_MIN_N_OWNED, we balance the
	slots. */

	if (cur_n_owned <= PAGE_DIR_SLOT_MIN_N_OWNED) {
		page_dir_balance_slot(page, cur_slot_no);
	}
}
