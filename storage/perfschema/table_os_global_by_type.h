/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef TABLE_OBJECTS_SUMMARY_GLOBAL_BY_TYPE_H
#define TABLE_OBJECTS_SUMMARY_GLOBAL_BY_TYPE_H

/**
  @file storage/perfschema/table_os_global_by_type.h
  Table OBJECTS_SUMMARY_GLOBAL_BY_TYPE (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_helper.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.OBJECTS_SUMMARY_GLOBAL_BY_TYPE.
*/
struct row_os_global_by_type
{
  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column SCHEMA_NAME. */
  char m_schema_name[NAME_LEN];
  /** Length in bytes of @c m_schema_name. */
  uint m_schema_name_length;
  /** Column OBJECT_NAME. */
  char m_object_name[NAME_LEN];
  /** Length in bytes of @c m_object_name. */
  uint m_object_name_length;
  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.OBJECTS_SUMMARY_GLOBAL_BY_TYPE.
  Index 1 on object type
  Index 2 on object instance (0 based)
*/
struct pos_os_global_by_type : public PFS_double_index,
                               public PFS_object_view_constants
{
  pos_os_global_by_type()
    : PFS_double_index(FIRST_VIEW, 0)
  {}

  inline void reset(void)
  {
    m_index_1= FIRST_VIEW;
    m_index_2= 0;
  }

  inline bool has_more_view(void)
  { return (m_index_1 <= LAST_VIEW); }

  inline void next_view(void)
  {
    m_index_1++;
    m_index_2= 0;
  }
};

/** Table PERFORMANCE_SCHEMA.OBJECTS_SUMMARY_GLOBAL_BY_TYPE. */
class table_os_global_by_type : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_os_global_by_type();

public:
  ~table_os_global_by_type()
  {}

protected:
  void make_row(PFS_table_share *table_share);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_os_global_by_type m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_os_global_by_type m_pos;
  /** Next position. */
  pos_os_global_by_type m_next_pos;
};


/** @} */
#endif
