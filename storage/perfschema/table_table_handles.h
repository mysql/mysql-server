/* Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_TABLE_HANDLES_H
#define TABLE_TABLE_HANDLES_H

/**
  @file storage/perfschema/table_table_handles.h
  Table TABLE_HANDLES (declarations).
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
  PERFORMANCE_SCHEMA.TABLE_HANDLES.
*/
struct row_table_handles
{
  /** Column OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME. */
  PFS_object_row m_object;
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
  /** Column OWNER_THREAD_ID. */
  ulonglong m_owner_thread_id;
  /** Column OWNER_EVENT_ID. */
  ulonglong m_owner_event_id;
  /** Column INTERNAL_LOCK. */
  PFS_TL_LOCK_TYPE m_internal_lock;
  /** Column EXTERNAL_LOCK. */
  PFS_TL_LOCK_TYPE m_external_lock;
};

/** Table PERFORMANCE_SCHEMA.TABLE_HANDLES. */
class table_table_handles : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static ha_rows get_row_count();

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_table_handles();

public:
  ~table_table_handles()
  {}

protected:
  void make_row(PFS_table *table);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_table_handles m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
