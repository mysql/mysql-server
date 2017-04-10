/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_FILE_INSTANCES_H
#define TABLE_FILE_INSTANCES_H

/**
  @file storage/perfschema/table_file_instances.h
  Table FILE_INSTANCES (declarations).
*/

#include <sys/types.h>

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.FILE_INSTANCES. */
struct row_file_instances
{
  /** Column FILE_NAME. */
  const char *m_filename;
  /** Length in bytes of @c m_filename. */
  uint m_filename_length;
  /** Column EVENT_NAME. */
  const char *m_event_name;
  /** Length in bytes of @c m_event_name. */
  uint m_event_name_length;
  /** Column OPEN_COUNT. */
  uint m_open_count;
};

class PFS_index_file_instances : public PFS_engine_index
{
public:
  PFS_index_file_instances(PFS_engine_key *key_1) : PFS_engine_index(key_1)
  {
  }

  ~PFS_index_file_instances()
  {
  }

  virtual bool match(const PFS_file *pfs) = 0;
};

class PFS_index_file_instances_by_file_name : public PFS_index_file_instances
{
public:
  PFS_index_file_instances_by_file_name()
    : PFS_index_file_instances(&m_key), m_key("FILE_NAME")
  {
  }

  ~PFS_index_file_instances_by_file_name()
  {
  }

  bool match(const PFS_file *pfs);

private:
  PFS_key_file_name m_key;
};

class PFS_index_file_instances_by_event_name : public PFS_index_file_instances
{
public:
  PFS_index_file_instances_by_event_name()
    : PFS_index_file_instances(&m_key), m_key("EVENT_NAME")
  {
  }

  ~PFS_index_file_instances_by_event_name()
  {
  }

  bool match(const PFS_file *pfs);

private:
  PFS_key_event_name m_key;
};

/** Table PERFORMANCE_SCHEMA.FILE_INSTANCES. */
class table_file_instances : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);
  table_file_instances();

public:
  ~table_file_instances()
  {
  }

private:
  int make_row(PFS_file *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_file_instances m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_file_instances *m_opened_index;
};

/** @} */
#endif
