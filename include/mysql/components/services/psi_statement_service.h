/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPONENTS_SERVICES_PSI_STATEMENT_SERVICE_H
#define COMPONENTS_SERVICES_PSI_STATEMENT_SERVICE_H

#include <mysql/components/services/psi_statement_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_statement_v1)
  /** @sa register_statement_v1_t. */
  register_statement_v1_t register_statement;
  /** @sa get_thread_statement_locker_v1_t. */
  get_thread_statement_locker_v1_t get_thread_statement_locker;
  /** @sa refine_statement_v1_t. */
  refine_statement_v1_t refine_statement;
  /** @sa start_statement_v1_t. */
  start_statement_v1_t start_statement;
  /** @sa set_statement_text_v1_t. */
  set_statement_text_v1_t set_statement_text;
  /** @sa set_statement_lock_time_t. */
  set_statement_lock_time_t set_statement_lock_time;
  /** @sa set_statement_rows_sent_t. */
  set_statement_rows_sent_t set_statement_rows_sent;
  /** @sa set_statement_rows_examined_t. */
  set_statement_rows_examined_t set_statement_rows_examined;
  /** @sa inc_statement_created_tmp_disk_tables. */
  inc_statement_created_tmp_disk_tables_t inc_statement_created_tmp_disk_tables;
  /** @sa inc_statement_created_tmp_tables. */
  inc_statement_created_tmp_tables_t inc_statement_created_tmp_tables;
  /** @sa inc_statement_select_full_join. */
  inc_statement_select_full_join_t inc_statement_select_full_join;
  /** @sa inc_statement_select_full_range_join. */
  inc_statement_select_full_range_join_t inc_statement_select_full_range_join;
  /** @sa inc_statement_select_range. */
  inc_statement_select_range_t inc_statement_select_range;
  /** @sa inc_statement_select_range_check. */
  inc_statement_select_range_check_t inc_statement_select_range_check;
  /** @sa inc_statement_select_scan. */
  inc_statement_select_scan_t inc_statement_select_scan;
  /** @sa inc_statement_sort_merge_passes. */
  inc_statement_sort_merge_passes_t inc_statement_sort_merge_passes;
  /** @sa inc_statement_sort_range. */
  inc_statement_sort_range_t inc_statement_sort_range;
  /** @sa inc_statement_sort_rows. */
  inc_statement_sort_rows_t inc_statement_sort_rows;
  /** @sa inc_statement_sort_scan. */
  inc_statement_sort_scan_t inc_statement_sort_scan;
  /** @sa set_statement_no_index_used. */
  set_statement_no_index_used_t set_statement_no_index_used;
  /** @sa set_statement_no_good_index_used. */
  set_statement_no_good_index_used_t set_statement_no_good_index_used;
  /** @sa end_statement_v1_t. */
  end_statement_v1_t end_statement;

  /** @sa create_prepared_stmt_v1_t. */
  create_prepared_stmt_v1_t create_prepared_stmt;
  /** @sa destroy_prepared_stmt_v1_t. */
  destroy_prepared_stmt_v1_t destroy_prepared_stmt;
  /** @sa reprepare_prepared_stmt_v1_t. */
  reprepare_prepared_stmt_v1_t reprepare_prepared_stmt;
  /** @sa execute_prepared_stmt_v1_t. */
  execute_prepared_stmt_v1_t execute_prepared_stmt;
  /** @sa set_prepared_stmt_text_v1_t. */
  set_prepared_stmt_text_v1_t set_prepared_stmt_text;

  /** @sa digest_start_v1_t. */
  digest_start_v1_t digest_start;
  /** @sa digest_end_v1_t. */
  digest_end_v1_t digest_end;

  /** @sa get_sp_share_v1_t. */
  get_sp_share_v1_t get_sp_share;
  /** @sa release_sp_share_v1_t. */
  release_sp_share_v1_t release_sp_share;
  /** @sa start_sp_v1_t. */
  start_sp_v1_t start_sp;
  /** @sa start_sp_v1_t. */
  end_sp_v1_t end_sp;
  /** @sa drop_sp_v1_t. */
  drop_sp_v1_t drop_sp;
END_SERVICE_DEFINITION(psi_statement_v1)

#define REQUIRES_PSI_STATEMENT_SERVICE REQUIRES_SERVICE(psi_statement_v1)

#endif /* COMPONENTS_SERVICES_PSI_STATEMENT_SERVICE_H */

