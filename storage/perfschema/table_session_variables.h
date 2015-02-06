/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_SESSION_VARIABLES_H
#define TABLE_SESSION_VARIABLES_H

/**
  @file storage/perfschema/table_session_variables.h
  Table SESSION_VARIABLES (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_helper.h"
#include "pfs_variable.h"
/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  Store and retrieve table state information during queries that reinstantiate
  the table object.
*/
class table_session_variables_context : public PFS_table_context
{
public:
  table_session_variables_context(ulonglong hash_version, bool restore) :
    PFS_table_context(hash_version, restore, THR_PFS_SV) {}
};

/**
  A row of table
  PERFORMANCE_SCHEMA.SESSION_VARIABLES.
*/
struct row_session_variables
{
  /** Column VARIABLE_NAME. */
  PFS_variable_name_row m_variable_name;
  /** Column VARIABLE_VALUE. */
  PFS_variable_value_row m_variable_value;
};

/** Table PERFORMANCE_SCHEMA.SESSION_VARIABLES. */
class table_session_variables : public PFS_engine_table
{
  typedef PFS_simple_index pos_t;

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
  table_session_variables();

public:
  ~table_session_variables()
  {}

protected:
  void make_row(const System_variable *system_var);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current THD variables. */
  PFS_system_variable_cache m_sysvar_cache;
  /** Current row. */
  row_session_variables m_row;
  /** True if the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  /** Table context with system variable hash version. */
  table_session_variables_context *m_context;
};

/** @} */
#endif
