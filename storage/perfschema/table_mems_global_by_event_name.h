/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <sys/types.h>

#include "pfs_builtin_memory.h"
#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_mems_global_by_event_name : public PFS_engine_index
{
public:
  PFS_index_mems_global_by_event_name()
    : PFS_engine_index(&m_key), m_key("EVENT_NAME")
  {
  }

  ~PFS_index_mems_global_by_event_name()
  {
  }

  virtual bool match(PFS_instr_class *instr_class);

private:
  PFS_key_event_name m_key;
};

/** A row of PERFORMANCE_SCHEMA.MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME. */
struct row_mems_global_by_event_name
{
  /** Column EVENT_NAME. */
  PFS_event_name_row m_event_name;
  /** Columns COUNT_ALLOC, ... */
  PFS_memory_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME.
  Index 1 on view
  Index 2 on instrument key (1 based)
*/
struct pos_mems_global_by_event_name : public PFS_double_index
{
  static const uint FIRST_VIEW = 1;
  static const uint VIEW_BUILTIN_MEMORY = 1;
  static const uint VIEW_MEMORY = 2;
  static const uint LAST_VIEW = 2;

  pos_mems_global_by_event_name() : PFS_double_index(FIRST_VIEW, 1)
  {
  }

  inline void
  reset(void)
  {
    m_index_1 = FIRST_VIEW;
    m_index_2 = 1;
  }

  inline bool
  has_more_view(void)
  {
    return (m_index_1 <= LAST_VIEW);
  }

  inline void
  next_view(void)
  {
    m_index_1++;
    m_index_2 = 1;
  }
};

/** Table PERFORMANCE_SCHEMA.MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME. */
class table_mems_global_by_event_name : public PFS_engine_table
{
  typedef pos_mems_global_by_event_name pos_t;

public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create();
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next(void);

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_mems_global_by_event_name();

public:
  ~table_mems_global_by_event_name()
  {
  }

private:
  int make_row(PFS_builtin_memory_class *klass);
  int make_row(PFS_memory_class *klass);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_mems_global_by_event_name m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_mems_global_by_event_name *m_opened_index;
};

/** @} */
#endif
