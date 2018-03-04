/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef _XPL_COMMON_STATUS_VARIABLES_H_
#define _XPL_COMMON_STATUS_VARIABLES_H_

#include "plugin/x/ngs/include/ngs_common/atomic.h"


namespace xpl
{

class Common_status_variables
{
public:
  class Variable : private ngs::atomic<int64>
  {
  public:
    Variable() : ngs::atomic<int64>(0) {}
    void operator=(const Variable& other) { store(other.load()); }
    using ngs::atomic<int64>::operator++;
    using ngs::atomic<int64>::operator--;
    using ngs::atomic<int64>::operator+=;
    using ngs::atomic<int64>::load;
  };

  Common_status_variables() {}

  Variable m_stmt_execute_sql;
  Variable m_stmt_execute_xplugin;
  Variable m_stmt_execute_mysqlx;
  Variable m_crud_insert;
  Variable m_crud_update;
  Variable m_crud_find;
  Variable m_crud_delete;
  Variable m_expect_open;
  Variable m_expect_close;
  Variable m_stmt_create_collection;
  Variable m_stmt_ensure_collection;
  Variable m_stmt_drop_collection;
  Variable m_stmt_create_collection_index;
  Variable m_stmt_drop_collection_index;
  Variable m_stmt_list_objects;
  Variable m_stmt_enable_notices;
  Variable m_stmt_disable_notices;
  Variable m_stmt_list_notices;
  Variable m_stmt_list_clients;
  Variable m_stmt_kill_client;
  Variable m_stmt_ping;
  Variable m_bytes_sent;
  Variable m_bytes_received;
  Variable m_errors_sent;
  Variable m_rows_sent;
  Variable m_notice_warning_sent;
  Variable m_notice_other_sent;
  Variable m_errors_unknown_message_type;
  Variable m_crud_create_view;
  Variable m_crud_modify_view;
  Variable m_crud_drop_view;

private:
  Common_status_variables(const Common_status_variables &);
};


} // namespace xpl


#endif // _XPL_COMMON_STATUS_VARIABLES_H_
