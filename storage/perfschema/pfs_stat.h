/* Copyright (C) 2008-2009 Sun Microsystems, Inc

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef PFS_STAT_H
#define PFS_STAT_H

/**
  @file storage/perfschema/pfs_stat.h
  Statistics (declarations).
*/

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Usage statistics chain, for a single value and its aggregates. */
struct PFS_single_stat_chain
{
  /**
    Control flag.
    Statistics are aggregated only if the control flag is true.
  */
  bool *m_control_flag;
  /** Next link in the statistics chain. */
  struct PFS_single_stat_chain *m_parent;
  /** Count of values. */
  ulonglong m_count;
  /** Sum of values. */
  ulonglong m_sum;
  /** Minimum value. */
  ulonglong m_min;
  /** Maximum value. */
  ulonglong m_max;
};

/**
  Reset a single statistic link.
  Only the current link is reset, parents are not affected.
  @param stat                         the statistics link to reset
*/
inline void reset_single_stat_link(PFS_single_stat_chain *stat)
{
  stat->m_count= 0;
  stat->m_sum= 0;
  stat->m_min= ULONGLONG_MAX;
  stat->m_max= 0;
}

/**
  Aggregate a value to a statistic chain.
  @param stat                         the aggregated statistic chain
  @param value                        the value to aggregate
*/
inline void aggregate_single_stat_chain(PFS_single_stat_chain *stat,
                                        ulonglong value)
{
  do
  {
    if (*stat->m_control_flag)
    {
      stat->m_count++;
      stat->m_sum+= value;
      if (stat->m_min > value)
        stat->m_min= value;
      if (stat->m_max < value)
        stat->m_max= value;
    }
    stat= stat->m_parent;
  }
  while (stat);
}

/** Statistics for COND usage. */
struct PFS_cond_stat
{
  /** Number of times a condition was signalled. */
  ulonglong m_signal_count;
  /** Number of times a condition was broadcasted. */
  ulonglong m_broadcast_count;
};

/** Statistics for FILE usage. */
struct PFS_file_stat
{
  /** Number of current open handles. */
  ulong m_open_count;
  /** Count of READ operations. */
  ulonglong m_count_read;
  /** Count of WRITE operations. */
  ulonglong m_count_write;
  /** Number of bytes read. */
  ulonglong m_read_bytes;
  /** Number of bytes written. */
  ulonglong m_write_bytes;
};

/**
  Reset file statistic.
  @param stat                         the statistics to reset
*/
inline void reset_file_stat(PFS_file_stat *stat)
{
  stat->m_open_count= 0;
  stat->m_count_read= 0;
  stat->m_count_write= 0;
  stat->m_read_bytes= 0;
  stat->m_write_bytes= 0;
}

/** @} */
#endif

