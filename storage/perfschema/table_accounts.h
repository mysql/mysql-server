/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_ACCOUNTS_H
#define TABLE_ACCOUNTS_H

#include "pfs_column_types.h"
#include "cursor_by_account.h"
#include "table_helper.h"

struct PFS_account;

/**
  \addtogroup Performance_schema_tables
  @{
*/

/**
  A row of PERFORMANCE_SCHEMA.ACCOUNTS.
*/
struct row_accounts
{
  /** Column USER, HOST. */
  PFS_account_row m_account;
  /** Columns CURRENT_CONNECTIONS, TOTAL_CONNECTIONS. */
  PFS_connection_stat_row m_connection_stat;
};

/** Table PERFORMANCE_SCHEMA.ACCOUNTS. */
class table_accounts : public cursor_by_account
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table* create();
  static int delete_all_rows();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);


protected:
  table_accounts();

public:
  ~table_accounts()
  {}

private:
  virtual void make_row(PFS_account *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_accounts m_row;
  /** True if the current row exists. */
  bool m_row_exists;
};

/** @} */
#endif
