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

#ifndef TABLE_FILE_SUMMARY_H
#define TABLE_FILE_SUMMARY_H

/**
  @file storage/perfschema/table_file_summary.h
  Table FILE_SUMMARY_BY_xxx (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.FILE_SUMMARY_BY_EVENT_NAME. */
struct row_file_summary_by_event_name
{
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /**
    Columns COUNT_READ, COUNT_WRITE,
    SUM_NUMBER_OF_BYTES_READ, SUM_NUMBER_OF_BYTES_WRITE.
  */
  PFS_file_stat m_file_stat;
};

/** Table PERFORMANCE_SCHEMA.FILE_SUMMARY_BY_EVENT_NAME. */
class table_file_summary_by_event_name : public PFS_engine_table
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
  void make_row(PFS_file_class *klass);

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_file_summary_by_event_name();

public:
  ~table_file_summary_by_event_name()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_file_summary_by_event_name m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** A row of PERFORMANCE_SCHEMA.FILE_SUMMARY_BY_INSTANCE. */
struct row_file_summary_by_instance
{
  /** Column FILE_NAME. */
  const char *m_filename;
  /** Length in bytes of @c m_filename. */
  uint m_filename_length;
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /**
    Columns COUNT_READ, COUNT_WRITE,
    SUM_NUMBER_OF_BYTES_READ, SUM_NUMBER_OF_BYTES_WRITE.
  */
  PFS_file_stat m_file_stat;
};

/** Table PERFORMANCE_SCHEMA.FILE_UMMARY_BY_INSTANCE. */
class table_file_summary_by_instance : public PFS_engine_table
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
  void make_row(PFS_file *pfs);

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_file_summary_by_instance();

public:
  ~table_file_summary_by_instance()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_file_summary_by_instance m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
