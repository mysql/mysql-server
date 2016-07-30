/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _XPL_COMMON_STATUS_VARIABLES_H_
#define _XPL_COMMON_STATUS_VARIABLES_H_

#include "my_global.h"
#include "my_atomic.h"


namespace xpl
{


class Common_status_variables
{
public:
  Common_status_variables()
  {
    reset();
  }


  void reset()
  {
    my_atomic_store64(&m_stmt_execute_sql, 0);
    my_atomic_store64(&m_stmt_execute_xplugin, 0);
    my_atomic_store64(&m_stmt_execute_mysqlx, 0);
    my_atomic_store64(&m_crud_insert, 0);
    my_atomic_store64(&m_crud_update, 0);
    my_atomic_store64(&m_crud_find, 0);
    my_atomic_store64(&m_crud_delete, 0);
    my_atomic_store64(&m_expect_open, 0);
    my_atomic_store64(&m_expect_close, 0);
    my_atomic_store64(&m_stmt_create_collection, 0);
    my_atomic_store64(&m_stmt_ensure_collection, 0);
    my_atomic_store64(&m_stmt_create_collection_index, 0);
    my_atomic_store64(&m_stmt_drop_collection, 0);
    my_atomic_store64(&m_stmt_drop_collection_index, 0);
    my_atomic_store64(&m_stmt_list_objects, 0);
    my_atomic_store64(&m_stmt_enable_notices, 0);
    my_atomic_store64(&m_stmt_disable_notices, 0);
    my_atomic_store64(&m_stmt_list_notices, 0);
    my_atomic_store64(&m_stmt_list_clients, 0);
    my_atomic_store64(&m_stmt_kill_client, 0);
    my_atomic_store64(&m_stmt_ping, 0);
    my_atomic_store64(&m_bytes_sent, 0);
    my_atomic_store64(&m_bytes_received, 0);
    my_atomic_store64(&m_errors_sent, 0);
    my_atomic_store64(&m_rows_sent, 0);
    my_atomic_store64(&m_notice_warning_sent, 0);
    my_atomic_store64(&m_notice_other_sent, 0);
  }


  void inc_stmt_execute_sql()
  {
    my_atomic_add64(&m_stmt_execute_sql, 1);
  }


  long long get_stmt_execute_sql() const
  {
    return my_atomic_load64(&m_stmt_execute_sql);
  }


  void inc_stmt_execute_xplugin()
  {
    my_atomic_add64(&m_stmt_execute_xplugin, 1);
  }


  void inc_stmt_execute_mysqlx()
  {
    my_atomic_add64(&m_stmt_execute_mysqlx, 1);
  }


  long long get_stmt_execute_xplugin() const
  {
    return my_atomic_load64(&m_stmt_execute_xplugin);
  }


  long long get_stmt_execute_mysqlx() const
  {
    return my_atomic_load64(&m_stmt_execute_mysqlx);
  }


  void inc_crud_insert()
  {
    my_atomic_add64(&m_crud_insert, 1);
  }


  long long get_crud_insert() const
  {
    return my_atomic_load64(&m_crud_insert);
  }


  void inc_crud_update()
  {
    my_atomic_add64(&m_crud_update, 1);
  }


  long long get_crud_update() const
  {
    return my_atomic_load64(&m_crud_update);
  }


  void inc_crud_find()
  {
    my_atomic_add64(&m_crud_find, 1);
  }


  long long get_crud_find() const
  {
    return my_atomic_load64(&m_crud_find);
  }


  void inc_crud_delete()
  {
    my_atomic_add64(&m_crud_delete, 1);
  }


  long long get_crud_delete() const
  {
    return my_atomic_load64(&m_crud_delete);
  }


  void inc_expect_open()
  {
    my_atomic_add64(&m_expect_open, 1);
  }


  long long get_expect_open() const
  {
    return my_atomic_load64(&m_expect_open);
  }


  void inc_expect_close()
  {
    my_atomic_add64(&m_expect_close, 1);
  }


  long long get_expect_close() const
  {
    return my_atomic_load64(&m_expect_close);
  }


  void inc_stmt_create_collection()
  {
    my_atomic_add64(&m_stmt_create_collection, 1);
  }


  void inc_stmt_ensure_collection()
  {
    my_atomic_add64(&m_stmt_ensure_collection, 1);
  }


  long long get_stmt_create_collection() const
  {
    return my_atomic_load64(&m_stmt_create_collection);
  }


  long long get_stmt_ensure_collection() const
  {
    return my_atomic_load64(&m_stmt_ensure_collection);
  }


  void inc_stmt_create_collection_index()
  {
    my_atomic_add64(&m_stmt_create_collection_index, 1);
  }


  long long get_stmt_create_collection_index() const
  {
    return my_atomic_load64(&m_stmt_create_collection_index);
  }


  void inc_stmt_drop_collection()
  {
    my_atomic_add64(&m_stmt_drop_collection, 1);
  }


  long long get_stmt_drop_collection() const
  {
    return my_atomic_load64(&m_stmt_drop_collection);
  }


  void inc_stmt_drop_collection_index()
  {
    my_atomic_add64(&m_stmt_drop_collection_index, 1);
  }


  long long get_stmt_drop_collection_index() const
  {
    return my_atomic_load64(&m_stmt_drop_collection_index);
  }


  void inc_stmt_list_objects()
  {
    my_atomic_add64(&m_stmt_list_objects, 1);
  }


  long long get_stmt_list_objects() const
  {
    return my_atomic_load64(&m_stmt_list_objects);
  }


  void inc_stmt_enable_notices()
  {
    my_atomic_add64(&m_stmt_enable_notices, 1);
  }


  long long get_stmt_enable_notices() const
  {
    return my_atomic_load64(&m_stmt_enable_notices);
  }


  void inc_stmt_disable_notices()
  {
    my_atomic_add64(&m_stmt_disable_notices, 1);
  }


  long long get_stmt_disable_notices() const
  {
    return my_atomic_load64(&m_stmt_disable_notices);
  }


  void inc_stmt_list_notices()
  {
    my_atomic_add64(&m_stmt_list_notices, 1);
  }


  long long get_stmt_list_notices() const
  {
    return my_atomic_load64(&m_stmt_list_notices);
  }


  void inc_stmt_list_clients()
  {
    my_atomic_add64(&m_stmt_list_clients, 1);
  }


  long long get_stmt_list_clients() const
  {
    return my_atomic_load64(&m_stmt_list_clients);
  }


  void inc_stmt_kill_client()
  {
    my_atomic_add64(&m_stmt_kill_client, 1);
  }


  long long get_stmt_kill_client() const
  {
    return my_atomic_load64(&m_stmt_kill_client);
  }

  void inc_stmt_ping()
  {
    my_atomic_add64(&m_stmt_ping, 1);
  }


  long long get_stmt_ping() const
  {
    return my_atomic_load64(&m_stmt_ping);
  }


  void inc_bytes_sent(long bytes_sent)
  {
    my_atomic_add64(&m_bytes_sent, bytes_sent);
  }


  long long get_bytes_sent() const
  {
    return my_atomic_load64(&m_bytes_sent);
  }


  void inc_bytes_received(long bytes_received)
  {
    my_atomic_add64(&m_bytes_received, bytes_received);
  }


  long long get_bytes_received() const
  {
    return my_atomic_load64(&m_bytes_received);
  }


  void inc_errors_sent()
  {
    my_atomic_add64(&m_errors_sent, 1);
  }


  long long get_errors_sent() const
  {
    return my_atomic_load64(&m_errors_sent);
  }


  void inc_rows_sent()
  {
    my_atomic_add64(&m_rows_sent, 1);
  }


  long long get_rows_sent() const
  {
    return my_atomic_load64(&m_rows_sent);
  }


  void inc_notice_warning_sent()
  {
    my_atomic_add64(&m_notice_warning_sent, 1);
  }


  long long get_notice_warning_sent() const
  {
    return my_atomic_load64(&m_notice_warning_sent);
  }


  void inc_notice_other_sent()
  {
    my_atomic_add64(&m_notice_other_sent, 1);
  }


  long long get_notice_other_sent() const
  {
    return my_atomic_load64(&m_notice_other_sent);
  }


private:
  Common_status_variables(const Common_status_variables &);
  Common_status_variables &operator=(const Common_status_variables &);

  mutable volatile int64 m_stmt_execute_sql;
  mutable volatile int64 m_stmt_execute_xplugin;
  mutable volatile int64 m_stmt_execute_mysqlx;
  mutable volatile int64 m_crud_insert;
  mutable volatile int64 m_crud_update;
  mutable volatile int64 m_crud_find;
  mutable volatile int64 m_crud_delete;
  mutable volatile int64 m_expect_open;
  mutable volatile int64 m_expect_close;
  mutable volatile int64 m_stmt_create_collection;
  mutable volatile int64 m_stmt_ensure_collection;
  mutable volatile int64 m_stmt_create_collection_index;
  mutable volatile int64 m_stmt_drop_collection;
  mutable volatile int64 m_stmt_drop_collection_index;
  mutable volatile int64 m_stmt_list_objects;
  mutable volatile int64 m_stmt_enable_notices;
  mutable volatile int64 m_stmt_disable_notices;
  mutable volatile int64 m_stmt_list_notices;
  mutable volatile int64 m_stmt_list_clients;
  mutable volatile int64 m_stmt_kill_client;
  mutable volatile int64 m_stmt_ping;
  mutable volatile int64 m_bytes_sent;
  mutable volatile int64 m_bytes_received;
  mutable volatile int64 m_errors_sent;
  mutable volatile int64 m_rows_sent;
  mutable volatile int64 m_notice_warning_sent;
  mutable volatile int64 m_notice_other_sent;
};


} // namespace xpl


#endif // _XPL_COMMON_STATUS_VARIABLES_H_
