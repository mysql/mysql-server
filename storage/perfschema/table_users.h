/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_USERS_H
#define TABLE_USERS_H

/**
  @file storage/perfschema/table_users.h
  TABLE USERS.
*/

#include <sys/types.h>

#include "cursor_by_user.h"
#include "pfs_column_types.h"
#include "table_helper.h"

struct PFS_user;

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of PERFORMANCE_SCHEMA.USERS.
*/
struct row_users
{
  /** Column USER. */
  PFS_user_row m_user;
  /** Columns CURRENT_CONNECTIONS, TOTAL_CONNECTIONS. */
  PFS_connection_stat_row m_connection_stat;
};

class PFS_index_users_by_user : public PFS_index_users
{
public:
  PFS_index_users_by_user() : PFS_index_users(&m_key), m_key("USER")
  {
  }

  ~PFS_index_users_by_user()
  {
  }

  virtual bool match(PFS_user *pfs);

private:
  PFS_key_user m_key;
};

/** Table PERFORMANCE_SCHEMA.USERS. */
class table_users : public cursor_by_user
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

protected:
  table_users();

public:
  ~table_users()
  {
  }

  int index_init(uint idx, bool sorted);

private:
  virtual int make_row(PFS_user *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_users m_row;
};

/** @} */
#endif
