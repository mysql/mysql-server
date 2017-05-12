/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SESSION_STATUS_H
#define TABLE_SESSION_STATUS_H

/**
  @file storage/perfschema/table_session_status.h
  Table SESSION_STATUS (declarations).
*/

#include <sys/types.h>

#include "my_inttypes.h"
#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr.h"
#include "pfs_instr_class.h"
#include "pfs_variable.h"
#include "table_helper.h"
/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.SESSION_STATUS.
*/
struct row_session_status
{
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column VARIABLE_NAME. */
  PFS_variable_name_row m_variable_name;
  /** Column VARIABLE_VALUE. */
  PFS_variable_value_row m_variable_value;
};

class PFS_index_session_status : public PFS_engine_index
{
public:
  PFS_index_session_status() : PFS_engine_index(&m_key), m_key("VARIABLE_NAME")
  {
  }

  ~PFS_index_session_status()
  {
  }

  virtual bool match(const Status_variable *pfs);

private:
  PFS_key_variable_name m_key;
};

/**
  Store and retrieve table state information for queries that reinstantiate
  the table object.
*/
class table_session_status_context : public PFS_table_context
{
public:
  table_session_status_context(ulonglong current_version, bool restore)
    : PFS_table_context(current_version, restore, THR_PFS_SS)
  {
  }
};

/** Table PERFORMANCE_SCHEMA.SESSION_STATUS. */
class table_session_status : public PFS_engine_table
{
  typedef PFS_simple_index pos_t;

public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create();
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
  table_session_status();

public:
  ~table_session_status()
  {
  }

protected:
  int make_row(const Status_variable *status_var);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current THD variables. */
  PFS_status_variable_cache m_status_cache;
  /** Current row. */
  row_session_status m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  /** Table context with global status array version. */
  table_session_status_context *m_context;

  PFS_index_session_status *m_opened_index;
};

/** @} */
#endif
