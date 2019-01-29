/*****************************************************************************

Copyright (c) 2019, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file row/row0pread-adapter.cc
Parallel read adapter interface implementation

Created 2018-02-28 by Darshan M N */

#include "row0pread-adapter.h"
#include "row0sel.h"

dberr_t Parallel_reader_adapter::worker(size_t id, Queue &ctxq, Function &f) {
  ulong *mysql_row_offsets;
  ulong *mysql_nullbit_offsets;
  ulong *mysql_null_bit_mask;

  mysql_row_offsets =
      static_cast<ulong *>(alloca(sizeof(ulong) * m_prebuilt->n_template));
  mysql_nullbit_offsets =
      static_cast<ulong *>(alloca(sizeof(ulong) * m_prebuilt->n_template));
  mysql_null_bit_mask =
      static_cast<ulong *>(alloca(sizeof(ulong) * m_prebuilt->n_template));

  for (uint i = 0; i < m_prebuilt->n_template; i++) {
    mysql_row_offsets[i] = m_prebuilt->mysql_template[i].mysql_col_offset;
    mysql_nullbit_offsets[i] =
        m_prebuilt->mysql_template[i].mysql_null_byte_offset;
    mysql_null_bit_mask[i] = m_prebuilt->mysql_template[i].mysql_null_bit_mask;
  }

  dberr_t err = DB_SUCCESS;
  bool initError = m_load_init(m_thread_contexts[id], m_prebuilt->n_template,
                               m_prebuilt->mysql_row_len, mysql_row_offsets,
                               mysql_nullbit_offsets, mysql_null_bit_mask);
  if (initError) {
    err = DB_INTERRUPTED;
  } else {
    err = Key_reader::worker(id, ctxq, f);

    /** It's possible that we might not have sent the records in the buffer when
    we have reached the end of records and the buffer is not full. Send them
    now. */
    if (n_recs[id] % m_send_num_recs != 0 && err == DB_SUCCESS) {
      if (m_load_rows(m_thread_contexts[id], n_recs[id] % m_send_num_recs,
                      (void *)m_bufs[id])) {
        err = DB_INTERRUPTED;
      }
      n_total_recs_sent.add(id, n_recs[id] % m_send_num_recs);
    }
  }
  m_load_end(m_thread_contexts[id]);

  return (err);
}

dberr_t Parallel_reader_adapter::process_rows(size_t thread_id,
                                              const rec_t *rec,
                                              dict_index_t *index,
                                              row_prebuilt_t *prebuilt) {
  dberr_t err = DB_ERROR;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  mem_heap_t *heap = nullptr;

  rec_offs_init(offsets_);

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

  auto rec_offset =
      (n_recs[thread_id] % m_send_num_recs) * prebuilt->mysql_row_len;

  if (row_sel_store_mysql_rec(m_bufs[thread_id] + rec_offset, prebuilt, rec,
                              NULL, true, index, offsets, false, nullptr)) {
    err = DB_SUCCESS;

    n_recs.add(thread_id, 1);

    /** Call the adapter callback if we have filled the buffer with
    ADAPTER_SEND_NUM_RECS number of records. */
    if (n_recs[thread_id] % m_send_num_recs == 0) {
      if (m_load_rows(m_thread_contexts[thread_id], m_send_num_recs,
                      (void *)m_bufs[thread_id])) {
        err = DB_INTERRUPTED;
      }
      n_total_recs_sent.add(thread_id, m_send_num_recs);
    }
  }

  if (heap != NULL) {
    mem_heap_free(heap);
  }

  return (err);
}

void Parallel_partition_reader_adapter::set_info(dict_table_t *table,
                                                 dict_index_t *index,
                                                 trx_t *trx,
                                                 row_prebuilt_t *prebuilt) {
  m_index.push_back(index);
  m_table.push_back(table);
  m_trx.push_back(trx);
  m_prebuilt.push_back(prebuilt);
}

size_t Parallel_partition_reader_adapter::calc_num_threads() {
  size_t num_threads = 0;

  if (m_num_parts == 0) {
    return (0);
  }

  /** If partitions have already been created just return the number of threads
   that needs to be spawned */
  if (!m_partitions.empty()) {
    for (uint i = 0; i < m_num_parts; ++i) {
      num_threads += m_partitions[i].size();
    }

    return (std::min(num_threads, m_n_threads));
  }

  for (uint i = 0; i < m_num_parts; ++i) {
    Parallel_reader_adapter::set_info(m_table[i], m_index[i], m_trx[i],
                                      m_prebuilt[i]);
    m_partitions.push_back(partition());
    num_threads += m_partitions[i].size();
    std::cerr << "Partition " << i << " would be using "
              << std::min(m_partitions[i].size(), m_n_threads) << " threads."
              << std::endl;
  }

  Parallel_reader_adapter::set_info(nullptr, nullptr, nullptr, m_prebuilt[0]);

  num_threads = std::min(num_threads, m_n_threads);

  return (num_threads);
}

dberr_t Parallel_partition_reader_adapter::read(Function &&f) {
  m_n_threads = calc_num_threads();

  if (m_n_threads == 0) {
    return (DB_SUCCESS);
  }

  size_t id = 0;

  dberr_t err = DB_SUCCESS;

  for (uint i = 0; i < m_num_parts; ++i) {
    for (auto range : m_partitions[i]) {
      m_ctxs.push_back(UT_NEW_NOKEY(
          Ctx(id, range, m_table[i], m_index[i], m_trx[i], m_prebuilt[i])));

      if (m_ctxs.back() == nullptr) {
        err = DB_OUT_OF_MEMORY;
        break;
      }

      ++id;
    }
  }

  if (err != DB_SUCCESS) {
    uint part_first_ctx = 0;

    for (uint i = 0; i < m_num_parts && part_first_ctx < m_ctxs.size(); ++i) {
      if (m_partitions[i].size()) {
        auto &ctx = m_ctxs[part_first_ctx];
        ctx->destroy(ctx->m_range.first);
        part_first_ctx += m_partitions[i].size();
      }
    }

    for (auto &ctx : m_ctxs) {
      UT_DELETE(ctx);
    }

    return (err);
  }

  start_parallel_load(f);

  uint part_first_ctx = 0;

  for (uint i = 0; i < m_num_parts && part_first_ctx < m_ctxs.size(); ++i) {
    if (m_partitions[i].size()) {
      auto &ctx = m_ctxs[part_first_ctx];
      ctx->destroy(ctx->m_range.first);
      part_first_ctx += m_partitions[i].size();
    }
  }

  for (auto &ctx : m_ctxs) {
    if (ctx->m_err != DB_SUCCESS) {
      /* Note: We return the error of the last context. We can't return
      multiple values. The expectation is that the callback function has
      the error state per thread. */
      err = ctx->m_err;
    }

    UT_DELETE(ctx);
  }

  m_ctxs.clear();

  return (err);
}
