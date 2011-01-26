/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

  inline void aggregate_timed(ulonglong value)
  {
    m_count++;
    m_sum+= value;
    if (unlikely(m_min > value))
      m_min= value;
    if (unlikely(m_max < value))
      m_max= value;
  }
};

/** Statistics for COND usage. */
struct PFS_cond_stat
{
  /** Number of times a condition was signalled. */
  ulonglong m_signal_count;
  /** Number of times a condition was broadcasted. */
  ulonglong m_broadcast_count;
};

/** Statistics for FILE IO usage. */
struct PFS_file_io_stat
{
  /** Count of READ operations. */
  ulonglong m_count_read;
  /** Count of WRITE operations. */
  ulonglong m_count_write;
  /** Number of bytes read. */
  ulonglong m_read_bytes;
  /** Number of bytes written. */
  ulonglong m_write_bytes;

  /** Reset file statistic. */
  inline void reset(void)
  {
    m_count_read= 0;
    m_count_write= 0;
    m_read_bytes= 0;
    m_write_bytes= 0;
  }

  inline void aggregate(const PFS_file_io_stat *stat)
  {
    m_count_read+= stat->m_count_read;
    m_count_write+= stat->m_count_write;
    m_read_bytes+= stat->m_read_bytes;
    m_write_bytes+= stat->m_write_bytes;
  }

  inline void aggregate_read(ulonglong bytes)
  {
    m_count_read++;
    m_read_bytes+= bytes;
  }

  inline void aggregate_write(ulonglong bytes)
  {
    m_count_write++;
    m_write_bytes+= bytes;
  }
};

/** Statistics for FILE usage. */
struct PFS_file_stat
{
  /** Number of current open handles. */
  ulong m_open_count;
  /** File IO statistics. */
  PFS_file_io_stat m_io_stat;
};

/** Single table io statistic. */
struct PFS_table_io_stat
{
  /** FETCH statistics */
  PFS_single_stat m_fetch;
  /** INSERT statistics */
  PFS_single_stat m_insert;
  /** UPDATE statistics */
  PFS_single_stat m_update;
  /** DELETE statistics */
  PFS_single_stat m_delete;

  inline void reset(void)
  {
    m_fetch.reset();
    m_insert.reset();
    m_update.reset();
    m_delete.reset();
  }

  inline void aggregate(const PFS_table_io_stat *stat)
  {
    m_fetch.aggregate(&stat->m_fetch);
    m_insert.aggregate(&stat->m_insert);
    m_update.aggregate(&stat->m_update);
    m_delete.aggregate(&stat->m_delete);
  }

  inline void sum(PFS_single_stat *result)
  {
    result->aggregate(& m_fetch);
    result->aggregate(& m_insert);
    result->aggregate(& m_update);
    result->aggregate(& m_delete);
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
    Each index stat is in [0, MAX_KEY-1],
    stats when using no index are in [MAX_KEY].
  */
  PFS_table_io_stat m_index_stat[MAX_KEY + 1];

  /**
    Statistics, per lock type.
  */
  PFS_table_lock_stat m_lock_stat;

  /** Reset table io statistic. */
  inline void reset_io(void)
  {
    PFS_table_io_stat *stat= & m_index_stat[0];
    PFS_table_io_stat *stat_last= & m_index_stat[MAX_KEY + 1];
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

  inline void aggregate_io(const PFS_table_stat *stat)
  {
    PFS_table_io_stat *to_stat= & m_index_stat[0];
    PFS_table_io_stat *to_stat_last= & m_index_stat[MAX_KEY + 1];
    const PFS_table_io_stat *from_stat= & stat->m_index_stat[0];
    for ( ; to_stat < to_stat_last ; from_stat++, to_stat++)
      to_stat->aggregate(from_stat);
  }

  inline void aggregate_lock(const PFS_table_stat *stat)
  {
    m_lock_stat.aggregate(& stat->m_lock_stat);
  }

  inline void aggregate(const PFS_table_stat *stat)
  {
    aggregate_io(stat);
    aggregate_lock(stat);
  }

  inline void sum_io(PFS_single_stat *result)
  {
    PFS_table_io_stat *stat= & m_index_stat[0];
    PFS_table_io_stat *stat_last= & m_index_stat[MAX_KEY + 1];
    for ( ; stat < stat_last ; stat++)
      stat->sum(result);
  }

  inline void sum_lock(PFS_single_stat *result)
  {
    m_lock_stat.sum(result);
  }

  inline void sum(PFS_single_stat *result)
  {
    sum_io(result);
    sum_lock(result);
  }
};

/** Statistics for SOCKET IO usage. */
struct PFS_socket_io_stat
{
  /** Count of READ operations. */
  ulonglong m_count_read;
  /** Count of WRITE operations. */
  ulonglong m_count_write;
  /** Number of bytes read. */
  ulonglong m_read_bytes;
  /** Number of bytes written. */
  ulonglong m_write_bytes;

  /** Reset socket statistics. */
  inline void reset(void)
  {
    m_count_read= 0;
    m_count_write= 0;
    m_read_bytes= 0;
    m_write_bytes= 0;
  }

  inline void aggregate(const PFS_socket_io_stat *stat)
  {
    m_count_read+= stat->m_count_read;
    m_count_write+= stat->m_count_write;
    m_read_bytes+= stat->m_read_bytes;
    m_write_bytes+= stat->m_write_bytes;
  }

  inline void aggregate_read(ulonglong bytes)
  {
    m_count_read++;
    m_read_bytes+= bytes;
  }

  inline void aggregate_write(ulonglong bytes)
  {
    m_count_write++;
    m_write_bytes+= bytes;
  }
};

/** Statistics for SOCKET usage. */
struct PFS_socket_stat
{
  /** Number of current open sockets. //TBD Not relevant */
  ulong m_open_count;
  /** Socket IO statistics. */
  PFS_socket_io_stat m_io_stat;
};


/** @} */
#endif

