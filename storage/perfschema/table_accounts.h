/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_ACCOUNTS_H
#define TABLE_ACCOUNTS_H

/**
  @file storage/perfschema/table_accounts.h
  TABLE ACCOUNTS.
*/

#include <sys/types.h>

#include "storage/perfschema/cursor_by_account.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/table_helper.h"

struct PFS_account;

/**
  @addtogroup performance_schema_tables
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

class PFS_index_accounts_by_user_host : public PFS_index_accounts
{
public:
  PFS_index_accounts_by_user_host()
    : PFS_index_accounts(&m_key_1, &m_key_2), m_key_1("USER"), m_key_2("HOST")
  {
  }

  ~PFS_index_accounts_by_user_host()
  {
  }

  virtual bool match(PFS_account *pfs);

private:
  PFS_key_user m_key_1;
  PFS_key_host m_key_2;
};

/** Table PERFORMANCE_SCHEMA.ACCOUNTS. */
class table_accounts : public cursor_by_account
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
  table_accounts();

public:
  ~table_accounts()
  {
  }

  int index_init(uint idx, bool sorted);

private:
  virtual int make_row(PFS_account *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;
  /** Current row. */
  row_accounts m_row;
};

/** @} */
#endif
