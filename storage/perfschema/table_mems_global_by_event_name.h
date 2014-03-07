/* Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME_H
#define TABLE_MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME_H

/**
  @file storage/perfschema/table_mems_global_by_event_name.h
  Table MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "table_helper.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME. */
struct row_mems_global_by_event_name
{
  /** Column EVENT_NAME. */
  PFS_event_name_row m_event_name;
  /** Columns COUNT_ALLOC, ... */
  PFS_memory_stat_row m_stat;
};

/** Table PERFORMANCE_SCHEMA.MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME. */
class table_mems_global_by_event_name : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_mems_global_by_event_name();

public:
  ~table_mems_global_by_event_name()
  {}

private:
  void make_row(PFS_memory_class *klass);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_mems_global_by_event_name m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
