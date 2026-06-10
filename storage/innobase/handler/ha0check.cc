/*****************************************************************************

Copyright (c) 2026, Oracle and/or its affiliates.

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
#include "ha_innodb.h"
#include "rem0rec.h"
#include "row0ext.h"
#include "row0pread.h"
#include "row0row.h"
#include "scope_guard.h"

struct Check_foreign_keys {
  Check_foreign_keys(dict_table_t *table, trx_t *trx, mem_heap_t *heap)
      : m_table(table), m_trx(trx), m_heap(heap) {}

  dberr_t operator()(const Parallel_reader::Ctx *ctx) {
    btr_pcur_t pcur;
    mtr_t mtr;
    dberr_t err{DB_SUCCESS};
    auto &fk_set = m_table->foreign_set;
    dict_index_t *clust_index = m_table->first_index();

    m_offsets = rec_get_offsets(ctx->m_rec, clust_index, m_offsets,
                                ULINT_UNDEFINED, UT_LOCATION_HERE, &m_heap);

    row_ext_t *ext = nullptr;
    dtuple_t *clust_row =
        row_build(ROW_COPY_POINTERS, clust_index, ctx->m_rec, m_offsets,
                  nullptr, nullptr, nullptr, &ext, m_heap);

    for (auto &fk : fk_set) {
      auto &fk_index = fk->foreign_index;
      dtuple_t *entry = row_build_index_entry_low(clust_row, ext, fk_index,
                                                  m_heap, ROW_BUILD_NORMAL);
      if (dtuple_contains_null(entry)) {
        continue;
      }

      dict_index_t *parent_index = fk->referenced_index;
      const size_t n_uniq = dict_index_get_n_unique(parent_index);

      if (dtuple_get_n_fields_cmp(entry) > n_uniq) {
        dtuple_set_n_fields_cmp(entry, n_uniq);
      }

      mtr_start(&mtr);
      pcur.open_on_user_rec(parent_index, entry, PAGE_CUR_GE, BTR_SEARCH_LEAF,
                            &mtr, UT_LOCATION_HERE);

      if (pcur.get_up_match() < parent_index->n_uniq) {
        err = DB_NO_REFERENCED_ROW;
      }
      pcur.close();
      mtr_commit(&mtr);
    }

    return err;
  }

  dict_table_t *m_table;
  trx_t *m_trx;
  mem_heap_t *m_heap{};
  ulint *m_offsets{};
};

int ha_innobase::check_foreign_constraints(THD *thd, size_t n_threads) const {
  ut_ad(thd != nullptr);
  ut_a(thd == ha_thd());
  ut_a(m_prebuilt->trx->magic_n == TRX_MAGIC_N);
  ut_a(m_prebuilt->trx == thd_to_trx(thd));

  mem_heap_t *heap = mem_heap_create(1024, UT_LOCATION_HERE);
  auto guard = create_scope_guard([heap]() { mem_heap_free(heap); });

  Check_foreign_keys fkcheck(m_prebuilt->table, m_prebuilt->trx, heap);

  dict_index_t *index = m_prebuilt->table->first_index();
  const Parallel_reader::Scan_range FULL_SCAN;

  Parallel_reader reader{n_threads};
  Parallel_reader::Config config(FULL_SCAN, index);

  reader.set_finish_callback(
      [&](Parallel_reader::Thread_ctx *) { return DB_SUCCESS; });

  auto err = reader.add_scan(m_prebuilt->trx, config, fkcheck);

  err = reader.run(n_threads);
  if (err == DB_OUT_OF_RESOURCES) {
    err = reader.run(0);
  }

  return convert_error_code_to_mysql(err, 0, m_prebuilt->trx->mysql_thd);
}
