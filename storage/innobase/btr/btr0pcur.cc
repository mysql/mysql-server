/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file btr/btr0pcur.cc
 The index tree persistent cursor

 Created 2/23/1996 Heikki Tuuri
 *******************************************************/

#include "btr0pcur.h"

#include <stddef.h>

#include "rem0cmp.h"
#include "trx0trx.h"
#include "ut0byte.h"

void btr_pcur_t::store_position(mtr_t *mtr) {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  auto block = get_block();
  auto index = btr_cur_get_index(get_btr_cur());

  auto page_cursor = get_page_cur();

  auto rec = page_cur_get_rec(page_cursor);
  auto page = page_align(rec);
  auto offs = page_offset(rec);

#ifdef UNIV_DEBUG
  if (dict_index_is_spatial(index)) {
    /* For spatial index, when we do positioning on parent
    buffer if necessary, it might not hold latches, but the
    tree must be locked to prevent change on the page */
    ut_ad((mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
                                     MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK) ||
           mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_S_FIX) ||
           mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX)) &&
          (block->page.buf_fix_count > 0));
  } else {
    ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_S_FIX) ||
          mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX) ||
          index->table->is_intrinsic());
  }
#endif /* UNIV_DEBUG */

  if (page_is_empty(page)) {
    /* It must be an empty index tree; NOTE that in this case
    we do not store the modify_clock, but always do a search
    if we restore the cursor position */

    ut_a(btr_page_get_next(page, mtr) == FIL_NULL);
    ut_a(btr_page_get_prev(page, mtr) == FIL_NULL);
    ut_ad(page_is_leaf(page));
    ut_ad(page_get_page_no(page) == index->page);

    m_old_stored = true;

    if (page_rec_is_supremum_low(offs)) {
      m_rel_pos = BTR_PCUR_AFTER_LAST_IN_TREE;
    } else {
      m_rel_pos = BTR_PCUR_BEFORE_FIRST_IN_TREE;
    }

    return;
  }

  if (page_rec_is_supremum_low(offs)) {
    rec = page_rec_get_prev(rec);

    m_rel_pos = BTR_PCUR_AFTER;

  } else if (page_rec_is_infimum_low(offs)) {
    rec = page_rec_get_next(rec);

    m_rel_pos = BTR_PCUR_BEFORE;
  } else {
    m_rel_pos = BTR_PCUR_ON;
  }

  m_old_stored = true;

  m_old_rec = dict_index_copy_rec_order_prefix(index, rec, &m_old_n_fields,
                                               &m_old_rec_buf, &m_buf_size);

  m_block_when_stored = block;

  /* Function try to check if block is S/X latch. */
  m_modify_clock = buf_block_get_modify_clock(block);
  m_withdraw_clock = buf_withdraw_clock;
}

void btr_pcur_t::copy_stored_position(btr_pcur_t *dst, const btr_pcur_t *src) {
  ut_free(dst->m_old_rec_buf);

  dst->m_old_rec_buf = nullptr;

  memcpy(dst, src, sizeof(*dst));

  if (src->m_old_rec_buf != nullptr) {
    dst->m_old_rec_buf = static_cast<byte *>(ut_malloc_nokey(src->m_buf_size));

    memcpy(dst->m_old_rec_buf, src->m_old_rec_buf, src->m_buf_size);

    dst->m_old_rec = dst->m_old_rec_buf + (src->m_old_rec - src->m_old_rec_buf);
  }

  dst->m_old_n_fields = src->m_old_n_fields;
}

bool btr_pcur_t::restore_position(ulint latch_mode, mtr_t *mtr,
                                  const char *file, ulint line) {
  dtuple_t *tuple;
  page_cur_mode_t mode;

  ut_ad(mtr->is_active());
  ut_ad(m_old_stored);
  ut_ad(is_positioned());

  auto index = btr_cur_get_index(get_btr_cur());

  if (m_rel_pos == BTR_PCUR_AFTER_LAST_IN_TREE ||
      m_rel_pos == BTR_PCUR_BEFORE_FIRST_IN_TREE) {
    /* In these cases we do not try an optimistic restoration,
    but always do a search */

    btr_cur_open_at_index_side(m_rel_pos == BTR_PCUR_BEFORE_FIRST_IN_TREE,
                               index, latch_mode, get_btr_cur(), 0, mtr);

    m_latch_mode = BTR_LATCH_MODE_WITHOUT_INTENTION(latch_mode);

    m_pos_state = BTR_PCUR_IS_POSITIONED;

    m_block_when_stored = get_block();

    return (false);
  }

  ut_a(m_old_rec != nullptr);
  ut_a(m_old_n_fields > 0);

  /* Optimistic latching involves S/X latch not required for
  intrinsic table instead we would prefer to search fresh. */
  if ((latch_mode == BTR_SEARCH_LEAF || latch_mode == BTR_MODIFY_LEAF ||
       latch_mode == BTR_SEARCH_PREV || latch_mode == BTR_MODIFY_PREV) &&
      !m_btr_cur.index->table->is_intrinsic()) {
    /* Try optimistic restoration. */

    if (!buf_pool_is_obsolete(m_withdraw_clock) &&
        btr_cur_optimistic_latch_leaves(m_block_when_stored, m_modify_clock,
                                        &latch_mode, &m_btr_cur, file, line,
                                        mtr)) {
      m_pos_state = BTR_PCUR_IS_POSITIONED;

      m_latch_mode = latch_mode;

      buf_block_dbg_add_level(get_block(), dict_index_is_ibuf(index)
                                               ? SYNC_IBUF_TREE_NODE
                                               : SYNC_TREE_NODE);

      if (m_rel_pos == BTR_PCUR_ON) {
#ifdef UNIV_DEBUG
        const rec_t *rec;
        const ulint *offsets1;
        const ulint *offsets2;

        rec = get_rec();

        auto heap = mem_heap_create(256);

        offsets1 =
            rec_get_offsets(m_old_rec, index, nullptr, m_old_n_fields, &heap);

        offsets2 = rec_get_offsets(rec, index, nullptr, m_old_n_fields, &heap);

        ut_ad(!cmp_rec_rec(m_old_rec, rec, offsets1, offsets2, index));
        mem_heap_free(heap);
#endif /* UNIV_DEBUG */
        return (true);
      }

      /* This is the same record as stored,
      may need to be adjusted for BTR_PCUR_BEFORE/AFTER,
      depending on search mode and direction. */
      if (is_on_user_rec()) {
        m_pos_state = BTR_PCUR_IS_POSITIONED_OPTIMISTIC;
      }
      return (false);
    }
  }

  /* If optimistic restoration did not succeed, open the cursor anew */

  auto heap = mem_heap_create(256);

  tuple = dict_index_build_data_tuple(index, m_old_rec, m_old_n_fields, heap);

  /* Save the old search mode of the cursor */
  auto old_mode = m_search_mode;

  switch (m_rel_pos) {
    case BTR_PCUR_ON:
      mode = PAGE_CUR_LE;
      break;
    case BTR_PCUR_AFTER:
      mode = PAGE_CUR_G;
      break;
    case BTR_PCUR_BEFORE:
      mode = PAGE_CUR_L;
      break;
    default:
      ut_error;
  }

  open_no_init(index, tuple, mode, latch_mode, 0, mtr, file, line);

  /* Restore the old search mode */
  m_search_mode = old_mode;

  ut_ad(m_rel_pos == BTR_PCUR_ON || m_rel_pos == BTR_PCUR_BEFORE ||
        m_rel_pos == BTR_PCUR_AFTER);

  if (m_rel_pos == BTR_PCUR_ON && is_on_user_rec() &&
      !cmp_dtuple_rec(
          tuple, get_rec(), index,
          rec_get_offsets(get_rec(), index, nullptr, ULINT_UNDEFINED, &heap))) {
    /* We have to store the NEW value for the modify clock,
    since the cursor can now be on a different page!
    But we can retain the value of old_rec */

    m_block_when_stored = get_block();

    m_modify_clock = buf_block_get_modify_clock(m_block_when_stored);

    m_old_stored = true;

    m_withdraw_clock = buf_withdraw_clock;

    mem_heap_free(heap);

    return (true);
  }

  mem_heap_free(heap);

  /* We have to store new position information, modify_clock etc.,
  to the cursor because it can now be on a different page, the record
  under it may have been removed, etc. */

  store_position(mtr);

  return (false);
}

void btr_pcur_t::move_to_next_page(mtr_t *mtr) {
  dict_table_t *table = get_btr_cur()->index->table;

  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);
  ut_ad(is_after_last_on_page());

  m_old_stored = false;

  auto page = get_page();
  auto next_page_no = btr_page_get_next(page, mtr);

  ut_ad(next_page_no != FIL_NULL);

  auto mode = m_latch_mode;

  switch (mode) {
    case BTR_SEARCH_TREE:
    case BTR_PARALLEL_READ_INIT:
      mode = BTR_SEARCH_LEAF;
      break;
    case BTR_MODIFY_TREE:
      mode = BTR_MODIFY_LEAF;
  }

  /* For intrinsic tables we avoid taking any latches as table is
  accessed by only one thread at any given time. */
  if (table->is_intrinsic()) {
    mode = BTR_NO_LATCHES;
  }

  auto block = get_block();

  auto next_block =
      btr_block_get(page_id_t(block->page.id.space(), next_page_no),
                    block->page.size, mode, get_btr_cur()->index, mtr);

  auto next_page = buf_block_get_frame(next_block);

#ifdef UNIV_BTR_DEBUG
  ut_a(page_is_comp(next_page) == page_is_comp(page));
  ut_a(btr_page_get_prev(next_page, mtr) == get_block()->page.id.page_no());
#endif /* UNIV_BTR_DEBUG */

  btr_leaf_page_release(get_block(), mode, mtr);

  page_cur_set_before_first(next_block, get_page_cur());

  ut_d(page_check_dir(next_page));
}

void btr_pcur_t::move_backward_from_page(mtr_t *mtr) {
  ut_ad(m_latch_mode != BTR_NO_LATCHES);
  ut_ad(is_before_first_on_page());
  ut_ad(!is_before_first_in_tree(mtr));

  ulint latch_mode2;
  auto old_latch_mode = m_latch_mode;

  if (m_latch_mode == BTR_SEARCH_LEAF) {
    latch_mode2 = BTR_SEARCH_PREV;

  } else if (m_latch_mode == BTR_MODIFY_LEAF) {
    latch_mode2 = BTR_MODIFY_PREV;
  } else {
    latch_mode2 = 0; /* To eliminate compiler warning */
    ut_error;
  }

  store_position(mtr);

  mtr_commit(mtr);

  mtr_start(mtr);

  restore_position(latch_mode2, mtr, __FILE__, __LINE__);

  auto page = get_page();
  auto prev_page_no = btr_page_get_prev(page, mtr);

  /* For intrinsic table we don't do optimistic restore and so there is
  no left block that is pinned that needs to be released. */
  if (!btr_cur_get_index(get_btr_cur())->table->is_intrinsic()) {
    buf_block_t *prev_block;

    if (prev_page_no == FIL_NULL) {
      ;
    } else if (is_before_first_on_page()) {
      prev_block = get_btr_cur()->left_block;

      btr_leaf_page_release(get_block(), old_latch_mode, mtr);

      page_cur_set_after_last(prev_block, get_page_cur());
    } else {
      /* The repositioned cursor did not end on an infimum
      record on a page. Cursor repositioning acquired a latch
      also on the previous page, but we do not need the latch:
      release it. */

      prev_block = get_btr_cur()->left_block;

      btr_leaf_page_release(prev_block, old_latch_mode, mtr);
    }
  }

  m_latch_mode = old_latch_mode;
  m_old_stored = false;
}

bool btr_pcur_t::move_to_prev(mtr_t *mtr) {
  ut_ad(m_pos_state == BTR_PCUR_IS_POSITIONED);
  ut_ad(m_latch_mode != BTR_NO_LATCHES);

  m_old_stored = false;

  if (is_before_first_on_page()) {
    if (is_before_first_in_tree(mtr)) {
      return (false);
    }

    move_backward_from_page(mtr);

    return (true);
  }

  move_to_prev_on_page();

  return (true);
}

void btr_pcur_t::open_on_user_rec(dict_index_t *index, const dtuple_t *tuple,
                                  page_cur_mode_t mode, ulint latch_mode,
                                  mtr_t *mtr, const char *file, ulint line) {
  open(index, 0, tuple, mode, latch_mode, mtr, file, line);

  if (mode == PAGE_CUR_GE || mode == PAGE_CUR_G) {
    if (is_after_last_on_page()) {
      move_to_next_user_rec(mtr);
    }
  } else {
    ut_ad(mode == PAGE_CUR_LE || mode == PAGE_CUR_L);

    /* Not implemented yet */

    ut_error;
  }
}
