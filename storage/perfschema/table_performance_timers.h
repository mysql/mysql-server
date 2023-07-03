/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_PERFORMANCE_TIMERS_H
#define TABLE_PERFORMANCE_TIMERS_H

/**
  @file storage/perfschema/table_performance_timers.h
  Table PERFORMANCE_TIMERS (declarations).
*/

#include "my_base.h"
#include "my_rdtsc.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"

class Field;
class Plugin_table;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.PERFORMANCE_TIMERS. */
struct row_performance_timers {
  /** Column TIMER_NAME. */
  enum_timer_name m_timer_name;
  /**
    Columns ROUTINE (not displayed), TIMER_OVERHEAD,
    TIMER_FREQUENCY, TIMER_RESOLUTION.
  */
  struct my_timer_unit_info m_info;
};

/** Table PERFORMANCE_SCHEMA.PERFORMANCE_TIMERS. */
class table_performance_timers : public PFS_engine_table {
 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  int rnd_next() override;
  int rnd_pos(const void *pos) override;
  void reset_position(void) override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

 protected:
  table_performance_timers();

 public:
  ~table_performance_timers() override = default;

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

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
