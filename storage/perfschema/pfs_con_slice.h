/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "pfs_lock.h"
#include "lf.h"

struct PFS_single_stat;
struct PFS_stage_stat;
struct PFS_statement_stat;

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
  /**
    Allocate memory for waits statistics.
    @param sizing the number of wait classes.
    @return wait statistics for this slice.
  */
  static PFS_single_stat *alloc_waits_slice(uint sizing);
  /**
    Allocate memory for stages statistics.
    @param sizing the number of stage classes.
    @return stage statistics for this slice.
  */
  static PFS_stage_stat *alloc_stages_slice(uint sizing);
  /**
    Allocate memory for statement statistics.
    @param sizing the number of statement classes.
    @return statement statistics for this slice.
  */
  static PFS_statement_stat *alloc_statements_slice(uint sizing);

  /** Reset all statistics. */
  inline void reset_stats()
  {
    reset_waits_stats();
    reset_stages_stats();
    reset_statements_stats();
  }

  /** Reset all wait statistics. */
  void reset_waits_stats();
  /** Reset all stages statistics. */
  void reset_stages_stats();
  /** Reset all statements statistics. */
  void reset_statements_stats();

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
};

/** @} */
#endif

