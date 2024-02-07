/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_USERS_H
#define TABLE_USERS_H

/**
  @file storage/perfschema/table_users.h
  TABLE USERS.
*/

#include <sys/types.h>

#include "storage/perfschema/cursor_by_user.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/table_helper.h"

struct PFS_user;

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of PERFORMANCE_SCHEMA.USERS.
*/
struct row_users {
  /** Column USER. */
  PFS_user_row m_user;
  /** Columns CURRENT_CONNECTIONS, TOTAL_CONNECTIONS. */
  PFS_connection_stat_row m_connection_stat;
};

class PFS_index_users_by_user : public PFS_index_users {
 public:
  PFS_index_users_by_user() : PFS_index_users(&m_key), m_key("USER") {}

  ~PFS_index_users_by_user() override = default;

  bool match(PFS_user *pfs) override;

 private:
  PFS_key_user m_key;
};

/** Table PERFORMANCE_SCHEMA.USERS. */
class table_users : public cursor_by_user {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

 protected:
  table_users();

 public:
  ~table_users() override = default;

  int index_init(uint idx, bool sorted) override;

 private:
  int make_row(PFS_user *pfs) override;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_users m_row;
};

/** @} */
#endif
