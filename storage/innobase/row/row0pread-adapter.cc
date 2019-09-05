/*****************************************************************************

Copyright (c) 2018, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

#ifndef UNIV_HOTBACKUP

#include "row0pread-adapter.h"
#include "row0sel.h"

Parallel_reader_adapter::Parallel_reader_adapter(size_t max_threads,
                                                 ulint rowlen)
    : m_parallel_reader(max_threads) {
  m_buffers.resize(max_threads);
  m_blob_heaps.resize(max_threads);

  for (auto &buffer : m_buffers) {
    buffer.resize(ADAPTER_SEND_BUFFER_SIZE);
  }

  for (auto &blob_heap : m_blob_heaps) {
    /* Keep the size small because it's currently not used. */
    blob_heap = mem_heap_create(UNIV_PAGE_SIZE / 64);
  }

  m_batch_size = ADAPTER_SEND_BUFFER_SIZE / rowlen;

  Counter::clear(m_n_read);
  Counter::clear(m_n_sent);
}

Parallel_reader_adapter::~Parallel_reader_adapter() {
  for (auto &blob_heap : m_blob_heaps) {
    mem_heap_free(blob_heap);
  }
}

bool Parallel_reader_adapter::add_scan(trx_t *trx,
                                       const Parallel_reader::Config &config,
                                       Parallel_reader::F &&f) {
  return (m_parallel_reader.add_scan(trx, config, std::move(f)));
}

void Parallel_reader_adapter::set(row_prebuilt_t *prebuilt) {
  ut_a(prebuilt->n_template > 0);
  ut_a(m_mysql_row.m_offsets.empty());
  ut_a(m_mysql_row.m_null_bit_mask.empty());
  ut_a(m_mysql_row.m_null_bit_offsets.empty());

  /* Partition structure should be the same across all partitions.
  Therefore MySQL row meta-data is common across all paritions. */

  for (uint i = 0; i < prebuilt->n_template; ++i) {
    const auto &templt = prebuilt->mysql_template[i];

    m_mysql_row.m_offsets.push_back(templt.mysql_col_offset);
    m_mysql_row.m_null_bit_mask.push_back(templt.mysql_null_bit_mask);
    m_mysql_row.m_null_bit_offsets.push_back(templt.mysql_null_byte_offset);
  }

  ut_a(m_mysql_row.m_max_len == 0);
  ut_a(prebuilt->mysql_row_len > 0);
  m_mysql_row.m_max_len = prebuilt->mysql_row_len;

  // clang-format off
  m_parallel_reader.set_start_callback([=](size_t thread_id) {
      return (init(thread_id));
  });

  m_parallel_reader.set_finish_callback([=](size_t thread_id) {
      return (end(thread_id));
  });
  // clang-format on

  ut_a(m_prebuilt == nullptr);
  m_prebuilt = prebuilt;
}

dberr_t Parallel_reader_adapter::run(void **thread_contexts, Init_fn init_fn,
                                     Load_fn load_fn, End_fn end_fn) {
  m_end_fn = end_fn;
  m_init_fn = init_fn;
  m_load_fn = load_fn;
  m_thread_contexts = thread_contexts;

  return (m_parallel_reader.run());
}

dberr_t Parallel_reader_adapter::send_batch(size_t thread_id, uint64_t n_recs) {
  ut_a(n_recs <= m_batch_size);

  dberr_t err{DB_SUCCESS};

  /* Push the row buffer to the caller if we have filled the buffer with
  ADAPTER_SEND_NUM_RECS number of records or it's a start of a new range. */

  const auto start = Counter::get(m_n_sent, thread_id) % m_batch_size;

  ut_a(start + n_recs <= m_batch_size);

  auto &buffer = m_buffers[thread_id];

  const auto p = &buffer[start * m_mysql_row.m_max_len];

  if (m_load_fn(m_thread_contexts[thread_id], n_recs, p)) {
    err = DB_INTERRUPTED;
    m_parallel_reader.set_error_state(DB_INTERRUPTED);
  }

  Counter::add(m_n_sent, thread_id, n_recs);

  return (err);
}

dberr_t Parallel_reader_adapter::init(size_t thread_id) {
  auto ret = m_init_fn(
      m_thread_contexts[thread_id], m_mysql_row.m_offsets.size(),
      m_mysql_row.m_max_len, &m_mysql_row.m_offsets[0],
      &m_mysql_row.m_null_bit_offsets[0], &m_mysql_row.m_null_bit_mask[0]);

  return (ret ? DB_INTERRUPTED : DB_SUCCESS);
}

dberr_t Parallel_reader_adapter::process_rows(const Parallel_reader::Ctx *ctx) {
  mem_heap_t *heap{};
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;

  rec_offs_init(offsets_);

  offsets = rec_get_offsets(ctx->m_rec, ctx->index(), offsets, ULINT_UNDEFINED,
                            &heap);

  const auto thread_id = ctx->m_thread_id;

  ut_a(Counter::get(m_n_sent, thread_id) <= Counter::get(m_n_read, thread_id));

  ut_a((Counter::get(m_n_read, thread_id) -
        Counter::get(m_n_sent, thread_id)) <= m_batch_size);

  dberr_t err{DB_SUCCESS};

  {
    const auto n_pending = pending(thread_id);

    /* Start of a new range, send what we have buffered. */
    if (ctx->m_start && n_pending > 0) {
      err = send_batch(thread_id, n_pending);

      if (err != DB_SUCCESS) {
        if (heap != nullptr) {
          mem_heap_free(heap);
        }
        return (err);
      }
    }
  }

  const auto next_rec = Counter::get(m_n_read, thread_id) % m_batch_size;

  auto &buffer = m_buffers[thread_id];

  const auto p = &buffer[0] + next_rec * m_mysql_row.m_max_len;

  if (row_sel_store_mysql_rec(p, m_prebuilt, ctx->m_rec, nullptr, true,
                              ctx->index(), offsets, false, nullptr,
                              m_blob_heaps[thread_id])) {
    Counter::inc(m_n_read, thread_id);

    if (m_parallel_reader.is_error_set()) {
      /* Simply skip sending the records to RAPID in case of an error in the
      parallel reader and return DB_ERROR as the error could have been
      originated from RAPID threads. */
      err = DB_ERROR;
    } else if (is_buffer_full(thread_id)) {
      err = send_batch(thread_id, pending(thread_id));
    }
  } else {
    err = DB_ERROR;
  }

  if (heap != nullptr) {
    mem_heap_free(heap);
  }

  return (err);
}

dberr_t Parallel_reader_adapter::end(size_t thread_id) {
  ut_a(Counter::get(m_n_sent, thread_id) <= Counter::get(m_n_read, thread_id));

  ut_a((Counter::get(m_n_read, thread_id) -
        Counter::get(m_n_sent, thread_id)) <= m_batch_size);

  dberr_t err{DB_SUCCESS};

  if (!m_parallel_reader.is_error_set()) {
    /* It's possible that we might not have sent the records in the buffer
    when we have reached the end of records and the buffer is not full.
    Send them now. */
    err = (pending(thread_id) != 0) ? send_batch(thread_id, pending(thread_id))
                                    : DB_SUCCESS;
  }

  m_end_fn(m_thread_contexts[thread_id]);

  return (err);
}
#endif /* !UNIV_HOTBACKUP */
