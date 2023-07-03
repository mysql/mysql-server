/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

#include "storage/perfschema/pfs_con_slice.h"

#include "my_thread.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_stat.h"

/**
  @file storage/perfschema/pfs_con_slice.cc
  Performance schema connection slice (implementation).
*/

/**
  @addtogroup performance_schema_buffers
  @{
*/

void PFS_connection_slice::reset_waits_stats() {
  PFS_single_stat *stat = m_instr_class_waits_stats;
  PFS_single_stat *stat_last = stat + wait_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_connection_slice::reset_stages_stats() {
  PFS_stage_stat *stat = m_instr_class_stages_stats;
  PFS_stage_stat *stat_last = stat + stage_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_connection_slice::reset_statements_stats() {
  PFS_statement_stat *stat = m_instr_class_statements_stats;
  PFS_statement_stat *stat_last = stat + statement_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_connection_slice::reset_transactions_stats() {
  PFS_transaction_stat *stat =
      &m_instr_class_transactions_stats[GLOBAL_TRANSACTION_INDEX];
  if (stat) {
    stat->reset();
  }
}

void PFS_connection_slice::reset_errors_stats() {
  PFS_error_stat *stat = &m_instr_class_errors_stats[GLOBAL_ERROR_INDEX];
  if (stat) {
    stat->reset();
  }
}

/** @} */
