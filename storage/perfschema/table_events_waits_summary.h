/* Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_EVENTS_WAITS_SUMMARY_H
#define TABLE_EVENTS_WAITS_SUMMARY_H

/**
  @file storage/perfschema/table_events_waits_summary.h
  Table EVENTS_WAITS_SUMMARY_BY_xxx (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_all_instr.h"
#include "table_helper.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_INSTANCE. */
struct row_events_waits_summary_by_instance
{
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  intptr m_object_instance_addr;
  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_stat_row m_stat;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_INSTANCE. */
class table_events_waits_summary_by_instance : public table_all_instr
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();

protected:
  void make_instr_row(PFS_instr *pfs, PFS_instr_class *klass,
                      const void *object_instance_begin,
                      PFS_single_stat *pfs_stat);
  virtual void make_mutex_row(PFS_mutex *pfs);
  virtual void make_rwlock_row(PFS_rwlock *pfs);
  virtual void make_cond_row(PFS_cond *pfs);
  virtual void make_file_row(PFS_file *pfs);
  virtual void make_socket_row(PFS_socket *pfs);

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_events_waits_summary_by_instance();

public:
  ~table_events_waits_summary_by_instance()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_events_waits_summary_by_instance m_row;
  /** True if the current row exists. */
  bool m_row_exists;
};

/** @} */
#endif
