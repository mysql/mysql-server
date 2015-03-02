/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_UVAR_BY_THREAD_H
#define TABLE_UVAR_BY_THREAD_H

/**
  @file storage/perfschema/table_uvar_by_thread.h
  Table USER_VARIABLES_BY_THREAD (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_helper.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

struct User_variable
{
public:
  User_variable()
  {}

  User_variable(const User_variable& uv)
    : m_name(uv.m_name), m_value(uv.m_value)
  {}

  ~User_variable()
  {}

  PFS_variable_name_row m_name;
  PFS_user_variable_value_row m_value;
};

class User_variables
{
  typedef Prealloced_array<User_variable, 100, false> User_variable_array;

public:
  User_variables()
    : m_pfs(NULL), m_thread_internal_id(0), m_array(PSI_INSTRUMENT_ME)
  {
  }

  void reset()
  {
    m_pfs= NULL;
    m_thread_internal_id= 0;
    m_array.clear();
  }

  void materialize(PFS_thread *pfs, THD *thd);

  bool is_materialized(PFS_thread *pfs)
  {
    DBUG_ASSERT(pfs != NULL);
    if (m_pfs != pfs)
      return false;
    if (m_thread_internal_id != pfs->m_thread_internal_id)
      return false;
    return true;
  }

  const User_variable *get(uint index) const
  {
    if (index >= m_array.size())
      return NULL;

    const User_variable *p= & m_array.at(index);
    return p;
  }

private:
  PFS_thread *m_pfs;
  ulonglong m_thread_internal_id;
  User_variable_array m_array;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.USER_VARIABLES_BY_THREAD.
*/
struct row_uvar_by_thread
{
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column VARIABLE_NAME. */
  const PFS_variable_name_row *m_variable_name;
  /** Column VARIABLE_VALUE. */
  const PFS_user_variable_value_row *m_variable_value;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.USER_VARIABLES_BY_THREAD.
  Index 1 on thread (0 based)
  Index 2 on user variable (0 based)
*/
struct pos_uvar_by_thread
: public PFS_double_index
{
  pos_uvar_by_thread()
    : PFS_double_index(0, 0)
  {}

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= 0;
  }

  inline void next_thread(void)
  {
    m_index_1++;
    m_index_2= 0;
  }
};

/** Table PERFORMANCE_SCHEMA.USER_VARIABLES_BY_THREAD. */
class table_uvar_by_thread : public PFS_engine_table
{
  typedef pos_uvar_by_thread pos_t;

public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static ha_rows get_row_count();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_uvar_by_thread();

public:
  ~table_uvar_by_thread()
  { m_THD_cache.reset(); }

protected:
  int materialize(PFS_thread *thread);
  void make_row(PFS_thread *thread, const User_variable *uvar);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current THD user variables. */
  User_variables m_THD_cache;
  /** Current row. */
  row_uvar_by_thread m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;
};

/** @} */
#endif
