/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_VARIABLES_INFO_H
#define TABLE_VARIABLES_INFO_H

/**
  @file storage/perfschema/table_variables_info.h
  Table VARIABLES_INFO (declarations).
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
  PERFORMANCE_SCHEMA.VARIABLES_INFO.
*/
struct row_variables_info {
  /** Column VARIABLE_NAME. */
  char m_variable_name[COL_SOURCE_SIZE];
  uint m_variable_name_length;
  /** Column VARIABLE_SOURCE. */
  enum_variable_source m_variable_source;
  /** Column VARIABLE_PATH. */
  char m_variable_path[COL_INFO_SIZE];
  uint m_variable_path_length;
  /** Column MIN_VALUE. */
  char m_min_value[COL_SOURCE_SIZE];
  uint m_min_value_length;
  /** Column MAX_VALUE. */
  char m_max_value[COL_SOURCE_SIZE];
  uint m_max_value_length;
  /** Column SET_TIME. */
  ulonglong m_set_time;
  /** Column SET_USER. */
  char m_set_user_str[USERNAME_LENGTH];
  uint m_set_user_str_length;
  /** Column SET_HOST. */
  char m_set_host_str[HOSTNAME_LENGTH];
  uint m_set_host_str_length;
};

/** Table PERFORMANCE_SCHEMA.VARIABLES_INFO. */
class table_variables_info : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

 protected:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);
  table_variables_info();

 public:
  ~table_variables_info() {}

 protected:
  int make_row(const System_variable *system_var);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current THD variables. */
  PFS_system_variable_info_cache m_sysvarinfo_cache;
  /** Current row. */
  row_variables_info m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;
};

/** @} */
#endif
