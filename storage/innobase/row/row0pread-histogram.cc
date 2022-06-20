/*****************************************************************************

Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

/** @file row/row0pread-histogram.cc
Parallel read histogram interface implementation

Created 2019-04-20 by Darshan M N */

#include "row0pread-histogram.h"
#include "row0row.h"
#include "row0sel.h"
#include "srv0srv.h"

#ifdef UNIV_DEBUG
#include <current_thd.h>
#endif /* UNIV_DEBUG */

std::uniform_real_distribution<double> Histogram_sampler::m_distribution(0,
                                                                         100);

Histogram_sampler::Histogram_sampler(size_t max_threads, int sampling_seed,
                                     double sampling_percentage,
                                     enum_sampling_method sampling_method)
    : m_parallel_reader(max_threads),
      m_random_generator(sampling_seed),
      m_sampling_method(sampling_method),
      m_sampling_percentage(sampling_percentage),
      m_sampling_seed(sampling_seed) {
  ut_ad(max_threads == 1);

  m_start_buffer_event = os_event_create();
  m_end_buffer_event = os_event_create();

  os_event_reset(m_start_buffer_event);
  os_event_reset(m_end_buffer_event);

  m_n_sampled = 0;

#ifdef UNIV_DEBUG
  THD *thd = current_thd;
#endif /* UNIV_DEBUG */

  m_parallel_reader.set_start_callback(
      [=](Parallel_reader::Thread_ctx *thread_ctx) {
        if (thread_ctx->get_state() == Parallel_reader::State::THREAD) {
#ifdef UNIV_DEBUG
          /* for debug sync calls */
          current_thd = thd;
#endif /* UNIV_DEBUG */
          return start_callback(thread_ctx);
        } else {
          return DB_SUCCESS;
        }
      });

  m_parallel_reader.set_finish_callback(
      [=](Parallel_reader::Thread_ctx *thread_ctx) {
        if (thread_ctx->get_state() == Parallel_reader::State::THREAD) {
          return finish_callback(thread_ctx);
        } else {
          return DB_SUCCESS;
        }
      });
}

Histogram_sampler::~Histogram_sampler() {
  buffer_end();

  os_event_destroy(m_start_buffer_event);
  os_event_destroy(m_end_buffer_event);
}

dberr_t Histogram_sampler::start_callback(
    Parallel_reader::Thread_ctx *reader_thread_ctx) {
  ut_a(reader_thread_ctx->get_state() == Parallel_reader::State::THREAD);
  /** There are data members in row_prebuilt_t that cannot be accessed in
  multi-threaded mode e.g., blob_heap.

  row_prebuilt_t is designed for single threaded access and to share
  it among threads is not recommended unless "you know what you are doing".
  This is very fragile code as it stands.

  To solve the blob heap issue in prebuilt we request parallel reader thread
  to use blob heap per thread and we pass this blob heap to the InnoDB to
  MySQL row format conversion function. */
  reader_thread_ctx->create_blob_heap();

  return DB_SUCCESS;
}

dberr_t Histogram_sampler::finish_callback(
    Parallel_reader::Thread_ctx *reader_thread_ctx) {
  ut_a(reader_thread_ctx->get_state() == Parallel_reader::State::THREAD);

  DBUG_PRINT("histogram_sampler_buffering_print", ("-> Buffering complete."));

  DBUG_LOG("histogram_sampler_buffering_print",
           "Total number of rows sampled : "
               << m_n_sampled.load(std::memory_order_relaxed));

  if (is_error_set()) {
    signal_end_of_buffering();
    return m_err;
  }

  wait_for_start_of_buffering();

  auto err = m_parallel_reader.get_error_state();

  set_error_state(err == DB_SUCCESS ? DB_END_OF_INDEX : err);

  signal_end_of_buffering();

  return DB_SUCCESS;
}

bool Histogram_sampler::init(trx_t *trx, dict_index_t *index,
                             row_prebuilt_t *prebuilt) {
  mtr_t mtr;
  mtr_start(&mtr);
  mtr_sx_lock(dict_index_get_lock(index), &mtr, UT_LOCATION_HERE);

  /* Read pages from one level above the leaf page. */
  ulint read_level = btr_height_get(index, &mtr);

  mtr_commit(&mtr);

  if (read_level > 1) {
    read_level = 1;
  }

  Parallel_reader::Scan_range full_scan{};
  Parallel_reader::Config config(full_scan, index, read_level);

  dberr_t err = m_parallel_reader.add_scan(
      trx, config, [=](const Parallel_reader::Ctx *ctx) {
        if (read_level == 0) {
          return (process_leaf_rec(ctx, prebuilt));
        } else {
          return (process_non_leaf_rec(ctx, prebuilt));
        }
      });

  return (err == DB_SUCCESS);
}

void Histogram_sampler::wait_for_start_of_buffering() {
  os_event_wait(m_start_buffer_event);

  os_event_reset(m_start_buffer_event);
}

void Histogram_sampler::wait_for_end_of_buffering() {
  os_event_wait(m_end_buffer_event);

  os_event_reset(m_end_buffer_event);
}

void Histogram_sampler::signal_start_of_buffering() {
  os_event_set(m_start_buffer_event);
}

void Histogram_sampler::signal_end_of_buffering() {
  os_event_set(m_end_buffer_event);
}

bool Histogram_sampler::skip() {
  if (m_sampling_percentage == 0.00) {
    return (true);
  } else if (m_sampling_percentage == 100.00) {
    return (false);
  }

  bool ret = false;

  switch (m_sampling_method) {
    case enum_sampling_method::SYSTEM: {
      double rand = m_distribution(m_random_generator);

      DBUG_PRINT("histogram_sampler_buffering_print",
                 ("-> New page. Random value generated - %lf", rand));

      /* Check if the records in the block needs to be read for sampling. */
      if (rand > m_sampling_percentage) {
        ret = true;
      }
    } break;

    default:
      ut_d(ut_error);
      ut_o(break);
  }

  return (ret);
}

dberr_t Histogram_sampler::buffer_next() {
  /* Return if the tree is empty. */
  if (m_parallel_reader.is_tree_empty()) {
    return (DB_END_OF_INDEX);
  }

  signal_start_of_buffering();

  wait_for_end_of_buffering();

  if (is_error_set()) {
    /* End of records to be buffered. */
    m_parallel_reader.join();
  }

  return (m_err);
}

void Histogram_sampler::buffer_end() {
  set_error_state(DB_END_SAMPLE_READ);

  signal_start_of_buffering();

  m_parallel_reader.join();

  return;
}

dberr_t Histogram_sampler::run() {
  return m_parallel_reader.spawn(m_parallel_reader.max_threads());
}

dberr_t Histogram_sampler::sample_rec(const Parallel_reader::Ctx *reader_ctx,
                                      const rec_t *rec, ulint *offsets,
                                      const dict_index_t *index,
                                      row_prebuilt_t *prebuilt) {
  dberr_t err{DB_SUCCESS};

  auto reader_thread_ctx = reader_ctx->thread_ctx();

  wait_for_start_of_buffering();

  /* Return as the sampler has been requested to end sampling. */
  if (m_err == DB_END_SAMPLE_READ) {
    signal_end_of_buffering();
    return (m_err);
  }

  if (row_sel_store_mysql_rec(m_buf, prebuilt, rec, nullptr, true, index, index,
                              offsets, false, nullptr,
                              reader_thread_ctx->m_blob_heap)) {
    m_n_sampled.fetch_add(1, std::memory_order_relaxed);
  } else {
    err = DB_ERROR;
    ut_d(ut_error);
  }

  DBUG_EXECUTE_IF("simulate_sample_read_error", err = DB_ERROR;);

  signal_end_of_buffering();

  return (err);
}

dberr_t Histogram_sampler::process_non_leaf_rec(
    const Parallel_reader::Ctx *ctx_const, row_prebuilt_t *prebuilt) {
  DBUG_EXECUTE_IF("parallel_reader_histogram_induce_error",
                  set_error_state(DB_ERROR);
                  return DB_ERROR;);

  if (skip()) {
    srv_stats.n_sampled_pages_skipped.inc();

    DBUG_PRINT("histogram_sampler_buffering_print", ("Skipping block."));
    return (DB_SUCCESS);
  }

  Parallel_reader::Ctx *ctx = const_cast<Parallel_reader::Ctx *>(ctx_const);
  const dict_index_t *index = ctx->index();

  ut_ad(!page_is_leaf(ctx->m_block->frame));

  /* Get the child page pointed to by the record. */

  srv_stats.n_sampled_pages_read.inc();

  mtr_t mtr;
  mtr.start();
  mtr.set_log_mode(MTR_LOG_NO_REDO);

  buf_block_t *leaf_block =
      btr_node_ptr_get_child(ctx->m_rec, const_cast<dict_index_t *>(index),
                             ctx->m_offsets, &mtr, RW_S_LATCH);

  ut_ad(page_is_leaf(leaf_block->frame));

  /* Sample all the records in the leaf page. */

  page_cur_t cur;
  page_cur_set_before_first(leaf_block, &cur);
  page_cur_move_to_next(&cur);

  auto heap = mem_heap_create(srv_page_size / 4, UT_LOCATION_HERE);
  dberr_t err{DB_SUCCESS};

  for (;;) {
    if (page_cur_is_after_last(&cur)) {
      break;
    }

    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    ulint *offsets = offsets_;
    rec_offs_init(offsets_);

    const rec_t *rec = page_cur_get_rec(&cur);
    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    if (ctx->is_rec_visible(rec, offsets, heap, &mtr)) {
      err = sample_rec(ctx, rec, offsets, index, prebuilt);

      if (err != DB_SUCCESS) {
        set_error_state(err);
      }

      if (is_error_set()) {
        break;
      }
    }

    page_cur_move_to_next(&cur);
  }

  mtr.commit();

  mem_heap_free(heap);

  return (m_err);
}

dberr_t Histogram_sampler::process_leaf_rec(const Parallel_reader::Ctx *ctx,
                                            row_prebuilt_t *prebuilt) {
  ut_ad(page_is_leaf(ctx->m_block->frame));

  if (ctx->m_first_rec) {
    srv_stats.n_sampled_pages_read.inc();
  }

  return sample_rec(ctx, ctx->m_rec, ctx->m_offsets, ctx->index(), prebuilt);
}
