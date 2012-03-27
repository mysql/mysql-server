/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME_H
#define TABLE_EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME_H

/**
  @file storage/perfschema/table_ews_global_by_event_name.h
  Table EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_all_instr.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME. */
struct row_ews_global_by_event_name
{
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column COUNT_STAR. */
  ulonglong m_count;
  /** Column SUM_TIMER_WAIT. */
  ulonglong m_sum;
  /** Column MIN_TIMER_WAIT. */
  ulonglong m_min;
  /** Column AVG_TIMER_WAIT. */
  ulonglong m_avg;
  /** Column MAX_TIMER_WAIT. */
  ulonglong m_max;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME. */
class table_ews_global_by_event_name : public table_all_instr_class
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();

protected:
  virtual void make_instr_row(PFS_instr_class *klass);

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_ews_global_by_event_name();

public:
  ~table_ews_global_by_event_name()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_ews_global_by_event_name m_row;
};

/** @} */
#endif
