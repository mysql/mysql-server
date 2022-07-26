/*****************************************************************************

Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
#include "univ.i"

Parallel_reader_adapter::Parallel_reader_adapter(size_t max_threads,
                                                 ulint rowlen)
    : m_parallel_reader(max_threads) {
  m_batch_size = ADAPTER_SEND_BUFFER_SIZE / rowlen;
}

dberr_t Parallel_reader_adapter::add_scan(trx_t *trx,
                                          const Parallel_reader::Config &config,
                                          Parallel_reader::F &&f) {
  return m_parallel_reader.add_scan(trx, config, std::move(f));
}

Parallel_reader_adapter::Thread_ctx::Thread_ctx() {
  m_buffer.resize(ADAPTER_SEND_BUFFER_SIZE);
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

    m_mysql_row.m_offsets.push_back(
        static_cast<ulong>(templt.mysql_col_offset));
    m_mysql_row.m_null_bit_mask.push_back(
        static_cast<ulong>(templt.mysql_null_bit_mask));
    m_mysql_row.m_null_bit_offsets.push_back(
        static_cast<ulong>(templt.mysql_null_byte_offset));
  }

  ut_a(m_mysql_row.m_max_len == 0);
  ut_a(prebuilt->mysql_row_len > 0);
  m_mysql_row.m_max_len = static_cast<ulong>(prebuilt->mysql_row_len);

  m_parallel_reader.set_start_callback(
      [=](Parallel_reader::Thread_ctx *reader_thread_ctx) {
        if (reader_thread_ctx->get_state() == Parallel_reader::State::THREAD) {
          return init(reader_thread_ctx, prebuilt);
        } else {
          return DB_SUCCESS;
        }
      });

  m_parallel_reader.set_finish_callback(
      [=](Parallel_reader::Thread_ctx *reader_thread_ctx) {
        if (reader_thread_ctx->get_state() == Parallel_reader::State::THREAD) {
          return end(reader_thread_ctx);
        } else {
          return DB_SUCCESS;
        }
      });

  ut_a(m_prebuilt == nullptr);
  m_prebuilt = prebuilt;
}

dberr_t Parallel_reader_adapter::run(void **thread_ctxs, Init_fn init_fn,
                                     Load_fn load_fn, End_fn end_fn) {
  m_end_fn = end_fn;
  m_init_fn = init_fn;
  m_load_fn = load_fn;
  m_thread_ctxs = thread_ctxs;

  m_parallel_reader.set_n_threads(m_parallel_reader.max_threads());

  return m_parallel_reader.run(m_parallel_reader.max_threads());
}

dberr_t Parallel_reader_adapter::init(
    Parallel_reader::Thread_ctx *reader_thread_ctx, row_prebuilt_t *prebuilt) {
  auto thread_ctx =
      ut::new_withkey<Thread_ctx>(ut::make_psi_memory_key(mem_key_archive));

  if (thread_ctx == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  reader_thread_ctx->set_callback_ctx<Thread_ctx>(thread_ctx);

  /** There are data members in row_prebuilt_t that cannot be accessed in
  multi-threaded mode e.g., blob_heap.

  row_prebuilt_t is designed for single threaded access and to share
  it among threads is not recommended unless "you know what you are doing".
  This is very fragile code as it stands.

  To solve the blob heap issue in prebuilt we request parallel reader thread to
  use blob heap per thread and we pass this blob heap to the InnoDB to MySQL
  row format conversion function. */
  if (prebuilt->templ_contains_blob) {
    reader_thread_ctx->create_blob_heap();
  }

  auto ret = m_init_fn(m_thread_ctxs[reader_thread_ctx->m_thread_id],
                       static_cast<ulong>(m_mysql_row.m_offsets.size()),
                       m_mysql_row.m_max_len, &m_mysql_row.m_offsets[0],
                       &m_mysql_row.m_null_bit_offsets[0],
                       &m_mysql_row.m_null_bit_mask[0]);

  return (ret ? DB_INTERRUPTED : DB_SUCCESS);
}

dberr_t Parallel_reader_adapter::send_batch(
    Parallel_reader::Thread_ctx *reader_thread_ctx, size_t partition_id,
    uint64_t n_recs) {
  auto ctx = reader_thread_ctx->get_callback_ctx<Thread_ctx>();
  const auto thread_id = reader_thread_ctx->m_thread_id;

  const auto start = ctx->m_n_sent % m_batch_size;

  ut_a(n_recs <= m_batch_size);
  ut_a(start + n_recs <= m_batch_size);

  const auto rec_loc = &ctx->m_buffer[start * m_mysql_row.m_max_len];

  dberr_t err{DB_SUCCESS};

  if (m_load_fn(m_thread_ctxs[thread_id], n_recs, rec_loc, partition_id)) {
    err = DB_INTERRUPTED;
    m_parallel_reader.set_error_state(DB_INTERRUPTED);
  }

  ctx->m_n_sent += n_recs;

  return err;
}

dberr_t Parallel_reader_adapter::process_rows(
    const Parallel_reader::Ctx *reader_ctx) {
  auto reader_thread_ctx = reader_ctx->thread_ctx();
  auto ctx = reader_thread_ctx->get_callback_ctx<Thread_ctx>();
  auto blob_heap = reader_thread_ctx->m_blob_heap;

  ut_a(ctx->m_n_read >= ctx->m_n_sent);
  ut_a(ctx->m_n_read - ctx->m_n_sent <= m_batch_size);

  dberr_t err{DB_SUCCESS};

  {
    auto n_pending = pending(ctx);

    /* Start of a new range, send what we have buffered. */
    if ((reader_ctx->m_start && n_pending > 0) || is_buffer_full(ctx)) {
      err = send_batch(reader_thread_ctx, ctx->m_partition_id, n_pending);

      if (err != DB_SUCCESS) {
        return (err);
      }

      /* Empty the heap for the next batch */
      if (blob_heap != nullptr) {
        mem_heap_empty(blob_heap);
      }
    }
  }

  mem_heap_t *heap{};
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;

  rec_offs_init(offsets_);

  offsets = rec_get_offsets(reader_ctx->m_rec, reader_ctx->index(), offsets,
                            ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const auto next_rec = ctx->m_n_read % m_batch_size;

  const auto buffer_loc = &ctx->m_buffer[0] + next_rec * m_mysql_row.m_max_len;

  if (row_sel_store_mysql_rec(buffer_loc, m_prebuilt, reader_ctx->m_rec,
                              nullptr, true, reader_ctx->index(),
                              reader_ctx->index(), offsets, false, nullptr,
                              blob_heap)) {
    /* If there is any pending records, then we should not overwrite the
    partition ID with a different one. */
    if (pending(ctx) && ctx->m_partition_id != reader_ctx->partition_id()) {
      err = DB_ERROR;
      ut_d(ut_error);
    } else {
      ++ctx->m_n_read;
      ctx->m_partition_id = reader_ctx->partition_id();
    }

    if (m_parallel_reader.is_error_set()) {
      /* Simply skip sending the records to RAPID in case of an error in the
      parallel reader and return DB_ERROR as the error could have been
      originated from RAPID threads. */
      err = DB_ERROR;
    }
  } else {
    err = DB_ERROR;
  }

  if (heap != nullptr) {
    mem_heap_free(heap);
  }

  return (err);
}

dberr_t Parallel_reader_adapter::end(
    Parallel_reader::Thread_ctx *reader_thread_ctx) {
  dberr_t err{DB_SUCCESS};

  auto thread_id = reader_thread_ctx->m_thread_id;
  auto thread_ctx = reader_thread_ctx->get_callback_ctx<Thread_ctx>();

  ut_a(thread_ctx->m_n_sent <= thread_ctx->m_n_read);
  ut_a(thread_ctx->m_n_read - thread_ctx->m_n_sent <= m_batch_size);

  if (!m_parallel_reader.is_error_set()) {
    /* It's possible that we might not have sent the records in the buffer
    when we have reached the end of records and the buffer is not full.
    Send them now. */
    size_t n_pending = pending(thread_ctx);

    if (n_pending != 0) {
      err =
          send_batch(reader_thread_ctx, thread_ctx->m_partition_id, n_pending);
    }
  }

  m_end_fn(m_thread_ctxs[thread_id]);

  ut::delete_(thread_ctx);
  reader_thread_ctx->set_callback_ctx<Thread_ctx>(nullptr);

  return (err);
}
#endif /* !UNIV_HOTBACKUP */
