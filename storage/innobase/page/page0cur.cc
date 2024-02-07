/*****************************************************************************

Copyright (c) 1994, 2024, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

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
 X[0] = number based on std::chrono::steady_clock::now()
 a = 1103515245 (3^5 * 5 * 7 * 129749)
 c = 12345 (3 * 5 * 823)
 m = 18446744073709551616 (2^64)

 @return number between 0 and 2^64-1 */
static uint64_t page_cur_lcg_prng(void) {
#define LCG_a 1103515245
#define LCG_c 12345
  static uint64_t lcg_current = 0;
  static bool initialized = false;

  if (!initialized) {
    lcg_current = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now() -
                      std::chrono::steady_clock::time_point{})
                      .count();
    initialized = true;
  }

  /* no need to "% 2^64" explicitly because lcg_current is
  64 bit and this will be done anyway */
  lcg_current = LCG_a * lcg_current + LCG_c;

  return (lcg_current);
}

/** Try a search shortcut based on the last insert.
@param[in]      block                   index page
@param[in]      index                   index tree
@param[in]      tuple                   search key
@param[in,out]  iup_matched_fields      already matched fields in the
upper limit record
@param[in,out]  ilow_matched_fields     already matched fields in the
lower limit record
@param[out]     cursor                  page cursor
@return true on success */
static inline bool page_cur_try_search_shortcut(
    const buf_block_t *block, const dict_index_t *index, const dtuple_t *tuple,
    ulint *iup_matched_fields, ulint *ilow_matched_fields, page_cur_t *cursor) {
  const rec_t *rec;
  const rec_t *next_rec;
  ulint low_match;
  ulint up_match;
  bool success = false;
  const page_t *page = buf_block_get_frame(block);
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(dtuple_check_typed(tuple));

  rec = page_header_get_ptr(page, PAGE_LAST_INSERT);
  offsets = rec_get_offsets(rec, index, offsets, dtuple_get_n_fields(tuple),
                            UT_LOCATION_HERE, &heap);

  ut_ad(rec);
  ut_ad(page_rec_is_user_rec(rec));

  low_match = up_match = std::min(*ilow_matched_fields, *iup_matched_fields);

  if (tuple->compare(rec, index, offsets, &low_match) < 0) {
    goto exit_func;
  }

  next_rec = page_rec_get_next_const(rec);
  if (!page_rec_is_supremum(next_rec)) {
    offsets =
        rec_get_offsets(next_rec, index, offsets, dtuple_get_n_fields(tuple),
                        UT_LOCATION_HERE, &heap);

    if (tuple->compare(next_rec, index, offsets, &up_match) >= 0) {
      goto exit_func;
    }

    *iup_matched_fields = up_match;
  }

  page_cur_position(rec, block, cursor);

  *ilow_matched_fields = low_match;

#ifdef UNIV_SEARCH_PERF_STAT
  page_cur_short_succ++;
#endif
  success = true;
exit_func:
  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }
  return (success);
}

/** Try a search shortcut based on the last insert.
@param[in]      block                   index page
@param[in]      index                   index tree
@param[in]      tuple                   search key
@param[in,out]  iup_matched_fields      already matched fields in the
upper limit record
@param[in,out]  iup_matched_bytes       already matched bytes in the
first partially matched field in the upper limit record
@param[in,out]  ilow_matched_fields     already matched fields in the
lower limit record
@param[in,out]  ilow_matched_bytes      already matched bytes in the
first partially matched field in the lower limit record
@param[out]     cursor                  page cursor
@return true on success */
static inline bool page_cur_try_search_shortcut_bytes(
    const buf_block_t *block, const dict_index_t *index, const dtuple_t *tuple,
    ulint *iup_matched_fields, ulint *iup_matched_bytes,
    ulint *ilow_matched_fields, ulint *ilow_matched_bytes, page_cur_t *cursor) {
  const rec_t *rec;
  const rec_t *next_rec;
  ulint low_match;
  ulint low_bytes;
  ulint up_match;
  ulint up_bytes;
  bool success = false;
  const page_t *page = buf_block_get_frame(block);
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(dtuple_check_typed(tuple));

  rec = page_header_get_ptr(page, PAGE_LAST_INSERT);
  offsets = rec_get_offsets(rec, index, offsets, dtuple_get_n_fields(tuple),
                            UT_LOCATION_HERE, &heap);

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
    offsets =
        rec_get_offsets(next_rec, index, offsets, dtuple_get_n_fields(tuple),
                        UT_LOCATION_HERE, &heap);

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
  success = true;
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
 @param[in]     tuple   data tuple
 @param[in]     rec     record
 @param[in]     offsets array returned by rec_get_offsets()
 @param[in]     n       compare nth field
 @param[in]     index   index where the record resides
 @return true if rec field extends tuple field */
static bool page_cur_rec_field_extends(const dtuple_t *tuple, const rec_t *rec,
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
      return true;
    }
  }

  return false;
}
#endif /* PAGE_CUR_LE_OR_EXTENDS */
#endif /* PAGE_CUR_ADAPT */

#ifndef UNIV_HOTBACKUP
/** Check if rec has at least one NULL value among the columns for which
rec_cache.offsets was cached, which means rec_cache.offsets should not be used
for this rec as it has a different layout of fields than the cached version.
@param[in]      rec     B-Tree record
@param[in]      index   The index from which the rec was taken
@return true iff the rec has at least one relevant column with NULL */
static bool page_cur_has_null(const rec_t *rec, const dict_index_t *index) {
  ut_ad(index->rec_cache.offsets);
  const auto nullable_cols = index->rec_cache.nullable_cols;
  ut_ad(nullable_cols <= index->n_nullable);
  if (!nullable_cols) {
    return false;
  }
  /* Check if this record has a NULL value. */
  const byte *nulls = rec - (1 + REC_N_NEW_EXTRA_BYTES);
  size_t n_bytes_to_scan = UT_BITS_IN_BYTES(nullable_cols);
  byte null_mask = 0xff;
  size_t bits_examined = 0;

  for (size_t i = 0; i < n_bytes_to_scan - 1; i++) {
    if (*nulls & null_mask) {
      return true;
    }
    --nulls;
    bits_examined += 8;
  }

  null_mask >>= (8 - (nullable_cols - bits_examined));
  return (*nulls & null_mask);
}

/** Searches the right position for a page cursor.
@param[in] block Buffer block
@param[in] index Record descriptor
@param[in] tuple Data tuple
@param[in] mode PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G, or PAGE_CUR_GE
@param[in,out] iup_matched_fields Already matched fields in upper limit record
@param[in,out] ilow_matched_fields Already matched fields in lower limit record
@param[out] cursor Page cursor
@param[in,out] rtr_info Rtree search stack */
void page_cur_search_with_match(const buf_block_t *block,
                                const dict_index_t *index,
                                const dtuple_t *tuple, page_cur_mode_t mode,
                                ulint *iup_matched_fields,

                                ulint *ilow_matched_fields,

                                page_cur_t *cursor, rtr_info_t *rtr_info) {
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
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
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

  const ulint *const cached_offsets{index->rec_cache.offsets};
#ifdef UNIV_DEBUG
  if (cached_offsets) {
    ut_ad(dict_table_is_comp(index->table));
    ut_ad(!dict_index_has_virtual(index));
    ut_ad(!index->table->has_instant_cols());
    ut_ad(!index->table->has_row_versions());
    ut_ad(!dict_index_is_spatial(index));
    const size_t n = dtuple_get_n_fields_cmp(tuple);
    const size_t searchable = dict_index_get_n_unique_in_tree(index);
    // assert there was no change in number of columns since caching
    ut_ad(searchable == index->rec_cache.offsets[1]);
    // assert that the number of nullable columns hasn't changed since caching
    {
      size_t nullable{0};
      for (size_t i = 0; i < searchable; i++) {
        if (!(index->get_field(i)->col->prtype & DATA_NOT_NULL)) {
          ++nullable;
        }
      }
      ut_ad(index->rec_cache.nullable_cols == nullable);
    }
    /* We never do a binary search on columns which are not part of internal
    nodes, nor on node_ptr field. If we ever start doing this, make sure to
    not use cached_offset offsets then, as it has only searchable columns. */
    ut_ad(n <= searchable);
  }
#endif
  auto get_mid_rec_offsets = [&]() -> const auto * {
    if (cached_offsets && !page_cur_has_null(mid_rec, index)) {
#ifdef UNIV_DEBUG
      {
        const size_t n = dtuple_get_n_fields_cmp(tuple);
        const auto *const offsets = rec_get_offsets(mid_rec, index, offsets_, n,
                                                    UT_LOCATION_HERE, &heap);
        ut_a(n <= offsets[0]);
        ut_a(n <= offsets[1]);
        ut_a(n <= cached_offsets[0]);
        ut_a(n <= cached_offsets[1]);
        for (size_t i = 0; i < n; ++i) {
          ulint len;
          ulint len2;

          const auto off = rec_get_nth_field(index, mid_rec, offsets, i, &len);
          const auto off2 =
              rec_get_nth_field(index, mid_rec, cached_offsets, i, &len2);
          ut_a(off == off2);
          ut_a(len == len2);
        }
      }
#endif
      return cached_offsets;
    }
    return rec_get_offsets(mid_rec, index, offsets_,
                           dtuple_get_n_fields_cmp(tuple), UT_LOCATION_HERE,
                           &heap);
  };

  /* Perform binary search until the lower and upper limit directory
  slots come to the distance 1 of each other */

  while (up - low > 1) {
    mid = (low + up) / 2;
    slot = page_dir_get_nth_slot(page, mid);
    mid_rec = page_dir_slot_get_rec(slot);

    cur_matched_fields = std::min(low_matched_fields, up_matched_fields);

    auto offsets = get_mid_rec_offsets();

    cmp = tuple->compare(mid_rec, index, offsets, &cur_matched_fields);

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

    auto offsets = get_mid_rec_offsets();

    cmp = tuple->compare(mid_rec, index, offsets, &cur_matched_fields);

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
@param[in]      block                   buffer block
@param[in]      index                   index tree
@param[in]      tuple                   key to be searched for
@param[in]      mode                    search mode
@param[in,out]  iup_matched_fields      already matched fields in the
upper limit record
@param[in,out]  iup_matched_bytes       already matched bytes in the
first partially matched field in the upper limit record
@param[in,out]  ilow_matched_fields     already matched fields in the
lower limit record
@param[in,out]  ilow_matched_bytes      already matched bytes in the
first partially matched field in the lower limit record
@param[out]     cursor                  page cursor */
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
  mem_heap_t *heap = nullptr;
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
                              dtuple_get_n_fields_cmp(tuple), UT_LOCATION_HERE,
                              &heap);

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
                              dtuple_get_n_fields_cmp(tuple), UT_LOCATION_HERE,
                              &heap);

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
    byte *log_ptr = nullptr;
    if (!mlog_open(mtr, 0, log_ptr)) {
      return;
    }
    mlog_close(mtr, log_ptr);
    log_ptr = nullptr;
  }

  ut_a(rec_size < UNIV_PAGE_SIZE);
  ut_ad(page_align(insert_rec) == page_align(cursor_rec));
  ut_ad(!page_rec_is_comp(insert_rec) == !dict_table_is_comp(index->table));

  {
    mem_heap_t *heap = nullptr;
    ulint cur_offs_[REC_OFFS_NORMAL_SIZE];
    ulint ins_offs_[REC_OFFS_NORMAL_SIZE];

    ulint *cur_offs;
    ulint *ins_offs;

    rec_offs_init(cur_offs_);
    rec_offs_init(ins_offs_);

    cur_offs = rec_get_offsets(cursor_rec, index, cur_offs_, ULINT_UNDEFINED,
                               UT_LOCATION_HERE, &heap);
    ins_offs = rec_get_offsets(insert_rec, index, ins_offs_, ULINT_UNDEFINED,
                               UT_LOCATION_HERE, &heap);

    extra_size = rec_offs_extra_size(ins_offs);
    ut_ad(rec_size == rec_offs_size(ins_offs));

    cur_rec_size = rec_offs_size(cur_offs);
    cur_extra_size = rec_offs_extra_size(cur_offs);

    if (heap != nullptr) {
      mem_heap_free(heap);
    }
  }

  ins_ptr = insert_rec - extra_size;

  i = 0;

  uint8_t cur_version = 0;
  uint8_t ins_version = 0;
  if (index->has_row_versions()) {
    const bool is_cmp = page_rec_is_comp(insert_rec);

    auto has_version = [is_cmp](rec_t *rec) {
      return (is_cmp ? rec_new_is_versioned(rec) : rec_old_is_versioned(rec));
    };

    if (has_version(cursor_rec)) {
      cur_version = is_cmp ? rec_get_instant_row_version_new(cursor_rec)
                           : rec_get_instant_row_version_old(cursor_rec);
    }

    /* New records always have the version except the case when records are
    being moved from one page to another (pessimistic, reorg) */
    if (has_version(insert_rec)) {
      ins_version = is_cmp ? rec_get_instant_row_version_new(insert_rec)
                           : rec_get_instant_row_version_old(insert_rec);
    }
  }

  /* If versions are different, then don't compare the records */
  if (cur_version != ins_version && cur_extra_size == extra_size) {
    ulint min_rec_size = std::min(cur_rec_size, rec_size);

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

  /* Length needed on REDO log :
   11 -> REDO_LOG_INITIAL_INFO_SIZE
   2  -> cursor rec offset
   5  -> record end segment length
   1  -> info bits
   5  -> record origin offset
   5  -> mismatch index */
  byte *log_ptr = nullptr;

  if (mtr_get_log_mode(mtr) != MTR_LOG_SHORT_INSERTS) {
    if (!mlog_open_and_write_index(mtr, insert_rec, index, MLOG_REC_INSERT,
                                   2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN,
                                   log_ptr)) {
      /* Logging in mtr is switched off during crash recovery: in that case
      mlog_open returns NULL */
      return;
    }

    log_end = &log_ptr[2 + 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN];
    /* Write the cursor rec offset as a 2-byte ulint */
    mach_write_to_2(log_ptr, page_offset(cursor_rec));
    log_ptr += 2;
  } else {
    if (!mlog_open(mtr, 5 + 1 + 5 + 5 + MLOG_BUF_MARGIN, log_ptr)) {
      /* Logging in mtr is switched off during crash
      recovery: in that case mlog_open returns NULL */
      return;
    }
    log_end = &log_ptr[5 + 1 + 5 + 5 + MLOG_BUF_MARGIN];
  }

  if (page_rec_is_comp(insert_rec)) {
    if (UNIV_UNLIKELY(rec_get_info_and_status_bits(insert_rec, true) !=
                      rec_get_info_and_status_bits(cursor_rec, true))) {
      goto need_extra_info;
    }
  } else {
    if (UNIV_UNLIKELY(rec_get_info_and_status_bits(insert_rec, false) !=
                      rec_get_info_and_status_bits(cursor_rec, false))) {
      goto need_extra_info;
    }
  }

  if (extra_size != cur_extra_size || rec_size != cur_rec_size ||
      cur_version != ins_version) {
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
const byte *page_cur_parse_insert_rec(
    bool is_short,       /*!< in: true if short inserts */
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
  rec_t *cursor_rec{nullptr};
  byte buf1[1024];
  byte *buf;
  const byte *ptr2 = ptr;
  ulint info_and_status_bits = 0; /* remove warning */
  page_cur_t cursor;
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  page = block ? buf_block_get_frame(block) : nullptr;

  if (is_short) {
    cursor_rec = page_rec_get_prev(page_get_supremum_rec(page));
  } else {
    ulint offset;

    /* Read the cursor rec offset as a 2-byte ulint */

    if (UNIV_UNLIKELY(end_ptr < ptr + 2)) {
      return (nullptr);
    }

    offset = mach_read_from_2(ptr);
    ptr += 2;

    if (page != nullptr) cursor_rec = page + offset;

    if (offset >= UNIV_PAGE_SIZE) {
      recv_sys->found_corrupt_log = true;

      return (nullptr);
    }
  }

  end_seg_len = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {
    return (nullptr);
  }

  if (end_seg_len >= UNIV_PAGE_SIZE << 1) {
    recv_sys->found_corrupt_log = true;

    return (nullptr);
  }

  if (end_seg_len & 0x1UL) {
    /* Read the info bits */

    if (end_ptr < ptr + 1) {
      return (nullptr);
    }

    info_and_status_bits = mach_read_from_1(ptr);
    ptr++;

    origin_offset = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == nullptr) {
      return (nullptr);
    }

    ut_a(origin_offset < UNIV_PAGE_SIZE);

    mismatch_index = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == nullptr) {
      return (nullptr);
    }

    ut_a(mismatch_index < UNIV_PAGE_SIZE);
  }

  if (end_ptr < ptr + (end_seg_len >> 1)) {
    return (nullptr);
  }

  if (!block) {
    return (const_cast<byte *>(ptr + (end_seg_len >> 1)));
  }

  ut_ad(page_is_comp(page) == dict_table_is_comp(index->table));
  ut_ad(!buf_block_get_page_zip(block) || page_is_comp(page));

  /* Read from the log the inserted index record end segment which
  differs from the cursor record */

  if ((end_seg_len & 0x1UL) && mismatch_index == 0) {
    /* This is a record has nothing common to cursor record. */
  } else {
    offsets = rec_get_offsets(cursor_rec, index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    if (!(end_seg_len & 0x1UL)) {
      info_and_status_bits =
          rec_get_info_and_status_bits(cursor_rec, page_is_comp(page));
      origin_offset = rec_offs_extra_size(offsets);
      mismatch_index = rec_offs_size(offsets) - (end_seg_len >> 1);
    }
  }

  end_seg_len >>= 1;

  if (mismatch_index + end_seg_len < sizeof buf1) {
    buf = buf1;
  } else {
    buf = static_cast<byte *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                                 mismatch_index + end_seg_len));
  }

  /* Build the inserted record to buf */

  if (UNIV_UNLIKELY(mismatch_index >= UNIV_PAGE_SIZE)) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_859)
        << "is_short " << is_short << ", "
        << "info_and_status_bits " << info_and_status_bits << ", offset "
        << page_offset(cursor_rec)
        << ","
           " o_offset "
        << origin_offset << ", mismatch index " << mismatch_index
        << ", end_seg_len " << end_seg_len << " parsed len " << (ptr - ptr2);
  }

  if (mismatch_index) {
    ut_memcpy(buf, rec_get_start(cursor_rec, offsets), mismatch_index);
  }
  ut_memcpy(buf + mismatch_index, ptr, end_seg_len);

  if (page_is_comp(page)) {
    rec_set_info_and_status_bits(buf + origin_offset, info_and_status_bits);
  } else {
    rec_set_info_bits_old(buf + origin_offset, info_and_status_bits);
  }

  page_cur_position(cursor_rec, block, &cursor);

  offsets = rec_get_offsets(buf + origin_offset, index, offsets,
                            ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);
  if (UNIV_UNLIKELY(!page_cur_rec_insert(&cursor, buf + origin_offset, index,
                                         offsets, mtr))) {
    /* The redo log record should only have been written
    after the write was successful. */
    ut_error;
  }

  if (buf != buf1) {
    ut::free(buf);
  }

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  return ptr + end_seg_len;
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
  ut_ad(dict_table_is_comp(index->table) == page_is_comp(page));
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
    mem_heap_t *heap = nullptr;

    rec_offs_init(foffsets_);

    foffsets = rec_get_offsets(free_rec, index, foffsets, ULINT_UNDEFINED,
                               UT_LOCATION_HERE, &heap);

    if (rec_offs_size(foffsets) < rec_size) {
      if (UNIV_LIKELY_NULL(heap)) {
        mem_heap_free(heap);
      }

      goto use_heap;
    }

    insert_buf = free_rec - rec_offs_extra_size(foffsets);

    if (page_is_comp(page)) {
      heap_no = rec_get_heap_no_new(free_rec);
      page_mem_alloc_free(page, nullptr, rec_get_next_ptr(free_rec, true),
                          rec_size);
    } else {
      heap_no = rec_get_heap_no_old(free_rec);
      page_mem_alloc_free(page, nullptr, rec_get_next_ptr(free_rec, false),
                          rec_size);
    }

    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  } else {
  use_heap:
    free_rec = nullptr;
    insert_buf = page_mem_alloc_heap(page, nullptr, rec_size, &heap_no);

    if (UNIV_UNLIKELY(insert_buf == nullptr)) {
      return (nullptr);
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

  page_header_set_field(page, nullptr, PAGE_N_RECS, 1 + page_get_n_recs(page));

  /* 5. Set the n_owned field in the inserted record to zero,
  and set the heap_no field */
  if (page_is_comp(page)) {
    rec_set_n_owned_new(insert_rec, nullptr, 0);
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
    if (UNIV_UNLIKELY(last_insert == nullptr)) {
      page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_NO_DIRECTION);
      page_header_set_field(page, nullptr, PAGE_N_DIRECTION, 0);

    } else if ((last_insert == current_rec) &&
               (page_header_get_field(page, PAGE_DIRECTION) != PAGE_LEFT)) {
      page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_RIGHT);
      page_header_set_field(page, nullptr, PAGE_N_DIRECTION,
                            page_header_get_field(page, PAGE_N_DIRECTION) + 1);

    } else if ((page_rec_get_next(insert_rec) == last_insert) &&
               (page_header_get_field(page, PAGE_DIRECTION) != PAGE_RIGHT)) {
      page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_LEFT);
      page_header_set_field(page, nullptr, PAGE_N_DIRECTION,
                            page_header_get_field(page, PAGE_N_DIRECTION) + 1);
    } else {
      page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_NO_DIRECTION);
      page_header_set_field(page, nullptr, PAGE_N_DIRECTION, 0);
    }
  }

  page_header_set_ptr(page, nullptr, PAGE_LAST_INSERT, insert_rec);

  /* 7. It remains to update the owner record. */
  {
    rec_t *owner_rec = page_rec_find_owner_rec(insert_rec);
    ulint n_owned;
    if (page_is_comp(page)) {
      n_owned = rec_get_n_owned_new(owner_rec);
      rec_set_n_owned_new(owner_rec, nullptr, n_owned + 1);
    } else {
      n_owned = rec_get_n_owned_old(owner_rec);
      rec_set_n_owned_old(owner_rec, n_owned + 1);
    }

    /* 8. Now we have incremented the n_owned field of the owner
    record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
    we have to split the corresponding directory slot in two. */

    if (UNIV_UNLIKELY(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED)) {
      page_dir_split_slot(page, nullptr, page_dir_find_owner_slot(owner_rec));
    }
  }

  /* 9. Write log record of the insert */
  if (UNIV_LIKELY(mtr != nullptr)) {
    page_cur_insert_rec_write_log(insert_rec, rec_size, current_rec, index,
                                  mtr);
  }

  return (insert_rec);
}

rec_t *page_cur_direct_insert_rec_low(rec_t *current_rec, dict_index_t *index,
                                      const dtuple_t *tuple, mtr_t *mtr,
                                      ulint rec_size) {
  byte *insert_buf;
  page_t *page;       /*!< the relevant page */
  rec_t *last_insert; /*!< cursor position at previous
                      insert */
  rec_t *free_rec;    /*!< a free record that was reused,
                      or NULL */
  rec_t *insert_rec;  /*!< inserted record */
  ulint heap_no;      /*!< heap number of the inserted
                      record */

  page = page_align(current_rec);

  ut_ad(dict_table_is_comp(index->table) == page_is_comp(page));

  ut_ad(fil_page_index_page_check(page));

  ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID) == index->id);

  ut_ad(!page_rec_is_supremum(current_rec));

  /* Try to find suitable space from page memory management */
  free_rec = page_header_get_ptr(page, PAGE_FREE);
  if (free_rec) {
    /* Try to allocate from the head of the free list. */
    ulint foffsets_[REC_OFFS_NORMAL_SIZE];
    ulint *foffsets = foffsets_;
    mem_heap_t *heap = nullptr;

    rec_offs_init(foffsets_);

    foffsets = rec_get_offsets(free_rec, index, foffsets, ULINT_UNDEFINED,
                               UT_LOCATION_HERE, &heap);
    if (rec_offs_size(foffsets) < rec_size) {
      if (heap != nullptr) {
        mem_heap_free(heap);
        heap = nullptr;
      }

      free_rec = nullptr;
      insert_buf = page_mem_alloc_heap(page, nullptr, rec_size, &heap_no);

      if (insert_buf == nullptr) {
        return (nullptr);
      }
    } else {
      insert_buf = free_rec - rec_offs_extra_size(foffsets);

      if (page_is_comp(page)) {
        heap_no = rec_get_heap_no_new(free_rec);
        page_mem_alloc_free(page, nullptr, rec_get_next_ptr(free_rec, true),
                            rec_size);
      } else {
        heap_no = rec_get_heap_no_old(free_rec);
        page_mem_alloc_free(page, nullptr, rec_get_next_ptr(free_rec, false),
                            rec_size);
      }

      if (heap != nullptr) {
        mem_heap_free(heap);
        heap = nullptr;
      }
    }
  } else {
    free_rec = nullptr;
    insert_buf = page_mem_alloc_heap(page, nullptr, rec_size, &heap_no);

    if (insert_buf == nullptr) {
      return (nullptr);
    }
  }

  /* Create the record */
  insert_rec = rec_convert_dtuple_to_rec(insert_buf, index, tuple);

  /* Insert the record in the linked list of records */
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

  page_header_set_field(page, nullptr, PAGE_N_RECS, 1 + page_get_n_recs(page));

  /* Set the n_owned field in the inserted record to zero, and set the heap_no
  field */
  if (page_is_comp(page)) {
    rec_set_n_owned_new(insert_rec, nullptr, 0);
    rec_set_heap_no_new(insert_rec, heap_no);
  } else {
    rec_set_n_owned_old(insert_rec, 0);
    rec_set_heap_no_old(insert_rec, heap_no);
  }

  /* Update the last insertion info in page header */

  last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
  ut_ad(!last_insert || !page_is_comp(page) ||
        rec_get_node_ptr_flag(last_insert) ==
            rec_get_node_ptr_flag(insert_rec));

  if (last_insert == nullptr) {
    page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_NO_DIRECTION);
    page_header_set_field(page, nullptr, PAGE_N_DIRECTION, 0);

  } else if ((last_insert == current_rec) &&
             (page_header_get_field(page, PAGE_DIRECTION) != PAGE_LEFT)) {
    page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_RIGHT);
    page_header_set_field(page, nullptr, PAGE_N_DIRECTION,
                          page_header_get_field(page, PAGE_N_DIRECTION) + 1);

  } else if ((page_rec_get_next(insert_rec) == last_insert) &&
             (page_header_get_field(page, PAGE_DIRECTION) != PAGE_RIGHT)) {
    page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_LEFT);
    page_header_set_field(page, nullptr, PAGE_N_DIRECTION,
                          page_header_get_field(page, PAGE_N_DIRECTION) + 1);
  } else {
    page_header_set_field(page, nullptr, PAGE_DIRECTION, PAGE_NO_DIRECTION);
    page_header_set_field(page, nullptr, PAGE_N_DIRECTION, 0);
  }

  page_header_set_ptr(page, nullptr, PAGE_LAST_INSERT, insert_rec);

  /* It remains to update the owner record. */
  {
    rec_t *owner_rec = page_rec_find_owner_rec(insert_rec);
    ulint n_owned;
    if (page_is_comp(page)) {
      n_owned = rec_get_n_owned_new(owner_rec);
      rec_set_n_owned_new(owner_rec, nullptr, n_owned + 1);
    } else {
      n_owned = rec_get_n_owned_old(owner_rec);
      rec_set_n_owned_old(owner_rec, n_owned + 1);
    }

    /* Now we have incremented the n_owned field of the owner
    record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
    we have to split the corresponding directory slot in two. */

    if (n_owned == PAGE_DIR_SLOT_MAX_N_OWNED) {
      page_dir_split_slot(page, nullptr, page_dir_find_owner_slot(owner_rec));
    }
  }

  /* Open the mtr for name sake to set the modification flag
  to true failing which no flush would be done. */
  byte *log_ptr = nullptr;
  if (mlog_open(mtr, 0, log_ptr)) {
    ut_d(ut_error);
    ut_o(mlog_close(mtr, log_ptr));
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
      return (nullptr);
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
      return (nullptr);
    }

    /* Try compressing the whole page afterwards. */
    insert_rec =
        page_cur_insert_rec_low(cursor->rec, index, rec, offsets, nullptr);

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
    if (insert_rec == nullptr) {
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
        if (page_zip_compress(page_zip, page, index, level, nullptr)) {
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

          insert_rec = page + rec_get_next_offs(cursor->rec, true);
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
      if (!page_zip_decompress(page_zip, page, false)) {
        ut_error; /* Memory corrupted? */
      }
      ut_ad(page_validate(page, index));
      insert_rec = nullptr;
    }

    return (insert_rec);
  }

  free_rec = page_header_get_ptr(page, PAGE_FREE);
  if (UNIV_LIKELY_NULL(free_rec)) {
    /* Try to allocate from the head of the free list. */
    lint extra_size_diff;
    ulint foffsets_[REC_OFFS_NORMAL_SIZE];
    ulint *foffsets = foffsets_;
    mem_heap_t *heap = nullptr;

    rec_offs_init(foffsets_);

    foffsets = rec_get_offsets(free_rec, index, foffsets, ULINT_UNDEFINED,
                               UT_LOCATION_HERE, &heap);
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
    page_mem_alloc_free(page, page_zip, rec_get_next_ptr(free_rec, true),
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

      trx_id_offs = rec_get_nth_field_offs(index, foffsets, trx_id_col, &len);
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
            rec_get_nth_field(index, free_rec, foffsets, trx_id_col + 1, &len));
      ut_ad(len == DATA_ROLL_PTR_LEN);
    }

    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  } else {
  use_heap:
    free_rec = nullptr;
    insert_buf = page_mem_alloc_heap(page, page_zip, rec_size, &heap_no);

    if (UNIV_UNLIKELY(insert_buf == nullptr)) {
      return (nullptr);
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
    const rec_t *next_rec = page_rec_get_next_low(cursor->rec, true);
    ut_ad(rec_get_status(cursor->rec) <= REC_STATUS_INFIMUM);
    ut_ad(rec_get_status(insert_rec) < REC_STATUS_INFIMUM);
    ut_ad(rec_get_status(next_rec) != REC_STATUS_INFIMUM);

    page_rec_set_next(insert_rec, next_rec);
    page_rec_set_next(cursor->rec, insert_rec);
  }

  page_header_set_field(page, page_zip, PAGE_N_RECS, 1 + page_get_n_recs(page));

  /* 5. Set the n_owned field in the inserted record to zero,
  and set the heap_no field */
  rec_set_n_owned_new(insert_rec, nullptr, 0);
  rec_set_heap_no_new(insert_rec, heap_no);

  UNIV_MEM_ASSERT_RW(rec_get_start(insert_rec, offsets),
                     rec_offs_size(offsets));

  page_zip_dir_insert(page_zip, cursor->rec, free_rec, insert_rec);

  /* 6. Update the last insertion info in page header */

  last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
  ut_ad(!last_insert || rec_get_node_ptr_flag(last_insert) ==
                            rec_get_node_ptr_flag(insert_rec));

  if (!dict_index_is_spatial(index)) {
    if (UNIV_UNLIKELY(last_insert == nullptr)) {
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
  if (UNIV_LIKELY(mtr != nullptr)) {
    page_cur_insert_rec_write_log(insert_rec, rec_size, cursor->rec, index,
                                  mtr);
  }

  return (insert_rec);
}

#ifndef UNIV_HOTBACKUP
/** Writes a log record of copying a record list end to a new created page.
@param[in,out]  page    Index page
@param[in,out]  index   Record descriptor
@param[in,out]  mtr     Mini-transaction
@param[out]     log_ptr 4-byte field where to write the log data length
@retval true if mtr log is opened successfully.
@retval false if mtr log is not opened. One case is when redo is disabled. */
static inline bool page_copy_rec_list_to_created_page_write_log(
    page_t *page, dict_index_t *index, mtr_t *mtr, byte *&log_ptr) {
  ut_ad(page_is_comp(page) == dict_table_is_comp(index->table));

  const bool opened = mlog_open_and_write_index(
      mtr, page, index, MLOG_LIST_END_COPY_CREATED, 4, log_ptr);

  if (opened) {
    mlog_close(mtr, log_ptr + 4);
  }

  return (opened);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses a log record of copying a record list end to a new created page.
 @return end of log record or NULL */
const byte *page_parse_copy_rec_list_to_created_page(
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mtr or NULL */
{
  const byte *rec_end;
  ulint log_data_len;
  page_t *page;
  page_zip_des_t *page_zip;

  if (ptr + 4 > end_ptr) {
    return (nullptr);
  }

  log_data_len = mach_read_from_4(ptr);
  ptr += 4;

  rec_end = ptr + log_data_len;

  if (rec_end > end_ptr) {
    return (nullptr);
  }

  if (!block) {
    return (rec_end);
  }

  while (ptr < rec_end) {
    ptr = page_cur_parse_insert_rec(true, ptr, end_ptr, block, index, mtr);
  }

  ut_a(ptr == rec_end);

  page = buf_block_get_frame(block);
  page_zip = buf_block_get_page_zip(block);

  page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, nullptr);

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
  page_dir_slot_t *slot = nullptr; /* remove warning */
  byte *heap_top;
  rec_t *insert_rec = nullptr; /* remove warning */
  rec_t *prev_rec;
  ulint count;
  ulint n_recs;
  ulint slot_index;
  ulint rec_size;
  byte *log_ptr = nullptr;
  ulint log_data_len;
  mem_heap_t *heap = nullptr;
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
  page_dir_set_n_slots(new_page, nullptr, UNIV_PAGE_SIZE / 2);
  page_header_set_ptr(new_page, nullptr, PAGE_HEAP_TOP,
                      new_page + UNIV_PAGE_SIZE - 1);
#endif

  bool opened = page_copy_rec_list_to_created_page_write_log(new_page, index,
                                                             mtr, log_ptr);

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
    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);
    insert_rec = rec_copy(heap_top, rec, offsets);

    if (page_is_comp(new_page)) {
      rec_set_next_offs_new(prev_rec, page_offset(insert_rec));

      rec_set_n_owned_new(insert_rec, nullptr, 0);
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
      page_dir_slot_set_n_owned(slot, nullptr, count);

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

    page_dir_slot_set_n_owned(slot, nullptr, 0);

    slot_index--;
  }

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }

  log_data_len = mtr->get_log()->size() - log_data_len;

  ut_a(log_data_len < 100 * UNIV_PAGE_SIZE);

  if (opened) {
    mach_write_to_4(log_ptr, log_data_len);
  }

  if (page_is_comp(new_page)) {
    rec_set_next_offs_new(insert_rec, PAGE_NEW_SUPREMUM);
  } else {
    rec_set_next_offs_old(insert_rec, PAGE_OLD_SUPREMUM);
  }

  slot = page_dir_get_nth_slot(new_page, 1 + slot_index);

  page_dir_slot_set_rec(slot, page_get_supremum_rec(new_page));
  page_dir_slot_set_n_owned(slot, nullptr, count + 1);

  page_dir_set_n_slots(new_page, nullptr, 2 + slot_index);
  page_header_set_ptr(new_page, nullptr, PAGE_HEAP_TOP, heap_top);
  page_dir_set_n_heap(new_page, nullptr, PAGE_HEAP_NO_USER_LOW + n_recs);
  page_header_set_field(new_page, nullptr, PAGE_N_RECS, n_recs);

  page_header_set_ptr(new_page, nullptr, PAGE_LAST_INSERT, nullptr);

  page_header_set_field(new_page, nullptr, PAGE_DIRECTION, PAGE_NO_DIRECTION);
  page_header_set_field(new_page, nullptr, PAGE_N_DIRECTION, 0);

  /* Restore the log mode */

  mtr_set_log_mode(mtr, log_mode);
}

/** Writes log record of a record delete on a page. */
static inline void page_cur_delete_rec_write_log(
    rec_t *rec,                /*!< in: record to be deleted */
    const dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)                /*!< in: mini-transaction handle */
{
  byte *log_ptr = nullptr;
  ut_ad(page_rec_is_comp(rec) == dict_table_is_comp(index->table));

  if (!mlog_open_and_write_index(mtr, rec, index, MLOG_REC_DELETE, 2,
                                 log_ptr)) {
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
const byte *page_cur_parse_delete_rec(
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    buf_block_t *block,  /*!< in: page or NULL */
    dict_index_t *index, /*!< in: record descriptor */
    mtr_t *mtr)          /*!< in: mtr or NULL */
{
  ulint offset;
  page_cur_t cursor;

  if (end_ptr < ptr + 2) {
    return (nullptr);
  }

  /* Read the cursor rec offset as a 2-byte ulint */
  offset = mach_read_from_2(ptr);
  ptr += 2;

  ut_a(offset <= UNIV_PAGE_SIZE);

  if (block) {
    page_t *page = buf_block_get_frame(block);
    mem_heap_t *heap = nullptr;
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    rec_t *rec = page + offset;
    rec_offs_init(offsets_);

    page_cur_position(rec, block, &cursor);
#ifdef UNIV_HOTBACKUP
    ib::trace_1() << "page_cur_parse_delete_rec: offset " << offset;
#endif /* UNIV_HOTBACKUP */
    ut_ad(!buf_block_get_page_zip(block) || page_is_comp(page));

    page_cur_delete_rec(&cursor, index,
                        rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED,
                                        UT_LOCATION_HERE, &heap),
                        mtr);
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
  rec_t *prev_rec = nullptr;
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
  ut_ad(page_is_comp(page) == dict_table_is_comp(index->table));
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
  if (mtr != nullptr) {
    page_cur_delete_rec_write_log(current_rec, index, mtr);
  }

  /* 1. Reset the last insert info in the page header and increment
  the modify clock for the frame */

  page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, nullptr);

  /* The page gets invalid for optimistic searches: increment the
  frame modify clock only if there is an mini-transaction covering
  the change. During IMPORT we allocate local blocks that are not
  part of the buffer pool. */

  if (mtr != nullptr) {
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

  static_assert(PAGE_DIR_SLOT_MIN_N_OWNED >= 2,
                "PAGE_DIR_SLOT_MIN_N_OWNED < 2");
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

#ifndef UNIV_HOTBACKUP
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
#endif /* !UNIV_HOTBACKUP */
