/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TABLE_IO_WAIT_SUMMARY_BY_INDEX_USAGE_H
#define TABLE_IO_WAIT_SUMMARY_BY_INDEX_USAGE_H

/**
  @file storage/perfschema/table_tiws_by_index_usage.h
  Table TABLE_IO_WAIT_SUMMARY_BY_INDEX_USAGE (declarations).
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
  PERFORMANCE_SCHEMA.TABLE_IO_WAIT_SUMMARY_BY_INDEX.
*/
struct row_tiws_by_index_usage
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
  /** Column INDEX_NAME. */
  char m_index_name[NAME_LEN];
  /** Length in bytes of @c m_index_name. */
  uint m_index_name_length;
  /** Columns COUNT/SUM/MIN/AVG/MAX (+_READ, +WRITE). */
  PFS_table_io_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.TABLE_IO_WAIT_SUMMARY_BY_INDEX.
  Index 1 on table_share_array (0 based)
  Index 2 on index (0 based)
*/
struct pos_tiws_by_index_usage : public PFS_double_index
{
  pos_tiws_by_index_usage()
    : PFS_double_index(0, 0)
  {}

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= 0;
  }

  inline bool has_more_table(void)
  {
    return (m_index_1 < table_share_max);
  }

  inline void next_table(void)
  {
    m_index_1++;
    m_index_2= 0;
  }
};

/** Table PERFORMANCE_SCHEMA.TABLE_IO_WAIT_SUMMARY_BY_INDEX. */
class table_tiws_by_index_usage : public PFS_engine_table
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

  table_tiws_by_index_usage();

public:
  ~table_tiws_by_index_usage()
  {}

protected:
  void make_row(PFS_table_share *table_share, uint index);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_tiws_by_index_usage m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_tiws_by_index_usage m_pos;
  /** Next position. */
  pos_tiws_by_index_usage m_next_pos;
};

/** @} */
#endif
