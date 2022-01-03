/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_USER_DEFINED_FUNCTIONS_H
#define TABLE_USER_DEFINED_FUNCTIONS_H

/**
  @file storage/perfschema/table_user_defined_functions.h
  Table USER_DEFINED_FUNCTIONS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "mysql_com.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
class THD;
struct TABLE;
struct THR_LOCK;
struct udf_func;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.USER_DEFINED_FUNCTIONS. */
struct row_user_defined_functions {
  /** Column UDF_NAME. */
  char m_name[NAME_CHAR_LEN + 1];
  uint m_name_length;
  /** Column UDF_RETURN_TYPE. */
  const char *m_return_type;
  uint m_return_type_length;
  /** Column UDF_TYPE. */
  const char *m_type;
  uint m_type_length;
  /** Column UDF_LIBRARY. */
  char m_library[1024];
  uint m_library_length;
  /** Column UDF_USAGE_COUNT. */
  ulonglong m_usage_count;
};

class PFS_index_user_defined_functions : public PFS_engine_index {
 public:
  explicit PFS_index_user_defined_functions(PFS_engine_key *key_1)
      : PFS_engine_index(key_1) {}

  ~PFS_index_user_defined_functions() override = default;

  virtual bool match(const row_user_defined_functions *row) = 0;
};

class PFS_index_user_defined_functions_by_name
    : public PFS_index_user_defined_functions {
 public:
  PFS_index_user_defined_functions_by_name()
      : PFS_index_user_defined_functions(&m_key), m_key("UDF_NAME") {}

  ~PFS_index_user_defined_functions_by_name() override = default;

  bool match(const row_user_defined_functions *row) override;

 private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.USER_DEFINED_FUNCTIONS. */
class table_user_defined_functions : public PFS_engine_table {
 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position(void) override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  table_user_defined_functions();

 public:
  ~table_user_defined_functions() override = default;

 private:
  void materialize(THD *thd);
  static int make_row(const udf_func *entry, row_user_defined_functions *row);
  static void materialize_udf_funcs(udf_func *udf, void *arg);

  /** Table share lock. */
  static THR_LOCK m_table_lock;

  row_user_defined_functions *m_all_rows;
  uint m_row_count;
  /** Current row. */
  row_user_defined_functions *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_user_defined_functions *m_opened_index;

  /** Table definition. */
  static Plugin_table m_table_def;
};

/** @} */
#endif
