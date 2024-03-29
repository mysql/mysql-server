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

#ifndef TABLE_EVENTS_STATEMENTS_H
#define TABLE_EVENTS_STATEMENTS_H

/**
  @file storage/perfschema/table_events_statements.h
  Table EVENTS_STATEMENTS_xxx (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "mysql_com.h"
#include "sql/sql_digest.h"
#include "sql_string.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_name.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_events;
struct PFS_events_statements;
struct PFS_thread;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_events_statements : public PFS_engine_index {
 public:
  PFS_index_events_statements()
      : PFS_engine_index(&m_key_1, &m_key_2),
        m_key_1("THREAD_ID"),
        m_key_2("EVENT_ID") {}

  ~PFS_index_events_statements() override = default;

  bool match(PFS_thread *pfs);
  bool match(PFS_events *pfs);

 private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

/** A row of table_events_statements_common. */
struct row_events_statements {
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column EVENT_ID. */
  ulonglong m_event_id;
  /** Column END_EVENT_ID. */
  ulonglong m_end_event_id;
  /** Column NESTING_EVENT_ID. */
  ulonglong m_nesting_event_id;
  /** Column NESTING_EVENT_TYPE. */
  enum_event_type m_nesting_event_type;
  /** Column NESTING_EVENT_LEVEL. */
  uint m_nesting_event_level;
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column TIMER_START. */
  ulonglong m_timer_start;
  /** Column TIMER_END. */
  ulonglong m_timer_end;
  /** Column TIMER_WAIT. */
  ulonglong m_timer_wait;
  /** Column LOCK_TIME. */
  ulonglong m_lock_time;
  /** Column SOURCE. */
  char m_source[COL_SOURCE_SIZE];
  /** Length in bytes of @c m_source. */
  uint m_source_length;
  /** Column SQL_TEXT. */
  String m_sqltext;
  /** Column DIGEST and DIGEST_TEXT. */
  PFS_digest_row m_digest;
  /** Column CURRENT_SCHEMA. */
  PFS_schema_name m_current_schema_name;

  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column OBJECT_SCHEMA. */
  PFS_schema_name m_schema_name;
  /** Column OBJECT_NAME. */
  PFS_object_name m_object_name;

  /** Column MESSAGE_TEXT. */
  char m_message_text[MYSQL_ERRMSG_SIZE + 1];
  /** Column MYSQL_ERRNO. */
  uint m_sql_errno;
  /** Column RETURNED_SQLSTATE. */
  char m_sqlstate[SQLSTATE_LENGTH];
  /** Column ERRORS. */
  uint m_error_count;
  /** Column WARNINGS. */
  uint m_warning_count;
  /** Column ROWS_AFFECTED. */
  ulonglong m_rows_affected;
  /** Column ROWS_SENT. */
  ulonglong m_rows_sent;
  /** Column ROWS_EXAMINED. */
  ulonglong m_rows_examined;
  /** Column CREATED_TMP_DISK_TABLES. */
  ulonglong m_created_tmp_disk_tables;
  /** Column CREATED_TMP_TABLES. */
  ulonglong m_created_tmp_tables;
  /** Column SELECT_FULL_JOIN. */
  ulonglong m_select_full_join;
  /** Column SELECT_FULL_RANGE_JOIN. */
  ulonglong m_select_full_range_join;
  /** Column SELECT_RANGE. */
  ulonglong m_select_range;
  /** Column SELECT_RANGE_CHECK. */
  ulonglong m_select_range_check;
  /** Column SELECT_SCAN. */
  ulonglong m_select_scan;
  /** Column SORT_MERGE_PASSES. */
  ulonglong m_sort_merge_passes;
  /** Column SORT_RANGE. */
  ulonglong m_sort_range;
  /** Column SORT_ROWS. */
  ulonglong m_sort_rows;
  /** Column SORT_SCAN. */
  ulonglong m_sort_scan;
  /** Column NO_INDEX_USED. */
  ulonglong m_no_index_used;
  /** Column NO_GOOD_INDEX_USED. */
  ulonglong m_no_good_index_used;
  /** Column CPU_TIME. */
  ulonglong m_cpu_time;
  /** Column MAX_CONTROLLED_MEMORY. */
  ulonglong m_max_controlled_memory;
  /** Column MAX_TOTAL_MEMORY. */
  ulonglong m_max_total_memory;

  /** Column STATEMENT_ID. */
  ulonglong m_statement_id;
  /** Column EXECUTION_ENGINE. */
  bool m_secondary;
};

/** Position of a cursor on PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_CURRENT. */
struct pos_events_statements_current : public PFS_double_index {
  pos_events_statements_current() : PFS_double_index(0, 0) {}

  inline void reset() {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline void next_thread() {
    m_index_1++;
    m_index_2 = 0;
  }
};

/** Position of a cursor on PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTORY. */
struct pos_events_statements_history : public PFS_double_index {
  pos_events_statements_history() : PFS_double_index(0, 0) {}

  inline void reset() {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline void next_thread() {
    m_index_1++;
    m_index_2 = 0;
  }
};

/**
  Adapter, for table sharing the structure of
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_CURRENT.
*/
class table_events_statements_common : public PFS_engine_table {
 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_events_statements_common(const PFS_engine_table_share *share,
                                 void *pos);

  ~table_events_statements_common() override = default;

  int make_row_part_1(PFS_events_statements *statement,
                      sql_digest_storage *digest);

  int make_row_part_2(const sql_digest_storage *digest);

  /** Current row. */
  row_events_statements m_row;
  unsigned char m_token_array[MAX_DIGEST_STORAGE_SIZE];
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_CURRENT. */
class table_events_statements_current : public table_events_statements_common {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  table_events_statements_current();

 public:
  ~table_events_statements_current() override = default;

 private:
  friend class table_events_statements_history;
  friend class table_events_statements_history_long;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  int make_row(PFS_thread *pfs_thread, PFS_events_statements *statement);

  /** Current position. */
  pos_events_statements_current m_pos;
  /** Next position. */
  pos_events_statements_current m_next_pos;

  PFS_index_events_statements *m_opened_index;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTORY. */
class table_events_statements_history : public table_events_statements_common {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;
  void reset_position() override;

 protected:
  table_events_statements_history();

 public:
  ~table_events_statements_history() override = default;

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  int make_row(PFS_thread *pfs_thread, PFS_events_statements *statement);

  /** Current position. */
  pos_events_statements_history m_pos;
  /** Next position. */
  pos_events_statements_history m_next_pos;

  PFS_index_events_statements *m_opened_index;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTORY_LONG. */
class table_events_statements_history_long
    : public table_events_statements_common {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;
  void reset_position() override;

 protected:
  table_events_statements_history_long();

 public:
  ~table_events_statements_history_long() override = default;

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  int make_row(PFS_events_statements *statement);

  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
