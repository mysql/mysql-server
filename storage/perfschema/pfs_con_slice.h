/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_CON_SLICE_H
#define PFS_CON_SLICE_H

/**
  @file storage/perfschema/pfs_con_slice.h
  Performance schema connection slice (declarations).
*/

#include <stddef.h>

#include "lf.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_status.h"

struct PFS_single_stat;
struct PFS_stage_stat;
struct PFS_statement_stat;
struct PFS_transaction_stat;
struct PFS_error_stat;
class PFS_opaque_container_page;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/**
  A connection slice, an arbitrary grouping of several connections.
  This structure holds statistics for grouping of connections.
*/
struct PFS_connection_slice {
  /** Reset all statistics. */
  inline void reset_stats() {
    m_has_waits_stats = false;
    m_has_stages_stats = false;
    m_has_statements_stats = false;
    m_has_transactions_stats = false;
    m_has_errors_stats = false;
    m_has_memory_stats = false;
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
  /** Reset all errors statistics. */
  void reset_errors_stats();
  /** Reset all status variable statistics. */
  void reset_status_stats() { m_status_stats.reset(); }

  void set_instr_class_waits_stats(PFS_single_stat *array) {
    m_has_waits_stats = false;
    m_instr_class_waits_stats = array;
  }

  const PFS_single_stat *read_instr_class_waits_stats() const {
    if (!m_has_waits_stats) {
      return NULL;
    }
    return m_instr_class_waits_stats;
  }

  PFS_single_stat *write_instr_class_waits_stats() {
    if (!m_has_waits_stats) {
      reset_waits_stats();
      m_has_waits_stats = true;
    }
    return m_instr_class_waits_stats;
  }

  void set_instr_class_stages_stats(PFS_stage_stat *array) {
    m_has_stages_stats = false;
    m_instr_class_stages_stats = array;
  }

  const PFS_stage_stat *read_instr_class_stages_stats() const {
    if (!m_has_stages_stats) {
      return NULL;
    }
    return m_instr_class_stages_stats;
  }

  PFS_stage_stat *write_instr_class_stages_stats() {
    if (!m_has_stages_stats) {
      reset_stages_stats();
      m_has_stages_stats = true;
    }
    return m_instr_class_stages_stats;
  }

  void set_instr_class_statements_stats(PFS_statement_stat *array) {
    m_has_statements_stats = false;
    m_instr_class_statements_stats = array;
  }

  const PFS_statement_stat *read_instr_class_statements_stats() const {
    if (!m_has_statements_stats) {
      return NULL;
    }
    return m_instr_class_statements_stats;
  }

  PFS_statement_stat *write_instr_class_statements_stats() {
    if (!m_has_statements_stats) {
      reset_statements_stats();
      m_has_statements_stats = true;
    }
    return m_instr_class_statements_stats;
  }

  void set_instr_class_transactions_stats(PFS_transaction_stat *array) {
    m_has_transactions_stats = false;
    m_instr_class_transactions_stats = array;
  }

  const PFS_transaction_stat *read_instr_class_transactions_stats() const {
    if (!m_has_transactions_stats) {
      return NULL;
    }
    return m_instr_class_transactions_stats;
  }

  PFS_transaction_stat *write_instr_class_transactions_stats() {
    if (!m_has_transactions_stats) {
      reset_transactions_stats();
      m_has_transactions_stats = true;
    }
    return m_instr_class_transactions_stats;
  }

  void set_instr_class_errors_stats(PFS_error_stat *array) {
    m_has_errors_stats = false;
    m_instr_class_errors_stats = array;
  }

  const PFS_error_stat *read_instr_class_errors_stats() const {
    if (!m_has_errors_stats) {
      return NULL;
    }
    return m_instr_class_errors_stats;
  }

  PFS_error_stat *write_instr_class_errors_stats() {
    if (!m_has_errors_stats) {
      reset_errors_stats();
      m_has_errors_stats = true;
    }
    return m_instr_class_errors_stats;
  }

 protected:
  bool m_has_memory_stats;

 private:
  bool m_has_waits_stats;
  bool m_has_stages_stats;
  bool m_has_statements_stats;
  bool m_has_transactions_stats;
  bool m_has_errors_stats;

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
    Per connection slice error aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_ERRORS_SUMMARY_BY_*_BY_ERROR.
    Immutable, safe to use without internal lock.
  */
  PFS_error_stat *m_instr_class_errors_stats;

 public:
  void aggregate_status_stats(const System_status_var *status_vars) {
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
