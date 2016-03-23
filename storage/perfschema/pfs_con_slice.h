/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_CON_SLICE_H
#define PFS_CON_SLICE_H

/**
  @file storage/perfschema/pfs_con_slice.h
  Performance schema connection slice (declarations).
*/

#include "sql_class.h"
#include "pfs_lock.h"
#include "lf.h"
#include "pfs_status.h"

struct PFS_single_stat;
struct PFS_stage_stat;
struct PFS_statement_stat;
struct PFS_transaction_stat;
struct PFS_memory_stat;
class PFS_opaque_container_page;

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/**
  A connection slice, an arbitrary grouping of several connections.
  This structure holds statistics for grouping of connections.
*/
struct PFS_connection_slice
{
  /** Reset all statistics. */
  inline void reset_stats()
  {
    m_has_waits_stats= false;
    m_has_stages_stats= false;
    m_has_statements_stats= false;
    m_has_transactions_stats= false;
    m_has_memory_stats= false;
    reset_status_stats();
  }

  /** Reset all wait statistics. */
  void reset_waits_stats();
  /** Reset all stages statistics. */
  void reset_stages_stats();
  /** Reset all statements statistics. */
  void reset_statements_stats();
  /** Reset all transactions statistics. */
  void reset_transactions_stats();
  /** Reset all memory statistics. */
  void rebase_memory_stats();
  /** Reset all status variable statistics. */
  void reset_status_stats()
  {
    m_status_stats.reset();
  }

  void set_instr_class_waits_stats(PFS_single_stat *array)
  {
    m_has_waits_stats= false;
    m_instr_class_waits_stats= array;
  }

  const PFS_single_stat* read_instr_class_waits_stats() const
  {
    if (! m_has_waits_stats)
      return NULL;
    return m_instr_class_waits_stats;
  }

  PFS_single_stat* write_instr_class_waits_stats()
  {
    if (! m_has_waits_stats)
    {
      reset_waits_stats();
      m_has_waits_stats= true;
    }
    return m_instr_class_waits_stats;
  }

  void set_instr_class_stages_stats(PFS_stage_stat *array)
  {
    m_has_stages_stats= false;
    m_instr_class_stages_stats= array;
  }

  const PFS_stage_stat* read_instr_class_stages_stats() const
  {
    if (! m_has_stages_stats)
      return NULL;
    return m_instr_class_stages_stats;
  }

  PFS_stage_stat* write_instr_class_stages_stats()
  {
    if (! m_has_stages_stats)
    {
      reset_stages_stats();
      m_has_stages_stats= true;
    }
    return m_instr_class_stages_stats;
  }

  void set_instr_class_statements_stats(PFS_statement_stat *array)
  {
    m_has_statements_stats= false;
    m_instr_class_statements_stats= array;
  }

  const PFS_statement_stat* read_instr_class_statements_stats() const
  {
    if (! m_has_statements_stats)
      return NULL;
    return m_instr_class_statements_stats;
  }

  PFS_statement_stat* write_instr_class_statements_stats()
  {
    if (! m_has_statements_stats)
    {
      reset_statements_stats();
      m_has_statements_stats= true;
    }
    return m_instr_class_statements_stats;
  }

  void set_instr_class_transactions_stats(PFS_transaction_stat *array)
  {
    m_has_transactions_stats= false;
    m_instr_class_transactions_stats= array;
  }

  const PFS_transaction_stat* read_instr_class_transactions_stats() const
  {
    if (! m_has_transactions_stats)
      return NULL;
    return m_instr_class_transactions_stats;
  }

  PFS_transaction_stat* write_instr_class_transactions_stats()
  {
    if (! m_has_transactions_stats)
    {
      reset_transactions_stats();
      m_has_transactions_stats= true;
    }
    return m_instr_class_transactions_stats;
  }

  void set_instr_class_memory_stats(PFS_memory_stat *array)
  {
    m_has_memory_stats= false;
    m_instr_class_memory_stats= array;
  }

  const PFS_memory_stat* read_instr_class_memory_stats() const
  {
    if (! m_has_memory_stats)
      return NULL;
    return m_instr_class_memory_stats;
  }

  PFS_memory_stat* write_instr_class_memory_stats()
  {
    if (! m_has_memory_stats)
    {
      rebase_memory_stats();
      m_has_memory_stats= true;
    }
    return m_instr_class_memory_stats;
  }

private:
  bool m_has_waits_stats;
  bool m_has_stages_stats;
  bool m_has_statements_stats;
  bool m_has_transactions_stats;
  bool m_has_memory_stats;

  /**
    Per connection slice waits aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_*_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_single_stat *m_instr_class_waits_stats;

  /**
    Per connection slice stages aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_STAGES_SUMMARY_BY_*_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_stage_stat *m_instr_class_stages_stats;

  /**
    Per connection slice statements aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_*_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_statement_stat *m_instr_class_statements_stats;

  /**
    Per connection slice transactions aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_TRANSACTIONS_SUMMARY_BY_*_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_transaction_stat *m_instr_class_transactions_stats;

  /**
    Per connection slice memory aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.MEMORY_SUMMARY_BY_*_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_memory_stat *m_instr_class_memory_stats;

public:

  void aggregate_status_stats(const STATUS_VAR *status_vars)
  {
    m_status_stats.aggregate_from(status_vars);
  }

  /**
    Aggregated status variables.
  */
  PFS_status_stats m_status_stats;

  /** Container page. */
  PFS_opaque_container_page *m_page;
};

/** @} */
#endif

