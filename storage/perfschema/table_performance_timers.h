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

#ifndef TABLE_PERFORMANCE_TIMERS_H
#define TABLE_PERFORMANCE_TIMERS_H

/**
  @file storage/perfschema/table_performance_timers.h
  Table PERFORMANCE_TIMERS (declarations).
*/

#include <my_rdtsc.h>
#include "pfs_column_types.h"
#include "pfs_engine_table.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.PERFORMANCE_TIMERS. */
struct row_performance_timers
{
  /** Column TIMER_NAME. */
  enum_timer_name m_timer_name;
  /**
    Columns ROUTINE (not displayed), TIMER_OVERHEAD,
    TIMER_FREQUENCY, TIMER_RESOLUTION.
  */
  struct my_timer_unit_info m_info;
};

/** Table PERFORMANCE_SCHEMA.PERFORMANCE_TIMERS. */
class table_performance_timers : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

protected:
  table_performance_timers();

public:
  ~table_performance_timers()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_performance_timers *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
  row_performance_timers m_data[COUNT_TIMER_NAME];
};

/** @} */
#endif
