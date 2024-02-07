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

/** @file ddl/ddl0loader.cc
 DDL index loader implementation.
 Created 2020-11-01 by Sunny Bains. */

#include "btr0load.h"
#include "ddl0impl-builder.h"
#include "ddl0impl-cursor.h"
#include "ddl0impl-loader.h"
#include "handler0alter.h"
#include "os0thread-create.h"
#include "ut0stage.h"

#include "sql/table.h"
#include "sql_class.h"

namespace ddl {

/** Unbounded task queue. */
class Loader::Task_queue {
 public:
  /** Constructor.
  @param[in] ctx                DDL context.
  @param[in] sync               True for synchronous execution. */
  explicit Task_queue(const Context &ctx, bool sync) noexcept;

  /** Destructor. */
  ~Task_queue() noexcept;

  /** Enqueue a task to execute.
  @param[in] task               Task to queue. */
  void enqueue(const Task &task) noexcept {
    if (!m_sync) {
      mutex_enter(&m_mutex);
    }

    m_tasks.push_back(task);
    IF_DEBUG(++m_n_tasks_submitted;)

    if (!m_sync) {
      mutex_exit(&m_mutex);
      os_event_set(m_consumer_event);
    }
  }

  /** Dequeue and execute a task.
  @return DB_SUCCESS or error code. */
  dberr_t execute() {
    if (!m_sync) {
      ut_a(m_n_threads >= 1);
      return mt_execute();
    } else {
      m_n_threads = 0;
      return st_execute();
    }
  }

  /** Note that we failed to create the configured number of threads. */
  void thread_create_failed() noexcept {
    ut_a(!m_sync);
    mutex_enter(&m_mutex);
    ut_a(m_n_threads > 0);
    --m_n_threads;
    mutex_exit(&m_mutex);
  }

#ifdef UNIV_DEBUG
  /** @return true if number of tasks submitted == number of tasks executed. */
  bool validate() const noexcept {
    return m_n_tasks_executed == m_n_tasks_submitted && m_n_threads == 0 &&
           m_n_idle == 0;
  }
#endif /* UNIV_DEBUG */

  /** Wakeup the other threads e.g., on error. */
  void signal() noexcept { os_event_set(m_consumer_event); }

 private:
  /** Execute function when there is more than one thread. The general idea
  is as follows:
   1. Some initial tasks are added before threads come here to execute tasks.
   2. While executing, a task can generate more tasks. That is the only way
     a task can be added.

  [1] & [2] implies that when all threads are idle, all tasks are completed
  and no more tasks can be added.

  We exit here when all running threads are idle. */
  dberr_t mt_execute() {
    ut_a(!m_sync);

    dberr_t err{DB_SUCCESS};

    do {
      mutex_enter(&m_mutex);

      while (m_tasks.empty()) {
        const auto sig_count = os_event_reset(m_consumer_event);

        ++m_n_idle;

        if (m_n_idle >= m_n_threads) {
          ut_a(m_n_threads > 0);
          --m_n_threads;

          ut_a(m_n_idle > 0);
          --m_n_idle;

          mutex_exit(&m_mutex);

          os_event_set(m_consumer_event);

          return DB_SUCCESS;
        }

        mutex_exit(&m_mutex);

        os_event_wait_low(m_consumer_event, sig_count);

        err = m_ctx.get_error();

        if (err != DB_SUCCESS) {
          mutex_enter(&m_mutex);

          ut_a(m_n_threads > 0);
          --m_n_threads;

          ut_a(m_n_idle > 0);
          --m_n_idle;

          mutex_exit(&m_mutex);

          os_event_set(m_consumer_event);

          return err;
        }

        os_event_reset(m_consumer_event);

        mutex_enter(&m_mutex);

        --m_n_idle;
      }

      auto task = m_tasks.front();
      m_tasks.pop_front();

      IF_DEBUG(++m_n_tasks_executed;)

      mutex_exit(&m_mutex);

      err = task();

    } while (err == DB_SUCCESS);

    mutex_enter(&m_mutex);

    ut_a(m_n_threads > 0);
    --m_n_threads;

    mutex_exit(&m_mutex);

    os_event_set(m_consumer_event);

    return err;
  }

  /** Execute function when there is a single thread. */
  dberr_t st_execute() {
    ut_a(m_sync);

    dberr_t err{DB_SUCCESS};

    while (!m_tasks.empty()) {
      auto task = m_tasks.front();
      m_tasks.pop_front();

      IF_DEBUG(++m_n_tasks_executed;)

      err = task();

      if (err != DB_SUCCESS) {
        break;
      }
    }

    return err;
  }

 private:
  using Tasks = std::deque<Task, ut::allocator<Task>>;

  /** DDL context. */
  const Context &m_ctx;

  /** true if synchronous execution model. */
  bool m_sync{};

  /** The task queue. */
  Tasks m_tasks{};

  /** Mutex protecting m_tasks access. */
  ib_mutex_t m_mutex;

  /** Task queue consumer event. */
  os_event_t m_consumer_event{};

  /** Number of threads (including foreground thread). */
  size_t m_n_threads{};

  /** Number of threads idle. */
  size_t m_n_idle{};

  /** Number of tasks executed. */
  IF_DEBUG(size_t m_n_tasks_executed{};)

  /** Number of tasks submitted. */
  IF_DEBUG(size_t m_n_tasks_submitted{};)
};

Loader::Task_queue::Task_queue(const Context &ctx, bool sync) noexcept
    : m_ctx(ctx), m_sync(sync), m_n_threads(ctx.m_max_threads) {
  if (!m_sync) {
    m_consumer_event = os_event_create();
    mutex_create(LATCH_ID_WORK_QUEUE, &m_mutex);
  }
}

Loader::Task_queue::~Task_queue() noexcept {
  if (!m_sync) {
    mutex_destroy(&m_mutex);

    if (m_consumer_event != nullptr) {
      os_event_destroy(m_consumer_event);
    }
  }
}

Loader::Loader(Context &ctx) noexcept : m_ctx(ctx) {}

Loader::~Loader() noexcept {
  if (m_ctx.m_stage != nullptr) {
    Alter_stages alter_stages{};

    for (auto &builder : m_builders) {
      alter_stages.push_back(builder->stage());
    }

    m_ctx.m_stage->aggregate(alter_stages);
  }

  for (auto builder : m_builders) {
    ut::delete_(builder);
  }

  if (m_taskq != nullptr) {
    ut::delete_(m_taskq);
  }
}

void Loader::add_task(Task task) noexcept { m_taskq->enqueue(task); }

dberr_t Loader::load() noexcept {
  ut_a(m_taskq == nullptr);

  const bool sync = m_ctx.m_max_threads <= 1;

  m_taskq = ut::new_withkey<Task_queue>(ut::make_psi_memory_key(mem_key_ddl),
                                        m_ctx, sync);

  if (m_taskq == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  for (auto builder : m_builders) {
    ut_a(builder->get_state() == Builder::State::ADD);
    /* RTrees are built during the scan phase, using row by row insert. */
    if (!builder->is_spatial_index()) {
      builder->set_next_state();
      add_task(Task{builder});
    }
  }

  std::vector<std::thread> threads{};

  if (!sync) {
    auto fn = [this](PSI_thread_seqnum seqnum) -> dberr_t {
#ifdef UNIV_PFS_THREAD
      Runnable runnable{ddl_thread_key, seqnum};
#else
      Runnable runnable{PSI_NOT_INSTRUMENTED, seqnum};
#endif /* UNIV_PFS_THREAD */

      current_thd = nullptr;

      const auto err = runnable(&Task_queue::execute, m_taskq);

      if (err != DB_SUCCESS) {
        m_taskq->signal();
      }

      return err;
    };

    for (size_t i = 1; i < m_ctx.m_max_threads; ++i) {
      try {
        threads.push_back(std::thread{fn, i});
      } catch (...) {
        ib::warn(ER_DDL_MSG_1);
        m_taskq->thread_create_failed();
        break;
      }
    }
  }

  auto err = m_taskq->execute();

  if (!sync) {
    if (err != DB_SUCCESS) {
      m_taskq->signal();
    }

    for (auto &thread : threads) {
      thread.join();
    }
  }

  if (err == DB_SUCCESS) {
    err = m_ctx.get_error();
  }

  ut_ad(m_taskq->validate() || err != DB_SUCCESS);

  ut::delete_(m_taskq);
  m_taskq = nullptr;

  return err;
}

dberr_t Loader::prepare() noexcept {
  ut_a(m_builders.empty());
  ut_a(!srv_read_only_mode);
  ut_a(!m_ctx.m_add_cols || m_ctx.m_col_map != nullptr);
  ut_a((m_ctx.m_old_table == m_ctx.m_new_table) == !m_ctx.m_col_map);

  /* Allocate memory for merge file data structure and initialize fields */

  auto err = m_ctx.setup_fts_build();

  if (err != DB_SUCCESS) {
    return err;
  }

  for (size_t i = 0; i < m_ctx.m_indexes.size(); ++i) {
    auto builder = ut::new_withkey<Builder>(
        ut::make_psi_memory_key(mem_key_ddl), m_ctx, *this, i);

    if (builder == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    m_builders.push_back(builder);
  }

  return DB_SUCCESS;
}

#define SYNC_POINT_ADD(t, n)                      \
  DBUG_EXECUTE_IF((n), Sync_point::add((t), (n)); \
                  sync_points.push_back({(t), (n)});)

dberr_t Loader::scan_and_build_indexes() noexcept {
#ifdef UNIV_DEBUG
  using Sync_points = std::vector<std::pair<THD *, const std::string>>;

  auto thd = m_ctx.thd();
  Sync_points sync_points{};

  SYNC_POINT_ADD(thd, "ddl_tmpfile_fail");
  SYNC_POINT_ADD(thd, "ddl_read_failure");
  SYNC_POINT_ADD(thd, "ddl_write_failure");
  SYNC_POINT_ADD(thd, "ddl_ins_spatial_fail");
  SYNC_POINT_ADD(thd, "ddl_fts_write_failure");
  SYNC_POINT_ADD(thd, "ddl_merge_sort_interrupt");
  SYNC_POINT_ADD(thd, "ddl_instrument_log_check_flush");
  SYNC_POINT_ADD(thd, "fts_instrument_sync_interrupted");
  SYNC_POINT_ADD(thd, "ddl_btree_build_too_big_record");
  SYNC_POINT_ADD(thd, "ddl_btree_build_oom");
  SYNC_POINT_ADD(thd, "ddl_btree_build_interrupt");
  SYNC_POINT_ADD(thd, "ddl_btree_build_sleep");
  SYNC_POINT_ADD(thd, "ddl_btree_build_insert_return_interrupt");

  auto cleanup = [&]() {
    for (auto &s : sync_points) {
      Sync_point::erase(s.first, s.second);
    }
  };

#endif /* UNIV_DEBUG */

  auto cursor = Cursor::create_cursor(m_ctx);

  if (cursor == nullptr) {
    ut_d(cleanup());
    return DB_OUT_OF_MEMORY;
  }

  auto err = m_ctx.read_init(cursor);

  if (err == DB_SUCCESS) {
    cursor->open();

    /* Reset the MySQL row buffer that is used when reporting duplicate keys.
    Return needs to be checked since innobase_rec_reset tries to evaluate
    set_default() which can also be a function and might return errors */
    innobase_rec_reset(m_ctx.m_table);

    if (m_ctx.m_table->in_use->is_error()) {
      err = DB_COMPUTE_VALUE_FAILED;
    } else {
      /* Read clustered index of the table and create files for secondary
      index entries for merge sort and bulk build of the indexes. */
      err = cursor->scan(m_builders);
    }

    /* Close the mtr and release any locks, wait for FTS etc. */
    err = cursor->finish(err);

    DBUG_EXECUTE_IF("force_virtual_col_build_fail",
                    err = DB_COMPUTE_VALUE_FAILED;);

    DEBUG_SYNC_C("ddl_after_scan");

    if (err == DB_SUCCESS) {
      err = load();
    }

    DBUG_EXECUTE_IF("ddl_insert_big_row", err = DB_TOO_BIG_RECORD;);
  }

  if (cursor != nullptr) {
    ut::delete_(cursor);
    cursor = nullptr;
  }

  ut_d(cleanup());

  return err;
}

dberr_t Loader::build_all() noexcept {
  auto err = prepare();

  if (err == DB_SUCCESS) {
    err = scan_and_build_indexes();
  }

  DBUG_EXECUTE_IF("ib_build_indexes_too_many_concurrent_trxs",
                  err = DB_TOO_MANY_CONCURRENT_TRXS;
                  m_ctx.m_trx->error_state = err;);

  if (m_ctx.m_fts.m_ptr != nullptr) {
    /* Clean up FTS psort related resource */
    ut::delete_(m_ctx.m_fts.m_ptr);
    m_ctx.m_fts.m_ptr = nullptr;
  }

  DICT_TF2_FLAG_UNSET(m_ctx.m_new_table, DICT_TF2_FTS_ADD_DOC_ID);

  if (err == DB_AUTOINC_READ_ERROR) {
    auto trx = m_ctx.m_trx;
    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_AUTOINC_READ_FAILED);
  }

  if (err != DB_SUCCESS) {
    m_ctx.set_error(err);
  }

  return err;
}

#ifdef UNIV_DEBUG
bool Loader::validate_indexes() const noexcept {
  for (auto &builder : m_builders) {
    if (!builder->is_fts_index() &&
        !btr_validate_index(builder->index(), nullptr, false)) {
      return false;
    }
  }

  return true;
}
#endif /* UNIV_DEBUG */

}  // namespace ddl
