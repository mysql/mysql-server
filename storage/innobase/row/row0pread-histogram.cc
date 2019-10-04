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

/** @file row/row0pread-histogram.cc
Parallel read histogram interface implementation

Created 2019-04-20 by Darshan M N */

#include "row0pread-histogram.h"
#include "row0row.h"
#include "row0sel.h"
#include "srv0srv.h"

std::uniform_real_distribution<double> Histogram_sampler::m_distribution(0,
                                                                         100);

bool Histogram_sampler::m_sampling_done{false};

Histogram_sampler::Histogram_sampler(size_t max_threads, int sampling_seed,
                                     double sampling_percentage,
                                     enum_sampling_method sampling_method)
    : m_parallel_reader(Parallel_reader::available_threads(max_threads), false),
      m_random_generator(sampling_seed),
      m_sampling_method(sampling_method),
      m_sampling_percentage(sampling_percentage),
      m_sampling_seed(sampling_seed) {
  ut_ad(max_threads == 1);

  m_blob_heaps.resize(max_threads);

  for (auto &blob_heap : m_blob_heaps) {
    /* Keep the size small because it's currently not used. */
    blob_heap = mem_heap_create(UNIV_PAGE_SIZE / 64);
  }

  m_start_buffer_event = os_event_create("Histogram sampler buffering start");
  m_end_buffer_event = os_event_create("Histogram sampler buffering end");

  os_event_reset(m_start_buffer_event);
  os_event_reset(m_end_buffer_event);

  m_n_sampled = 0;

  m_parallel_reader.set_finish_callback([&](size_t thread_id) {
    DBUG_PRINT("histogram_sampler_buffering_print", ("-> Buffering complete."));

    DBUG_LOG("histogram_sampler_buffering_print",
             "Total number of rows sampled : "
                 << m_n_sampled.load(std::memory_order_relaxed));

    /* No more rows to buffer. So the next time we're asked to buffer set the
    error status signalling the end of buffering. */

    Histogram_sampler::m_sampling_done = true;

    if (m_err != DB_SUCCESS) {
      signal_end_of_buffering();

      return (m_err);
    }

    wait_for_start_of_buffering();

    auto err = m_parallel_reader.get_error_state();

    if (err == DB_SUCCESS) {
      m_err = DB_END_OF_INDEX;
    } else {
      m_err = err;
    }

    signal_end_of_buffering();

    return (DB_SUCCESS);
  });
}

Histogram_sampler::~Histogram_sampler() {
  /** Check if sampling is complete or we need to abort sampling. */
  if (!Histogram_sampler::m_sampling_done) {
    buffer_end();
  }

  for (auto &blob_heap : m_blob_heaps) {
    mem_heap_free(blob_heap);
  }

  os_event_destroy(m_start_buffer_event);
  os_event_destroy(m_end_buffer_event);
  Histogram_sampler::m_sampling_done = false;
}

bool Histogram_sampler::init(trx_t *trx, dict_index_t *index,
                             row_prebuilt_t *prebuilt) {
  mtr_t mtr;
  mtr_start(&mtr);
  mtr_sx_lock(dict_index_get_lock(index), &mtr);

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
      ut_ad(0);
      break;
  }

  return (ret);
}

dberr_t Histogram_sampler::buffer_next() {
  /* Return if the tree is empty. */
  if (m_parallel_reader.is_tree_empty()) {
    Histogram_sampler::m_sampling_done = true;
    return (DB_END_OF_INDEX);
  }

  signal_start_of_buffering();

  wait_for_end_of_buffering();

  if (m_err != DB_SUCCESS) {
    /* End of records to be buffered. */
    m_parallel_reader.join();
  }

  return (m_err);
}

void Histogram_sampler::buffer_end() {
  m_err = DB_END_SAMPLE_READ;

  signal_start_of_buffering();

  wait_for_end_of_buffering();

  /* Wait for the parallel reader to clean up its threads. */
  m_parallel_reader.join();

  return;
}

dberr_t Histogram_sampler::run() { return (m_parallel_reader.run()); }

dberr_t Histogram_sampler::sample_rec(ulint thread_id, const rec_t *rec,
                                      ulint *offsets, const dict_index_t *index,
                                      row_prebuilt_t *prebuilt) {
  dberr_t err{DB_SUCCESS};

  wait_for_start_of_buffering();

  /* Return as the sampler has been requested to end sampling. */
  if (m_err == DB_END_SAMPLE_READ) {
    signal_end_of_buffering();
    return (DB_END_SAMPLE_READ);
  }

  if (row_sel_store_mysql_rec(m_buf, prebuilt, rec, nullptr, true, index,
                              offsets, false, nullptr,
                              m_blob_heaps[thread_id])) {
    m_n_sampled.fetch_add(1, std::memory_order_relaxed);
  } else {
    err = DB_ERROR;
    ut_ad(0);
  }

  DBUG_EXECUTE_IF("simulate_sample_read_error", err = DB_ERROR;);

  signal_end_of_buffering();

  return (err);
}

dberr_t Histogram_sampler::process_non_leaf_rec(const Parallel_reader::Ctx *ctx,
                                                row_prebuilt_t *prebuilt) {
  DBUG_EXECUTE_IF("parallel_reader_histogram_induce_error", m_err = DB_ERROR;
                  return DB_ERROR;);

  ut_ad(!page_is_leaf(ctx->m_block->frame));

  if (skip()) {
    srv_stats.n_sampled_pages_skipped.inc();

    DBUG_PRINT("histogram_sampler_buffering_print", ("Skipping block."));
    return (DB_SUCCESS);
  }

  /* Get the child page pointed to by the record. */

  srv_stats.n_sampled_pages_read.inc();

  mtr_t mtr;
  mtr.start();
  mtr.set_log_mode(MTR_LOG_NO_REDO);

  const dict_index_t *index = ctx->index();

  buf_block_t *leaf_block = btr_node_ptr_get_child(
      ctx->m_rec, const_cast<dict_index_t *>(index), ctx->m_offsets, &mtr);

  ut_ad(page_is_leaf(leaf_block->frame));

  /* Sample all the records in the leaf page. */

  page_cur_t cur;
  page_cur_set_before_first(leaf_block, &cur);
  page_cur_move_to_next(&cur);

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  mem_heap_t *heap{};

  for (;;) {
    if (page_cur_is_after_last(&cur)) {
      break;
    }

    offsets = rec_get_offsets(cur.rec, index, offsets, ULINT_UNDEFINED, &heap);

    m_err = sample_rec(ctx->m_thread_id, cur.rec, offsets, index, prebuilt);

    if (m_err != DB_SUCCESS) {
      break;
    }

    page_cur_move_to_next(&cur);

    if (heap != nullptr) {
      mem_heap_free(heap);
    }
  }

  mtr.commit();

  return (m_err);
}

dberr_t Histogram_sampler::process_leaf_rec(const Parallel_reader::Ctx *ctx,
                                            row_prebuilt_t *prebuilt) {
  ut_ad(page_is_leaf(ctx->m_block->frame));

  if (ctx->m_first_rec) {
    srv_stats.n_sampled_pages_read.inc();
  }

  return (sample_rec(ctx->m_thread_id, ctx->m_rec, ctx->m_offsets, ctx->index(),
                     prebuilt));
}
