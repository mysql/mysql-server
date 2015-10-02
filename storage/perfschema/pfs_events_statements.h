/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_EVENTS_STATEMENTS_H
#define PFS_EVENTS_STATEMENTS_H

/**
  @file storage/perfschema/pfs_events_statements.h
  Events statements data structures (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_events.h"
#include "pfs_digest.h"

struct PFS_thread;
struct PFS_account;
struct PFS_user;
struct PFS_host;

/** A statement record. */
struct PFS_events_statements : public PFS_events
{
  enum_object_type m_sp_type;
  char m_schema_name[NAME_LEN];
  uint m_schema_name_length;
  char m_object_name[NAME_LEN];
  uint m_object_name_length;

  /** Database name. */
  char m_current_schema_name[NAME_LEN];
  /** Length of @c m_current_schema_name. */
  uint m_current_schema_name_length;

  /** Locked time. */
  ulonglong m_lock_time;

  /** Diagnostics area, message text. */
  char m_message_text[MYSQL_ERRMSG_SIZE+1];
  /** Diagnostics area, error number. */
  uint m_sql_errno;
  /** Diagnostics area, SQLSTATE. */
  char m_sqlstate[SQLSTATE_LENGTH];
  /** Diagnostics area, error count. */
  uint m_error_count;
  /** Diagnostics area, warning count. */
  uint m_warning_count;
  /** Diagnostics area, rows affected. */
  ulonglong m_rows_affected;

  /** Optimizer metric, number of rows sent. */
  ulonglong m_rows_sent;
  /** Optimizer metric, number of rows examined. */
  ulonglong m_rows_examined;
  /** Optimizer metric, number of temporary tables created on disk. */
  ulonglong m_created_tmp_disk_tables;
  /** Optimizer metric, number of temporary tables created. */
  ulonglong m_created_tmp_tables;
  /** Optimizer metric, number of full join. */
  ulonglong m_select_full_join;
  /** Optimizer metric, number of full range join. */
  ulonglong m_select_full_range_join;
  /** Optimizer metric, number of select range. */
  ulonglong m_select_range;
  /** Optimizer metric, number of select range checks. */
  ulonglong m_select_range_check;
  /** Optimizer metric, number of select scans. */
  ulonglong m_select_scan;
  /** Optimizer metric, number of sort merge passes. */
  ulonglong m_sort_merge_passes;
  /** Optimizer metric, number of sort ranges. */
  ulonglong m_sort_range;
  /** Optimizer metric, number of sort rows. */
  ulonglong m_sort_rows;
  /** Optimizer metric, number of sort scans. */
  ulonglong m_sort_scan;
  /** Optimizer metric, number of 'no index used'. */
  ulonglong m_no_index_used;
  /** Optimizer metric, number of 'no good index used'. */
  ulonglong m_no_good_index_used;

  /** True if sqltext was truncated. */
  bool m_sqltext_truncated;
  /** Statement character set number. */
  uint m_sqltext_cs_number;

  /**
    SQL_TEXT.
    This pointer is immutable,
    and always point to pre allocated memory.
  */
  char *m_sqltext;
  /** Length of @ m_info. */
  uint m_sqltext_length;
  /**
    Statement digest.
    This underlying token array storage pointer is immutable,
    and always point to pre allocated memory.
  */
  sql_digest_storage m_digest_storage;
};

void insert_events_statements_history(PFS_thread *thread, PFS_events_statements *statement);
void insert_events_statements_history_long(PFS_events_statements *statement);

extern ulong nested_statement_lost;

extern bool flag_events_statements_current;
extern bool flag_events_statements_history;
extern bool flag_events_statements_history_long;

extern bool events_statements_history_long_full;
extern PFS_ALIGNED PFS_cacheline_uint32 events_statements_history_long_index;
extern PFS_events_statements *events_statements_history_long_array;
extern size_t events_statements_history_long_size;

int init_events_statements_history_long(size_t events_statements_history_long_sizing);
void cleanup_events_statements_history_long();

void reset_events_statements_current();
void reset_events_statements_history();
void reset_events_statements_history_long();
void reset_events_statements_by_thread();
void reset_events_statements_by_account();
void reset_events_statements_by_user();
void reset_events_statements_by_host();
void reset_events_statements_global();
void aggregate_account_statements(PFS_account *account);
void aggregate_user_statements(PFS_user *user);
void aggregate_host_statements(PFS_host *host);

#endif

