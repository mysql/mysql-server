/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_SESSION_VARIABLES_H
#define TABLE_SESSION_VARIABLES_H

/**
  @file storage/perfschema/table_session_variables.h
  Table SESSION_VARIABLES (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_variable.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct TABLE;
struct THR_LOCK;
/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_session_variables : public PFS_engine_index {
 public:
  PFS_index_session_variables()
      : PFS_engine_index(&m_key), m_key("VARIABLE_NAME") {}

  ~PFS_index_session_variables() override = default;

  virtual bool match(const System_variable *pfs);

 private:
  PFS_key_variable_name m_key;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.SESSION_VARIABLES.
*/
struct row_session_variables {
  /** Column VARIABLE_NAME. */
  PFS_variable_name_row m_variable_name;
  /** Column VARIABLE_VALUE. */
  PFS_variable_value_row m_variable_value;
};

/** Table PERFORMANCE_SCHEMA.SESSION_VARIABLES. */
class table_session_variables : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  table_session_variables();

 public:
  ~table_session_variables() override = default;

 protected:
  int make_row(const System_variable *system_var);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current THD variables. */
  PFS_system_variable_cache m_sysvar_cache;
  /** Current row. */
  row_session_variables m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_session_variables *m_opened_index;
};

/** @} */
#endif
