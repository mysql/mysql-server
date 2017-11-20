/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_USER_DEFINED_FUNCTIONS_H
#define TABLE_USER_DEFINED_FUNCTIONS_H

/**
  @file storage/perfschema/table_user_defined_functions.h
  Table USER_DEFINED_FUNCTIONS (declarations).
*/

#include <sys/types.h>

#include "my_inttypes.h"
#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "table_helper.h"

struct st_udf_func;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.USER_DEFINED_FUNCTIONS. */
struct row_user_defined_functions
{
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

class PFS_index_user_defined_functions : public PFS_engine_index
{
public:
  PFS_index_user_defined_functions(PFS_engine_key *key_1)
    : PFS_engine_index(key_1)
  {
  }

  ~PFS_index_user_defined_functions()
  {
  }

  virtual bool match(const row_user_defined_functions *row) = 0;
};

class PFS_index_user_defined_functions_by_name
  : public PFS_index_user_defined_functions
{
public:
  PFS_index_user_defined_functions_by_name()
    : PFS_index_user_defined_functions(&m_key), m_key("UDF_NAME")
  {
  }

  ~PFS_index_user_defined_functions_by_name()
  {
  }

  bool match(const row_user_defined_functions *row);

private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.USER_DEFINED_FUNCTIONS. */
class table_user_defined_functions : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);
  table_user_defined_functions();

public:
  ~table_user_defined_functions()
  {
  }

private:
  void materialize(THD *thd);
  static int make_row(const struct st_udf_func *entry,
                      row_user_defined_functions *row);
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
