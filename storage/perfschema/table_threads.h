/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_THREADS_H
#define TABLE_THREADS_H

#include "pfs_column_types.h"
#include "cursor_by_thread.h"

struct PFS_thread;

/**
  \addtogroup Performance_schema_tables
  @{
*/

/**
  A row of PERFORMANCE_SCHEMA.THREADS.
*/
struct row_threads
{
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column PROCESSLIST_ID. */
  ulonglong m_processlist_id;
  /** Column NAME. */
  const char* m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column PROCESSLIST_USER. */
  char m_username[USERNAME_LENGTH];
  /** Length in bytes of @c m_username. */
  uint m_username_length;
  /** Column PROCESSLIST_HOST. */
  char m_hostname[HOSTNAME_LENGTH];
  /** Length in bytes of @c m_hostname. */
  uint m_hostname_length;
  /** Column PROCESSLIST_DB. */
  char m_dbname[NAME_LEN];
  /** Length in bytes of @c m_dbname. */
  uint m_dbname_length;
  /** Column PROCESSLIST_COMMAND. */
  int m_command;
  /** Column PROCESSLIST_TIME. */
  time_t m_start_time;
  /** Column PROCESSLIST_STATE. */
  const char* m_processlist_state_ptr;
  /** Length in bytes of @c m_processlist_state_ptr. */
  uint m_processlist_state_length;
  /** Column PROCESSLIST_INFO. */
  const char* m_processlist_info_ptr;
  /** Length in bytes of @c m_processlist_info_ptr. */
  uint m_processlist_info_length;
  /** Column INSTRUMENTED. */
  bool *m_enabled_ptr;
  /** Column PARENT_THREAD_ID. */
  ulonglong m_parent_thread_internal_id;
};

/** Table PERFORMANCE_SCHEMA.THREADS. */
class table_threads : public cursor_by_thread
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table* create();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);


  virtual int update_row_values(TABLE *table,
                                const unsigned char *old_buf,
                                unsigned char *new_buf,
                                Field **fields);

protected:
  table_threads();

public:
  ~table_threads()
  {}

private:
  virtual void make_row(PFS_thread *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_threads m_row;
  /** True if the current row exists. */
  bool m_row_exists;
};

/** @} */
#endif
