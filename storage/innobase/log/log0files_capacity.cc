/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file log/log0files_capacity.cc

`physical_capacity`
(total size on disk except tmp files)
|
+- `*2/LOG_N_FILES`
|  ("snake's head&tail cages")
|
+- `*FREE_RATIO`
   ("the snake's body")
   |
   +- `OVERHEAD`
   |  (space which shouldn't be used for deltas)
   |  |
   |  +- `(LOG_N_FILES - 1) * LOG_FILE_HDR_SIZE`
   |  |  (file headers in cages with snake's body)
   |  |
   |  +- `LOG_EXTRA_SAFETY_MARGIN`
   |     (just in case)
   |
   +- `lsn_capacity`
      (space to use for deltas)
      |
      +- `next_file_earlier_margin`
      |  ("snake's tongue" - we want to never *need* more than LOG_N_FILES-1)
      |
      +- `hard_logical_capacity`
         (this is how log writer sees the capacity)
         |
         +- `*LOG_EXTRA_WRITER_MARGIN_PCT/100`
         |  (log writer's private workspace to pull of "desperate rescue")
         |
         +- `soft_logical_capacity`
            (this is how threads other than log writer see the capacity)
            |
            +- `free_check_margin`
            |  (it's "reserved" - you may write to it, if you did reservation)
            |  |
            |  +- `concurrency_margin`
            |  |  (space jointly reserved by threads in log_free_check_wait)
            |  |  |
            |  |  +- `margin_per_thread * max_total_threads`
            |  |  |
            |  |  +- `LOG_FILES_DUMMY_INTAKE_SIZE`
            |  |  |  (dummy intake which might be required during redo resize)
            |  |  |
            |  |  +- `LOG_EXTRA_CONC_MARGIN_PCT / 100.0 * soft_logical_capacity`
            |  |     (just in case)
            |  |
            |  +- `dict_persist_margin`
            |     (reserved for future dd metadata changes writes on checkpoint)
            |
            +- `log_free_check_capacity`
               (if redo is this long threads should wait in log_free_check_wait)

 *******************************************************/

/* ceil, floor */
#include <cmath>

/* arch_log_sys */
#include "arch0arch.h"

/* log_free_check_margin, log_free_check_capacity */
#include "log0chkp.h"

/* Log_files_capacity */
#include "log0files_capacity.h"

/* log_get_lsn, log_limits_mutex */
#include "log0log.h"

/* log_t::X */
#include "log0sys.h"

/* log_sync_point */
#include "log0test.h"

/* LOG_CHECKPOINT_FREE_PER_THREAD, ... */
#include "log0types.h"

/* srv_thread_concurrency */
#include "srv0conc.h"

/* MONITOR_SET */
#include "srv0mon.h"

/* srv_is_being_started */
#include "srv0start.h"

/* ut_uint64_align_down */
#include "ut0byte.h"

#ifndef UNIV_HOTBACKUP

void log_files_capacity_get_limits(const log_t &log,
                                   lsn_t &limit_for_free_check,
                                   lsn_t &limit_for_dirty_page_age) {
  IB_mutex_guard limits_latch{&(log.limits_mutex), UT_LOCATION_HERE};

  const lsn_t adaptive_flush_min_age = log.m_capacity.adaptive_flush_min_age();
  ut_a(adaptive_flush_min_age != 0);

  const lsn_t margin = log_free_check_margin(log);
  ut_a(margin < adaptive_flush_min_age);

  limit_for_free_check = log_free_check_capacity(log, margin);

  limit_for_dirty_page_age = ut_uint64_align_down(
      adaptive_flush_min_age - margin, OS_FILE_LOG_BLOCK_SIZE);
}

#endif /* !UNIV_HOTBACKUP */

/**************************************************/ /**

 @name Log_files_capacity implementation

 *******************************************************/

/** @{ */

void Log_files_capacity::initialize(const Log_files_dict &files,
                                    lsn_t current_logical_size,
                                    lsn_t current_checkpoint_age) {
  m_resize_mode = Log_resize_mode::NONE;
  m_current_physical_capacity = LOG_CAPACITY_MAX;

  os_offset_t min_t = LOG_CAPACITY_MIN, max_t = LOG_CAPACITY_MAX;

  /* One could compute the m_target_physical_capacity backward by reverting
  computations made by criteria inside is_target_reached_for_resizing_down(),
  but the binary-search-based approach seems safer and is fast enough. */
  while (min_t / 1024 * 1024UL < max_t / 1024 * 1024UL) {
    m_target_physical_capacity =
        ut_uint64_align_down((min_t + max_t) / 2, 1024 * 1024UL);

    if (is_target_reached_for_resizing_down(files, current_logical_size)) {
      max_t = m_target_physical_capacity;
    } else {
      min_t = m_target_physical_capacity + 1024 * 1024UL;
    }
  }

  /* In external tools, which don't need to resize the redo log,
  there is srv_redo_log_capacity == srv_redo_log_capacity_used == 0
  (no target for redo size). */
  ut_a(LOG_CAPACITY_MIN <= srv_redo_log_capacity_used ||
       (srv_redo_log_capacity == 0 && srv_redo_log_capacity_used == 0));

  m_target_physical_capacity = m_current_physical_capacity =
      std::max(max_t, os_offset_t{srv_redo_log_capacity_used});

  ut_a(is_target_reached_for_resizing_down(files, current_logical_size));

  update_exposed(
      hard_logical_capacity_for_physical(m_current_physical_capacity));

  update(files, current_logical_size, current_checkpoint_age);
}

void Log_files_capacity::update(const Log_files_dict &files,
                                lsn_t current_logical_size,
                                lsn_t current_checkpoint_age) {
  ut_a(m_current_physical_capacity > 0);

  /* Check if a new goal has been set and start a new resize if needed
  (cancelling a pending resize if there is one). */
  update_target();

  /* Check if the existing goal has been reached. */
  update_if_target_reached(files, current_logical_size);

  const lsn_t hard_logical_capacity =
      get_suggested_hard_logical_capacity(current_checkpoint_age);

  /* Update fields of m_exposed, which describe logical capacity limitations,
  which are exposed by this class to the page cleaner coordinator and the log
  writer threads. */
  update_exposed(hard_logical_capacity);

  /* Update InnoDB status variables reflecting all possible changes which have
  been done within this update() call. */
  update_status_variables(files, current_logical_size);
}

void Log_files_capacity::cancel_resize() {
  if (m_resize_mode == Log_resize_mode::NONE) {
    /* There is no resize in progress, return now to avoid emitting
    the message to the error log. */
    return;
  }
  m_resize_mode = Log_resize_mode::NONE;
  m_target_physical_capacity = m_current_physical_capacity;
  ib::info(ER_IB_MSG_LOG_FILES_RESIZE_CANCELLED);
}

void Log_files_capacity::update_target() {
  const os_offset_t target_physical_capacity = srv_redo_log_capacity_used;
  if (m_target_physical_capacity == target_physical_capacity) {
    /* Target has not been changed since last call to update_target().
    Return now to avoid emitting messages to the error log. */
    return;
  }
  if (target_physical_capacity == 0) {
    /* There is no target. No resize is needed. This allows to use
    Log_files_capacity in external tools which don't need to resize
    the redo log. */
    return;
  }

  /* Target has been changed (the innodb_redo_log_capacity has been changed),
  so first: cancel any resize operation which possibly is in progress. */
  cancel_resize();

  /* There is no resize in progress now. */
  ut_a(m_resize_mode == Log_resize_mode::NONE);
  ut_a(m_current_physical_capacity == m_target_physical_capacity);

  /* Start a new resize if needed. Note, that user could have started
  a downsize operation and then reset the innodb_redo_log_capacity to its
  previous value (equal to m_current_physical_capacity). In such case,
  it is enough that the cancel_resize() emitted message to the error log,
  and all the required work has already been done by the cancel_resize(). */
  if (target_physical_capacity != m_current_physical_capacity) {
    m_target_physical_capacity = target_physical_capacity;

    ib::info(ER_IB_MSG_LOG_FILES_RESIZE_REQUESTED,
             ulonglong{m_current_physical_capacity} / (1024 * 1024UL),
             ulonglong{m_target_physical_capacity} / (1024 * 1024UL));

    if (m_target_physical_capacity < m_current_physical_capacity) {
      m_resize_mode = Log_resize_mode::RESIZING_DOWN;
    } else {
      ut_a(m_resize_mode == Log_resize_mode::NONE);
      m_current_physical_capacity = m_target_physical_capacity;
      ib::info(ER_IB_MSG_LOG_FILES_RESIZE_FINISHED,
               ulonglong{m_current_physical_capacity} / (1024 * 1024UL));
    }
  }

  ut_a(m_target_physical_capacity <= m_current_physical_capacity);
}

lsn_t Log_files_capacity::hard_logical_capacity_for_physical(
    os_offset_t physical_capacity) {
  constexpr auto LOG_CONCURRENT_MARGIN_MIN = log_translate_sn_to_lsn(
      LOG_BACKGROUND_THREADS_USING_RW_MTRS * LOG_CHECKPOINT_FREE_PER_THREAD *
          UNIV_PAGE_SIZE_MAX +
      LOG_FILES_DUMMY_INTAKE_SIZE);

  constexpr auto LOG_WRITER_SOFT_CAPACITY_MIN =
      UNIV_PAGE_SIZE_MAX +
      LOG_CONCURRENT_MARGIN_MIN / (LOG_CONCCURENCY_MARGIN_MAX_PCT / 100.0);

  constexpr auto LOG_WRITER_HARD_CAPACITY_MIN =
      LOG_WRITER_SOFT_CAPACITY_MIN / (1 - LOG_EXTRA_WRITER_MARGIN_PCT / 100.0);

  /* The goal is to have one file free, so InnoDB could always create a next
  redo log file. Because logical redo data might begin at the very end of the
  oldest redo file, and end at the very beginning of the newest file, we need
  to ensure its size leaves room for at least two files, which would guarantee
  that at most LOG_N_FILES-1 exist. */
  constexpr double FREE_FILE_RATIO = 1.0 * (LOG_N_FILES - 2) / LOG_N_FILES;

  /* At most LOG_N_FILES - 1 are expected to be seen (LOG_N_FILES for a short
  moment when new redo file is created but the oldest hasn't yet been removed,
  but in this case, the oldest file isn't providing any capacity).

  Each of these files has header which occupies LOG_FILE_HDR_SIZE bytes. That
  gives (LOG_N_FILES - 1) * LOG_FILE_HDR_SIZE bytes on disk which do not give
  any space for bytes counted to lsn sequence. Additionally, it is guaranteed
  that extra LOG_EXTRA_SAFETY_MARGIN bytes, within the space occupied on disk,
  are never allocated to redo data. */

  const os_offset_t OVERHEAD =
      (LOG_N_FILES - 1) * LOG_FILE_HDR_SIZE + LOG_EXTRA_SAFETY_MARGIN;

  constexpr lsn_t NEXT_FILE_EARLIER_MARGIN_FOR_LOG_CAPACITY_MIN =
      (LOG_NEXT_FILE_EARLIER_MARGIN / 100.0) * LOG_CAPACITY_MIN / LOG_N_FILES +
      OS_FILE_LOG_BLOCK_SIZE;

  static_assert(
      LOG_WRITER_HARD_CAPACITY_MIN + OVERHEAD + OS_FILE_LOG_BLOCK_SIZE +
              NEXT_FILE_EARLIER_MARGIN_FOR_LOG_CAPACITY_MIN <=
          LOG_CAPACITY_MIN * FREE_FILE_RATIO,
      "The minimum redo capacity should be considered good enough for "
      "innodb_thread_concurrency = 0 and 64k pages.");

  ut_a(LOG_CAPACITY_MIN <= physical_capacity);

  /* Combining these two assertions we get: */
  ut_a(LOG_WRITER_HARD_CAPACITY_MIN + OS_FILE_LOG_BLOCK_SIZE <=
       physical_capacity * FREE_FILE_RATIO - OVERHEAD);

  const lsn_t lsn_capacity = physical_capacity * FREE_FILE_RATIO - OVERHEAD;

  const auto ret = ut_uint64_align_down(
      lsn_capacity - next_file_earlier_margin(physical_capacity),
      OS_FILE_LOG_BLOCK_SIZE);

  ut_a(LOG_WRITER_HARD_CAPACITY_MIN <= ret);

  return ret;
}

bool Log_files_capacity::is_target_reached_for_logical_size(
    lsn_t current_logical_size) const {
  return current_logical_size <=
         soft_logical_capacity_for_hard(
             hard_logical_capacity_for_physical(m_target_physical_capacity));
}

bool Log_files_capacity::is_target_reached_for_physical_size(
    os_offset_t current_physical_size) const {
  /* Note, that is_target_reached_for_logical_size() guarantees that this
  condition should also hold unless there was no time to consume old redo
  log files yet or other than checkpointer redo log consumers prevented the
  consumption. Note, that the log_files_governor updates Log_files_capacity
  before it decides if consumption is needed. Also, after starting up InnoDB
  it might happen that the oldest files might be consumed, but before actual
  consumption is done, InnoDB must not assume redo is resized down. */
  constexpr double FREE_FILE_RATIO = (LOG_N_FILES - 1) * 1.0 / LOG_N_FILES;
  return current_physical_size <= FREE_FILE_RATIO * m_target_physical_capacity;
}

bool Log_files_capacity::is_target_reached_for_max_file_size(
    const Log_files_dict &files) const {
  auto it = log_files_find_largest(files);

  return it == files.end() ||
         it->m_size_in_bytes <= m_target_physical_capacity / LOG_N_FILES;
}

bool Log_files_capacity::is_target_reached_for_resizing_down(
    const Log_files_dict &files, lsn_t current_logical_size) const {
  return is_target_reached_for_logical_size(current_logical_size) &&
         is_target_reached_for_physical_size(
             log_files_size_of_existing_files(files)) &&
         is_target_reached_for_max_file_size(files);
}

void Log_files_capacity::update_if_target_reached(const Log_files_dict &files,
                                                  lsn_t current_logical_size) {
  switch (m_resize_mode) {
    case Log_resize_mode::NONE:
      break;
    case Log_resize_mode::RESIZING_DOWN:
      if (is_target_reached_for_resizing_down(files, current_logical_size)) {
        m_current_physical_capacity = m_target_physical_capacity;
        m_resize_mode = Log_resize_mode::NONE;
        ib::info(ER_IB_MSG_LOG_FILES_RESIZE_FINISHED,
                 ulonglong{m_current_physical_capacity} / (1024 * 1024UL));
      }
      break;
    default:
      ut_error;
  }
}

void Log_files_capacity::update_status_variables(const Log_files_dict &files,
                                                 lsn_t current_logical_size) {
  const auto current_physical_size = log_files_size_of_existing_files(files);
  ut_a(current_physical_size <= m_current_physical_capacity);

  update_resize_status_variable();

  export_vars.innodb_redo_log_logical_size = ulonglong{current_logical_size};
  export_vars.innodb_redo_log_physical_size = ulonglong{current_physical_size};
  export_vars.innodb_redo_log_capacity_resized =
      ulonglong{m_current_physical_capacity};

#ifndef UNIV_HOTBACKUP
  log_sync_point("log_status_variables_updated");
#endif /* !UNIV_HOTBACKUP */
}

void Log_files_capacity::update_resize_status_variable() {
  const char *status;
  switch (m_resize_mode) {
    case Log_resize_mode::NONE:
      status = "OK";
      break;
    case Log_resize_mode::RESIZING_DOWN:
      status = "Resizing down";
      break;
    default:
      ut_error;
      return;
  }
  ut_a(sizeof(export_vars.innodb_redo_log_resize_status) >= strlen(status) + 1);
  strcpy(export_vars.innodb_redo_log_resize_status, status);
}

lsn_t Log_files_capacity::guess_hard_logical_capacity_for_soft(
    lsn_t soft_logical_capacity) {
  const double ratio = 1.0 - LOG_EXTRA_WRITER_MARGIN_PCT / 100.0;
  return ut_uint64_align_up(
      static_cast<lsn_t>(ceil(soft_logical_capacity / ratio)),
      OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Log_files_capacity::soft_logical_capacity_for_hard(
    lsn_t hard_logical_capacity) {
  const double ratio = 1.0 - LOG_EXTRA_WRITER_MARGIN_PCT / 100.0;
  return ut_uint64_align_down(
      static_cast<lsn_t>(floor(hard_logical_capacity * ratio)),
      OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Log_files_capacity::guess_soft_logical_capacity_for_sync_flush(
    lsn_t adaptive_flush_max_age) {
  const double ratio = 1.0 - 1.0 / LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MAX;
  return ut_uint64_align_up(
      static_cast<lsn_t>(ceil(adaptive_flush_max_age / ratio)),
      OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Log_files_capacity::sync_flush_logical_capacity_for_soft(
    lsn_t soft_logical_capacity) {
  const double ratio = 1.0 - 1.0 / LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MAX;
  return ut_uint64_align_down(
      static_cast<lsn_t>(floor(soft_logical_capacity * ratio)),
      OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Log_files_capacity::get_suggested_hard_logical_capacity(
    lsn_t current_checkpoint_age) const {
  switch (m_resize_mode) {
    case Log_resize_mode::NONE:
      ut_a(m_current_physical_capacity == m_target_physical_capacity);
      return hard_logical_capacity_for_physical(m_current_physical_capacity);

    case Log_resize_mode::RESIZING_DOWN:
      return std::max(
          hard_logical_capacity_for_physical(m_target_physical_capacity),
          std::min(m_exposed.m_hard_logical_capacity.load(),
                   guess_hard_logical_capacity_for_soft(
                       guess_soft_logical_capacity_for_sync_flush(
                           current_checkpoint_age))));

    default:
      ut_error;
  }
}

void Log_files_capacity::update_exposed(lsn_t hard_logical_capacity) {
  const lsn_t soft_logical_capacity =
      soft_logical_capacity_for_hard(hard_logical_capacity);

  ut_a(2 * OS_FILE_LOG_BLOCK_SIZE <= soft_logical_capacity);

  m_exposed.m_hard_logical_capacity.store(hard_logical_capacity);

  m_exposed.m_soft_logical_capacity.store(soft_logical_capacity);

  /* Set limits used in flushing and checkpointing mechanism. */

  m_exposed.m_adaptive_flush_max_age.store(
      sync_flush_logical_capacity_for_soft(soft_logical_capacity));

  m_exposed.m_adaptive_flush_min_age.store(ut_uint64_align_down(
      soft_logical_capacity -
          soft_logical_capacity / LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MIN,
      OS_FILE_LOG_BLOCK_SIZE));

  m_exposed.m_agressive_checkpoint_min_age.store(ut_uint64_align_down(
      soft_logical_capacity -
          soft_logical_capacity / LOG_AGGRESSIVE_CHECKPOINT_RATIO_MIN,
      OS_FILE_LOG_BLOCK_SIZE));
}

lsn_t Log_files_capacity::hard_logical_capacity() const {
  return m_exposed.m_hard_logical_capacity.load();
}

lsn_t Log_files_capacity::soft_logical_capacity() const {
  return m_exposed.m_soft_logical_capacity.load();
}

lsn_t Log_files_capacity::adaptive_flush_min_age() const {
  return m_exposed.m_adaptive_flush_min_age.load();
}

lsn_t Log_files_capacity::adaptive_flush_max_age() const {
  return m_exposed.m_adaptive_flush_max_age.load();
}

lsn_t Log_files_capacity::aggressive_checkpoint_min_age() const {
  return m_exposed.m_agressive_checkpoint_min_age.load();
}

bool Log_files_capacity::is_resizing_down() const {
  return m_target_physical_capacity < m_current_physical_capacity;
}

os_offset_t Log_files_capacity::target_physical_capacity() const {
  return m_target_physical_capacity;
}

os_offset_t Log_files_capacity::current_physical_capacity() const {
  return m_current_physical_capacity;
}

os_offset_t Log_files_capacity::next_file_size() const {
  return next_file_size(m_target_physical_capacity);
}

os_offset_t Log_files_capacity::next_file_size(os_offset_t physical_capacity) {
  const auto file_size =
      ut_uint64_align_down(physical_capacity / LOG_N_FILES, UNIV_PAGE_SIZE);
  ut_a(LOG_FILE_MIN_SIZE <= file_size);
  ut_a(file_size <= LOG_FILE_MAX_SIZE);
  ut_a(file_size % UNIV_PAGE_SIZE == 0);
  return file_size;
}

lsn_t Log_files_capacity::next_file_earlier_margin(
    os_offset_t physical_capacity) {
  const auto file_size = next_file_size(physical_capacity);
  return ut_uint64_align_up(
      ceil(LOG_NEXT_FILE_EARLIER_MARGIN / 100.0 * file_size),
      OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Log_files_capacity::next_file_earlier_margin() const {
  return next_file_earlier_margin(m_target_physical_capacity);
}

/** @} */
