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

/** Statistics for TABLE usage. */
struct PFS_table_stat
{
  /**
    Statistics, per index.
    Each index stat is in [0, MAX_KEY-1],
    stats when using no index are in [MAX_KEY].
  */
  PFS_table_io_stat m_index_stat[MAX_KEY + 1];

  /** Reset table statistic. */
  inline void reset(void)
  {
    PFS_table_io_stat *stat= & m_index_stat[0];
    PFS_table_io_stat *stat_last= & m_index_stat[MAX_KEY + 1];
    for ( ; stat < stat_last ; stat++)
      stat->reset();
  }

  inline void aggregate(const PFS_table_stat *stat)
  {
    PFS_table_io_stat *to_stat= & m_index_stat[0];
    PFS_table_io_stat *to_stat_last= & m_index_stat[MAX_KEY + 1];
    const PFS_table_io_stat *from_stat= & stat->m_index_stat[0];
    for ( ; to_stat < to_stat_last ; from_stat++, to_stat++)
      to_stat->aggregate(from_stat);
  }

  inline void sum_io(PFS_single_stat *result)
  {
    PFS_table_io_stat *stat= & m_index_stat[0];
    PFS_table_io_stat *stat_last= & m_index_stat[MAX_KEY + 1];
    for ( ; stat < stat_last ; stat++)
      stat->sum(result);
  }

  inline void sum(PFS_single_stat *result)
  {
    sum_io(result);
    /* sum_lock(result); */
  }
};

/** Statistics for SOCKET usage. */
struct PFS_socket_stat
{
  /** Number of current open sockets. */
  ulong m_open_count;
  /** Count of RECV operations. */
  ulonglong m_count_recv;
  /** Count of SEND operations. */
  ulonglong m_count_send;
  /** Number of bytes received. */
  ulonglong m_recv_bytes;
  /** Number of bytes sent. */
  ulonglong m_send_bytes;

  /** Reset socket statistics. */
  inline void reset(void)
  {
    m_open_count= 0;
    m_count_recv= 0;
    m_count_send= 0;
    m_count_recv_bytes= 0;
    m_count_send_bytes= 0;
  }

  inline void aggregate(const PFS_file_io_stat *stat)
  {
    m_count_read+= stat->m_count_read;
    m_count_write+= stat->m_count_write;
    m_read_bytes+= stat->m_read_bytes;
    m_write_bytes+= stat->m_write_bytes;
  }

  inline void aggregate_recv(ulonglong bytes)
  {
    m_count_recv++;
    m_recv_bytes+= bytes;
  }

  inline void aggregate_send(ulonglong bytes)
  {
    m_count_send++;
    m_send_bytes+= bytes;
  }

};

/**
  Reset socket statistic.
  @param stat                         the statistics to reset
*/
inline void reset_socket_stat(PFS_socket_stat *stat)
{
  stat->m_open_count= 0;
  stat->m_count_recv= 0;
  stat->m_count_send= 0;
  stat->m_recv_bytes= 0;
  stat->m_send_bytes= 0;
}


/** @} */
#endif

