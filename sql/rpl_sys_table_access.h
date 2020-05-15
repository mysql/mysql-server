/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#ifndef RPL_SYS_TABLE_ACCESS_INCLUDED
#define RPL_SYS_TABLE_ACCESS_INCLUDED

#include "sql/rpl_table_access.h"  // System_table_access
#include "sql/sql_class.h"
#include "sql/table.h"
#include "thr_lock.h"

class THD;

/**
  @class Rpl_sys_table_access

  The class is used to simplify table data access. It creates new thread/session
  context (THD) and open table on class object creation, and destroys thread and
  closes all open thread tables on class object destruction.
*/
class Rpl_sys_table_access : public System_table_access {
 public:
  /**
    Construction.
  */
  Rpl_sys_table_access() {}

  /**
    Destruction. All opened tables with the open_tables are closed during
    destruction if not already done in deinit().
  */
  ~Rpl_sys_table_access() override;

  /**
    Creates new thread/session context (THD) and open's table on class object
    creation.

    @param[in]  db            Database where the table resides
    @param[in]  table         Table to be openned
    @param[in]  max_num_field Maximum number of fields
    @param[in]  lock_type     How to lock the table

    @retval true  if there is error
    @retval false if there is no error
  */
  bool init(std::string db, std::string table, uint max_num_field,
            enum thr_lock_type lock_type);

  /**
    All opened tables with the open_tables are closed and removes
    THD created in init().

    @retval true  if there is error
    @retval false if there is no error
  */
  bool deinit();

  /**
    Get TABLE object created for the table access purposes.

    @return TABLE pointer.
  */
  TABLE *get_table() { return m_table; }

  /**
    Set error.
  */
  void set_error() { m_error = true; }

  /**
    Verify if error is set.

    @retval true  if there is error
    @retval false if there is no error
  */
  bool get_error() { return m_error; }

  /**
    Prepares before opening table.

    @param[in]  thd  Thread requesting to open the table
  */
  void before_open(THD *thd) override;

 protected:
  /* THD created for TableAccess object purpose. */
  THD *m_thd{nullptr};

  /* THD associated with the calling thread. */
  THD *m_current_thd{nullptr};

  /* The variable determine if table is opened or closed successfully. */
  bool m_error{false};

  /* Determine if index is deinitialized. */
  bool m_key_deinit{false};

  /* TABLE object */
  TABLE *m_table{nullptr};

  /* Save the lock info. */
  Open_tables_backup m_backup;
};
#endif /* RPL_SYS_TABLE_ACCESS_INCLUDED */
