/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include "pfs_engine_table.h"

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
  ulong m_thread_internal_id;
  /** Column ID. */
  ulong m_thread_id;
  /** Column NAME. */
  const char* m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;

  char m_username[USERNAME_LENGTH];
  uint m_username_length;
  char m_hostname[HOSTNAME_LENGTH];
  uint m_hostname_length;
  char m_dbname[NAME_LEN];
  uint m_dbname_length;
  int m_command;
  time_t m_start_time;
  const char* m_processlist_state_ptr;
  uint m_processlist_state_length;
  const char* m_processlist_info_ptr;
  uint m_processlist_info_length;
  bool *m_enabled_ptr;
};

/** Table PERFORMANCE_SCHEMA.THREADS. */
class table_threads : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

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
  void make_row(PFS_thread *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_threads m_row;
  /** True if the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
