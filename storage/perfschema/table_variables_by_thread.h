/* Copyright (c) 2015, 2021, Oracle and/or its affiliates.

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

#ifndef TABLE_VARIABLES_BY_THREAD_H
#define TABLE_VARIABLES_BY_THREAD_H

/**
  @file storage/perfschema/table_variables_by_thread.h
  Table VARIABLES_BY_THREAD (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_helper.h"
#include "pfs_variable.h"
#include "pfs_buffer_container.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.VARIABLES_BY_THREAD.
*/
struct row_variables_by_thread
{
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column VARIABLE_NAME. */
  PFS_variable_name_row m_variable_name;
  /** Column VARIABLE_VALUE. */
  PFS_variable_value_row m_variable_value;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.VARIABLES_BY_THREAD.
  Index 1 on thread (0 based)
  Index 2 on system variable (0 based)
*/
struct pos_variables_by_thread
: public PFS_double_index
{
  pos_variables_by_thread()
    : PFS_double_index(0, 0)
  {}

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= 0;
  }

  inline bool has_more_thread(void)
  { return (m_index_1 < global_thread_container.get_row_count()); }

  inline void next_thread(void)
  {
    m_index_1++;
    m_index_2= 0;
  }
};

/**
  Store and retrieve table state information during queries that reinstantiate
  the table object.
*/
class table_variables_by_thread_context : public PFS_table_context
{
public:
  table_variables_by_thread_context(ulonglong hash_version, bool restore) :
    PFS_table_context(hash_version, global_thread_container.get_row_count(), restore, THR_PFS_VBT) { }
};

/** Table PERFORMANCE_SCHEMA.VARIABLES_BY_THREAD. */
class table_variables_by_thread : public PFS_engine_table
{
  typedef pos_variables_by_thread pos_t;

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
  table_variables_by_thread();

public:
  ~table_variables_by_thread()
  {}

protected:
  int materialize(PFS_thread *thread);
  void make_row(PFS_thread *thread, const System_variable *system_var);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current THD variables. */
  PFS_system_variable_cache m_sysvar_cache;
  /** Current row. */
  row_variables_by_thread m_row;
  /** True if the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  /** Table context with system variable hash version and map of materialized threads. */
  table_variables_by_thread_context *m_context;
};

/** @} */
#endif
