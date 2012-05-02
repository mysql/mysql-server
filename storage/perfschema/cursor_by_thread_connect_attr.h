/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CURSOR_BY_THREAD_CONNECT_ATTR_H
#define CURSOR_BY_THREAD_CONNECT_ATTR_H

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr.h"

/**
  \addtogroup Performance_schema_tables
  @{
*/

struct pos_connect_attr_by_thread_by_attr
: public PFS_double_index
{
  pos_connect_attr_by_thread_by_attr()
    : PFS_double_index(0, 0)
  {}

  inline bool has_more_thread(void)
  {
    return (m_index_1 < thread_max);
  }

  inline void next_thread(void)
  {
    m_index_1++;
    m_index_2= 0;
  }

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= 0;
  }
};

#define MAX_ATTR_NAME_CHARS 32
#define MAX_ATTR_VALUE_CHARS 1024
#define MAX_UTF8_BYTES 6
/**
  A row of PERFORMANCE_SCHEMA.SESSION_CONNECT_ATTRS and
  PERFORMANCE_SCHEMA.SESSION_ACCOUNT_CONNECT_ATTRS.
*/
struct row_session_connect_attrs
{
  /** Column PROCESS_ID. */
  ulong m_process_id;
  /** Column ATTR_NAME. In UTF-8 */
  char m_attr_name[MAX_ATTR_NAME_CHARS * MAX_UTF8_BYTES];
  /** Length in bytes of @c m_attr_name. */
  uint m_attr_name_length;
  /** Column ATTR_VALUE. In UTF-8 */
  char m_attr_value[MAX_ATTR_VALUE_CHARS * MAX_UTF8_BYTES];
  /** Length in bytes of @c m_attr_name. */
  uint m_attr_value_length;
  /** Column ORDINAL_POSITION. */
  ulong m_ordinal_position;
};


/** Cursor CURSOR_BY_THREAD_CONNECT_ATTR. */
class cursor_by_thread_connect_attr : public PFS_engine_table
{
public:
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  cursor_by_thread_connect_attr(const PFS_engine_table_share *share);
  virtual int read_row_values(TABLE *table, unsigned char *buf,
                              Field **fields, bool read_all);

public:
  ~cursor_by_thread_connect_attr()
  {}

protected:
  void make_row(PFS_thread *thread, uint ordinal);
  virtual bool thread_fits(PFS_thread *thread, PFS_thread *current_thread) = 0;

private:
  /** Current position. */
  pos_connect_attr_by_thread_by_attr m_pos;
  /** Next position. */
  pos_connect_attr_by_thread_by_attr m_next_pos;

protected:
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;
  /** Current row. */
  row_session_connect_attrs m_row;
  /** True if the current row exists. */
  bool m_row_exists;
};

/** @} */
#endif
