/*****************************************************************************

Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

/** @file ddl/ddl0rtree.cc
 DDL cluster index scan implementation.
 Created 2020-11-01 by Sunny Bains. */

#include "btr0load.h"
#include "ddl0impl-cursor.h"
#include "ddl0impl-rtree.h"
#include "log0chkp.h"
#include "row0vers.h"

namespace ddl {

RTree_inserter::RTree_inserter(Context &ctx, dict_index_t *index) noexcept
    : m_dtuples(ut::new_withkey<Tuples>(ut::make_psi_memory_key(mem_key_ddl))),
      m_index(index),
      m_ctx(ctx) {
  m_dml_heap = mem_heap_create(512, UT_LOCATION_HERE);
  m_dtuple_heap = mem_heap_create(512, UT_LOCATION_HERE);
}

RTree_inserter::~RTree_inserter() noexcept {
  if (m_dtuples != nullptr) {
    ut::delete_(m_dtuples);
  }
  if (m_dtuple_heap != nullptr) {
    mem_heap_free(m_dtuple_heap);
  }
  if (m_dml_heap != nullptr) {
    mem_heap_free(m_dml_heap);
  }
}

void RTree_inserter::deep_copy_tuples(Tuples::iterator it) noexcept {
  /* Since the data of the tuple pk fields are pointers to cluster index rows.
  After mtr commit, these pointers could point to invalid data. Therefore,
  we need to do a deep copy of the data from scanned index buffer.  */
  for (; it != m_dtuples->end(); ++it) {
    auto dtuple = *it;

    /* The first field is the spatial MBR field, which is the key for
    spatial indexes.  So skip it. */
    for (size_t i = 1; i < dtuple_get_n_fields(dtuple); ++i) {
      dfield_dup(&dtuple->fields[i], m_dtuple_heap);
    }
  }
}

void RTree_inserter::add_to_batch(const dtuple_t *row,
                                  const row_ext_t *ext) noexcept {
  auto dtuple = row_build_index_entry(row, ext, m_index, m_dtuple_heap);
  ut_a(dtuple != nullptr);
  m_dtuples->push_back(dtuple);
}

dberr_t RTree_inserter::batch_insert(trx_id_t trx_id,
                                     Latch_release &&latch_release) noexcept {
  rec_t *rec{};
  btr_cur_t cursor;
  ulint *offsets{};
  rtr_info_t rtr_info;
  big_rec_t *big_rec{};
  bool latches_released{};
  dberr_t err{DB_SUCCESS};
  IF_DEBUG(bool force_log_free_check{};)

  static constexpr ulint flag = BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG |
                                BTR_KEEP_SYS_FLAG | BTR_CREATE_FLAG;

  ut_a(dict_index_is_spatial(m_index));

  IF_ENABLED("ddl_instrument_log_check_flush", force_log_free_check = true;)

  mtr_t mtr;

  cursor.index = m_index;

  for (auto it = m_dtuples->begin(); it != m_dtuples->end(); ++it) {
    auto dtuple = *it;

    ut_ad(dtuple != nullptr);

    if (log_free_check_is_required() IF_DEBUG(|| force_log_free_check)) {
      if (!latches_released) {
        deep_copy_tuples(it);

        err = latch_release();

        if (err != DB_SUCCESS) {
          return err;
        }

        latches_released = true;

        IF_DEBUG(force_log_free_check = false;)
      }
    }

    mtr.start();

    rtr_init_rtr_info(&rtr_info, false, &cursor, m_index, false);

    rtr_info_update_btr(&cursor, &rtr_info);

    btr_cur_search_to_nth_level(m_index, 0, dtuple, PAGE_CUR_RTREE_INSERT,
                                BTR_MODIFY_LEAF, &cursor, 0, __FILE__, __LINE__,
                                &mtr);

    /* Update MBR in parent entry, change search mode to BTR_MODIFY_TREE */
    if (rtr_info.mbr_adj) {
      mtr.commit();

      rtr_clean_rtr_info(&rtr_info, true);

      rtr_init_rtr_info(&rtr_info, false, &cursor, m_index, false);

      rtr_info_update_btr(&cursor, &rtr_info);

      mtr.start();

      btr_cur_search_to_nth_level(m_index, 0, dtuple, PAGE_CUR_RTREE_INSERT,
                                  BTR_MODIFY_TREE, &cursor, 0, __FILE__,
                                  __LINE__, &mtr);
    }

    err = btr_cur_optimistic_insert(flag, &cursor, &offsets, &m_dml_heap,
                                    dtuple, &rec, &big_rec, nullptr, &mtr);

    if (err == DB_FAIL) {
      ut_ad(big_rec == nullptr);

      mtr.commit();

      mtr.start();

      rtr_clean_rtr_info(&rtr_info, true);

      rtr_init_rtr_info(&rtr_info, false, &cursor, m_index, false);

      rtr_info_update_btr(&cursor, &rtr_info);

      btr_cur_search_to_nth_level(m_index, 0, dtuple, PAGE_CUR_RTREE_INSERT,
                                  BTR_MODIFY_TREE, &cursor, 0, __FILE__,
                                  __LINE__, &mtr);

      err = btr_cur_pessimistic_insert(flag, &cursor, &offsets, &m_dml_heap,
                                       dtuple, &rec, &big_rec, nullptr, &mtr);
    }

    IF_ENABLED("ddl_ins_spatial_fail", err = DB_FAIL;)

    if (err == DB_SUCCESS) {
      if (rtr_info.mbr_adj) {
        err = rtr_ins_enlarge_mbr(&cursor, &mtr);
      }

      if (err == DB_SUCCESS) {
        page_update_max_trx_id(btr_cur_get_block(&cursor),
                               btr_cur_get_page_zip(&cursor), trx_id, &mtr);
      }
    }

    mtr.commit();

    rtr_clean_rtr_info(&rtr_info, true);

    if (err != DB_SUCCESS) {
      m_ctx.set_error(err);
    } else {
      err = m_ctx.get_error();
    }

    if (err != DB_SUCCESS) {
      break;
    }
  }

  m_dtuples->clear();

  mem_heap_empty(m_dml_heap);
  mem_heap_empty(m_dtuple_heap);

  return err;
}

}  // namespace ddl
