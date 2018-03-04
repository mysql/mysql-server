/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_PERSISTED_VARIABLES_H
#define TABLE_PERSISTED_VARIABLES_H

/**
  @file storage/perfschema/table_persisted_variables.h
  Table PERSISTED_VARIABLES (declarations).
*/

#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_variable.h"
#include "storage/perfschema/table_helper.h"
/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_persisted_variables : public PFS_engine_index
{
public:
  PFS_index_persisted_variables()
    : PFS_engine_index(&m_key), m_key("VARIABLE_NAME")
  {
  }

  ~PFS_index_persisted_variables()
  {
  }

  virtual bool match(const System_variable *pfs);

private:
  PFS_key_variable_name m_key;
};

/**
  Store and retrieve table state information during queries that reinstantiate
  the table object.
*/
class table_persisted_variables_context : public PFS_table_context
{
public:
  table_persisted_variables_context(ulonglong hash_version, bool restore)
    : PFS_table_context(hash_version, restore, THR_PFS_SV)
  {
  }
};

/**
  A row of table
  PERFORMANCE_SCHEMA.PERSISTED_VARIABLES.
*/
struct row_persisted_variables
{
  /** Column VARIABLE_NAME. */
  PFS_variable_name_row m_variable_name;
  /** Column VARIABLE_VALUE. */
  PFS_variable_value_row m_variable_value;
};

/** Table PERFORMANCE_SCHEMA.PERSISTED_VARIABLES. */
class table_persisted_variables : public PFS_engine_table
{
  typedef PFS_simple_index pos_t;

public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);
  table_persisted_variables();

public:
  ~table_persisted_variables()
  {
  }

protected:
  int make_row(const System_variable *system_var);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current THD variables. */
  PFS_system_persisted_variables_cache m_sysvar_cache;
  /** Current row. */
  row_persisted_variables m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  /** Table context with system variable hash version. */
  table_persisted_variables_context *m_context;

  PFS_index_persisted_variables *m_opened_index;
};

/** @} */
#endif
