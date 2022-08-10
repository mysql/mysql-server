/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_STATEMENT_PROVIDER_H
#define PFS_STATEMENT_PROVIDER_H

/**
  @file include/pfs_statement_provider.h
  Performance schema instrumentation (declarations).
*/

#include <sys/types.h>

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
#ifndef MYSQL_DYNAMIC_PLUGIN
#ifndef WITH_LOCK_ORDER

#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/psi/psi_statement.h"
#include "sql/sql_digest.h"

struct PSI_digest_locker;
struct sql_digest_storage;

#define PSI_STATEMENT_CALL(M) pfs_##M##_vc
#define PSI_DIGEST_CALL(M) pfs_##M##_vc
#define PSI_PS_CALL(M) pfs_##M##_vc

void pfs_register_statement_vc(const char *category, PSI_statement_info *info,
                               int count);

PSI_statement_locker *pfs_get_thread_statement_locker_vc(
    PSI_statement_locker_state *state, PSI_statement_key key,
    const void *charset, PSI_sp_share *sp_share);

PSI_statement_locker *pfs_refine_statement_vc(PSI_statement_locker *locker,
                                              PSI_statement_key key);

void pfs_start_statement_vc(PSI_statement_locker *locker, const char *db,
                            uint db_len, const char *src_file, uint src_line);

void pfs_set_statement_text_vc(PSI_statement_locker *locker, const char *text,
                               uint text_len);

void pfs_set_statement_query_id_vc(PSI_statement_locker *locker,
                                   ulonglong count);

void pfs_set_statement_lock_time_vc(PSI_statement_locker *locker,
                                    ulonglong count);

void pfs_set_statement_rows_sent_vc(PSI_statement_locker *locker,
                                    ulonglong count);

void pfs_set_statement_rows_examined_vc(PSI_statement_locker *locker,
                                        ulonglong count);

void pfs_inc_statement_created_tmp_disk_tables_vc(PSI_statement_locker *locker,
                                                  ulong count);

void pfs_inc_statement_created_tmp_tables_vc(PSI_statement_locker *locker,
                                             ulong count);

void pfs_inc_statement_select_full_join_vc(PSI_statement_locker *locker,
                                           ulong count);

void pfs_inc_statement_select_full_range_join_vc(PSI_statement_locker *locker,
                                                 ulong count);

void pfs_inc_statement_select_range_vc(PSI_statement_locker *locker,
                                       ulong count);

void pfs_inc_statement_select_range_check_vc(PSI_statement_locker *locker,
                                             ulong count);

void pfs_inc_statement_select_scan_vc(PSI_statement_locker *locker,
                                      ulong count);

void pfs_inc_statement_sort_merge_passes_vc(PSI_statement_locker *locker,
                                            ulong count);

void pfs_inc_statement_sort_range_vc(PSI_statement_locker *locker, ulong count);

void pfs_inc_statement_sort_rows_vc(PSI_statement_locker *locker, ulong count);

void pfs_inc_statement_sort_scan_vc(PSI_statement_locker *locker, ulong count);

void pfs_set_statement_no_index_used_vc(PSI_statement_locker *locker);

void pfs_set_statement_no_good_index_used_vc(PSI_statement_locker *locker);

void pfs_set_statement_secondary_engine_vc(PSI_statement_locker *locker,
                                           bool secondary);

void pfs_end_statement_vc(PSI_statement_locker *locker, void *stmt_da);

PSI_prepared_stmt *pfs_create_prepared_stmt_vc(void *identity, uint stmt_id,
                                               PSI_statement_locker *locker,
                                               const char *stmt_name,
                                               size_t stmt_name_length,
                                               const char *sql_text,
                                               size_t sql_text_length);

void pfs_destroy_prepared_stmt_vc(PSI_prepared_stmt *prepared_stmt);

void pfs_reprepare_prepared_stmt_vc(PSI_prepared_stmt *prepared_stmt);

void pfs_execute_prepared_stmt_vc(PSI_statement_locker *locker,
                                  PSI_prepared_stmt *ps);

void pfs_set_prepared_stmt_text_vc(PSI_prepared_stmt *prepared_stmt,
                                   const char *text, uint text_len);

void pfs_set_prepared_stmt_secondary_engine_vc(PSI_prepared_stmt *prepared_stmt,
                                               bool secondary);

PSI_digest_locker *pfs_digest_start_vc(PSI_statement_locker *locker);

void pfs_digest_end_vc(PSI_digest_locker *locker,
                       const sql_digest_storage *digest);

#endif /* WITH_LOCK_ORDER */
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER || PFS_DIRECT_CALL */
#endif /* HAVE_PSI_STATEMENT_INTERFACE */

#endif
