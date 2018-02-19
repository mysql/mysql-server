/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_STAT_H
#define PFS_STAT_H

#include <algorithm>

#include "my_dbug.h"
#include "my_sys.h"
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

  inline void reset(void) {
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

  inline void reset(void) {
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

  inline void reset(void) {
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

  inline void reset(void) {
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

  inline void reset(void) {
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

  inline void reset(void) {
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
  inline void reset(void) { m_io_stat.reset(); }
};

/** Statistics for stage usage. */
struct PFS_stage_stat {
  PFS_single_stat m_timer1_stat;

  inline void reset(void) { m_timer1_stat.reset(); }

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

  inline void reset(void) { m_timer1_stat.reset(); }

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

  inline void reset(void) { m_timer1_stat.reset(); }

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
  This structure uses lazy initialization,
  controlled by member @c m_timer1_stat.m_count.
*/
struct PFS_statement_stat {
  PFS_single_stat m_timer1_stat;
  ulonglong m_error_count;
  ulonglong m_warning_count;
  ulonglong m_rows_affected;
  ulonglong m_lock_time;
  ulonglong m_rows_sent;
  ulonglong m_rows_examined;
  ulonglong m_created_tmp_disk_tables;
  ulonglong m_created_tmp_tables;
  ulonglong m_select_full_join;
  ulonglong m_select_full_range_join;
  ulonglong m_select_range;
  ulonglong m_select_range_check;
  ulonglong m_select_scan;
  ulonglong m_sort_merge_passes;
  ulonglong m_sort_range;
  ulonglong m_sort_rows;
  ulonglong m_sort_scan;
  ulonglong m_no_index_used;
  ulonglong m_no_good_index_used;

  PFS_statement_stat() { reset(); }

  inline void reset() { m_timer1_stat.m_count = 0; }

  inline void mark_used() { delayed_reset(); }

 private:
  inline void delayed_reset(void) {
    if (m_timer1_stat.m_count == 0) {
      m_timer1_stat.reset();
      m_error_count = 0;
      m_warning_count = 0;
      m_rows_affected = 0;
      m_lock_time = 0;
      m_rows_sent = 0;
      m_rows_examined = 0;
      m_created_tmp_disk_tables = 0;
      m_created_tmp_tables = 0;
      m_select_full_join = 0;
      m_select_full_range_join = 0;
      m_select_range = 0;
      m_select_range_check = 0;
      m_select_scan = 0;
      m_sort_merge_passes = 0;
      m_sort_range = 0;
      m_sort_rows = 0;
      m_sort_scan = 0;
      m_no_index_used = 0;
      m_no_good_index_used = 0;
    }
  }

 public:
  inline void aggregate_counted() {
    delayed_reset();
    m_timer1_stat.aggregate_counted();
  }

  inline void aggregate_value(ulonglong value) {
    delayed_reset();
    m_timer1_stat.aggregate_value(value);
  }

  inline void aggregate(const PFS_statement_stat *stat) {
    if (stat->m_timer1_stat.m_count != 0) {
      delayed_reset();
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

  ulonglong count(void) {
    return (m_read_write_stat.m_count + m_read_only_stat.m_count);
  }

  inline void reset(void) {
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

  ulonglong count(void) { return m_count; }

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
        DBUG_ASSERT(0);
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

/* Statistics for all server errors. */
struct PFS_error_stat {
  PFS_error_single_stat *m_stat;

  PFS_error_stat() { m_stat = NULL; }

  const PFS_error_single_stat *get_stat(uint error_index) const {
    return &m_stat[error_index];
  }

  ulonglong count(void) {
    ulonglong total = 0;
    for (uint i = 0; i < max_server_errors + 1; i++) {
      total += m_stat[i].count();
    }
    return total;
  }

  ulonglong count(uint error_index) { return m_stat[error_index].count(); }

  inline void init(PFS_builtin_memory_class *memory_class) {
    if (max_server_errors == 0) {
      return;
    }

    /* allocate memory for errors' stats. +1 is for NULL row */
    m_stat = PFS_MALLOC_ARRAY(memory_class, max_server_errors + 1,
                              sizeof(PFS_error_single_stat),
                              PFS_error_single_stat, MYF(MY_ZEROFILL));
    reset();
  }

  inline void cleanup(PFS_builtin_memory_class *memory_class) {
    if (m_stat == NULL) {
      return;
    }

    PFS_FREE_ARRAY(memory_class, max_server_errors + 1,
                   sizeof(PFS_error_single_stat), m_stat);
    m_stat = NULL;
  }

  inline void reset() {
    if (m_stat == NULL) {
      return;
    }

    for (uint i = 0; i < max_server_errors + 1; i++) {
      m_stat[i].reset();
    }
  }

  inline void aggregate_count(int error_index, int error_operation) {
    if (m_stat == NULL) {
      return;
    }

    PFS_error_single_stat *stat = &m_stat[error_index];
    stat->aggregate_count(error_operation);
  }

  inline void aggregate(const PFS_error_single_stat *stat, uint error_index) {
    if (m_stat == NULL) {
      return;
    }

    DBUG_ASSERT(error_index <= max_server_errors);
    m_stat[error_index].aggregate(stat);
  }

  inline void aggregate(const PFS_error_stat *stat) {
    if (m_stat == NULL) {
      return;
    }

    for (uint i = 0; i < max_server_errors + 1; i++) {
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

  inline void reset(void) {
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

  inline void reset(void) {
    PFS_single_stat *pfs = &m_stat[0];
    PFS_single_stat *pfs_last = &m_stat[COUNT_PFS_TL_LOCK_TYPE];
    for (; pfs < pfs_last; pfs++) {
      pfs->reset();
    }
  }

  inline void aggregate(const PFS_table_lock_stat *stat) {
    PFS_single_stat *pfs = &m_stat[0];
    PFS_single_stat *pfs_last = &m_stat[COUNT_PFS_TL_LOCK_TYPE];
    const PFS_single_stat *pfs_from = &stat->m_stat[0];
    for (; pfs < pfs_last; pfs++, pfs_from++) {
      pfs->aggregate(pfs_from);
    }
  }

  inline void sum(PFS_single_stat *result) {
    PFS_single_stat *pfs = &m_stat[0];
    PFS_single_stat *pfs_last = &m_stat[COUNT_PFS_TL_LOCK_TYPE];
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
  inline void reset_io(void) {
    PFS_table_io_stat *stat = &m_index_stat[0];
    PFS_table_io_stat *stat_last = &m_index_stat[MAX_INDEXES + 1];
    for (; stat < stat_last; stat++) {
      stat->reset();
    }
  }

  /** Reset table lock statistic. */
  inline void reset_lock(void) { m_lock_stat.reset(); }

  /** Reset table statistic. */
  inline void reset(void) {
    reset_io();
    reset_lock();
  }

  inline void fast_reset_io(void) {
    memcpy(&m_index_stat, &g_reset_template.m_index_stat, sizeof(m_index_stat));
  }

  inline void fast_reset_lock(void) {
    memcpy(&m_lock_stat, &g_reset_template.m_lock_stat, sizeof(m_lock_stat));
  }

  inline void fast_reset(void) {
    memcpy(this, &g_reset_template, sizeof(*this));
  }

  inline void aggregate_io(const PFS_table_stat *stat, uint key_count) {
    PFS_table_io_stat *to_stat;
    PFS_table_io_stat *to_stat_last;
    const PFS_table_io_stat *from_stat;

    DBUG_ASSERT(key_count <= MAX_INDEXES);

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

    DBUG_ASSERT(key_count <= MAX_INDEXES);

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

  inline void reset(void) {
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
  inline void reset(void) { m_io_stat.reset(); }
};

struct PFS_memory_stat_delta {
  size_t m_alloc_count_delta;
  size_t m_free_count_delta;
  size_t m_alloc_size_delta;
  size_t m_free_size_delta;

  void reset() {
    m_alloc_count_delta = 0;
    m_free_count_delta = 0;
    m_alloc_size_delta = 0;
    m_free_size_delta = 0;
  }
};

template <typename F, typename T>
void memory_partial_aggregate(F *from, T *stat) {
  if (!from->m_used) {
    return;
  }

  size_t base;

  stat->m_used = true;

  base = std::min<size_t>(from->m_alloc_count, from->m_free_count);
  if (base != 0) {
    stat->m_alloc_count += base;
    stat->m_free_count += base;
    from->m_alloc_count -= base;
    from->m_free_count -= base;
  }

  base = std::min<size_t>(from->m_alloc_size, from->m_free_size);
  if (base != 0) {
    stat->m_alloc_size += base;
    stat->m_free_size += base;
    from->m_alloc_size -= base;
    from->m_free_size -= base;
  }

  size_t tmp;

  tmp = from->m_alloc_count_capacity;
  if (tmp != 0) {
    stat->m_alloc_count_capacity += tmp;
    from->m_alloc_count_capacity = 0;
  }

  tmp = from->m_free_count_capacity;
  if (tmp != 0) {
    stat->m_free_count_capacity += tmp;
    from->m_free_count_capacity = 0;
  }

  tmp = from->m_alloc_size_capacity;
  if (tmp != 0) {
    stat->m_alloc_size_capacity += tmp;
    from->m_alloc_size_capacity = 0;
  }

  tmp = from->m_free_size_capacity;
  if (tmp != 0) {
    stat->m_free_size_capacity += tmp;
    from->m_free_size_capacity = 0;
  }
}

template <typename F, typename T>
void memory_partial_aggregate(F *from, T *stat1, T *stat2) {
  if (!from->m_used) {
    return;
  }

  size_t base;

  stat1->m_used = true;
  stat2->m_used = true;

  base = std::min<size_t>(from->m_alloc_count, from->m_free_count);
  if (base != 0) {
    stat1->m_alloc_count += base;
    stat2->m_alloc_count += base;
    stat1->m_free_count += base;
    stat2->m_free_count += base;
    from->m_alloc_count -= base;
    from->m_free_count -= base;
  }

  base = std::min<size_t>(from->m_alloc_size, from->m_free_size);
  if (base != 0) {
    stat1->m_alloc_size += base;
    stat2->m_alloc_size += base;
    stat1->m_free_size += base;
    stat2->m_free_size += base;
    from->m_alloc_size -= base;
    from->m_free_size -= base;
  }

  size_t tmp;

  tmp = from->m_alloc_count_capacity;
  if (tmp != 0) {
    stat1->m_alloc_count_capacity += tmp;
    stat2->m_alloc_count_capacity += tmp;
    from->m_alloc_count_capacity = 0;
  }

  tmp = from->m_free_count_capacity;
  if (tmp != 0) {
    stat1->m_free_count_capacity += tmp;
    stat2->m_free_count_capacity += tmp;
    from->m_free_count_capacity = 0;
  }

  tmp = from->m_alloc_size_capacity;
  if (tmp != 0) {
    stat1->m_alloc_size_capacity += tmp;
    stat2->m_alloc_size_capacity += tmp;
    from->m_alloc_size_capacity = 0;
  }

  tmp = from->m_free_size_capacity;
  if (tmp != 0) {
    stat1->m_free_size_capacity += tmp;
    stat2->m_free_size_capacity += tmp;
    from->m_free_size_capacity = 0;
  }
}

template <typename F, typename T>
void memory_full_aggregate(const F *from, T *stat) {
  if (!from->m_used) {
    return;
  }

  stat->m_used = true;

  stat->m_alloc_count += from->m_alloc_count;
  stat->m_free_count += from->m_free_count;
  stat->m_alloc_size += from->m_alloc_size;
  stat->m_free_size += from->m_free_size;

  stat->m_alloc_count_capacity += from->m_alloc_count_capacity;
  stat->m_free_count_capacity += from->m_free_count_capacity;
  stat->m_alloc_size_capacity += from->m_alloc_size_capacity;
  stat->m_free_size_capacity += from->m_free_size_capacity;
}

template <typename F, typename T>
void memory_full_aggregate(const F *from, T *stat1, T *stat2) {
  if (!from->m_used) {
    return;
  }

  stat1->m_used = true;
  stat2->m_used = true;

  size_t tmp;

  tmp = from->m_alloc_count;
  stat1->m_alloc_count += tmp;
  stat2->m_alloc_count += tmp;

  tmp = from->m_free_count;
  stat1->m_free_count += tmp;
  stat2->m_free_count += tmp;

  tmp = from->m_alloc_size;
  stat1->m_alloc_size += tmp;
  stat2->m_alloc_size += tmp;

  tmp = from->m_free_size;
  stat1->m_free_size += tmp;
  stat2->m_free_size += tmp;

  tmp = from->m_alloc_count_capacity;
  stat1->m_alloc_count_capacity += tmp;
  stat2->m_alloc_count_capacity += tmp;

  tmp = from->m_free_count_capacity;
  stat1->m_free_count_capacity += tmp;
  stat2->m_free_count_capacity += tmp;

  tmp = from->m_alloc_size_capacity;
  stat1->m_alloc_size_capacity += tmp;
  stat2->m_alloc_size_capacity += tmp;

  tmp = from->m_free_size_capacity;
  stat1->m_free_size_capacity += tmp;
  stat2->m_free_size_capacity += tmp;
}

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

  inline void reset(void) {
    m_used = false;
    m_alloc_count = 0;
    m_free_count = 0;
    m_alloc_size = 0;
    m_free_size = 0;

    m_alloc_count_capacity = 0;
    m_free_count_capacity = 0;
    m_alloc_size_capacity = 0;
    m_free_size_capacity = 0;
  }

  inline void rebase(void) {
    if (!m_used) {
      return;
    }

    size_t base;

    base = std::min<size_t>(m_alloc_count, m_free_count);
    m_alloc_count -= base;
    m_free_count -= base;

    base = std::min<size_t>(m_alloc_size, m_free_size);
    m_alloc_size -= base;
    m_free_size -= base;

    m_alloc_count_capacity = 0;
    m_free_count_capacity = 0;
    m_alloc_size_capacity = 0;
    m_free_size_capacity = 0;
  }

  void count_builtin_alloc(size_t size) {
    m_used = true;

    m_alloc_count++;
    m_free_count_capacity++;
    m_alloc_size += size;
    m_free_size_capacity += size;

    size_t old_value;

    /* Optimistic */
    old_value = m_alloc_count_capacity.fetch_sub(1);

    /* Adjustment */
    if (old_value <= 0) {
      m_alloc_count_capacity++;
    }

    /* Optimistic */
    old_value = m_alloc_size_capacity.fetch_sub(size);

    /* Adjustment */
    if (old_value < size) {
      m_alloc_size_capacity = 0;
    }

    return;
  }

  void count_builtin_free(size_t size) {
    m_used = true;

    m_free_count++;
    m_alloc_count_capacity++;
    m_free_size += size;
    m_alloc_size_capacity += size;

    size_t old_value;

    /* Optimistic */
    old_value = m_free_count_capacity.fetch_sub(1);

    /* Adjustment */
    if (old_value <= 0) {
      m_free_count_capacity++;
    }

    /* Optimistic */
    old_value = m_free_size_capacity.fetch_sub(size);

    /* Adjustment */
    if (old_value < size) {
      m_free_size_capacity = 0;
    }

    return;
  }

  inline void count_global_alloc(size_t size) { count_builtin_alloc(size); }

  inline void count_global_realloc(size_t old_size, size_t new_size) {
    if (old_size == new_size) {
      return;
    }

    size_t size_delta;
    size_t old_value;

    if (new_size > old_size) {
      /* Growing */
      size_delta = new_size - old_size;
      m_free_size_capacity += size_delta;

      /* Optimistic */
      old_value = m_alloc_size_capacity.fetch_sub(size_delta);

      /* Adjustment */
      if (old_value < size_delta) {
        m_alloc_size_capacity = 0;
      }
    } else {
      /* Shrinking */
      size_delta = old_size - new_size;
      m_alloc_size_capacity += size_delta;

      /* Optimistic */
      old_value = m_free_size_capacity.fetch_sub(size_delta);

      /* Adjustment */
      if (old_value < size_delta) {
        m_free_size_capacity = 0;
      }
    }
  }

  inline void count_global_free(size_t size) { count_builtin_free(size); }

  inline PFS_memory_stat_delta *count_alloc(size_t size,
                                            PFS_memory_stat_delta *delta) {
    m_used = true;

    m_alloc_count++;
    m_free_count_capacity++;
    m_alloc_size += size;
    m_free_size_capacity += size;

    if ((m_alloc_count_capacity >= 1) && (m_alloc_size_capacity >= size)) {
      m_alloc_count_capacity--;
      m_alloc_size_capacity -= size;
      return NULL;
    }

    delta->reset();

    if (m_alloc_count_capacity >= 1) {
      m_alloc_count_capacity--;
    } else {
      delta->m_alloc_count_delta = 1;
    }

    if (m_alloc_size_capacity >= size) {
      m_alloc_size_capacity -= size;
    } else {
      delta->m_alloc_size_delta = size - m_alloc_size_capacity;
      m_alloc_size_capacity = 0;
    }

    return delta;
  }

  inline PFS_memory_stat_delta *count_realloc(size_t old_size, size_t new_size,
                                              PFS_memory_stat_delta *delta) {
    m_used = true;

    size_t size_delta = new_size - old_size;
    m_alloc_count++;
    m_alloc_size += new_size;
    m_free_count++;
    m_free_size += old_size;

    if (new_size == old_size) {
      return NULL;
    }

    if (new_size > old_size) {
      /* Growing */
      size_delta = new_size - old_size;
      m_free_size_capacity += size_delta;

      if (m_alloc_size_capacity >= size_delta) {
        m_alloc_size_capacity -= size_delta;
        return NULL;
      }

      delta->reset();
      delta->m_alloc_size_delta = size_delta - m_alloc_size_capacity;
      m_alloc_size_capacity = 0;
    } else {
      /* Shrinking */
      size_delta = old_size - new_size;
      m_alloc_size_capacity += size_delta;

      if (m_free_size_capacity >= size_delta) {
        m_free_size_capacity -= size_delta;
        return NULL;
      }

      delta->reset();
      delta->m_free_size_delta = size_delta - m_free_size_capacity;
      m_free_size_capacity = 0;
    }

    return delta;
  }

  inline PFS_memory_stat_delta *count_free(size_t size,
                                           PFS_memory_stat_delta *delta) {
    m_used = true;

    m_free_count++;
    m_alloc_count_capacity++;
    m_free_size += size;
    m_alloc_size_capacity += size;

    if ((m_free_count_capacity >= 1) && (m_free_size_capacity >= size)) {
      m_free_count_capacity--;
      m_free_size_capacity -= size;
      return NULL;
    }

    delta->reset();

    if (m_free_count_capacity >= 1) {
      m_free_count_capacity--;
    } else {
      delta->m_free_count_delta = 1;
    }

    if (m_free_size_capacity >= size) {
      m_free_size_capacity -= size;
    } else {
      delta->m_free_size_delta = size - m_free_size_capacity;
      m_free_size_capacity = 0;
    }

    return delta;
  }

  inline PFS_memory_stat_delta *apply_delta(
      const PFS_memory_stat_delta *delta, PFS_memory_stat_delta *delta_buffer) {
    size_t val;
    size_t remaining_alloc_count;
    size_t remaining_alloc_size;
    size_t remaining_free_count;
    size_t remaining_free_size;
    bool has_remaining = false;

    m_used = true;

    val = delta->m_alloc_count_delta;
    if (val <= m_alloc_count_capacity) {
      m_alloc_count_capacity -= val;
      remaining_alloc_count = 0;
    } else {
      remaining_alloc_count = val - m_alloc_count_capacity;
      m_alloc_count_capacity = 0;
      has_remaining = true;
    }

    val = delta->m_alloc_size_delta;
    if (val <= m_alloc_size_capacity) {
      m_alloc_size_capacity -= val;
      remaining_alloc_size = 0;
    } else {
      remaining_alloc_size = val - m_alloc_size_capacity;
      m_alloc_size_capacity = 0;
      has_remaining = true;
    }

    val = delta->m_free_count_delta;
    if (val <= m_free_count_capacity) {
      m_free_count_capacity -= val;
      remaining_free_count = 0;
    } else {
      remaining_free_count = val - m_free_count_capacity;
      m_free_count_capacity = 0;
      has_remaining = true;
    }

    val = delta->m_free_size_delta;
    if (val <= m_free_size_capacity) {
      m_free_size_capacity -= val;
      remaining_free_size = 0;
    } else {
      remaining_free_size = val - m_free_size_capacity;
      m_free_size_capacity = 0;
      has_remaining = true;
    }

    if (!has_remaining) {
      return NULL;
    }

    delta_buffer->m_alloc_count_delta = remaining_alloc_count;
    delta_buffer->m_alloc_size_delta = remaining_alloc_size;
    delta_buffer->m_free_count_delta = remaining_free_count;
    delta_buffer->m_free_size_delta = remaining_free_size;
    return delta_buffer;
  }
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

  inline void reset(void) {
    m_used = false;
    m_alloc_count = 0;
    m_free_count = 0;
    m_alloc_size = 0;
    m_free_size = 0;

    m_alloc_count_capacity = 0;
    m_free_count_capacity = 0;
    m_alloc_size_capacity = 0;
    m_free_size_capacity = 0;
  }

  inline void rebase(void) {
    if (!m_used) {
      return;
    }

    size_t base;

    base = std::min<size_t>(m_alloc_count, m_free_count);
    m_alloc_count -= base;
    m_free_count -= base;

    base = std::min<size_t>(m_alloc_size, m_free_size);
    m_alloc_size -= base;
    m_free_size -= base;

    m_alloc_count_capacity = 0;
    m_free_count_capacity = 0;
    m_alloc_size_capacity = 0;
    m_free_size_capacity = 0;
  }

  inline PFS_memory_stat_delta *count_alloc(size_t size,
                                            PFS_memory_stat_delta *delta) {
    m_used = true;

    m_alloc_count++;
    m_free_count_capacity++;
    m_alloc_size += size;
    m_free_size_capacity += size;

    if ((m_alloc_count_capacity >= 1) && (m_alloc_size_capacity >= size)) {
      m_alloc_count_capacity--;
      m_alloc_size_capacity -= size;
      return NULL;
    }

    delta->reset();

    if (m_alloc_count_capacity >= 1) {
      m_alloc_count_capacity--;
    } else {
      delta->m_alloc_count_delta = 1;
    }

    if (m_alloc_size_capacity >= size) {
      m_alloc_size_capacity -= size;
    } else {
      delta->m_alloc_size_delta = size - m_alloc_size_capacity;
      m_alloc_size_capacity = 0;
    }

    return delta;
  }

  inline PFS_memory_stat_delta *count_realloc(size_t old_size, size_t new_size,
                                              PFS_memory_stat_delta *delta) {
    m_used = true;

    size_t size_delta = new_size - old_size;
    m_alloc_count++;
    m_alloc_size += new_size;
    m_free_count++;
    m_free_size += old_size;

    if (new_size == old_size) {
      return NULL;
    }

    if (new_size > old_size) {
      /* Growing */
      size_delta = new_size - old_size;
      m_free_size_capacity += size_delta;

      if (m_alloc_size_capacity >= size_delta) {
        m_alloc_size_capacity -= size_delta;
        return NULL;
      }

      delta->reset();
      delta->m_alloc_size_delta = size_delta - m_alloc_size_capacity;
      m_alloc_size_capacity = 0;
    } else {
      /* Shrinking */
      size_delta = old_size - new_size;
      m_alloc_size_capacity += size_delta;

      if (m_free_size_capacity >= size_delta) {
        m_free_size_capacity -= size_delta;
        return NULL;
      }

      delta->reset();
      delta->m_free_size_delta = size_delta - m_free_size_capacity;
      m_free_size_capacity = 0;
    }

    return delta;
  }

  inline PFS_memory_stat_delta *count_free(size_t size,
                                           PFS_memory_stat_delta *delta) {
    m_used = true;

    m_free_count++;
    m_alloc_count_capacity++;
    m_free_size += size;
    m_alloc_size_capacity += size;

    if ((m_free_count_capacity >= 1) && (m_free_size_capacity >= size)) {
      m_free_count_capacity--;
      m_free_size_capacity -= size;
      return NULL;
    }

    delta->reset();

    if (m_free_count_capacity >= 1) {
      m_free_count_capacity--;
    } else {
      delta->m_free_count_delta = 1;
    }

    if (m_free_size_capacity >= size) {
      m_free_size_capacity -= size;
    } else {
      delta->m_free_size_delta = size - m_free_size_capacity;
      m_free_size_capacity = 0;
    }

    return delta;
  }
};

#define PFS_MEMORY_STAT_INITIALIZER \
  { false, 0, 0, 0, 0, 0, 0, 0, 0 }

/** Connections statistics. */
struct PFS_connection_stat {
  PFS_connection_stat() : m_current_connections(0), m_total_connections(0) {}

  ulonglong m_current_connections;
  ulonglong m_total_connections;

  inline void aggregate_active(ulonglong active) {
    m_current_connections += active;
    m_total_connections += active;
  }

  inline void aggregate_disconnected(ulonglong disconnected) {
    m_total_connections += disconnected;
  }
};

/** @} */
#endif
