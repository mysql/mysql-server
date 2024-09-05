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

/** @file ddl/ddl0par-scan.cc
 DDL cluster index parallel scan implementation.
 Created 2020-11-01 by Sunny Bains. */

#include "ddl0impl-builder.h"
#include "ddl0impl-cursor.h"
#include "row0pread.h"
#include "row0row.h"
#include "ut0stage.h"

#ifdef UNIV_DEBUG
#include <current_thd.h>
#endif /* UNIV_DEBUG */

namespace ddl {

/** Cursor used for parallel reads. */
struct Parallel_cursor : public Cursor {
  /** Constructor.
  @param[in,out] ctx            DDL context. */
  explicit Parallel_cursor(ddl::Context &ctx) noexcept
      : Cursor(ctx),
        m_index(const_cast<dict_index_t *>(m_ctx.index())),
        m_single_threaded_mode(m_ctx.has_virtual_columns() ||
                               m_ctx.has_fts_indexes()) {}

  /** Destructor. */
  ~Parallel_cursor() noexcept override = default;

  /** Open the cursor. */
  void open() noexcept override {}

  /** @return the index being iterated over. */
  [[nodiscard]] dict_index_t *index() noexcept override { return m_index; }

  /** Reads clustered index of the table and create temporary files
  containing the index entries for the indexes to be built.
  @param[in,out] builders Merge buffers to use for reading.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t scan(Builders &builders) noexcept override;

  /** Copy the row data, by default only the pointers are copied.
  @param[in] thread_id          Scan thread ID.
  @param[in,out] row            Row to copy.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] virtual dberr_t copy_row(size_t thread_id,
                                         Row &row) noexcept override;

  /** @return true if EOF reached. */
  [[nodiscard]] virtual bool eof() const noexcept override { return m_eof; }

 private:
  using Heaps = std::vector<mem_heap_t *, ut::allocator<mem_heap_t *>>;

  /** If true then no more rows to scan. */
  bool m_eof{};

  /** Heap per thread. */
  Heaps m_heaps{};

  /** Index to iterate over. */
  dict_index_t *m_index{};

  /** true if scan should be in single threaded mode. */
  bool m_single_threaded_mode{};
};

dberr_t Parallel_cursor::scan(Builders &builders) noexcept {
  ut_a(!builders.empty());

  ut_a(!m_ctx.m_online || m_ctx.m_trx->isolation_level ==
                              trx_t::isolation_level_t::REPEATABLE_READ);

  size_t n_threads{};

  if (!m_single_threaded_mode) {
    auto use_n_threads = thd_parallel_read_threads(m_ctx.m_trx->mysql_thd);

    if (use_n_threads > 1) {
      for (auto &builder : builders) {
        if (builder->is_skip_file_sort() || builder->is_spatial_index()) {
          /* Note: Parallel scan will break the order. If in the future we
          decide to force a parallel scan then we will need to force a file
          sort later. Or, figure out how to "stitch" the lists together after
          the dumping the rows from the scan. */
          m_single_threaded_mode = true;
          break;
        }
      }

      if (!m_single_threaded_mode) {
        /* Check how many threads can actually be allocated. */
        n_threads = Parallel_reader::available_threads(use_n_threads, false);

        /* May as well do a synchronous read. */
        if (n_threads == 1) {
          Parallel_reader::release_threads(n_threads);
          n_threads = 0;
        }
      }
    }
  }

  const auto use_n_threads = n_threads == 0 ? 1 : n_threads;

  Builders batch_insert{};

  for (auto &builder : builders) {
    auto err = builder->init(*this, use_n_threads);
    if (err != DB_SUCCESS) {
      return err;
    }

    if (builder->is_spatial_index()) {
      batch_insert.push_back(builder);
    }
  }

  using Rows = std::vector<Row, ut::allocator<Row>>;
  using Row_counters = std::vector<size_t, ut::allocator<size_t>>;

  /* Each thread has its own row instance and row count instance. */
  Rows rows{};
  Row_counters n_rows{};

  ut_a(m_heaps.empty());

  rows.resize(use_n_threads);
  n_rows.resize(rows.size());

  /* Delete the per thread heaps. */
  auto cleanup = [](Heaps &heaps, dberr_t err) {
    for (auto heap : heaps) {
      if (heap != nullptr) {
        mem_heap_free(heap);
      }
    }
    heaps.clear();
    return err == DB_END_OF_INDEX ? DB_SUCCESS : err;
  };

  /* Create the per thread heap for transient memory allocations. */
  for (size_t i = 0; i < use_n_threads; ++i) {
    m_heaps.push_back(mem_heap_create(1024, UT_LOCATION_HERE));

    if (m_heaps.back() == nullptr) {
      return cleanup(m_heaps, DB_OUT_OF_MEMORY);
    }

    if (m_ctx.m_dtuple_heap != nullptr) {
      rows[i].m_add_cols = m_ctx.create_add_cols();
      if (rows[i].m_add_cols == nullptr) {
        return cleanup(m_heaps, DB_OUT_OF_MEMORY);
      }
    }
  }

  Parallel_reader reader{n_threads};

  const Parallel_reader::Scan_range FULL_SCAN;

  using Thread_ctx = Parallel_reader::Thread_ctx;

  auto batch_inserter = [&](Thread_ctx *thread_ctx) {
    size_t i{};
    bool latches_released{};
    dberr_t err{DB_SUCCESS};
    const auto thread_id = thread_ctx->m_thread_id;

    /* End of page scan. */
    for (auto builder : batch_insert) {
      /* Do a batch insert of the cached rows, instead of one by one. */
      err = builder->batch_insert(thread_id, [&]() {
        if (!latches_released) {
          /* We are going to commit the mini-transaction, this will release
          the latches and so we must do a deep copy of the rows before we
          can commit the mini-transaction. */
          for (size_t j = i + 1; j < batch_insert.size(); ++j) {
            batch_insert[j]->batch_insert_deep_copy_tuples(thread_id);
          }
          thread_ctx->savepoint();
          latches_released = true;
        }
        return DB_SUCCESS;
      });

      if (err != DB_SUCCESS && err != DB_END_OF_INDEX) {
        return err;
      }

      ++i;
    }

    if (latches_released) {
      return thread_ctx->restore_from_savepoint();
    }

    return DB_SUCCESS;
  };

  /* Handle the case when the bulk loader wants to do an mtr commit. The
  log_free_checks require that the caller can't be holding any latches. */
  auto bulk_inserter = [&](Thread_ctx *thread_ctx, Row &row) {
    bool latches_released{};
    const auto thread_id = thread_ctx->m_thread_id;

    for (auto builder : builders) {
      const auto err = builder->add_row(*this, row, thread_id, [&]() {
        if (!latches_released &&
            thread_ctx->get_state() != Parallel_reader::State::THREAD) {
          thread_ctx->savepoint();
          latches_released = true;
        }
        return DB_SUCCESS;
      });

      if (err != DB_SUCCESS && err != DB_END_OF_INDEX) {
        return err;
      }
    }

    if (latches_released) {
      ut_a(row.m_ptr != nullptr);
      /* Resume from the savepoint (above). */
      return thread_ctx->restore_from_savepoint();
    }

    return DB_SUCCESS;
  };

  size_t nr{};

  /* current_thread is a thread local variable. Set current_thd it
  to the user thread's THD  instance so that the debug sync calls
  will trigger for the spawned threads too. */
  IF_DEBUG(reader.set_start_callback([&](Thread_ctx *thread_ctx) {
    if (thread_ctx->get_state() == Parallel_reader::State::THREAD) {
      current_thd = m_ctx.thd();
    }
    return DB_SUCCESS;
  });)

  /* Called when a thread finishes traversing a page and when it completes. */
  reader.set_finish_callback([&](Thread_ctx *thread_ctx) {
    dberr_t err{DB_SUCCESS};
    const auto thread_id = thread_ctx->m_thread_id;

    switch (thread_ctx->get_state()) {
      case Parallel_reader::State::PAGE:
        if (!batch_insert.empty()) {
          err = batch_inserter(thread_ctx);
        }

        /* Reset the heap. Note: row.m_offsets and row.m_ptr are invalid now. */
        mem_heap_empty(m_heaps[thread_id]);

        nr += n_rows[thread_id];

        for (auto &builder : builders) {
          if (builder->stage() != nullptr) {
            builder->stage()->n_pk_recs_inc(n_rows[thread_id]);
            builder->stage()->inc(1);
          }
        }

        n_rows[thread_id] = 0;

        /* End of page counter. */
        return err;

      case Parallel_reader::State::THREAD: {
        ut_a(n_rows[thread_id] == 0);

        /* End of index scan. */
        auto &row = rows[thread_id];

        row.m_ptr = nullptr;

        m_eof = true;

        return bulk_inserter(thread_ctx, row);
      }

      case Parallel_reader::State::CTX:
        return DB_SUCCESS;

      case Parallel_reader::State::UNKNOWN:
        ut_error;
    }
    return DB_ERROR;
  });

  Parallel_reader::Config config(FULL_SCAN, index());

  /* Called for each row during the scan. */
  auto err = reader.add_scan(
      /* Ignore read views for non-online scans. */
      m_ctx.m_online ? m_ctx.m_trx : nullptr, config,
      [&](const Parallel_reader::Ctx *read_ctx) {
        const auto thread_id = read_ctx->thread_id();

        auto &row = rows[thread_id];
        auto heap = m_heaps[thread_id];

        row.m_rec = read_ctx->m_rec;

        row.m_offsets =
            rec_get_offsets(row.m_rec, index(), nullptr, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

#ifdef UNIV_DEBUG
        {
          const auto rec = row.m_rec;
          const auto is_comp = dict_table_is_comp(m_ctx.m_old_table);

          ut_ad(!rec_get_deleted_flag(rec, is_comp));

          /* When !online, we are holding a lock on old_table, preventing
          any inserts that could have written a record 'stub' before
          writing out off-page columns. */
          ut_ad(m_ctx.m_online ||
                !rec_offs_any_null_extern(index(), rec, row.m_offsets));
        }
#endif /* UNIV_DEBUG */

        /* We only need to copy the data when the heap is emptied.
        @see Parallel_cursor::copy_row */
        auto err = row.build(m_ctx, m_index, heap, ROW_COPY_POINTERS);

        if (err != DB_SUCCESS) {
          return err;
        }

        err = bulk_inserter(read_ctx->thread_ctx(), row);

        ++n_rows[thread_id];

        return err;
      });

  if (err == DB_SUCCESS) {
    err = reader.run(n_threads);

    if (err == DB_OUT_OF_RESOURCES) {
      ut_a(!m_single_threaded_mode);

      ib::warn(ER_INNODB_OUT_OF_RESOURCES)
          << "Resource not available to create threads for parallel scan."
          << " Falling back to single thread mode.";

      reader.release_threads(n_threads);

      /* No need to for the extra thread states, release them. */
      for (auto builder : builders) {
        builder->fallback_to_single_thread();
      }

      err = reader.run(0);
    }
  }

  /* We completed reading the PK, now we can call its end
  in order to calculate metrics based on it. */
  for (auto &builder : builders) {
    if (builder->stage() != nullptr) {
      builder->stage()->end_phase_read_pk();
    }
  }
  return cleanup(m_heaps, err);
}

dberr_t Parallel_cursor::copy_row(size_t thread_id, Row &row) noexcept {
  ut_a(!eof());

  auto heap = m_heaps[thread_id];

  row.m_offsets = rec_get_offsets(row.m_rec, index(), nullptr, ULINT_UNDEFINED,
                                  UT_LOCATION_HERE, &heap);

  return row.build(m_ctx, index(), heap, ROW_COPY_DATA);
}

Cursor *Cursor::create_cursor(ddl::Context &ctx) noexcept {
  return ut::new_withkey<Parallel_cursor>(ut::make_psi_memory_key(mem_key_ddl),
                                          ctx);
}

}  // namespace ddl
