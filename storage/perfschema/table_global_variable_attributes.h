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

#ifndef TABLE_GLOBAL_VARIABLE_ATTRIBUTES_H
#define TABLE_GLOBAL_VARIABLE_ATTRIBUTES_H

/**
  @file storage/perfschema/table_global_variable_attributes.h
  Table GLOBAL_VARIABLE_ATTRIBUTES (declarations).
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
  A row of table
  PERFORMANCE_SCHEMA.GLOBAL_VARIABLE_ATTRIBUTES.
*/

struct row_global_variable_attributes {
  row_global_variable_attributes();
  row_global_variable_attributes(const sys_var *system_var,
                                 const char *attribute_name,
                                 const char *attribute_value);
  row_global_variable_attributes(const row_global_variable_attributes &other);
  row_global_variable_attributes &operator=(
      const row_global_variable_attributes &other);

  /** Column VARIABLE_NAME. */
  char m_variable_name[COL_OBJECT_NAME_SIZE];
  uint m_variable_name_length;
  /** Column ATTR_NAME. */
  char m_attr_name[COL_SHORT_NAME_SIZE];
  uint m_attr_name_length;
  /** Column ATTR_VALUE. */
  char m_attr_value[COL_INFO_SIZE];
  uint m_attr_value_length;
};

/** Table PERFORMANCE_SCHEMA.GLOBAL_VARIABLE_ATTRIBUTES. */
class table_global_variable_attributes : public PFS_engine_table {
  typedef PFS_simple_index pos_t;
  typedef Prealloced_array<row_global_variable_attributes,
                           SYSTEM_VARIABLE_PREALLOC>
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
  table_global_variable_attributes();

 public:
  ~table_global_variable_attributes() override = default;

 protected:
  /* Build sys_var tracker array. */
  bool init_sys_var_array();
  /* Build sys_var info cache. */
  int do_materialize_all();

  int make_row(const row_global_variable_attributes &row);

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
  row_global_variable_attributes m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;
};

#endif
