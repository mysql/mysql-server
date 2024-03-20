/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_VARIABLES_METADATA_H
#define TABLE_VARIABLES_METADATA_H

/**
  @file storage/perfschema/table_variables_metadata.h
  Table VARIABLES_META (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "mysql/components/services/system_variable_source_type.h"
#include "mysql_com.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_variable.h"

class Field;
class Plugin_table;
struct TABLE;
struct THR_LOCK;

/**
  Calculated from sys_var::flag_enum
*/
enum enum_variable_scope : int {
  SCOPE_GLOBAL = 1,
  SCOPE_SESSION,
  SCOPE_SESSION_ONLY
};

/**
  Calculated from enum_mysql_show_type
*/
enum enum_variable_data_type : int {
  TYPE_INTEGER = 1,
  TYPE_NUMERIC,  // double
  TYPE_STRING,
  TYPE_ENUMERATION,
  TYPE_BOOLEAN,
  TYPE_SET  // set, flagset
};

/**
  A row of table
  PERFORMANCE_SCHEMA.VARIABLES_METADATA.
*/
struct row_variables_metadata {
  row_variables_metadata();
  row_variables_metadata(sys_var *system_var);
  row_variables_metadata(const row_variables_metadata &other);
  row_variables_metadata &operator=(const row_variables_metadata &other);

  /** Column VARIABLE_NAME. */
  char m_variable_name[COL_OBJECT_NAME_SIZE];
  uint m_variable_name_length;
  /** Column VARIABLE_SCOPE. */
  enum_variable_scope m_variable_scope;
  /** Column DATA_TYPE. */
  enum_variable_data_type m_variable_data_type;
  /** Column MIN_VALUE. */
  char m_min_value[COL_OBJECT_NAME_SIZE];
  uint m_min_value_length;
  /** Column MAX_VALUE. */
  char m_max_value[COL_OBJECT_NAME_SIZE];
  uint m_max_value_length;
  /** Column DOCUMENTATION. */
  char m_documentation[COL_INFO_SIZE];
  uint m_documentation_length;
};

/** Table PERFORMANCE_SCHEMA.VARIABLES_METADATA. */
class table_variables_metadata : public PFS_engine_table {
  typedef PFS_simple_index pos_t;
  typedef Prealloced_array<row_variables_metadata, SYSTEM_VARIABLE_PREALLOC>
      Variable_array;

 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;
  void reset_position() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  table_variables_metadata();

 public:
  ~table_variables_metadata() override = default;

 protected:
  /* Build sys_var tracker array. */
  bool init_sys_var_array();
  /* Build sys_var info cache. */
  int do_materialize_all();

  int make_row(const row_variables_metadata &row);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /* True when system variable descriptors array is complete. */
  bool m_initialized;
  /* Array of system variable descriptors. */
  System_variable_tracker::Array m_sys_var_tracker_array;
  /* Cache of materialized variables. */
  Variable_array m_cache;
  /* True when cache is complete. */
  bool m_materialized;
  /* Version of global hash/array. Changes when vars added/removed. */
  ulonglong m_version;

  /** Current row. */
  row_variables_metadata m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;
};

#endif
