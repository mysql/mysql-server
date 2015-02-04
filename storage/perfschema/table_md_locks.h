/* Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_METADATA_LOCK_H
#define TABLE_METADATA_LOCK_H

/**
  @file storage/perfschema/table_md_locks.h
  Table METADATA_LOCKS (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "table_helper.h"

struct PFS_metadata_lock;

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of table PERFORMANCE_SCHEMA.MUTEX_INSTANCES. */
struct row_metadata_lock
{
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
  opaque_mdl_type m_mdl_type;
  opaque_mdl_duration m_mdl_duration;
  opaque_mdl_status m_mdl_status;
  /** Column SOURCE. */
  char m_source[COL_SOURCE_SIZE];
  /** Length in bytes of @c m_source. */
  uint m_source_length;
  /** Column OWNER_THREAD_ID. */
  ulong m_owner_thread_id;
  /** Column OWNER_EVENT_ID. */
  ulong m_owner_event_id;
  /** Columns OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME. */
  PFS_object_row m_object;
};

/** Table PERFORMANCE_SCHEMA.METADATA_LOCKS. */
class table_metadata_locks : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static ha_rows get_row_count();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_metadata_locks();

public:
  ~table_metadata_locks()
  {}

private:
  void make_row(PFS_metadata_lock *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_metadata_lock m_row;
  /** True if the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
