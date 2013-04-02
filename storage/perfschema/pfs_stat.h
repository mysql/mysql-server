/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_STAT_H
#define PFS_STAT_H

#include "sql_const.h"
/* memcpy */
#include "string.h"

/**
  @file storage/perfschema/pfs_stat.h
  Statistics (declarations).
*/

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Single statistic. */
struct PFS_single_stat
{
  /** Count of values. */
  ulonglong m_count;
  /** Sum of values. */
  ulonglong m_sum;
  /** Minimum value. */
  ulonglong m_min;
  /** Maximum value. */
  ulonglong m_max;

  PFS_single_stat()
  {
    m_count= 0;
    m_sum= 0;
    m_min= ULONGLONG_MAX;
    m_max= 0;
  }

  inline void reset(void)
  {
    m_count= 0;
    m_sum= 0;
    m_min= ULONGLONG_MAX;
    m_max= 0;
  }

  inline bool has_timed_stats() const
  {
    return (m_min <= m_max);
  }

  inline void aggregate(const PFS_single_stat *stat)
  {
    m_count+= stat->m_count;
    m_sum+= stat->m_sum;
    if (unlikely(m_min > stat->m_min))
      m_min= stat->m_min;
    if (unlikely(m_max < stat->m_max))
      m_max= stat->m_max;
  }

  inline void aggregate_counted()
  {
    m_count++;
  }

  inline void aggregate_counted(ulonglong count)
  {
    m_count+= count;
  }

  inline void aggregate_value(ulonglong value)
  {
    m_count++;
    m_sum+= value;
    if (unlikely(m_min > value))
      m_min= value;
    if (unlikely(m_max < value))
      m_max= value;
  }
};

/** Combined statistic. */
struct PFS_byte_stat : public PFS_single_stat
{
  /** Byte count statistics */
  ulonglong m_bytes;

  /* Aggregate wait stats, event count and byte count */
  inline void aggregate(const PFS_byte_stat *stat)
  {
    PFS_single_stat::aggregate(stat);
    m_bytes+= stat->m_bytes;
  }

  /* Aggregate individual wait time, event count and byte count */
  inline void aggregate(ulonglong wait, ulonglong bytes)
  {
    aggregate_value(wait);
    m_bytes+= bytes;
  }

  /* Aggregate wait stats and event count */
  inline void aggregate_waits(const PFS_byte_stat *stat)
  {
    PFS_single_stat::aggregate(stat);
  }

  /* Aggregate event count and byte count */
  inline void aggregate_counted()
  {
    PFS_single_stat::aggregate_counted();
  }

  /* Aggregate event count and byte count */
  inline void aggregate_counted(ulonglong bytes)
  {
    PFS_single_stat::aggregate_counted();
    m_bytes+= bytes;
  }
    
  PFS_byte_stat()
  {
    reset();
  }

  inline void reset(void)
  {
    PFS_single_stat::reset();
    m_bytes= 0;
  }
};

/** Statistics for mutex usage. */
struct PFS_mutex_stat
{
  /** Wait statistics. */
  PFS_single_stat m_wait_stat;
  /**
    Lock statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat m_lock_stat;

  inline void aggregate(const PFS_mutex_stat *stat)
  {
    m_wait_stat.aggregate(&stat->m_wait_stat);
    m_lock_stat.aggregate(&stat->m_lock_stat);
  }

  inline void reset(void)
  {
    m_wait_stat.reset();
    m_lock_stat.reset();
  }
};

/** Statistics for rwlock usage. */
struct PFS_rwlock_stat
{
  /** Wait statistics. */
  PFS_single_stat m_wait_stat;
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

  inline void aggregate(const PFS_rwlock_stat *stat)
  {
    m_wait_stat.aggregate(&stat->m_wait_stat);
    m_read_lock_stat.aggregate(&stat->m_read_lock_stat);
    m_write_lock_stat.aggregate(&stat->m_write_lock_stat);
  }

  inline void reset(void)
  {
    m_wait_stat.reset();
    m_read_lock_stat.reset();
    m_write_lock_stat.reset();
  }
};

/** Statistics for COND usage. */
struct PFS_cond_stat
{
  /** Wait statistics. */
  PFS_single_stat m_wait_stat;
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

  inline void aggregate(const PFS_cond_stat *stat)
  {
    m_wait_stat.aggregate(&stat->m_wait_stat);
    m_signal_count+= stat->m_signal_count;
    m_broadcast_count+= stat->m_broadcast_count;
  }

  inline void reset(void)
  {
    m_wait_stat.reset();
    m_signal_count= 0;
    m_broadcast_count= 0;
  }
};

/** Statistics for FILE IO. Used for both waits and byte counts. */
struct PFS_file_io_stat
{
  /** READ statistics */
  PFS_byte_stat m_read;
  /** WRITE statistics */
  PFS_byte_stat m_write;
  /** Miscelleanous statistics */
  PFS_byte_stat m_misc;

  inline void reset(void)
  {
    m_read.reset();
    m_write.reset();
    m_misc.reset();
  }

  inline void aggregate(const PFS_file_io_stat *stat)
  {
    m_read.aggregate(&stat->m_read);
    m_write.aggregate(&stat->m_write);
    m_misc.aggregate(&stat->m_misc);
  }

  /* Sum waits and byte counts */
  inline void sum(PFS_byte_stat *stat)
  {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }

  /* Sum waits only */
  inline void sum_waits(PFS_single_stat *stat)
  {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }
};

/** Statistics for FILE usage. */
struct PFS_file_stat
{
  /** Number of current open handles. */
  ulong m_open_count;
  /** File IO statistics. */
  PFS_file_io_stat m_io_stat;

  inline void aggregate(const PFS_file_stat *stat)
  {
    m_io_stat.aggregate(&stat->m_io_stat);
  }

  /** Reset file statistics. */
  inline void reset(void)
  {
    m_io_stat.reset();
  }
};

/** Statistics for stage usage. */
struct PFS_stage_stat
{
  PFS_single_stat m_timer1_stat;

  inline void reset(void)
  { m_timer1_stat.reset(); }

  inline void aggregate_counted()
  { m_timer1_stat.aggregate_counted(); }

  inline void aggregate_value(ulonglong value)
  { m_timer1_stat.aggregate_value(value); }

  inline void aggregate(PFS_stage_stat *stat)
  { m_timer1_stat.aggregate(& stat->m_timer1_stat); }
};

/** Statistics for statement usage. */
struct PFS_statement_stat
{
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

  PFS_statement_stat()
  {
    m_error_count= 0;
    m_warning_count= 0;
    m_rows_affected= 0;
    m_lock_time= 0;
    m_rows_sent= 0;
    m_rows_examined= 0;
    m_created_tmp_disk_tables= 0;
    m_created_tmp_tables= 0;
    m_select_full_join= 0;
    m_select_full_range_join= 0;
    m_select_range= 0;
    m_select_range_check= 0;
    m_select_scan= 0;
    m_sort_merge_passes= 0;
    m_sort_range= 0;
    m_sort_rows= 0;
    m_sort_scan= 0;
    m_no_index_used= 0;
    m_no_good_index_used= 0;
  }

  inline void reset(void)
  {
    m_timer1_stat.reset();
    m_error_count= 0;
    m_warning_count= 0;
    m_rows_affected= 0;
    m_lock_time= 0;
    m_rows_sent= 0;
    m_rows_examined= 0;
    m_created_tmp_disk_tables= 0;
    m_created_tmp_tables= 0;
    m_select_full_join= 0;
    m_select_full_range_join= 0;
    m_select_range= 0;
    m_select_range_check= 0;
    m_select_scan= 0;
    m_sort_merge_passes= 0;
    m_sort_range= 0;
    m_sort_rows= 0;
    m_sort_scan= 0;
    m_no_index_used= 0;
    m_no_good_index_used= 0;
  }

  inline void aggregate_counted()
  { m_timer1_stat.aggregate_counted(); }

  inline void aggregate_value(ulonglong value)
  { m_timer1_stat.aggregate_value(value); }

  inline void aggregate(PFS_statement_stat *stat)
  {
    m_timer1_stat.aggregate(& stat->m_timer1_stat);

    m_error_count+= stat->m_error_count;
    m_warning_count+= stat->m_warning_count;
    m_rows_affected+= stat->m_rows_affected;
    m_lock_time+= stat->m_lock_time;
    m_rows_sent+= stat->m_rows_sent;
    m_rows_examined+= stat->m_rows_examined;
    m_created_tmp_disk_tables+= stat->m_created_tmp_disk_tables;
    m_created_tmp_tables+= stat->m_created_tmp_tables;
    m_select_full_join+= stat->m_select_full_join;
    m_select_full_range_join+= stat->m_select_full_range_join;
    m_select_range+= stat->m_select_range;
    m_select_range_check+= stat->m_select_range_check;
    m_select_scan+= stat->m_select_scan;
    m_sort_merge_passes+= stat->m_sort_merge_passes;
    m_sort_range+= stat->m_sort_range;
    m_sort_rows+= stat->m_sort_rows;
    m_sort_scan+= stat->m_sort_scan;
    m_no_index_used+= stat->m_no_index_used;
    m_no_good_index_used+= stat->m_no_good_index_used;
  }
};

/** Single table io statistic. */
struct PFS_table_io_stat
{
  bool m_has_data;
  /** FETCH statistics */
  PFS_single_stat m_fetch;
  /** INSERT statistics */
  PFS_single_stat m_insert;
  /** UPDATE statistics */
  PFS_single_stat m_update;
  /** DELETE statistics */
  PFS_single_stat m_delete;

  PFS_table_io_stat()
  {
    m_has_data= false;
  }

  inline void reset(void)
  {
    m_has_data= false;
    m_fetch.reset();
    m_insert.reset();
    m_update.reset();
    m_delete.reset();
  }

  inline void aggregate(const PFS_table_io_stat *stat)
  {
    if (stat->m_has_data)
    {
      m_has_data= true;
      m_fetch.aggregate(&stat->m_fetch);
      m_insert.aggregate(&stat->m_insert);
      m_update.aggregate(&stat->m_update);
      m_delete.aggregate(&stat->m_delete);
    }
  }

  inline void sum(PFS_single_stat *result)
  {
    if (m_has_data)
    {
      result->aggregate(& m_fetch);
      result->aggregate(& m_insert);
      result->aggregate(& m_update);
      result->aggregate(& m_delete);
    }
  }
};

enum PFS_TL_LOCK_TYPE
{
  /* Locks from enum thr_lock */
  PFS_TL_READ= 0,
  PFS_TL_READ_WITH_SHARED_LOCKS= 1,
  PFS_TL_READ_HIGH_PRIORITY= 2,
  PFS_TL_READ_NO_INSERT= 3,
  PFS_TL_WRITE_ALLOW_WRITE= 4,
  PFS_TL_WRITE_CONCURRENT_INSERT= 5,
  PFS_TL_WRITE_DELAYED= 6,
  PFS_TL_WRITE_LOW_PRIORITY= 7,
  PFS_TL_WRITE= 8,

  /* Locks for handler::ha_external_lock() */
  PFS_TL_READ_EXTERNAL= 9,
  PFS_TL_WRITE_EXTERNAL= 10
};

#define COUNT_PFS_TL_LOCK_TYPE 11

/** Statistics for table locks. */
struct PFS_table_lock_stat
{
  PFS_single_stat m_stat[COUNT_PFS_TL_LOCK_TYPE];

  inline void reset(void)
  {
    PFS_single_stat *pfs= & m_stat[0];
    PFS_single_stat *pfs_last= & m_stat[COUNT_PFS_TL_LOCK_TYPE];
    for ( ; pfs < pfs_last ; pfs++)
      pfs->reset();
  }

  inline void aggregate(const PFS_table_lock_stat *stat)
  {
    PFS_single_stat *pfs= & m_stat[0];
    PFS_single_stat *pfs_last= & m_stat[COUNT_PFS_TL_LOCK_TYPE];
    const PFS_single_stat *pfs_from= & stat->m_stat[0];
    for ( ; pfs < pfs_last ; pfs++, pfs_from++)
      pfs->aggregate(pfs_from);
  }

  inline void sum(PFS_single_stat *result)
  {
    PFS_single_stat *pfs= & m_stat[0];
    PFS_single_stat *pfs_last= & m_stat[COUNT_PFS_TL_LOCK_TYPE];
    for ( ; pfs < pfs_last ; pfs++)
      result->aggregate(pfs);
  }
};

/** Statistics for TABLE usage. */
struct PFS_table_stat
{
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

  /** Reset table io statistic. */
  inline void reset_io(void)
  {
    PFS_table_io_stat *stat= & m_index_stat[0];
    PFS_table_io_stat *stat_last= & m_index_stat[MAX_INDEXES + 1];
    for ( ; stat < stat_last ; stat++)
      stat->reset();
  }

  /** Reset table lock statistic. */
  inline void reset_lock(void)
  {
    m_lock_stat.reset();
  }

  /** Reset table statistic. */
  inline void reset(void)
  {
    reset_io();
    reset_lock();
  }

  inline void fast_reset_io(void)
  {
    memcpy(& m_index_stat, & g_reset_template.m_index_stat, sizeof(m_index_stat));
  }

  inline void fast_reset_lock(void)
  {
    memcpy(& m_lock_stat, & g_reset_template.m_lock_stat, sizeof(m_lock_stat));
  }

  inline void fast_reset(void)
  {
    memcpy(this, & g_reset_template, sizeof(*this));
  }

  inline void aggregate_io(const PFS_table_stat *stat, uint key_count)
  {
    PFS_table_io_stat *to_stat;
    PFS_table_io_stat *to_stat_last;
    const PFS_table_io_stat *from_stat;

    DBUG_ASSERT(key_count <= MAX_INDEXES);

    /* Aggregate stats for each index, if any */
    to_stat= & m_index_stat[0];
    to_stat_last= to_stat + key_count;
    from_stat= & stat->m_index_stat[0];
    for ( ; to_stat < to_stat_last ; from_stat++, to_stat++)
      to_stat->aggregate(from_stat);

    /* Aggregate stats for the table */
    to_stat= & m_index_stat[MAX_INDEXES];
    from_stat= & stat->m_index_stat[MAX_INDEXES];
    to_stat->aggregate(from_stat);
  }

  inline void aggregate_lock(const PFS_table_stat *stat)
  {
    m_lock_stat.aggregate(& stat->m_lock_stat);
  }

  inline void aggregate(const PFS_table_stat *stat, uint key_count)
  {
    aggregate_io(stat, key_count);
    aggregate_lock(stat);
  }

  inline void sum_io(PFS_single_stat *result, uint key_count)
  {
    PFS_table_io_stat *stat;
    PFS_table_io_stat *stat_last;

    DBUG_ASSERT(key_count <= MAX_INDEXES);

    /* Sum stats for each index, if any */
    stat= & m_index_stat[0];
    stat_last= stat + key_count;
    for ( ; stat < stat_last ; stat++)
      stat->sum(result);

    /* Sum stats for the table */
    m_index_stat[MAX_INDEXES].sum(result);
  }

  inline void sum_lock(PFS_single_stat *result)
  {
    m_lock_stat.sum(result);
  }

  inline void sum(PFS_single_stat *result, uint key_count)
  {
    sum_io(result, key_count);
    sum_lock(result);
  }

  static struct PFS_table_stat g_reset_template;
};

/** Statistics for SOCKET IO. Used for both waits and byte counts. */
struct PFS_socket_io_stat
{
  /** READ statistics */
  PFS_byte_stat m_read;
  /** WRITE statistics */
  PFS_byte_stat m_write;
  /** Miscelleanous statistics */
  PFS_byte_stat m_misc;

  inline void reset(void)
  {
    m_read.reset();
    m_write.reset();
    m_misc.reset();
  }

  inline void aggregate(const PFS_socket_io_stat *stat)
  {
    m_read.aggregate(&stat->m_read);
    m_write.aggregate(&stat->m_write);
    m_misc.aggregate(&stat->m_misc);
  }

  /* Sum waits and byte counts */
  inline void sum(PFS_byte_stat *stat)
  {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }

  /* Sum waits only */
  inline void sum_waits(PFS_single_stat *stat)
  {
    stat->aggregate(&m_read);
    stat->aggregate(&m_write);
    stat->aggregate(&m_misc);
  }
};

/** Statistics for SOCKET usage. */
struct PFS_socket_stat
{
  /** Socket timing and byte count statistics per operation */
  PFS_socket_io_stat m_io_stat;

  /** Reset socket statistics. */
  inline void reset(void)
  {
    m_io_stat.reset();
  }
};

struct PFS_connection_stat
{
  PFS_connection_stat()
  : m_current_connections(0),
    m_total_connections(0)
  {}

  ulonglong m_current_connections;
  ulonglong m_total_connections;

  inline void aggregate_active(ulonglong active)
  {
    m_current_connections+= active;
    m_total_connections+= active;
  }

  inline void aggregate_disconnected(ulonglong disconnected)
  {
    m_total_connections+= disconnected;
  }
};

/** @} */
#endif

