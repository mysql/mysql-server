/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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

/** @file page/page0cur.cc
 The page cursor

 Created 10/4/1994 Heikki Tuuri
 *************************************************************************/

#include "page0cur.h"

#include "btr0btr.h"
#include "ha_prototypes.h"
#include "log0recv.h"
#include "mtr0log.h"

#include <algorithm>
#include "page0zip.h"

#ifndef UNIV_HOTBACKUP
#include "gis0rtree.h"
#include "rem0cmp.h"
#endif /* !UNIV_HOTBACKUP */

#ifdef PAGE_CUR_ADAPT
#ifdef UNIV_SEARCH_PERF_STAT
static ulint page_cur_short_succ = 0;
#endif /* UNIV_SEARCH_PERF_STAT */

#ifndef UNIV_HOTBACKUP
/** This is a linear congruential generator PRNG. Returns a pseudo random
 number between 0 and 2^64-1 inclusive. The formula and the constants
 being used are:
 X[n+1] = (a * X[n] + c) mod m
 where:
 X[0] = ut_time_us(NULL)
 a = 1103515245 (3^5 * 5 * 7 * 129749)
 c = 12345 (3 * 5 * 823)
 m = 18446744073709551616 (2^64)

 @return number between 0 and 2^64-1 */
static ib_uint64_t page_cur_lcg_prng(void) {
#define LCG_a 1103515245
#define LCG_c 12345
  static ib_uint64_t lcg_current = 0;
  static ibool initialized = FALSE;

  if (!initialized) {
    lcg_current = (ib_uint64_t)ut_time_us(NULL);
    initialized = TRUE;
  }

  /* no need to "% 2^64" explicitly because lcg_current is
  64 bit and this will be done anyway */
  lcg_current = LCG_a * lcg_current + LCG_c;

  return (lcg_current);
}

/** Try a search shortcut based on the last insert.
@param[in]	block			index page
@param[in]	index			index tree
@param[in]	tuple			search key
@param[in,out]	iup_matched_fields	already matched fields in the
upper limit record
@param[in,out]	ilow_matched_fields	already matched fields in the
lower limit record
@param[out]	cursor			page cursor
@return true on success */
UNIV_INLINE
bool page_cur_try_search_shortcut(
    const buf_block_t *block, const dict_index_t *index, const dtuple_t *tuple,
    ulint *iup_matched_fields, ulint *ilow_matched_fields, page_cur_t *cursor) {
  const rec_t *rec;
  const rec_t *next_rec;
  ulint low_match;
  ulint up_match;
  ibool success = FALSE;
  const page_t *page = buf_block_get_frame(block);
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(dtuple_check_typed(tuple));

  rec = page_header_get_ptr(page, PAGE_LAST_INSERT);
  offsets =
      rec_get_offsets(rec, index, offsets, dtuple_get_n_fields(tuple), &heap);

  ut_ad(rec);
  ut_ad(page_rec_is_user_rec(rec));

  low_match = up_match = std::min(*ilow_matched_fields, *iup_matched_fields);

  if (cmp_dtuple_rec_with_match(tuple, rec, index, offsets, &low_match) < 0) {
    goto exit_func;
  }

  next_rec = page_rec_get_next_const(rec);
  if (!page_rec_is_supremum(next_rec)) {
    offsets = rec_get_offsets(next_rec, index, offsets,
                              dtuple_get_n_fields(tuple), &heap);

    if (cmp_dtuple_rec_with_match(tuple, next_rec, index, offsets, &up_match) >=
        0) {
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
  return (success);
}

/** Try a search shortcut based on the last insert.
@param[in]	block			index page
@param[in]	index			index tree
@param[in]	tuple			search key
@param[in,out]	iup_matched_fields	already matched fields in the
upper limit record
@param[in,out]	iup_matched_bytes	already matched bytes in the
first partially matched field in the upper limit record
@param[in,out]	ilow_matched_fields	already matched fields in the
lower limit record
@param[in,out]	ilow_matched_bytes	already matched bytes in the
first partially matched field in the lower limit record
@param[out]	cursor			page cursor
@return true on success */
UNIV_INLINE
bool page_cur_try_search_shortcut_bytes(
    const buf_block_t *block, const dict_index_t *index, const dtuple_t *tuple,
    ulint *iup_matched_fields, ulint *iup_matched_bytes,
    ulint *ilow_matched_fields, ulint *ilow_matched_bytes, page_cur_t *cursor) {
  const rec_t *rec;
  const rec_t *next_rec;
  ulint low_match;
  ulint low_bytes;
  ulint up_match;
  ulint up_bytes;
  ibool success = FALSE;
  const page_t *page = buf_block_get_frame(block);
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(dtuple_check_typed(tuple));

  rec = page_header_get_ptr(page, PAGE_LAST_INSERT);
  offsets =
      rec_get_offsets(rec, index, offsets, dtuple_get_n_fields(tuple), &heap);

  ut_ad(rec);
  ut_ad(page_rec_is_user_rec(rec));
  if (ut_pair_cmp(*ilow_matched_fields, *ilow_matched_bytes,
                  *iup_matched_fields, *iup_matched_bytes) < 0) {
    up_match = low_match = *ilow_matched_fields;
    up_bytes = low_bytes = *ilow_matched_bytes;
  } else {
    up_match = low_match = *iup_matched_fields;
    up_bytes = low_bytes = *iup_matched_bytes;
  }

  if (cmp_dtuple_rec_with_match_bytes(tuple, rec, index, offsets, &low_match,
                                      &low_bytes) < 0) {
    goto exit_func;
  }

  next_rec = page_rec_get_next_const(rec);
  if (!page_rec_is_supremum(next_rec)) {
    offsets = rec_get_offsets(next_rec, index, offsets,
                              dtuple_get_n_fields(tuple), &heap);

    if (cmp_dtuple_rec_with_match_bytes(tuple, next_rec, index, offsets,
                                        &up_match, &up_bytes) >= 0) {
      goto exit_func;
    }

    *iup_matched_fields = up_match;
    *iup_matched_bytes = up_bytes;
  }

  page_cur_position(rec, block, cursor);

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
  return (success);
}
#endif /* !UNIV_HOTBACKUP */

#ifdef PAGE_CUR_LE_OR_EXTENDS
/** Checks if the nth field in a record is a character type field which extends
 the nth field in tuple, i.e., the field is longer or equal in length and has
 common first characters.
 @param[in]	tuple	data tuple
 @param[in]	rec	record
 @param[in]	offsets	array returned by rec_get_offsets()
 @param[in]	n	compare nth field
 @param[in]	index	index where the record resides
 @return true if rec field extends tuple field */
static ibool page_cur_rec_field_extends(const dtuple_t *tuple, const rec_t *rec,
                                        const ulint *offsets, ulint n,
                                        const dict_index_t *index) {
  const dtype_t *type;
  const dfield_t *dfield;
  const byte *rec_f;
  ulint rec_f_len;

  ut_ad(rec_offs_validate(rec, NULL, offsets));
  dfield = dtuple_get_nth_field(tuple, n);

  type = dfield_get_type(dfield);

  rec_f = rec_get_nth_field_instant(rec, offsets, n, index, &rec_f_len);

  if (type->mtype == DATA_VARCHAR || type->mtype == DATA_CHAR ||
      type->mtype == DATA_FIXBINARY || type->mtype == DATA_BINARY ||
      type->mtype == DATA_BLOB || DATA_GEOMETRY_MTYPE(type->mtype) ||
      type->mtype == DATA_VARMYSQL || type->mtype == DATA_MYSQL) {
    if (dfield_get_len(dfield) != UNIV_SQL_NULL && rec_f_len != UNIV_SQL_NULL &&
        rec_f_len >= dfield_get_len(dfield)
        /* is_ascending parameter in the below call is passed as a
        constant as we are only testing for equality and we are
        not interested in what the nonzero return value actually
        is. */
        &&
        !cmp_data_data(type->mtype, type->prtype, true, dfield_get_data(dfield),
                       dfield_get_len(dfield), rec_f, dfield_get_len(dfield))) {
      return (TRUE);
    }
  }

  return (FALSE);
}
#endif /* PAGE_CUR_LE_OR_EXTENDS */

#ifndef UNIV_HOTBACKUP
/** If key is fixed length then populate offset directly from
cached version.
@param[in]	rec	B-Tree record for which offset needs to be
                        populated.
@param[in,out]	index	index handler
@param[in]	tuple	data tuple
@param[in,out]	offsets	default offsets array
@param[in,out]	heap	heap
@return reference to populate offsets. */
static ulint *populate_offsets(const rec_t *rec, const dtuple_t *tuple,
                               dict_index_t *index, ulint *offsets,
                               mem_heap_t **heap) {
  ut_ad(index->table->is_intrinsic());

  bool rec_has_null_values = false;

  if (index->rec_cache.key_has_null_cols) {
    /* Check if record has null value. */
    const byte *nulls = rec - (1 + REC_N_NEW_EXTRA_BYTES);
    ulint n_bytes_to_scan = UT_BITS_IN_BYTES(index->n_nullable);
    byte null_mask = 0xff;
    ulint bits_examined = 0;

    for (ulint i = 0; i < n_bytes_to_scan - 1; i++) {
      if (*nulls & null_mask) {
        rec_has_null_values = true;
        break;
      }
      --nulls;
      bits_examined += 8;
    }

    if (!rec_has_null_values) {
      null_mask >>= (8 - (index->n_nullable - bits_examined));
      rec_has_null_values = *nulls & null_mask;
    }

    if (rec_has_null_values) {
      offsets = rec_get_offsets(rec, index, offsets,
                                dtuple_get_n_fields_cmp(tuple), heap);

      return (offsets);
    }
  }

  /* Check if offsets are cached else cache them first.
  There are queries that will first verify if key is present using index
  search and then initiate insert. If offsets are cached during index
  search it would be based on key part only but during insert that looks
  out for exact location to insert key + db_row_id both columns would
  be used and so re-compute offsets in such case. */
  if (!index->rec_cache.offsets_cached ||
      (rec_offs_n_fields(index->rec_cache.offsets) <
       dtuple_get_n_fields_cmp(tuple))) {
    offsets = rec_get_offsets(rec, index, offsets,
                              dtuple_get_n_fields_cmp(tuple), heap);

    /* Reallocate if our offset array is not big
    enough to hold the needed size. */
    ulint sz1 = index->rec_cache.sz_of_offsets;
    ulint sz2 = offsets[0];
    if (sz1 < sz2) {
      index->rec_cache.offsets = static_cast<ulint *>(
          mem_heap_alloc(index->heap, sizeof(ulint) * sz2));
      index->rec_cache.sz_of_offsets = static_cast<uint32_t>(sz2);
    }

    memcpy(index->rec_cache.offsets, offsets, (sizeof(ulint) * sz2));
    index->rec_cache.offsets_cached = true;
  }

  ut_ad(index->rec_cache.offsets[2] = (ulint)rec);

  return (index->rec_cache.offsets);
}
#endif /* !UNIV_HOTBACKUP */
#endif /* PAGE_CUR_ADAPT */

#ifndef UNIV_HOTBACKUP
/** Searches the right position for a page cursor. */
void page_cur_search_with_match(
    const buf_block_t *block,  /*!< in: buffer block */
    const dict_index_t *index, /*!< in/out: record descriptor */
    const dtuple_t *tuple,     /*!< in: data tuple */
    page_cur_mode_t mode,      /*!< in: PAGE_CUR_L,
                               PAGE_CUR_LE, PAGE_CUR_G, or
                               PAGE_CUR_GE */
    ulint *iup_matched_fields,
    /*!< in/out: already matched
    fields in upper limit record */
    ulint *ilow_matched_fields,
    /*!< in/out: already matched
    fields in lower limit record */
    page_cur_t *cursor,   /*!< out: page cursor */
    rtr_info_t *rtr_info) /*!< in/out: rtree search stack */
{
  ulint up;
  ulint low;
  ulint mid;
  const page_t *page;
  const page_dir_slot_t *slot;
  const rec_t *up_rec;
  const rec_t *low_rec;
  const rec_t *mid_rec;
  ulint up_matched_fields;
  ulint low_matched_fields;
  ulint cur_matched_fields;
  int cmp = 0;
#ifdef UNIV_ZIP_DEBUG
  const page_zip_des_t *page_zip = buf_block_get_page_zip(block);
#endif /* UNIV_ZIP_DEBUG */
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(dtuple_validate(tuple));
#ifdef UNIV_DEBUG
#ifdef PAGE_CUR_DBG
  if (mode != PAGE_CUR_DBG)
#endif /* PAGE_CUR_DBG */
#ifdef PAGE_CUR_LE_OR_EXTENDS
    if (mode != PAGE_CUR_LE_OR_EXTENDS)
#endif /* PAGE_CUR_LE_OR_EXTENDS */
      ut_ad(mode == PAGE_CUR_L || mode == PAGE_CUR_LE || mode == PAGE_CUR_G ||
            mode == PAGE_CUR_GE || dict_index_is_spatial(index));
#endif /* UNIV_DEBUG */
  page = buf_block_get_frame(block);
#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

  ut_d(page_check_dir(page));

#ifdef PAGE_CUR_ADAPT
  if (page_is_leaf(page) && (mode == PAGE_CUR_LE) &&
      !dict_index_is_spatial(index) &&
      (page_header_get_field(page, PAGE_N_DIRECTION) > 3) &&
      (page_header_get_ptr(page, PAGE_LAST_INSERT)) &&
      (page_header_get_field(page, PAGE_DIRECTION) == PAGE_RIGHT)) {
    if (page_cur_try_search_shortcut(block, index, tuple, iup_matched_fields,
                                     ilow_matched_fields, cursor)) {
      return;
    }
  }
#ifdef PAGE_CUR_DBG
  if (mode == PAGE_CUR_DBG) {
    mode = PAGE_CUR_LE;
  }
#endif
#endif

  /* If the mode is for R-tree indexes, use the special MBR
  related compare functions */
  if (dict_index_is_spatial(index) && mode > PAGE_CUR_LE) {
    /* For leaf level insert, we still use the traditional
    compare function for now */
    if (mode == PAGE_CUR_RTREE_INSERT && page_is_leaf(page)) {
      mode = PAGE_CUR_LE;
    } else {
      rtr_cur_search_with_match(block, (dict_index_t *)index, tuple, mode,
                                cursor, rtr_info);
      return;
    }
  }

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

  up_matched_fields = *iup_matched_fields;
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

    cur_matched_fields = std::min(low_matched_fields, up_matched_fields);

    offsets = offsets_;
    if (index->rec_cache.fixed_len_key) {
      offsets = populate_offsets(
          mid_rec, tuple, const_cast<dict_index_t *>(index), offsets, &heap);
    } else {
      offsets = rec_get_offsets(mid_rec, index, offsets,
                                dtuple_get_n_fields_cmp(tuple), &heap);
    }

    cmp = cmp_dtuple_rec_with_match(tuple, mid_rec, index, offsets,
                                    &cur_matched_fields);

    if (cmp > 0) {
    low_slot_match:
      low = mid;
      low_matched_fields = cur_matched_fields;

    } else if (cmp) {
#ifdef PAGE_CUR_LE_OR_EXTENDS
      if (mode == PAGE_CUR_LE_OR_EXTENDS &&
          page_cur_rec_field_extends(tuple, mid_rec, offsets,
                                     cur_matched_fields, index)) {
        goto low_slot_match;
      }
#endif /* PAGE_CUR_LE_OR_EXTENDS */
    up_slot_match:
      up = mid;
      up_matched_fields = cur_matched_fields;

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

  while (page_rec_get_next_const(low_rec) != up_rec) {
    mid_rec = page_rec_get_next_const(low_rec);

    cur_matched_fields = std::min(low_matched_fields, up_matched_fields);

    offsets = offsets_;
    if (index->rec_cache.fixed_len_key) {
      offsets = populate_offsets(
          mid_rec, tuple, const_cast<dict_index_t *>(index), offsets, &heap);
    } else {
      offsets = rec_get_offsets(mid_rec, index, offsets,
                                dtuple_get_n_fields_cmp(tuple), &heap);
    }

    cmp = cmp_dtuple_rec_with_match(tuple, mid_rec, index, offsets,
                                    &cur_matched_fields);

    if (cmp > 0) {
    low_rec_match:
      low_rec = mid_rec;
      low_matched_fields = cur_matched_fields;

    } else if (cmp) {
#ifdef PAGE_CUR_LE_OR_EXTENDS
      if (mode == PAGE_CUR_LE_OR_EXTENDS &&
          page_cur_rec_field_extends(tuple, mid_rec, offsets,
                                     cur_matched_fields, index)) {
        goto low_rec_match;
      }
#endif /* PAGE_CUR_LE_OR_EXTENDS */
    up_rec_match:
      up_rec = mid_rec;
      up_matched_fields = cur_matched_fields;
    } else if (mode == PAGE_CUR_G || mode == PAGE_CUR_LE
#ifdef PAGE_CUR_LE_OR_EXTENDS
               || mode == PAGE_CUR_LE_OR_EXTENDS
#endif /* PAGE_CUR_LE_OR_EXTENDS */
    ) {
      if (!cmp && !cur_matched_fields) {
#ifdef UNIV_DEBUG
        mtr_t mtr;
        mtr_start(&mtr);

        /* We got a match, but cur_matched_fields is
        0, it must have REC_INFO_MIN_REC_FLAG */
        ulint rec_info = rec_get_info_bits(mid_rec, rec_offs_comp(offsets));
        ut_ad(rec_info & REC_INFO_MIN_REC_FLAG);
        ut_ad(btr_page_get_prev(page, &mtr) == FIL_NULL);
        mtr_commit(&mtr);
#endif

        cur_matched_fields = dtuple_get_n_fields_cmp(tuple);
      }

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

  *iup_matched_fields = up_matched_fields;
  *ilow_matched_fields = low_matched_fields;
  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }
}

/** Search the right position for a page cursor.
@param[in]	block			buffer block
@param[in]	index			index tree
@param[in]	tuple			key to be searched for
@param[in]	mode			search mode
@param[in,out]	iup_matched_fields	already matched fields in the
upper limit record
@param[in,out]	iup_matched_bytes	already matched bytes in the
first partially matched field in the upper limit record
@param[in,out]	ilow_matched_fields	already matched fields in the
lower limit record
@param[in,out]	ilow_matched_bytes	already matched bytes in the
first partially matched field in the lower limit record
@param[out]	cursor			page cursor */
void page_cur_search_with_match_bytes(
    const buf_block_t *block, const dict_index_t *index, const dtuple_t *tuple,
    page_cur_mode_t mode, ulint *iup_matched_fields, ulint *iup_matched_bytes,
    ulint *ilow_matched_fields, ulint *ilow_matched_bytes, page_cur_t *cursor) {
  ulint up;
  ulint low;
  ulint mid;
  const page_t *page;
  const page_dir_slot_t *slot;
  const rec_t *up_rec;
  const rec_t *low_rec;
  const rec_t *mid_rec;
  ulint up_matched_fields;
  ulint up_matched_bytes;
  ulint low_matched_fields;
  ulint low_matched_bytes;
  ulint cur_matched_fields;
  ulint cur_matched_bytes;
  int cmp;
#ifdef UNIV_ZIP_DEBUG
  const page_zip_des_t *page_zip = buf_block_get_page_zip(block);
#endif /* UNIV_ZIP_DEBUG */
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(dtuple_validate(tuple));
#ifdef UNIV_DEBUG
#ifdef PAGE_CUR_DBG
  if (mode != PAGE_CUR_DBG)
#endif /* PAGE_CUR_DBG */
#ifdef PAGE_CUR_LE_OR_EXTENDS
    if (mode != PAGE_CUR_LE_OR_EXTENDS)
#endif /* PAGE_CUR_LE_OR_EXTENDS */
      ut_ad(mode == PAGE_CUR_L || mode == PAGE_CUR_LE || mode == PAGE_CUR_G ||
            mode == PAGE_CUR_GE);
#endif /* UNIV_DEBUG */
  page = buf_block_get_frame(block);
#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

  ut_d(page_check_dir(page));

#ifdef PAGE_CUR_ADAPT
  if (page_is_leaf(page) && (mode == PAGE_CUR_LE) &&
      (page_header_get_field(page, PAGE_N_DIRECTION) > 3) &&
      (page_header_get_ptr(page, PAGE_LAST_INSERT)) &&
      (page_header_get_field(page, PAGE_DIRECTION) == PAGE_RIGHT)) {
    if (page_cur_try_search_shortcut_bytes(
            block, index, tuple, iup_matched_fields, iup_matched_bytes,
            ilow_matched_fields, ilow_matched_bytes, cursor)) {
      return;
    }
  }
#ifdef PAGE_CUR_DBG
  if (mode == PAGE_CUR_DBG) {
    mode = PAGE_CUR_LE;
  }
#endif
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

  up_matched_fields = *iup_matched_fields;
  up_matched_bytes = *iup_matched_bytes;
  low_matched_fields = *ilow_matched_fields;
  low_matched_bytes = *ilow_matched_bytes;

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

    ut_pair_min(&cur_matched_fields, &cur_matched_bytes, low_matched_fields,
                low_matched_bytes, up_matched_fields, up_matched_bytes);

    offsets = rec_get_offsets(mid_rec, index, offsets_,
                              dtuple_get_n_fields_cmp(tuple), &heap);

    cmp = cmp_dtuple_rec_with_match_bytes(tuple, mid_rec, index, offsets,
                                          &cur_matched_fields,
                                          &cur_matched_bytes);

    if (cmp > 0) {
    low_slot_match:
      low = mid;
      low_matched_fields = cur_matched_fields;
      low_matched_bytes = cur_matched_bytes;

    } else if (cmp) {
#ifdef PAGE_CUR_LE_OR_EXTENDS
      if (mode == PAGE_CUR_LE_OR_EXTENDS &&
          page_cur_rec_field_extends(tuple, mid_rec, offsets,
                                     cur_matched_fields, index)) {
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

  while (page_rec_get_next_const(low_rec) != up_rec) {
    mid_rec = page_rec_get_next_const(low_rec);

    ut_pair_min(&cur_matched_fields, &cur_matched_bytes, low_matched_fields,
                low_matched_bytes, up_matched_fields, up_matched_bytes);

    offsets = rec_get_offsets(mid_rec, index, offsets_,
                              dtuple_get_n_fields_cmp(tuple), &heap);

    cmp = cmp_dtuple_rec_with_match_bytes(tuple, mid_rec, index, offsets,
                                          &cur_matched_fields,
                                          &cur_matched_bytes);

    if (cmp > 0) {
    low_rec_match:
      low_rec = mid_rec;
      low_matched_fields = cur_matched_fields;
      low_matched_bytes = cur_matched_bytes;

    } else if (cmp) {
#ifdef PAGE_CUR_LE_OR_EXTENDS
      if (mode == PAGE_CUR_LE_OR_EXTENDS &&
          page_cur_rec_field_extends(tuple, mid_rec, offsets,
                                     cur_matched_fields, index)) {
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
      if (!cmp && !cur_matched_fields) {
#ifdef UNIV_DEBUG
        mtr_t mtr;
        mtr_start(&mtr);

        /* We got a match, but cur_matched_fields is
        0, it must have REC_INFO_MIN_REC_FLAG */
        ulint rec_info = rec_get_info_bits(mid_rec, rec_offs_comp(offsets));
        ut_ad(rec_info & REC_INFO_MIN_REC_FLAG);
        ut_ad(btr_page_get_prev(page, &mtr) == FIL_NULL);
        mtr_commit(&mtr);
#endif

        cur_matched_fields = dtuple_get_n_fields_cmp(tuple);
      }

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

  *iup_matched_fields = up_matched_fields;
  *iup_matched_bytes = up_matched_bytes;
  *ilow_matched_fields = low_matched_fields;
  *ilow_matched_bytes = low_matched_bytes;
  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }
}

/** Positions a page cursor on a randomly chosen user record on a page. If there
 are no user records, sets the cursor on the infimum record. */
void page_cur_open_on_rnd_user_rec(buf_block_t *block, /*!< in: page */
                                   page_cur_t *cursor) /*!< out: page cursor */
{
  ulint rnd;
  ulint n_recs = page_get_n_recs(buf_block_get_frame(block));

  page_cur_set_before_first(block, cursor);

  if (UNIV_UNLIKELY(n_recs == 0)) {
    return;
  }

  rnd = (ulint)(page_cur_lcg_prng() % n_recs);

  do {
    page_cur_move_to_next(cursor);
  } while (rnd--);
}

/** Writes the log record of a record insert on a page. */
static void page_cur_insert_rec_write_log(
    rec_t *insert_rec,   /*!< in: inserted physical record */
    ulint rec_size,      /*!< in: insert_rec size */
    rec_t *cursor_rec,   /*!< in: record the
                         cursor is pointing to */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mini-transaction handle */
{
  ulint cur_rec_size;
  ulint extra_size;
  ulint cur_extra_size;
  const byte *ins_ptr;
  const byte *log_end;
  ulint i;

  /* Avoid REDO logging to save on costly IO because
  temporary tables are not recovered during crash recovery. */
  if (index->table->is_temporary()) {
    byte *log_ptr = mlog_open(mtr, 0);
    if (log_ptr == NULL) {
      return;
    }
    mlog_close(mtr, log_ptr);
    log_ptr = NULL;
  }

  ut_a(rec_size < UNIV_PAGE_SIZE);
  ut_ad(page_align(insert_rec) == page_align(cursor_rec));
  ut_ad(!page_rec_is_comp(insert_rec) == !dict_table_is_comp(index->table));

  {
    mem_heap_t *heap = NULL;
    ulint cur_offs_[REC_OFFS_NORMAL_SIZE];
    ulint ins_offs_[REC_OFFS_NORMAL_SIZE];

    ulint *cur_offs;
    ulint *ins_offs;

    rec_offs_init(cur_offs_);
    rec_offs_init(ins_offs_);

    cur_offs =
        rec_get_offsets(cursor_rec, index, cur_offs_, ULINT_UNDEFINED, &heap);
    ins_offs =
        rec_get_offsets(insert_rec, index, ins_offs_, ULINT_UNDEFINED, &heap);

    extra_size = rec_offs_extra_size(ins_offs);
    cur_extra_size = rec_offs_extra_size(cur_offs);
    ut_ad(rec_size == rec_offs_size(ins_offs));
    cur_rec_size = rec_offs_size(cur_offs);

    if (heap != nullptr) {
      mem_heap_free(heap);
    }
  }

  ins_ptr = insert_rec - extra_size;

  i = 0;

  if (cur_extra_size == extra_size) {
    ulint min_rec_size = ut_min(cur_rec_size, rec_size);

    const byte *cur_ptr = cursor_rec - cur_extra_size;

    /* Find out the first byte in insert_rec which differs from
    cursor_rec; skip the bytes in the record info */

    do {
      if (*ins_ptr == *cur_ptr) {
        i++;
        ins_ptr++;
        cur_ptr++;
      } else if ((i < extra_size) &&
                 (i >= extra_size - page_rec_get_base_extra_size(insert_rec))) {
        i = extra_size;
        ins_ptr = insert_rec;
        cur_ptr = cursor_rec;
      } else {
        break;
      }
    } while (i < min_rec_size);
  }

  byte *log_ptr;

  if (mtr_get_log_mode(mtr) != MTR_LOG_SHORT_INSERTS) {
    if (page_rec_is_comp(insert_rec)) {
      log_ptr = mlog_open_and_write_index(mtr, insert_rec, index,
                                          MLOG_COMP_REC_INSERT,
                                          2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN);
      if (UNIV_UNLIKELY(!log_ptr)) {
        /* Logging in mtr is switched off
        during crash recovery: in that case
        mlog_open returns NULL */
        return;
      }
    } else {
      log_ptr = mlog_open(mtr, 11 + 2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN);
      if (UNIV_UNLIKELY(!log_ptr)) {
        /* Logging in mtr is switched off
        during crash recovery: in that case
        mlog_open returns NULL */
        return;
      }

      log_ptr = mlog_write_initial_log_record_fast(insert_rec, MLOG_REC_INSERT,
                                                   log_ptr, mtr);
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
    if (UNIV_UNLIKELY(rec_get_info_and_status_bits(insert_rec, TRUE) !=
                      rec_get_info_and_status_bits(cursor_rec, TRUE))) {
      goto need_extra_info;
    }
  } else {
    if (UNIV_UNLIKELY(rec_get_info_and_status_bits(insert_rec, FALSE) !=
                      rec_get_info_and_status_bits(cursor_rec, FALSE))) {
      goto need_extra_info;
    }
  }

  if (extra_size != cur_extra_size || rec_size != cur_rec_size) {
  need_extra_info:
    /* Write the record end segment length
    and the extra info storage flag */
    log_ptr += mach_write_compressed(log_ptr, 2 * (rec_size - i) + 1);

    /* Write the info bits */
    mach_write_to_1(log_ptr, rec_get_info_and_status_bits(
                                 insert_rec, page_rec_is_comp(insert_rec)));
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
#define page_cur_insert_rec_write_log(ins_rec, size, cur, index, mtr) ((void)0)
#endif /* !UNIV_HOTBACKUP */

/** Parses a log record of a record insert on a page.
 @return end of log record or NULL */
byte *page_cur_parse_insert_rec(
    ibool is_short,      /*!< in: TRUE if short inserts */
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mtr or NULL */
{
  ulint origin_offset = 0; /* remove warning */
  ulint end_seg_len;
  ulint mismatch_index = 0; /* remove warning */
  page_t *page;
  rec_t *cursor_rec;
  byte buf1[1024];
  byte *buf;
  const byte *ptr2 = ptr;
  ulint info_and_status_bits = 0; /* remove warning */
  page_cur_t cursor;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  page = block ? buf_block_get_frame(block) : NULL;

  if (is_short) {
    cursor_rec = page_rec_get_prev(page_get_supremum_rec(page));
  } else {
    ulint offset;

    /* Read the cursor rec offset as a 2-byte ulint */

    if (UNIV_UNLIKELY(end_ptr < ptr + 2)) {
      return (NULL);
    }

    offset = mach_read_from_2(ptr);
    ptr += 2;

    cursor_rec = page + offset;

    if (offset >= UNIV_PAGE_SIZE) {
      recv_sys->found_corrupt_log = TRUE;

      return (NULL);
    }
  }

  end_seg_len = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == NULL) {
    return (NULL);
  }

  if (end_seg_len >= UNIV_PAGE_SIZE << 1) {
    recv_sys->found_corrupt_log = TRUE;

    return (NULL);
  }

  if (end_seg_len & 0x1UL) {
    /* Read the info bits */

    if (end_ptr < ptr + 1) {
      return (NULL);
    }

    info_and_status_bits = mach_read_from_1(ptr);
    ptr++;

    origin_offset = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == NULL) {
      return (NULL);
    }

    ut_a(origin_offset < UNIV_PAGE_SIZE);

    mismatch_index = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == NULL) {
      return (NULL);
    }

    ut_a(mismatch_index < UNIV_PAGE_SIZE);
  }

  if (end_ptr < ptr + (end_seg_len >> 1)) {
    return (NULL);
  }

  if (!block) {
    return (const_cast<byte *>(ptr + (end_seg_len >> 1)));
  }

  ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));
  ut_ad(!buf_block_get_page_zip(block) || page_is_comp(page));

  /* Read from the log the inserted index record end segment which
  differs from the cursor record */

  offsets = rec_get_offsets(cursor_rec, index, offsets, ULINT_UNDEFINED, &heap);

  if (!(end_seg_len & 0x1UL)) {
    info_and_status_bits =
        rec_get_info_and_status_bits(cursor_rec, page_is_comp(page));
    origin_offset = rec_offs_extra_size(offsets);
    mismatch_index = rec_offs_size(offsets) - (end_seg_len >> 1);
  }

  end_seg_len >>= 1;

  if (mismatch_index + end_seg_len < sizeof buf1) {
    buf = buf1;
  } else {
    buf = static_cast<byte *>(ut_malloc_nokey(mismatch_index + end_seg_len));
  }

  /* Build the inserted record to buf */

  if (UNIV_UNLIKELY(mismatch_index >= UNIV_PAGE_SIZE)) {
    ib::fatal(ER_IB_MSG_859)
        << "is_short " << is_short << ", "
        << "info_and_status_bits " << info_and_status_bits << ", offset "
        << page_offset(cursor_rec)
        << ","
           " o_offset "
        << origin_offset << ", mismatch index " << mismatch_index
        << ", end_seg_len " << end_seg_len << " parsed len " << (ptr - ptr2);
  }

  ut_memcpy(buf, rec_get_start(cursor_rec, offsets), mismatch_index);
  ut_memcpy(buf + mismatch_index, ptr, end_seg_len);

  if (page_is_comp(page)) {
    rec_set_info_and_status_bits(buf + origin_offset, info_and_status_bits);
  } else {
    rec_set_info_bits_old(buf + origin_offset, info_and_status_bits);
  }

  page_cur_position(cursor_rec, block, &cursor);

  offsets = rec_get_offsets(buf + origin_offset, index, offsets,
                            ULINT_UNDEFINED, &heap);
  if (UNIV_UNLIKELY(!page_cur_rec_insert(&cursor, buf + origin_offset, index,
                                         offsets, mtr))) {
    /* The redo log record should only have been written
    after the write was successful. */
    ut_error;
  }

  if (buf != buf1) {
    ut_free(buf);
  }

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  return (const_cast<byte *>(ptr + end_seg_len));
}

/** Inserts a record next to page cursor on an uncompressed page.
 Returns pointer to inserted record if succeed, i.e., enough
 space available, NULL otherwise. The cursor stays at the same position.
 @return pointer to record if succeed, NULL otherwise */
rec_t *page_cur_insert_rec_low(
    rec_t *current_rec,  /*!< in: pointer to current record after
                     which the new record is inserted */
    dict_index_t *index, /*!< in: record descriptor */
    const rec_t *rec,    /*!< in: pointer to a physical record */
    ulint *offsets,      /*!< in/out: rec_get_offsets(rec, index) */
    mtr_t *mtr)          /*!< in: mini-transaction handle, or NULL */
{
  byte *insert_buf;
  ulint rec_size;
  page_t *page;       /*!< the relevant page */
  rec_t *last_insert; /*!< cursor position at previous
                      insert */
  rec_t *free_rec;    /*!< a free record that was reused,
                      or NULL */
  rec_t *insert_rec;  /*!< inserted record */
  ulint heap_no;      /*!< heap number of the inserted
                      record */

  ut_ad(rec_offs_validate(rec, index, offsets));

  page = page_align(current_rec);
  ut_ad(dict_table_is_comp(index->table) == (ibool) !!page_is_comp(page));
  ut_ad(fil_page_index_page_check(page));
  ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID) == index->id ||
        recv_recovery_is_on() ||
        (mtr ? mtr->is_inside_ibuf() : dict_index_is_ibuf(index)));

  ut_ad(!page_rec_is_supremum(current_rec));

  /* 1. Get the size of the physical record in the page */
  rec_size = rec_offs_size(offsets);

#ifdef UNIV_DEBUG_VALGRIND
  {
    const void *rec_start = rec - rec_offs_extra_size(offsets);
    ulint extra_size = rec_offs_extra_size(offsets) -
                       (rec_offs_comp(offsets) ? REC_N_NEW_EXTRA_BYTES
                                               : REC_N_OLD_EXTRA_BYTES);

    /* All data bytes of the record must be valid. */
    UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
    /* The variable-length header must be valid. */
    UNIV_MEM_ASSERT_RW(rec_start, extra_size);
  }
#endif /* UNIV_DEBUG_VALGRIND */

  /* 2. Try to find suitable space from page memory management */

  free_rec = page_header_get_ptr(page, PAGE_FREE);
  if (UNIV_LIKELY_NULL(free_rec)) {
    /* Try to allocate from the head of the free list. */
    ulint foffsets_[REC_OFFS_NORMAL_SIZE];
    ulint *foffsets = foffsets_;
    mem_heap_t *heap = NULL;

    rec_offs_init(foffsets_);

    foffsets =
        rec_get_offsets(free_rec, index, foffsets, ULINT_UNDEFINED, &heap);
    if (rec_offs_size(foffsets) < rec_size) {
      if (UNIV_LIKELY_NULL(heap)) {
        mem_heap_free(heap);
      }

      goto use_heap;
    }

    insert_buf = free_rec - rec_offs_extra_size(foffsets);

    if (page_is_comp(page)) {
      heap_no = rec_get_heap_no_new(free_rec);
      page_mem_alloc_free(page, NULL, rec_get_next_ptr(free_rec, TRUE),
                          rec_size);
    } else {
      heap_no = rec_get_heap_no_old(free_rec);
      page_mem_alloc_free(page, NULL, rec_get_next_ptr(free_rec, FALSE),
                          rec_size);
    }

    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  } else {
  use_heap:
    free_rec = NULL;
    insert_buf = page_mem_alloc_heap(page, NULL, rec_size, &heap_no);

    if (UNIV_UNLIKELY(insert_buf == NULL)) {
      return (NULL);
    }
  }

  /* 3. Create the record */
  insert_rec = rec_copy(insert_buf, rec, offsets);
  rec_offs_make_valid(insert_rec, index, offsets);

  /* 4. Insert the record in the linked list of records */
  ut_ad(current_rec != insert_rec);

  {
    /* next record after current before the insertion */
    rec_t *next_rec = page_rec_get_next(current_rec);
#ifdef UNIV_DEBUG
    if (page_is_comp(page)) {
      ut_ad(rec_get_status(current_rec) <= REC_STATUS_INFIMUM);
      ut_ad(rec_get_status(insert_rec) < REC_STATUS_INFIMUM);
      ut_ad(rec_get_status(next_rec) != REC_STATUS_INFIMUM);
    }
#endif
    page_rec_set_next(insert_rec, next_rec);
    page_rec_set_next(current_rec, insert_rec);
  }

  page_header_set_field(page, NULL, PAGE_N_RECS, 1 + page_get_n_recs(page));

  /* 5. Set the n_owned field in the inserted record to zero,
  and set the heap_no field */
  if (page_is_comp(page)) {
    rec_set_n_owned_new(insert_rec, NULL, 0);
    rec_set_heap_no_new(insert_rec, heap_no);
  } else {
    rec_set_n_owned_old(insert_rec, 0);
    rec_set_heap_no_old(insert_rec, heap_no);
  }

  UNIV_MEM_ASSERT_RW(rec_get_start(insert_rec, offsets),
                     rec_offs_size(offsets));
  /* 6. Update the last insertion info in page header */

  last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
  ut_ad(!last_insert || !page_is_comp(page) ||
        rec_get_node_ptr_flag(last_insert) ==
            rec_get_node_ptr_flag(insert_rec));

  if (!dict_index_is_spatial(index)) {
    if (UNIV_UNLIKELY(last_insert == NULL)) {
      page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_NO_DIRECTION);
      page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);

    } else if ((last_insert == current_rec) &&
               (page_header_get_field(page, PAGE_DIRECTION) != PAGE_LEFT)) {
      page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_RIGHT);
      page_header_set_field(page, NULL, PAGE_N_DIRECTION,
                            page_header_get_field(page, PAGE_N_DIRECTION) + 1);

    } else if ((page_rec_get_next(insert_rec) == last_insert) &&
               (page_header_get_field(page, PAGE_DIRECTION) != PAGE_RIGHT)) {
      page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_LEFT);
      page_header_set_field(page, NULL, PAGE_N_DIRECTION,
                            page_header_get_field(page, PAGE_N_DIRECTION) + 1);
    } else {
      page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_NO_DIRECTION);
      page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);
    }
  }

  page_header_set_ptr(page, NULL, PAGE_LAST_INSERT, insert_rec);

  /* 7. It remains to update the owner record. */
  {
    rec_t *owner_rec = page_rec_find_owner_rec(insert_rec);
    ulint n_owned;
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
      page_dir_split_slot(page, NULL, page_dir_find_owner_slot(owner_rec));
    }
  }

  /* 9. Write log record of the insert */
  if (UNIV_LIKELY(mtr != NULL)) {
    page_cur_insert_rec_write_log(insert_rec, rec_size, current_rec, index,
                                  mtr);
  }

  return (insert_rec);
}

/** Inserts a record next to page cursor on an uncompressed page.
@param[in]	current_rec	pointer to current record after which
                                the new record is inserted.
@param[in]	index		record descriptor
@param[in]	tuple		pointer to a data tuple
@param[in]	n_ext		number of externally stored columns
@param[in]	mtr		mini-transaction handle, or NULL

@return pointer to record if succeed, NULL otherwise */
rec_t *page_cur_direct_insert_rec_low(rec_t *current_rec, dict_index_t *index,
                                      const dtuple_t *tuple, ulint n_ext,
                                      mtr_t *mtr) {
  byte *insert_buf;
  ulint rec_size;
  page_t *page;       /*!< the relevant page */
  rec_t *last_insert; /*!< cursor position at previous
                      insert */
  rec_t *free_rec;    /*!< a free record that was reused,
                      or NULL */
  rec_t *insert_rec;  /*!< inserted record */
  ulint heap_no;      /*!< heap number of the inserted
                      record */

  page = page_align(current_rec);

  ut_ad(dict_table_is_comp(index->table) == (ibool) !!page_is_comp(page));

  ut_ad(fil_page_index_page_check(page));

  ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID) == index->id);

  ut_ad(!page_rec_is_supremum(current_rec));

  /* 1. Get the size of the physical record in the page */
  rec_size = index->rec_cache.rec_size;

  /* 2. Try to find suitable space from page memory management */
  free_rec = page_header_get_ptr(page, PAGE_FREE);
  if (free_rec) {
    /* Try to allocate from the head of the free list. */
    ulint foffsets_[REC_OFFS_NORMAL_SIZE];
    ulint *foffsets = foffsets_;
    mem_heap_t *heap = NULL;

    rec_offs_init(foffsets_);

    foffsets =
        rec_get_offsets(free_rec, index, foffsets, ULINT_UNDEFINED, &heap);
    if (rec_offs_size(foffsets) < rec_size) {
      if (heap != NULL) {
        mem_heap_free(heap);
        heap = NULL;
      }

      free_rec = NULL;
      insert_buf = page_mem_alloc_heap(page, NULL, rec_size, &heap_no);

      if (insert_buf == NULL) {
        return (NULL);
      }
    } else {
      insert_buf = free_rec - rec_offs_extra_size(foffsets);

      if (page_is_comp(page)) {
        heap_no = rec_get_heap_no_new(free_rec);
        page_mem_alloc_free(page, NULL, rec_get_next_ptr(free_rec, TRUE),
                            rec_size);
      } else {
        heap_no = rec_get_heap_no_old(free_rec);
        page_mem_alloc_free(page, NULL, rec_get_next_ptr(free_rec, FALSE),
                            rec_size);
      }

      if (heap != NULL) {
        mem_heap_free(heap);
        heap = NULL;
      }
    }
  } else {
    free_rec = NULL;
    insert_buf = page_mem_alloc_heap(page, NULL, rec_size, &heap_no);

    if (insert_buf == NULL) {
      return (NULL);
    }
  }

  /* 3. Create the record */
  insert_rec = rec_convert_dtuple_to_rec(insert_buf, index, tuple, n_ext);

  /* 4. Insert the record in the linked list of records */
  ut_ad(current_rec != insert_rec);

  {
    /* next record after current before the insertion */
    rec_t *next_rec = page_rec_get_next(current_rec);
#ifdef UNIV_DEBUG
    if (page_is_comp(page)) {
      ut_ad(rec_get_status(current_rec) <= REC_STATUS_INFIMUM);
      ut_ad(rec_get_status(insert_rec) < REC_STATUS_INFIMUM);
      ut_ad(rec_get_status(next_rec) != REC_STATUS_INFIMUM);
    }
#endif
    page_rec_set_next(insert_rec, next_rec);
    page_rec_set_next(current_rec, insert_rec);
  }

  page_header_set_field(page, NULL, PAGE_N_RECS, 1 + page_get_n_recs(page));

  /* 5. Set the n_owned field in the inserted record to zero,
  and set the heap_no field */
  if (page_is_comp(page)) {
    rec_set_n_owned_new(insert_rec, NULL, 0);
    rec_set_heap_no_new(insert_rec, heap_no);
  } else {
    rec_set_n_owned_old(insert_rec, 0);
    rec_set_heap_no_old(insert_rec, heap_no);
  }

  /* 6. Update the last insertion info in page header */

  last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
  ut_ad(!last_insert || !page_is_comp(page) ||
        rec_get_node_ptr_flag(last_insert) ==
            rec_get_node_ptr_flag(insert_rec));

  if (last_insert == NULL) {
    page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_NO_DIRECTION);
    page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);

  } else if ((last_insert == current_rec) &&
             (page_header_get_field(page, PAGE_DIRECTION) != PAGE_LEFT)) {
    page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_RIGHT);
    page_header_set_field(page, NULL, PAGE_N_DIRECTION,
                          page_header_get_field(page, PAGE_N_DIRECTION) + 1);

  } else if ((page_rec_get_next(insert_rec) == last_insert) &&
             (page_header_get_field(page, PAGE_DIRECTION) != PAGE_RIGHT)) {
    page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_LEFT);
    page_header_set_field(page, NULL, PAGE_N_DIRECTION,
                          page_header_get_field(page, PAGE_N_DIRECTION) + 1);
  } else {
    page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_NO_DIRECTION);
    page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);
  }

  page_header_set_ptr(page, NULL, PAGE_LAST_INSERT, insert_rec);

  /* 7. It remains to update the owner record. */
  {
    rec_t *owner_rec = page_rec_find_owner_rec(insert_rec);
    ulint n_owned;
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

    if (n_owned == PAGE_DIR_SLOT_MAX_N_OWNED) {
      page_dir_split_slot(page, NULL, page_dir_find_owner_slot(owner_rec));
    }
  }

  /* 8. Open the mtr for name sake to set the modification flag
  to true failing which no flush would be done. */
  byte *log_ptr = mlog_open(mtr, 0);
  ut_ad(log_ptr == NULL);
  if (log_ptr != NULL) {
    /* To keep complier happy. */
    mlog_close(mtr, log_ptr);
  }

  return (insert_rec);
}

/** Inserts a record next to page cursor on a compressed and uncompressed
 page. Returns pointer to inserted record if succeed, i.e.,
 enough space available, NULL otherwise.
 The cursor stays at the same position.

 IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
 if this is a compressed leaf page in a secondary index.
 This has to be done either within the same mini-transaction,
 or by invoking ibuf_reset_free_bits() before mtr_commit().

 @return pointer to record if succeed, NULL otherwise */
rec_t *page_cur_insert_rec_zip(
    page_cur_t *cursor,  /*!< in/out: page cursor */
    dict_index_t *index, /*!< in: record descriptor */
    const rec_t *rec,    /*!< in: pointer to a physical record */
    ulint *offsets,      /*!< in/out: rec_get_offsets(rec, index) */
    mtr_t *mtr)          /*!< in: mini-transaction handle, or NULL */
{
  byte *insert_buf;
  ulint rec_size;
  page_t *page;       /*!< the relevant page */
  rec_t *last_insert; /*!< cursor position at previous
                      insert */
  rec_t *free_rec;    /*!< a free record that was reused,
                      or NULL */
  rec_t *insert_rec;  /*!< inserted record */
  ulint heap_no;      /*!< heap number of the inserted
                      record */
  page_zip_des_t *page_zip;

  page_zip = page_cur_get_page_zip(cursor);
  ut_ad(page_zip);

  ut_ad(rec_offs_validate(rec, index, offsets));

  page = page_cur_get_page(cursor);
  ut_ad(dict_table_is_comp(index->table));
  ut_ad(page_is_comp(page));
  ut_ad(fil_page_index_page_check(page));
  ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID) == index->id ||
        (mtr ? mtr->is_inside_ibuf() : dict_index_is_ibuf(index)) ||
        recv_recovery_is_on());

  ut_ad(!page_cur_is_after_last(cursor));
#ifdef UNIV_ZIP_DEBUG
  ut_a(page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

  /* 1. Get the size of the physical record in the page */
  rec_size = rec_offs_size(offsets);

#ifdef UNIV_DEBUG_VALGRIND
  {
    const void *rec_start = rec - rec_offs_extra_size(offsets);
    ulint extra_size = rec_offs_extra_size(offsets) -
                       (rec_offs_comp(offsets) ? REC_N_NEW_EXTRA_BYTES
                                               : REC_N_OLD_EXTRA_BYTES);

    /* All data bytes of the record must be valid. */
    UNIV_MEM_ASSERT_RW(rec, rec_offs_data_size(offsets));
    /* The variable-length header must be valid. */
    UNIV_MEM_ASSERT_RW(rec_start, extra_size);
  }
#endif /* UNIV_DEBUG_VALGRIND */

  const bool reorg_before_insert =
      page_has_garbage(page) && rec_size > page_get_max_insert_size(page, 1) &&
      rec_size <= page_get_max_insert_size_after_reorganize(page, 1);

  /* 2. Try to find suitable space from page memory management */
  if (!page_zip_available(page_zip, index->is_clustered(), rec_size, 1) ||
      reorg_before_insert) {
    /* The values can change dynamically. */
    bool log_compressed = page_zip_log_pages;
    ulint level = page_zip_level;
#ifdef UNIV_DEBUG
    rec_t *cursor_rec = page_cur_get_rec(cursor);
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
                      page_header_get_field(page, PAGE_LEVEL), 0, mtr,
                      fil_page_get_type(page));
      ut_ad(!page_header_get_ptr(page, PAGE_FREE));

      if (page_zip_available(page_zip, index->is_clustered(), rec_size, 1)) {
        goto use_heap;
      }

      /* The cursor should remain on the page infimum. */
      return (NULL);
    } else if (!page_zip->m_nonempty && !page_has_garbage(page)) {
      /* The page has been freshly compressed, so
      reorganizing it will not help. */
    } else if (log_compressed && !reorg_before_insert) {
      /* Insert into uncompressed page only, and
      try page_zip_reorganize() afterwards. */
    } else if (btr_page_reorganize_low(recv_recovery_is_on(), level, cursor,
                                       index, mtr)) {
      ut_ad(!page_header_get_ptr(page, PAGE_FREE));

      if (page_zip_available(page_zip, index->is_clustered(), rec_size, 1)) {
        /* After reorganizing, there is space
        available. */
        goto use_heap;
      }
    } else {
      ut_ad(cursor->rec == cursor_rec);
      return (NULL);
    }

    /* Try compressing the whole page afterwards. */
    insert_rec =
        page_cur_insert_rec_low(cursor->rec, index, rec, offsets, NULL);

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
      ulint pos = page_rec_get_n_recs_before(insert_rec);
      ut_ad(pos > 0);

      if (!log_compressed) {
        if (page_zip_compress(page_zip, page, index, level, NULL)) {
          page_cur_insert_rec_write_log(insert_rec, rec_size, cursor->rec,
                                        index, mtr);
          page_zip_compress_write_log_no_data(level, page, index, mtr);

          rec_offs_make_valid(insert_rec, index, offsets);
          return (insert_rec);
        }

        ut_ad(cursor->rec == (pos > 1 ? page_rec_get_nth(page, pos - 1)
                                      : page + PAGE_NEW_INFIMUM));
      } else {
        /* We are writing entire page images
        to the log. Reduce the redo log volume
        by reorganizing the page at the same time. */
        if (page_zip_reorganize(cursor->block, index, mtr)) {
          /* The page was reorganized:
          Seek to pos. */
          if (pos > 1) {
            cursor->rec = page_rec_get_nth(page, pos - 1);
          } else {
            cursor->rec = page + PAGE_NEW_INFIMUM;
          }

          insert_rec = page + rec_get_next_offs(cursor->rec, TRUE);
          rec_offs_make_valid(insert_rec, index, offsets);
          return (insert_rec);
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
      ut_ad(page_validate(page, index));
      insert_rec = NULL;
    }

    return (insert_rec);
  }

  free_rec = page_header_get_ptr(page, PAGE_FREE);
  if (UNIV_LIKELY_NULL(free_rec)) {
    /* Try to allocate from the head of the free list. */
    lint extra_size_diff;
    ulint foffsets_[REC_OFFS_NORMAL_SIZE];
    ulint *foffsets = foffsets_;
    mem_heap_t *heap = NULL;

    rec_offs_init(foffsets_);

    foffsets =
        rec_get_offsets(free_rec, index, foffsets, ULINT_UNDEFINED, &heap);
    if (rec_offs_size(foffsets) < rec_size) {
    too_small:
      if (UNIV_LIKELY_NULL(heap)) {
        mem_heap_free(heap);
      }

      goto use_heap;
    }

    insert_buf = free_rec - rec_offs_extra_size(foffsets);

    /* On compressed pages, do not relocate records from
    the free list.  If extra_size would grow, use the heap. */
    extra_size_diff =
        rec_offs_extra_size(offsets) - rec_offs_extra_size(foffsets);

    if (UNIV_UNLIKELY(extra_size_diff < 0)) {
      /* Add an offset to the extra_size. */
      if (rec_offs_size(foffsets) < rec_size - extra_size_diff) {
        goto too_small;
      }

      insert_buf -= extra_size_diff;
    } else if (UNIV_UNLIKELY(extra_size_diff)) {
      /* Do not allow extra_size to grow */

      goto too_small;
    }

    heap_no = rec_get_heap_no_new(free_rec);
    page_mem_alloc_free(page, page_zip, rec_get_next_ptr(free_rec, TRUE),
                        rec_size);

    if (!page_is_leaf(page)) {
      /* Zero out the node pointer of free_rec,
      in case it will not be overwritten by
      insert_rec. */

      ut_ad(rec_size > REC_NODE_PTR_SIZE);

      if (rec_offs_extra_size(foffsets) + rec_offs_data_size(foffsets) >
          rec_size) {
        memset(rec_get_end(free_rec, foffsets) - REC_NODE_PTR_SIZE, 0,
               REC_NODE_PTR_SIZE);
      }
    } else if (index->is_clustered()) {
      /* Zero out the DB_TRX_ID and DB_ROLL_PTR
      columns of free_rec, in case it will not be
      overwritten by insert_rec. */

      ulint trx_id_col;
      ulint trx_id_offs;
      ulint len;

      trx_id_col = index->get_sys_col_pos(DATA_TRX_ID);
      ut_ad(trx_id_col > 0);
      ut_ad(trx_id_col != ULINT_UNDEFINED);

      trx_id_offs = rec_get_nth_field_offs(foffsets, trx_id_col, &len);
      ut_ad(len == DATA_TRX_ID_LEN);

      if (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN + trx_id_offs +
              rec_offs_extra_size(foffsets) >
          rec_size) {
        /* We will have to zero out the
        DB_TRX_ID and DB_ROLL_PTR, because
        they will not be fully overwritten by
        insert_rec. */

        memset(free_rec + trx_id_offs, 0, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
      }

      ut_ad(free_rec + trx_id_offs + DATA_TRX_ID_LEN ==
            rec_get_nth_field(free_rec, foffsets, trx_id_col + 1, &len));
      ut_ad(len == DATA_ROLL_PTR_LEN);
    }

    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  } else {
  use_heap:
    free_rec = NULL;
    insert_buf = page_mem_alloc_heap(page, page_zip, rec_size, &heap_no);

    if (UNIV_UNLIKELY(insert_buf == NULL)) {
      return (NULL);
    }

    page_zip_dir_add_slot(page_zip, index->is_clustered());
  }

  /* 3. Create the record */
  insert_rec = rec_copy(insert_buf, rec, offsets);
  rec_offs_make_valid(insert_rec, index, offsets);

  /* 4. Insert the record in the linked list of records */
  ut_ad(cursor->rec != insert_rec);

  {
    /* next record after current before the insertion */
    const rec_t *next_rec = page_rec_get_next_low(cursor->rec, TRUE);
    ut_ad(rec_get_status(cursor->rec) <= REC_STATUS_INFIMUM);
    ut_ad(rec_get_status(insert_rec) < REC_STATUS_INFIMUM);
    ut_ad(rec_get_status(next_rec) != REC_STATUS_INFIMUM);

    page_rec_set_next(insert_rec, next_rec);
    page_rec_set_next(cursor->rec, insert_rec);
  }

  page_header_set_field(page, page_zip, PAGE_N_RECS, 1 + page_get_n_recs(page));

  /* 5. Set the n_owned field in the inserted record to zero,
  and set the heap_no field */
  rec_set_n_owned_new(insert_rec, NULL, 0);
  rec_set_heap_no_new(insert_rec, heap_no);

  UNIV_MEM_ASSERT_RW(rec_get_start(insert_rec, offsets),
                     rec_offs_size(offsets));

  page_zip_dir_insert(page_zip, cursor->rec, free_rec, insert_rec);

  /* 6. Update the last insertion info in page header */

  last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
  ut_ad(!last_insert || rec_get_node_ptr_flag(last_insert) ==
                            rec_get_node_ptr_flag(insert_rec));

  if (!dict_index_is_spatial(index)) {
    if (UNIV_UNLIKELY(last_insert == NULL)) {
      page_header_set_field(page, page_zip, PAGE_DIRECTION, PAGE_NO_DIRECTION);
      page_header_set_field(page, page_zip, PAGE_N_DIRECTION, 0);

    } else if ((last_insert == cursor->rec) &&
               (page_header_get_field(page, PAGE_DIRECTION) != PAGE_LEFT)) {
      page_header_set_field(page, page_zip, PAGE_DIRECTION, PAGE_RIGHT);
      page_header_set_field(page, page_zip, PAGE_N_DIRECTION,
                            page_header_get_field(page, PAGE_N_DIRECTION) + 1);

    } else if ((page_rec_get_next(insert_rec) == last_insert) &&
               (page_header_get_field(page, PAGE_DIRECTION) != PAGE_RIGHT)) {
      page_header_set_field(page, page_zip, PAGE_DIRECTION, PAGE_LEFT);
      page_header_set_field(page, page_zip, PAGE_N_DIRECTION,
                            page_header_get_field(page, PAGE_N_DIRECTION) + 1);
    } else {
      page_header_set_field(page, page_zip, PAGE_DIRECTION, PAGE_NO_DIRECTION);
      page_header_set_field(page, page_zip, PAGE_N_DIRECTION, 0);
    }
  }

  page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, insert_rec);

  /* 7. It remains to update the owner record. */
  {
    rec_t *owner_rec = page_rec_find_owner_rec(insert_rec);
    ulint n_owned;

    n_owned = rec_get_n_owned_new(owner_rec);
    rec_set_n_owned_new(owner_rec, page_zip, n_owned + 1);

    /* 8. Now we have incremented the n_owned field of the owner
    record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
    we have to split the corresponding directory slot in two. */

    if (UNIV_UNLIKELY(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED)) {
      page_dir_split_slot(page, page_zip, page_dir_find_owner_slot(owner_rec));
    }
  }

  page_zip_write_rec(page_zip, insert_rec, index, offsets, 1);

  /* 9. Write log record of the insert */
  if (UNIV_LIKELY(mtr != NULL)) {
    page_cur_insert_rec_write_log(insert_rec, rec_size, cursor->rec, index,
                                  mtr);
  }

  return (insert_rec);
}

#ifndef UNIV_HOTBACKUP
/** Writes a log record of copying a record list end to a new created page.
 @return 4-byte field where to write the log data length, or NULL if
 logging is disabled */
UNIV_INLINE
byte *page_copy_rec_list_to_created_page_write_log(
    page_t *page,        /*!< in: index page */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mtr */
{
  byte *log_ptr;

  ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

  log_ptr = mlog_open_and_write_index(mtr, page, index,
                                      page_is_comp(page)
                                          ? MLOG_COMP_LIST_END_COPY_CREATED
                                          : MLOG_LIST_END_COPY_CREATED,
                                      4);
  if (UNIV_LIKELY(log_ptr != NULL)) {
    mlog_close(mtr, log_ptr + 4);
  }

  return (log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses a log record of copying a record list end to a new created page.
 @return end of log record or NULL */
byte *page_parse_copy_rec_list_to_created_page(
    byte *ptr,           /*!< in: buffer */
    byte *end_ptr,       /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mtr or NULL */
{
  byte *rec_end;
  ulint log_data_len;
  page_t *page;
  page_zip_des_t *page_zip;

  if (ptr + 4 > end_ptr) {
    return (NULL);
  }

  log_data_len = mach_read_from_4(ptr);
  ptr += 4;

  rec_end = ptr + log_data_len;

  if (rec_end > end_ptr) {
    return (NULL);
  }

  if (!block) {
    return (rec_end);
  }

  while (ptr < rec_end) {
    ptr = page_cur_parse_insert_rec(TRUE, ptr, end_ptr, block, index, mtr);
  }

  ut_a(ptr == rec_end);

  page = buf_block_get_frame(block);
  page_zip = buf_block_get_page_zip(block);

  page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, NULL);

  if (!dict_index_is_spatial(index)) {
    page_header_set_field(page, page_zip, PAGE_DIRECTION, PAGE_NO_DIRECTION);
    page_header_set_field(page, page_zip, PAGE_N_DIRECTION, 0);
  }

  return (rec_end);
}

#ifndef UNIV_HOTBACKUP
/** Copies records from page to a newly created page, from a given record
 onward, including that record. Infimum and supremum records are not copied.

 IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
 if this is a compressed leaf page in a secondary index.
 This has to be done either within the same mini-transaction,
 or by invoking ibuf_reset_free_bits() before mtr_commit(). */
void page_copy_rec_list_end_to_created_page(
    page_t *new_page,    /*!< in/out: index page to copy to */
    rec_t *rec,          /*!< in: first record to copy */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mtr */
{
  page_dir_slot_t *slot = 0; /* remove warning */
  byte *heap_top;
  rec_t *insert_rec = 0; /* remove warning */
  rec_t *prev_rec;
  ulint count;
  ulint n_recs;
  ulint slot_index;
  ulint rec_size;
  byte *log_ptr;
  ulint log_data_len;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(page_dir_get_n_heap(new_page) == PAGE_HEAP_NO_USER_LOW);
  ut_ad(page_align(rec) != new_page);
  ut_ad(page_rec_is_comp(rec) == page_is_comp(new_page));

  if (page_rec_is_infimum(rec)) {
    rec = page_rec_get_next(rec);
  }

  if (page_rec_is_supremum(rec)) {
    return;
  }

#ifdef UNIV_DEBUG
  /* To pass the debug tests we have to set these dummy values
  in the debug version */
  page_dir_set_n_slots(new_page, NULL, UNIV_PAGE_SIZE / 2);
  page_header_set_ptr(new_page, NULL, PAGE_HEAP_TOP,
                      new_page + UNIV_PAGE_SIZE - 1);
#endif

  log_ptr = page_copy_rec_list_to_created_page_write_log(new_page, index, mtr);

  log_data_len = mtr->get_log()->size();

  /* Individual inserts are logged in a shorter form */

  mtr_log_t log_mode;

  if (index->table->is_temporary() ||
      index->table->ibd_file_missing /* IMPORT TABLESPACE */) {
    log_mode = mtr_get_log_mode(mtr);
  } else {
    log_mode = mtr_set_log_mode(mtr, MTR_LOG_SHORT_INSERTS);
  }

  prev_rec = page_get_infimum_rec(new_page);
  if (page_is_comp(new_page)) {
    heap_top = new_page + PAGE_NEW_SUPREMUM_END;
  } else {
    heap_top = new_page + PAGE_OLD_SUPREMUM_END;
  }
  count = 0;
  slot_index = 0;
  n_recs = 0;

  do {
    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);
    insert_rec = rec_copy(heap_top, rec, offsets);

    if (page_is_comp(new_page)) {
      rec_set_next_offs_new(prev_rec, page_offset(insert_rec));

      rec_set_n_owned_new(insert_rec, NULL, 0);
      rec_set_heap_no_new(insert_rec, PAGE_HEAP_NO_USER_LOW + n_recs);
    } else {
      rec_set_next_offs_old(prev_rec, page_offset(insert_rec));

      rec_set_n_owned_old(insert_rec, 0);
      rec_set_heap_no_old(insert_rec, PAGE_HEAP_NO_USER_LOW + n_recs);
    }

    count++;
    n_recs++;

    if (UNIV_UNLIKELY(count == (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2)) {
      slot_index++;

      slot = page_dir_get_nth_slot(new_page, slot_index);

      page_dir_slot_set_rec(slot, insert_rec);
      page_dir_slot_set_n_owned(slot, NULL, count);

      count = 0;
    }

    rec_size = rec_offs_size(offsets);

    ut_ad(heap_top < new_page + UNIV_PAGE_SIZE);

    heap_top += rec_size;

    rec_offs_make_valid(insert_rec, index, offsets);
    page_cur_insert_rec_write_log(insert_rec, rec_size, prev_rec, index, mtr);
    prev_rec = insert_rec;
    rec = page_rec_get_next(rec);
  } while (!page_rec_is_supremum(rec));

  if ((slot_index > 0) && (count + 1 + (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2 <=
                           PAGE_DIR_SLOT_MAX_N_OWNED)) {
    /* We can merge the two last dir slots. This operation is
    here to make this function imitate exactly the equivalent
    task made using page_cur_insert_rec, which we use in database
    recovery to reproduce the task performed by this function.
    To be able to check the correctness of recovery, it is good
    that it imitates exactly. */

    count += (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;

    page_dir_slot_set_n_owned(slot, NULL, 0);

    slot_index--;
  }

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  log_data_len = mtr->get_log()->size() - log_data_len;

  ut_a(log_data_len < 100 * UNIV_PAGE_SIZE);

  if (log_ptr != NULL) {
    mach_write_to_4(log_ptr, log_data_len);
  }

  if (page_is_comp(new_page)) {
    rec_set_next_offs_new(insert_rec, PAGE_NEW_SUPREMUM);
  } else {
    rec_set_next_offs_old(insert_rec, PAGE_OLD_SUPREMUM);
  }

  slot = page_dir_get_nth_slot(new_page, 1 + slot_index);

  page_dir_slot_set_rec(slot, page_get_supremum_rec(new_page));
  page_dir_slot_set_n_owned(slot, NULL, count + 1);

  page_dir_set_n_slots(new_page, NULL, 2 + slot_index);
  page_header_set_ptr(new_page, NULL, PAGE_HEAP_TOP, heap_top);
  page_dir_set_n_heap(new_page, NULL, PAGE_HEAP_NO_USER_LOW + n_recs);
  page_header_set_field(new_page, NULL, PAGE_N_RECS, n_recs);

  page_header_set_ptr(new_page, NULL, PAGE_LAST_INSERT, NULL);

  page_header_set_field(new_page, NULL, PAGE_DIRECTION, PAGE_NO_DIRECTION);
  page_header_set_field(new_page, NULL, PAGE_N_DIRECTION, 0);

  /* Restore the log mode */

  mtr_set_log_mode(mtr, log_mode);
}

/** Writes log record of a record delete on a page. */
UNIV_INLINE
void page_cur_delete_rec_write_log(
    rec_t *rec,                /*!< in: record to be deleted */
    const dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)                /*!< in: mini-transaction handle */
{
  byte *log_ptr;

  ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));

  log_ptr = mlog_open_and_write_index(
      mtr, rec, index,
      page_rec_is_comp(rec) ? MLOG_COMP_REC_DELETE : MLOG_REC_DELETE, 2);

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
#define page_cur_delete_rec_write_log(rec, index, mtr) ((void)0)
#endif /* !UNIV_HOTBACKUP */

/** Parses log record of a record delete on a page.
 @return pointer to record end or NULL */
byte *page_cur_parse_delete_rec(
    byte *ptr,           /*!< in: buffer */
    byte *end_ptr,       /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mtr or NULL */
{
  ulint offset;
  page_cur_t cursor;

  if (end_ptr < ptr + 2) {
    return (NULL);
  }

  /* Read the cursor rec offset as a 2-byte ulint */
  offset = mach_read_from_2(ptr);
  ptr += 2;

  ut_a(offset <= UNIV_PAGE_SIZE);

  if (block) {
    page_t *page = buf_block_get_frame(block);
    mem_heap_t *heap = NULL;
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    rec_t *rec = page + offset;
    rec_offs_init(offsets_);

    page_cur_position(rec, block, &cursor);
#ifdef UNIV_HOTBACKUP
    ib::trace_1() << "page_cur_parse_delete_rec { page: " << page << ", "
                  << "offset: " << offset << ", rec: " << rec << "\n";
#endif /* UNIV_HOTBACKUP */
    ut_ad(!buf_block_get_page_zip(block) || page_is_comp(page));

    page_cur_delete_rec(
        &cursor, index,
        rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap), mtr);
    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  }

  return (ptr);
}

/** Deletes a record at the page cursor. The cursor is moved to the next
 record after the deleted one. */
void page_cur_delete_rec(
    page_cur_t *cursor,        /*!< in/out: a page cursor */
    const dict_index_t *index, /*!< in: record descriptor */
    const ulint *offsets,      /*!< in: rec_get_offsets(
                               cursor->rec, index) */
    mtr_t *mtr)                /*!< in: mini-transaction handle
                               or NULL */
{
  page_dir_slot_t *cur_dir_slot;
  page_dir_slot_t *prev_slot;
  page_t *page;
  page_zip_des_t *page_zip;
  rec_t *current_rec;
  rec_t *prev_rec = NULL;
  rec_t *next_rec;
  ulint cur_slot_no;
  ulint cur_n_owned;
  rec_t *rec;

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
  ut_ad(fil_page_index_page_check(page));
  ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID) == index->id ||
        (mtr ? mtr->is_inside_ibuf() : dict_index_is_ibuf(index)) ||
        recv_recovery_is_on());

  /* The record must not be the supremum or infimum record. */
  ut_ad(page_rec_is_user_rec(current_rec));

  if (page_get_n_recs(page) == 1 && !recv_recovery_is_on()) {
    /* Empty the page, unless we are applying the redo log
    during crash recovery. During normal operation, the
    page_create_empty() gets logged as one of MLOG_PAGE_CREATE,
    MLOG_COMP_PAGE_CREATE, MLOG_ZIP_PAGE_COMPRESS. */
    ut_ad(page_is_leaf(page));
    /* Usually, this should be the root page,
    and the whole index tree should become empty.
    However, this could also be a call in
    btr_cur_pessimistic_update() to delete the only
    record in the page and to insert another one. */
    page_cur_move_to_next(cursor);
    ut_ad(page_cur_is_after_last(cursor));
    page_create_empty(page_cur_get_block(cursor),
                      const_cast<dict_index_t *>(index), mtr);
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

  rec = (rec_t *)page_dir_slot_get_rec(prev_slot);

  /* rec now points to the record of the previous directory slot. Look
  for the immediate predecessor of current_rec in a loop. */

  while (current_rec != rec) {
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
#error "PAGE_DIR_SLOT_MIN_N_OWNED < 2"
#endif
  ut_ad(cur_n_owned > 1);

  if (current_rec == page_dir_slot_get_rec(cur_dir_slot)) {
    page_dir_slot_set_rec(cur_dir_slot, prev_rec);
  }

  /* 5. Update the number of owned records of the slot */

  page_dir_slot_set_n_owned(cur_dir_slot, page_zip, cur_n_owned - 1);

  /* 6. Free the memory occupied by the record */
  page_mem_free(page, page_zip, current_rec, index, offsets);

  /* 7. Now we have decremented the number of owned records of the slot.
  If the number drops below PAGE_DIR_SLOT_MIN_N_OWNED, we balance the
  slots. */

  if (cur_n_owned <= PAGE_DIR_SLOT_MIN_N_OWNED) {
    page_dir_balance_slot(page, page_zip, cur_slot_no);
  }

#ifdef UNIV_ZIP_DEBUG
  ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
}

#ifdef UNIV_COMPILE_TEST_FUNCS

/** Print the first n numbers, generated by page_cur_lcg_prng() to make sure
 (visually) that it works properly. */
void test_page_cur_lcg_prng(int n) /*!< in: print first n numbers */
{
  int i;
  unsigned long long rnd;

  for (i = 0; i < n; i++) {
    rnd = page_cur_lcg_prng();
    printf("%llu\t%%2=%llu %%3=%llu %%5=%llu %%7=%llu %%11=%llu\n", rnd,
           rnd % 2, rnd % 3, rnd % 5, rnd % 7, rnd % 11);
  }
}

#endif /* UNIV_COMPILE_TEST_FUNCS */
