/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_STAT_H
#define PFS_STAT_H

#include <assert.h>
#include <algorithm>
#include <atomic>

#include "my_sys.h"
#include "my_systime.h"
#include "sql/sql_const.h"
#include "storage/perfschema/pfs_error.h"
#include "storage/perfschema/pfs_global.h"
/* memcpy */
#include "string.h"

struct PFS_builtin_memory_class;

/**
  @file storage/perfschema/pfs_stat.h
  Statistics (declarations).
*/

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Single statistic. */
struct PFS_single_stat {
  /** Count of values. */
  ulonglong m_count;
  /** Sum of values. */
  ulonglong m_sum;
  /** Minimum value. */
  ulonglong m_min;
  /** Maximum value. */
  ulonglong m_max;

  PFS_single_stat() {
    m_count = 0;
    m_sum = 0;
    m_min = ULLONG_MAX;
    m_max = 0;
  }

  inline void reset() {
    m_count = 0;
    m_sum = 0;
    m_min = ULLONG_MAX;
    m_max = 0;
  }

  inline bool has_timed_stats() const { return (m_min <= m_max); }

  inline void aggregate(const PFS_single_stat *stat) {
    if (stat->m_count != 0) {
      m_count += stat->m_count;
      m_sum += stat->m_sum;
      if (unlikely(m_min > stat->m_min)) {
        m_min = stat->m_min;
      }
      if (unlikely(m_max < stat->m_max)) {
        m_max = stat->m_max;
      }
    }
  }

  inline void aggregate_no_check(const PFS_single_stat *stat) {
    m_count += stat->m_count;
    m_sum += stat->m_sum;
    if (unlikely(m_min > stat->m_min)) {
      m_min = stat->m_min;
    }
    if (unlikely(m_max < stat->m_max)) {
      m_max = stat->m_max;
    }
  }

  inline void aggregate_counted() { m_count++; }

  inline void aggregate_counted(ulonglong count) { m_count += count; }

  inline void aggregate_value(ulonglong value) {
    m_count++;
    m_sum += value;
    if (unlikely(m_min > value)) {
      m_min = value;
    }
    if (unlikely(m_max < value)) {
      m_max = value;
    }
  }

  inline void aggregate_many_value(ulonglong value, ulonglong count) {
    m_count += count;
    m_sum += value;
    if (unlikely(m_min > value)) {
      m_min = value;
    }
    if (unlikely(m_max < value)) {
      m_max = value;
    }
  }
};

/** Combined statistic. */
struct PFS_byte_stat : public PFS_single_stat {
  /** Byte count statistics */
  ulonglong m_bytes;

  /** Aggregate wait stats, event count and byte count */
  inline void aggregate(const PFS_byte_stat *stat) {
    if (stat->m_count != 0) {
      PFS_single_stat::aggregate_no_check(stat);
      m_bytes += stat->m_bytes;
    }
  }

  /** Aggregate wait stats, event count and byte count */
  inline void aggregate_no_check(const PFS_byte_stat *stat) {
    PFS_single_stat::aggregate_no_check(stat);
    m_bytes += stat->m_bytes;
  }

  /** Aggregate individual wait time, event count and byte count */
  inline void aggregate(ulonglong wait, ulonglong bytes) {
    aggregate_value(wait);
    m_bytes += bytes;
  }

  /** Aggregate wait stats and event count */
  inline void aggregate_waits(const PFS_byte_stat *stat) {
    PFS_single_stat::aggregate(stat);
  }

  /** Aggregate event count and byte count */
  inline void aggregate_counted() { PFS_single_stat::aggregate_counted(); }

  /** Aggregate event count and byte count */
  inline void aggregate_counted(ulonglong bytes) {
    PFS_single_stat::aggregate_counted();
    m_bytes += bytes;
  }

  PFS_byte_stat() { reset(); }

  inline void reset() {
    PFS_single_stat::reset();
    m_bytes = 0;
  }
};

/** Statistics for mutex usage. */
struct PFS_mutex_stat {
  /** Wait statistics. */
  PFS_single_stat m_wait_stat;
#ifdef PFS_LATER
  /**
    Lock statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat m_lock_stat;
#endif

  inline void aggregate(const PFS_mutex_stat *stat) {
    m_wait_stat.aggregate(&stat->m_wait_stat);
#ifdef PFS_LATER
    m_lock_stat.aggregate(&stat->m_lock_stat);
#endif
  }

  inline void reset() {
    m_wait_stat.reset();
#ifdef PFS_LATER
    m_lock_stat.reset();
#endif
  }
};

/** Statistics for rwlock usage. */
struct PFS_rwlock_stat {
  /** Wait statistics. */
  PFS_single_stat m_wait_stat;
#ifdef PFS_LATER
  /**
    RWLock read lock usage statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat m_read_lock_stat;
  /**
    RWLock write lock usage statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat m_write_lock_stat;
#endif

  inline void aggregate(const PFS_rwlock_stat *stat) {
    m_wait_stat.aggregate(&stat->m_wait_stat);
#ifdef PFS_LATER
    m_read_lock_stat.aggregate(&stat->m_read_lock_stat);
    m_write_lock_stat.aggregate(&stat->m_write_lock_stat);
#endif
  }

  inline void reset() {
    m_wait_stat.reset();
#ifdef PFS_LATER
    m_read_lock_stat.reset();
    m_write_lock_stat.reset();
#endif
  }
};

/** Statistics for conditions usage. */
struct PFS_cond_stat {
  /** Wait statistics. */
  PFS_single_stat m_wait_stat;
#ifdef PFS_LATER
  /**
    Number of times a condition was signalled.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_signal_count;
  /**
    Number of times a condition was broadcast.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_broadcast_count;
#endif

  inline void aggregate(const PFS_cond_stat *stat) {
    m_wait_stat.aggregate(&stat->m_wait_stat);
#ifdef PFS_LATER
    m_signal_count += stat->m_signal_count;
    m_broadcast_count += stat->m_broadcast_count;
#endif
  }

  inline void reset() {
    m_wait_stat.reset();
#ifdef PFS_LATER
    m_signal_count = 0;
    m_broadcast_count = 0;
#endif
  }
};

/** Statistics for FILE I/O. Used for both waits and byte counts. */
struct PFS_file_io_stat {
  /** READ statistics */
  PFS_byte_stat m_read;
  /** WRITE statistics */
  PFS_byte_stat m_write;
  /** Miscellaneous statistics */
  PFS_byte_stat m_misc;

  inline void reset() {
    m_read.reset();
    m_write.reset();
    m_misc.reset();
  }

  inline void aggregate(const PFS_file_io_stat *stat) {
    m_read.aggregate(&stat->m_read);
    m_write.aggregate(&stat->m_write);
    m_misc.aggregate(&stat->m_misc);
  }

  /* Sum waits and byte counts */
  inline void sum(PFS_byte_stat *stat) {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }

  /* Sum waits only */
  inline void sum_waits(PFS_single_stat *stat) {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }
};

/** Statistics for FILE usage. */
struct PFS_file_stat {
  /** Number of current open handles. */
  ulong m_open_count;
  /** File I/O statistics. */
  PFS_file_io_stat m_io_stat;

  inline void aggregate(const PFS_file_stat *stat) {
    m_io_stat.aggregate(&stat->m_io_stat);
  }

  /** Reset file statistics. */
  inline void reset() { m_io_stat.reset(); }
};

/** Statistics for stage usage. */
struct PFS_stage_stat {
  PFS_single_stat m_timer1_stat;

  inline void reset() { m_timer1_stat.reset(); }

  inline void aggregate_counted() { m_timer1_stat.aggregate_counted(); }

  inline void aggregate_value(ulonglong value) {
    m_timer1_stat.aggregate_value(value);
  }

  inline void aggregate(const PFS_stage_stat *stat) {
    m_timer1_stat.aggregate(&stat->m_timer1_stat);
  }
};

/** Statistics for stored program usage. */
struct PFS_sp_stat {
  PFS_single_stat m_timer1_stat;

  inline void reset() { m_timer1_stat.reset(); }

  inline void aggregate_counted() { m_timer1_stat.aggregate_counted(); }

  inline void aggregate_value(ulonglong value) {
    m_timer1_stat.aggregate_value(value);
  }

  inline void aggregate(const PFS_stage_stat *stat) {
    m_timer1_stat.aggregate(&stat->m_timer1_stat);
  }
};

/** Statistics for prepared statement usage. */
struct PFS_prepared_stmt_stat {
  PFS_single_stat m_timer1_stat;

  inline void reset() { m_timer1_stat.reset(); }

  inline void aggregate_counted() { m_timer1_stat.aggregate_counted(); }

  inline void aggregate_value(ulonglong value) {
    m_timer1_stat.aggregate_value(value);
  }

  inline void aggregate(PFS_stage_stat *stat) {
    m_timer1_stat.aggregate(&stat->m_timer1_stat);
  }
};

/**
  Statistics for statement usage.
*/
struct PFS_statement_stat {
  PFS_single_stat m_timer1_stat;
  ulonglong m_error_count{0};
  ulonglong m_warning_count{0};
  ulonglong m_rows_affected{0};
  ulonglong m_lock_time{0};
  ulonglong m_rows_sent{0};
  ulonglong m_rows_examined{0};
  ulonglong m_created_tmp_disk_tables{0};
  ulonglong m_created_tmp_tables{0};
  ulonglong m_select_full_join{0};
  ulonglong m_select_full_range_join{0};
  ulonglong m_select_range{0};
  ulonglong m_select_range_check{0};
  ulonglong m_select_scan{0};
  ulonglong m_sort_merge_passes{0};
  ulonglong m_sort_range{0};
  ulonglong m_sort_rows{0};
  ulonglong m_sort_scan{0};
  ulonglong m_no_index_used{0};
  ulonglong m_no_good_index_used{0};
  /**
    CPU TIME.
    Expressed in STORAGE units (nanoseconds).
  */
  ulonglong m_cpu_time{0};
  ulonglong m_max_controlled_memory{0};
  ulonglong m_max_total_memory{0};
  ulonglong m_count_secondary{0};

  void reset() { new (this) PFS_statement_stat(); }

  void aggregate_counted() { m_timer1_stat.aggregate_counted(); }

  void aggregate_value(ulonglong value) {
    m_timer1_stat.aggregate_value(value);
  }

  void aggregate_memory_size(size_t controlled_size, size_t total_size) {
    if (controlled_size > m_max_controlled_memory) {
      m_max_controlled_memory = controlled_size;
    }
    if (total_size > m_max_total_memory) {
      m_max_total_memory = total_size;
    }
  }

  void aggregate(const PFS_statement_stat *stat) {
    if (stat->m_timer1_stat.m_count != 0) {
      m_timer1_stat.aggregate_no_check(&stat->m_timer1_stat);

      m_error_count += stat->m_error_count;
      m_warning_count += stat->m_warning_count;
      m_rows_affected += stat->m_rows_affected;
      m_lock_time += stat->m_lock_time;
      m_rows_sent += stat->m_rows_sent;
      m_rows_examined += stat->m_rows_examined;
      m_created_tmp_disk_tables += stat->m_created_tmp_disk_tables;
      m_created_tmp_tables += stat->m_created_tmp_tables;
      m_select_full_join += stat->m_select_full_join;
      m_select_full_range_join += stat->m_select_full_range_join;
      m_select_range += stat->m_select_range;
      m_select_range_check += stat->m_select_range_check;
      m_select_scan += stat->m_select_scan;
      m_sort_merge_passes += stat->m_sort_merge_passes;
      m_sort_range += stat->m_sort_range;
      m_sort_rows += stat->m_sort_rows;
      m_sort_scan += stat->m_sort_scan;
      m_no_index_used += stat->m_no_index_used;
      m_no_good_index_used += stat->m_no_good_index_used;
      m_cpu_time += stat->m_cpu_time;
      if (stat->m_max_controlled_memory > m_max_controlled_memory) {
        m_max_controlled_memory = stat->m_max_controlled_memory;
      }
      if (stat->m_max_total_memory > m_max_total_memory) {
        m_max_total_memory = stat->m_max_total_memory;
      }
      m_count_secondary += stat->m_count_secondary;
    }
  }
};

/** Statistics for transaction usage. */
struct PFS_transaction_stat {
  PFS_single_stat m_read_write_stat;
  PFS_single_stat m_read_only_stat;

  ulonglong m_savepoint_count;
  ulonglong m_rollback_to_savepoint_count;
  ulonglong m_release_savepoint_count;

  PFS_transaction_stat() {
    m_savepoint_count = 0;
    m_rollback_to_savepoint_count = 0;
    m_release_savepoint_count = 0;
  }

  ulonglong count() const {
    return (m_read_write_stat.m_count + m_read_only_stat.m_count);
  }

  inline void reset() {
    m_read_write_stat.reset();
    m_read_only_stat.reset();
    m_savepoint_count = 0;
    m_rollback_to_savepoint_count = 0;
    m_release_savepoint_count = 0;
  }

  inline void aggregate(const PFS_transaction_stat *stat) {
    m_read_write_stat.aggregate(&stat->m_read_write_stat);
    m_read_only_stat.aggregate(&stat->m_read_only_stat);
    m_savepoint_count += stat->m_savepoint_count;
    m_rollback_to_savepoint_count += stat->m_rollback_to_savepoint_count;
    m_release_savepoint_count += stat->m_release_savepoint_count;
  }
};

/** Statistics for a server error. */
struct PFS_error_single_stat {
  ulonglong m_count;
  ulonglong m_handled_count;
  /** First and last seen timestamps.*/
  ulonglong m_first_seen;
  ulonglong m_last_seen;

  PFS_error_single_stat() {
    m_count = 0;
    m_handled_count = 0;
    m_first_seen = 0;
    m_last_seen = 0;
  }

  ulonglong count() const { return m_count; }

  inline void reset() {
    m_count = 0;
    m_handled_count = 0;
    m_first_seen = 0;
    m_last_seen = 0;
  }

  inline void aggregate_count(int error_operation) {
    m_last_seen = my_micro_time();
    if (m_first_seen == 0) {
      m_first_seen = m_last_seen;
    }

    switch (error_operation) {
      case PSI_ERROR_OPERATION_RAISED:
        m_count++;
        break;
      case PSI_ERROR_OPERATION_HANDLED:
        m_handled_count++;
        m_count--;
        break;
      default:
        /* It must not be reached. */
        assert(0);
        break;
    }
  }

  inline void aggregate(const PFS_error_single_stat *stat) {
    if (stat->m_count == 0 && stat->m_handled_count == 0) {
      return;
    }

    m_count += stat->m_count;
    m_handled_count += stat->m_handled_count;

    if (m_first_seen == 0 || stat->m_first_seen < m_first_seen) {
      m_first_seen = stat->m_first_seen;
    }
    if (stat->m_last_seen > m_last_seen) {
      m_last_seen = stat->m_last_seen;
    }
  }
};

/** Statistics for all server errors. */
struct PFS_error_stat {
  /** The number of errors, including +1 for the NULL row. */
  size_t m_max_errors;
  PFS_error_single_stat *m_stat;

  PFS_error_stat() : m_max_errors(0), m_stat(nullptr) {}

  const PFS_error_single_stat *get_stat(uint error_index) const {
    return &m_stat[error_index];
  }

  ulonglong count() const {
    ulonglong total = 0;
    for (uint i = 0; i < m_max_errors; i++) {
      total += m_stat[i].count();
    }
    return total;
  }

  ulonglong count(uint error_index) const {
    return m_stat[error_index].count();
  }

  inline void init(PFS_builtin_memory_class *memory_class, size_t max_errors) {
    if (max_errors == 0) {
      return;
    }

    m_max_errors = max_errors;
    /* Allocate memory for errors' stats. The NULL row is already included. */
    m_stat = PFS_MALLOC_ARRAY(memory_class, m_max_errors,
                              sizeof(PFS_error_single_stat),
                              PFS_error_single_stat, MYF(MY_ZEROFILL));
    reset();
  }

  inline void cleanup(PFS_builtin_memory_class *memory_class) {
    if (m_stat == nullptr) {
      return;
    }

    PFS_FREE_ARRAY(memory_class, m_max_errors, sizeof(PFS_error_single_stat),
                   m_stat);
    m_stat = nullptr;
  }

  inline void reset() {
    if (m_stat == nullptr) {
      return;
    }

    for (uint i = 0; i < m_max_errors; i++) {
      m_stat[i].reset();
    }
  }

  inline void aggregate_count(int error_index, int error_operation) {
    if (m_stat == nullptr) {
      return;
    }

    PFS_error_single_stat *stat = &m_stat[error_index];
    stat->aggregate_count(error_operation);
  }

  inline void aggregate(const PFS_error_single_stat *stat, uint error_index) {
    if (m_stat == nullptr) {
      return;
    }

    assert(error_index < m_max_errors);
    m_stat[error_index].aggregate(stat);
  }

  inline void aggregate(const PFS_error_stat *stat) {
    if (m_stat == nullptr) {
      return;
    }

    /*
      Sizes can be different, for example when aggregating
      per session statistics into global statistics.
    */
    const size_t common_max = std::min(m_max_errors, stat->m_max_errors);
    for (uint i = 0; i < common_max; i++) {
      m_stat[i].aggregate(&stat->m_stat[i]);
    }
  }
};

/** Single table I/O statistic. */
struct PFS_table_io_stat {
  bool m_has_data;
  /** FETCH statistics */
  PFS_single_stat m_fetch;
  /** INSERT statistics */
  PFS_single_stat m_insert;
  /** UPDATE statistics */
  PFS_single_stat m_update;
  /** DELETE statistics */
  PFS_single_stat m_delete;

  PFS_table_io_stat() { m_has_data = false; }

  inline void reset() {
    m_has_data = false;
    m_fetch.reset();
    m_insert.reset();
    m_update.reset();
    m_delete.reset();
  }

  inline void aggregate(const PFS_table_io_stat *stat) {
    if (stat->m_has_data) {
      m_has_data = true;
      m_fetch.aggregate(&stat->m_fetch);
      m_insert.aggregate(&stat->m_insert);
      m_update.aggregate(&stat->m_update);
      m_delete.aggregate(&stat->m_delete);
    }
  }

  inline void sum(PFS_single_stat *result) {
    if (m_has_data) {
      result->aggregate(&m_fetch);
      result->aggregate(&m_insert);
      result->aggregate(&m_update);
      result->aggregate(&m_delete);
    }
  }
};

enum PFS_TL_LOCK_TYPE {
  /* Locks from enum thr_lock */
  PFS_TL_READ = 0,
  PFS_TL_READ_WITH_SHARED_LOCKS = 1,
  PFS_TL_READ_HIGH_PRIORITY = 2,
  PFS_TL_READ_NO_INSERT = 3,
  PFS_TL_WRITE_ALLOW_WRITE = 4,
  PFS_TL_WRITE_CONCURRENT_INSERT = 5,
  PFS_TL_WRITE_LOW_PRIORITY = 6,
  PFS_TL_WRITE = 7,

  /* Locks for handler::ha_external_lock() */
  PFS_TL_READ_EXTERNAL = 8,
  PFS_TL_WRITE_EXTERNAL = 9,

  PFS_TL_NONE = 99
};

#define COUNT_PFS_TL_LOCK_TYPE 10

/** Statistics for table locks. */
struct PFS_table_lock_stat {
  PFS_single_stat m_stat[COUNT_PFS_TL_LOCK_TYPE];

  inline void reset() {
    PFS_single_stat *pfs = &m_stat[0];
    const PFS_single_stat *pfs_last = &m_stat[COUNT_PFS_TL_LOCK_TYPE];
    for (; pfs < pfs_last; pfs++) {
      pfs->reset();
    }
  }

  inline void aggregate(const PFS_table_lock_stat *stat) {
    PFS_single_stat *pfs = &m_stat[0];
    const PFS_single_stat *pfs_last = &m_stat[COUNT_PFS_TL_LOCK_TYPE];
    const PFS_single_stat *pfs_from = &stat->m_stat[0];
    for (; pfs < pfs_last; pfs++, pfs_from++) {
      pfs->aggregate(pfs_from);
    }
  }

  inline void sum(PFS_single_stat *result) {
    const PFS_single_stat *pfs = &m_stat[0];
    const PFS_single_stat *pfs_last = &m_stat[COUNT_PFS_TL_LOCK_TYPE];
    for (; pfs < pfs_last; pfs++) {
      result->aggregate(pfs);
    }
  }
};

/** Statistics for TABLE usage. */
struct PFS_table_stat {
  /**
    Statistics, per index.
    Each index stat is in [0, MAX_INDEXES-1],
    stats when using no index are in [MAX_INDEXES].
  */
  PFS_table_io_stat m_index_stat[MAX_INDEXES + 1];

  /**
    Statistics, per lock type.
  */
  PFS_table_lock_stat m_lock_stat;

  /** Reset table I/O statistic. */
  inline void reset_io() {
    PFS_table_io_stat *stat = &m_index_stat[0];
    const PFS_table_io_stat *stat_last = &m_index_stat[MAX_INDEXES + 1];
    for (; stat < stat_last; stat++) {
      stat->reset();
    }
  }

  /** Reset table lock statistic. */
  inline void reset_lock() { m_lock_stat.reset(); }

  /** Reset table statistic. */
  inline void reset() {
    reset_io();
    reset_lock();
  }

  inline void fast_reset_io() {
    memcpy(&m_index_stat, &g_reset_template.m_index_stat, sizeof(m_index_stat));
  }

  inline void fast_reset_lock() {
    memcpy(&m_lock_stat, &g_reset_template.m_lock_stat, sizeof(m_lock_stat));
  }

  inline void fast_reset() { memcpy(this, &g_reset_template, sizeof(*this)); }

  inline void aggregate_io(const PFS_table_stat *stat, uint key_count) {
    PFS_table_io_stat *to_stat;
    PFS_table_io_stat *to_stat_last;
    const PFS_table_io_stat *from_stat;

    assert(key_count <= MAX_INDEXES);

    /* Aggregate stats for each index, if any */
    to_stat = &m_index_stat[0];
    to_stat_last = to_stat + key_count;
    from_stat = &stat->m_index_stat[0];
    for (; to_stat < to_stat_last; from_stat++, to_stat++) {
      to_stat->aggregate(from_stat);
    }

    /* Aggregate stats for the table */
    to_stat = &m_index_stat[MAX_INDEXES];
    from_stat = &stat->m_index_stat[MAX_INDEXES];
    to_stat->aggregate(from_stat);
  }

  inline void aggregate_lock(const PFS_table_stat *stat) {
    m_lock_stat.aggregate(&stat->m_lock_stat);
  }

  inline void aggregate(const PFS_table_stat *stat, uint key_count) {
    aggregate_io(stat, key_count);
    aggregate_lock(stat);
  }

  inline void sum_io(PFS_single_stat *result, uint key_count) {
    PFS_table_io_stat *stat;
    PFS_table_io_stat *stat_last;

    assert(key_count <= MAX_INDEXES);

    /* Sum stats for each index, if any */
    stat = &m_index_stat[0];
    stat_last = stat + key_count;
    for (; stat < stat_last; stat++) {
      stat->sum(result);
    }

    /* Sum stats for the table */
    m_index_stat[MAX_INDEXES].sum(result);
  }

  inline void sum_lock(PFS_single_stat *result) { m_lock_stat.sum(result); }

  inline void sum(PFS_single_stat *result, uint key_count) {
    sum_io(result, key_count);
    sum_lock(result);
  }

  static struct PFS_table_stat g_reset_template;
};

/** Statistics for SOCKET I/O. Used for both waits and byte counts. */
struct PFS_socket_io_stat {
  /** READ statistics */
  PFS_byte_stat m_read;
  /** WRITE statistics */
  PFS_byte_stat m_write;
  /** Miscellaneous statistics */
  PFS_byte_stat m_misc;

  inline void reset() {
    m_read.reset();
    m_write.reset();
    m_misc.reset();
  }

  inline void aggregate(const PFS_socket_io_stat *stat) {
    m_read.aggregate(&stat->m_read);
    m_write.aggregate(&stat->m_write);
    m_misc.aggregate(&stat->m_misc);
  }

  /* Sum waits and byte counts */
  inline void sum(PFS_byte_stat *stat) {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }

  /* Sum waits only */
  inline void sum_waits(PFS_single_stat *stat) {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }
};

/** Statistics for SOCKET usage. */
struct PFS_socket_stat {
  /** Socket timing and byte count statistics per operation */
  PFS_socket_io_stat m_io_stat;

  /** Reset socket statistics. */
  inline void reset() { m_io_stat.reset(); }
};

struct PFS_memory_stat_alloc_delta {
  size_t m_alloc_count_delta;
  size_t m_alloc_size_delta;
};

struct PFS_memory_stat_free_delta {
  size_t m_free_count_delta;
  size_t m_free_size_delta;
};

/**
  Memory statistics.
  Conceptually, the following statistics are maintained:
  - CURRENT_COUNT_USED,
  - LOW_COUNT_USED,
  - HIGH_COUNT_USED
  - CURRENT_SIZE_USED,
  - LOW_SIZE_USED,
  - HIGH_SIZE_USED
  Now, the implementation keeps different counters,
  which are easier (less overhead) to maintain while
  collecting statistics.
  Invariants are as follows:
  CURRENT_COUNT_USED = @c m_alloc_count - @c m_free_count
  LOW_COUNT_USED + @c m_free_count_capacity = CURRENT_COUNT_USED
  CURRENT_COUNT_USED + @c m_alloc_count_capacity = HIGH_COUNT_USED
  CURRENT_SIZE_USED = @c m_alloc_size - @c m_free_size
  LOW_SIZE_USED + @c m_free_size_capacity = CURRENT_SIZE_USED
  CURRENT_SIZE_USED + @c m_alloc_size_capacity = HIGH_SIZE_USED
*/
struct PFS_memory_safe_stat {
  bool m_used;

  size_t m_alloc_count;
  size_t m_free_count;
  size_t m_alloc_size;
  size_t m_free_size;

  size_t m_alloc_count_capacity;
  size_t m_free_count_capacity;
  size_t m_alloc_size_capacity;
  size_t m_free_size_capacity;

  void reset();

  void rebase();

  PFS_memory_stat_alloc_delta *count_alloc(size_t size,
                                           PFS_memory_stat_alloc_delta *delta);

  PFS_memory_stat_free_delta *count_free(size_t size,
                                         PFS_memory_stat_free_delta *delta);
};

struct PFS_memory_shared_stat {
  std::atomic<bool> m_used;

  std::atomic<size_t> m_alloc_count;
  std::atomic<size_t> m_free_count;
  std::atomic<size_t> m_alloc_size;
  std::atomic<size_t> m_free_size;

  std::atomic<size_t> m_alloc_count_capacity;
  std::atomic<size_t> m_free_count_capacity;
  std::atomic<size_t> m_alloc_size_capacity;
  std::atomic<size_t> m_free_size_capacity;

  void reset();

  void rebase();

  void count_builtin_alloc(size_t size);

  void count_builtin_free(size_t size);

  inline void count_global_alloc(size_t size) { count_builtin_alloc(size); }

  inline void count_global_free(size_t size) { count_builtin_free(size); }

  PFS_memory_stat_alloc_delta *count_alloc(size_t size,
                                           PFS_memory_stat_alloc_delta *delta);

  PFS_memory_stat_free_delta *count_free(size_t size,
                                         PFS_memory_stat_free_delta *delta);

  /**
    Expand the high water marks.
    @param [in] delta High watermark increments to apply
    @param [in] delta_buffer Working buffer
    @return High watermark increments to carry to the parent if any, or
nullptr.
  */
  PFS_memory_stat_alloc_delta *apply_alloc_delta(
      const PFS_memory_stat_alloc_delta *delta,
      PFS_memory_stat_alloc_delta *delta_buffer);

  /**
    Expand the low water marks.
    @param [in] delta Low watermark decrements to apply
    @param [in] delta_buffer Working buffer
    @return Low watermark decrements to carry to the parent if any, or
nullptr.
  */
  PFS_memory_stat_free_delta *apply_free_delta(
      const PFS_memory_stat_free_delta *delta,
      PFS_memory_stat_free_delta *delta_buffer);
};

struct PFS_memory_monitoring_stat {
  size_t m_alloc_count;
  size_t m_free_count;
  size_t m_alloc_size;
  size_t m_free_size;

  size_t m_alloc_count_capacity;
  size_t m_free_count_capacity;
  size_t m_alloc_size_capacity;
  size_t m_free_size_capacity;

  size_t m_missing_free_count_capacity;
  size_t m_missing_free_size_capacity;

  ssize_t m_low_count_used;
  ssize_t m_high_count_used;
  ssize_t m_low_size_used;
  ssize_t m_high_size_used;

  void reset();

  void normalize(bool global);
};

struct PFS_all_memory_stat {
  /** The current memory size allocated. */
  size_t m_size;
  /** The maximum memory size allocated, for a sub statement. */
  size_t m_max_local_size;
  /** The maximum memory size allocated, for a statement. */
  size_t m_max_stmt_size;
  /** The maximum memory size allocated, for this session. */
  size_t m_max_session_size;

  void reset() {
    m_size = 0;
    m_max_local_size = 0;
    m_max_stmt_size = 0;
    m_max_session_size = 0;
  }

  void start_top_statement() {
    if (m_max_session_size < m_max_local_size) {
      m_max_session_size = m_max_local_size;
    }
    m_max_stmt_size = m_size;
    m_max_local_size = m_size;
  }

  void end_top_statement(size_t *stmt_size) {
    if (m_max_stmt_size < m_max_local_size) {
      m_max_stmt_size = m_max_local_size;
    }
    *stmt_size = m_max_stmt_size;
  }

  void start_nested_statement(size_t *local_size_start,
                              size_t *stmt_size_start) {
    if (m_max_session_size < m_max_local_size) {
      m_max_session_size = m_max_local_size;
    }
    if (m_max_stmt_size < m_max_local_size) {
      m_max_stmt_size = m_max_local_size;
    }
    *local_size_start = m_size;
    m_max_local_size = m_size;

    /* PUSH m_max_stmt_size = m_size */
    *stmt_size_start = m_max_stmt_size;
    m_max_stmt_size = m_size;
  }

  void end_nested_statement(size_t local_size_start, size_t stmt_size_start,
                            size_t *stmt_size) {
    if (m_max_session_size < m_max_local_size) {
      m_max_session_size = m_max_local_size;
    }
    if (m_max_stmt_size < m_max_local_size) {
      m_max_stmt_size = m_max_local_size;
    }
    if (m_max_stmt_size > local_size_start) {
      *stmt_size = m_max_stmt_size - local_size_start;
    } else {
      *stmt_size = 0;
    }

    /* POP m_max_stmt_size */
    m_max_stmt_size = stmt_size_start;
  }

  void count_alloc(size_t size) {
    m_size += size;
    if (m_max_local_size < m_size) {
      m_max_local_size = m_size;
    }
  }

  void count_free(size_t size) {
    if (m_size >= size) {
      m_size -= size;
    } else {
      /*
        Thread 1, allocates memory : owner = T1
        Thread 1 puts the memory in a global cache,
        but does not use unclaim.
        Thread 1 disconnects.

        The memory in the cache has owner = T1,
        pointing to a defunct thread.

        Thread 2 connects,
        Thread 2 purges the global cache,
        but does not use claim.

        As a result, it appears that:
        - T1 'leaks' memory, even when there are no leaks.
        - T2 frees 'non existent' memory, even when it was allocated.

        Long term fix is to enforce use of:
        - unclaim in T1
        - claim in T2,
        to make sure memory transfers are properly accounted for.

        Short term fix, here, we discover (late) that:
        - the thread T2 uses more memory that was claimed
          (because it takes silently ownership of the global cache)
        - it releases more memory that it is supposed to own.

        The net consumption of T2 is adjusted to 0,
        as it can not be negative.
      */
      m_size = 0;
    }
  }

  size_t get_session_size() const { return m_size; }

  size_t get_session_max() const {
    if (m_max_session_size < m_max_local_size) {
      return m_max_local_size;
    }
    return m_max_session_size;
  }
};

struct PFS_session_all_memory_stat {
  PFS_all_memory_stat m_controlled;
  PFS_all_memory_stat m_total;

  void reset();

  void start_top_statement() {
    m_controlled.start_top_statement();
    m_total.start_top_statement();
  }

  void end_top_statement(size_t *controlled_size, size_t *total_size) {
    m_controlled.end_top_statement(controlled_size);
    m_total.end_top_statement(total_size);
  }

  void start_nested_statement(size_t *controlled_local_size_start,
                              size_t *controlled_stmt_size_start,
                              size_t *total_local_size_start,
                              size_t *total_stmt_size_start) {
    m_controlled.start_nested_statement(controlled_local_size_start,
                                        controlled_stmt_size_start);
    m_total.start_nested_statement(total_local_size_start,
                                   total_stmt_size_start);
  }

  void end_nested_statement(size_t controlled_local_size_start,
                            size_t controlled_stmt_size_start,
                            size_t total_local_size_start,
                            size_t total_stmt_size_start,
                            size_t *controlled_size, size_t *total_size) {
    m_controlled.end_nested_statement(controlled_local_size_start,
                                      controlled_stmt_size_start,
                                      controlled_size);
    m_total.end_nested_statement(total_local_size_start, total_stmt_size_start,
                                 total_size);
  }

  void count_controlled_alloc(size_t size);
  void count_uncontrolled_alloc(size_t size);
  void count_controlled_free(size_t size);
  void count_uncontrolled_free(size_t size);
};

void memory_partial_aggregate(PFS_memory_safe_stat *from,
                              PFS_memory_shared_stat *stat);

void memory_partial_aggregate(PFS_memory_shared_stat *from,
                              PFS_memory_shared_stat *stat);

void memory_partial_aggregate(PFS_memory_safe_stat *from,
                              PFS_memory_shared_stat *stat1,
                              PFS_memory_shared_stat *stat2);

void memory_partial_aggregate(PFS_memory_shared_stat *from,
                              PFS_memory_shared_stat *stat1,
                              PFS_memory_shared_stat *stat2);

/**
  Aggregate thread memory statistics to the parent bucket.
  Also, reassign net memory contributed by this thread
  to the global bucket.
  This is necessary to balance globally allocations done by one
  thread with deallocations done by another thread.
*/
void memory_full_aggregate_with_reassign(const PFS_memory_safe_stat *from,
                                         PFS_memory_shared_stat *stat,
                                         PFS_memory_shared_stat *global);

void memory_full_aggregate_with_reassign(const PFS_memory_safe_stat *from,
                                         PFS_memory_shared_stat *stat1,
                                         PFS_memory_shared_stat *stat2,
                                         PFS_memory_shared_stat *global);

void memory_full_aggregate(const PFS_memory_safe_stat *from,
                           PFS_memory_shared_stat *stat);

void memory_full_aggregate(const PFS_memory_shared_stat *from,
                           PFS_memory_shared_stat *stat);

void memory_full_aggregate(const PFS_memory_shared_stat *from,
                           PFS_memory_shared_stat *stat1,
                           PFS_memory_shared_stat *stat2);

void memory_monitoring_aggregate(const PFS_memory_safe_stat *from,
                                 PFS_memory_monitoring_stat *stat);

void memory_monitoring_aggregate(const PFS_memory_shared_stat *from,
                                 PFS_memory_monitoring_stat *stat);

/** Connections statistics. */
struct PFS_connection_stat {
  PFS_connection_stat()
      : m_current_connections(0),
        m_total_connections(0),
        m_max_session_controlled_memory(0),
        m_max_session_total_memory(0) {}

  ulonglong m_current_connections;
  ulonglong m_total_connections;
  ulonglong m_max_session_controlled_memory;
  ulonglong m_max_session_total_memory;

  inline void aggregate_active(ulonglong active, ulonglong controlled_memory,
                               ulonglong total_memory) {
    m_current_connections += active;
    m_total_connections += active;

    if (m_max_session_controlled_memory < controlled_memory) {
      m_max_session_controlled_memory = controlled_memory;
    }

    if (m_max_session_total_memory < total_memory) {
      m_max_session_total_memory = total_memory;
    }
  }

  inline void aggregate_disconnected(ulonglong disconnected,
                                     ulonglong controlled_memory,
                                     ulonglong total_memory) {
    m_total_connections += disconnected;

    if (m_max_session_controlled_memory < controlled_memory) {
      m_max_session_controlled_memory = controlled_memory;
    }

    if (m_max_session_total_memory < total_memory) {
      m_max_session_total_memory = total_memory;
    }
  }
};

/** @} */
#endif
