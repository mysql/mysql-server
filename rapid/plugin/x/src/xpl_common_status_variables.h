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
#include "ngs_common/atomic.h"


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
    m_stmt_execute_sql.store(0);
    m_stmt_execute_xplugin.store(0);
    m_stmt_execute_mysqlx.store(0);
    m_crud_insert.store(0);
    m_crud_update.store(0);
    m_crud_find.store(0);
    m_crud_delete.store(0);
    m_expect_open.store(0);
    m_expect_close.store(0);
    m_stmt_create_collection.store(0);
    m_stmt_ensure_collection.store(0);
    m_stmt_create_collection_index.store(0);
    m_stmt_drop_collection.store(0);
    m_stmt_drop_collection_index.store(0);
    m_stmt_list_objects.store(0);
    m_stmt_enable_notices.store(0);
    m_stmt_disable_notices.store(0);
    m_stmt_list_notices.store(0);
    m_stmt_list_clients.store(0);
    m_stmt_kill_client.store(0);
    m_stmt_ping.store(0);
    m_bytes_sent.store(0);
    m_bytes_received.store(0);
    m_errors_sent.store(0);
    m_rows_sent.store(0);
    m_notice_warning_sent.store(0);
    m_notice_other_sent.store(0);
  }


  void inc_stmt_execute_sql()
  {
    ++m_stmt_execute_sql;
  }


  long long get_stmt_execute_sql() const
  {
    return m_stmt_execute_sql.load();
  }


  void inc_stmt_execute_xplugin()
  {
    ++m_stmt_execute_xplugin;
  }


  void inc_stmt_execute_mysqlx()
  {
    ++m_stmt_execute_mysqlx;
  }


  long long get_stmt_execute_xplugin() const
  {
    return m_stmt_execute_xplugin.load();
  }


  long long get_stmt_execute_mysqlx() const
  {
    return m_stmt_execute_mysqlx.load();
  }


  void inc_crud_insert()
  {
    ++m_crud_insert;
  }


  long long get_crud_insert() const
  {
    return m_crud_insert.load();
  }


  void inc_crud_update()
  {
    ++m_crud_update;
  }


  long long get_crud_update() const
  {
    return m_crud_update.load();
  }


  void inc_crud_find()
  {
    ++m_crud_find;
  }


  long long get_crud_find() const
  {
    return m_crud_find.load();
  }


  void inc_crud_delete()
  {
    ++m_crud_delete;
  }


  long long get_crud_delete() const
  {
    return m_crud_delete.load();
  }


  void inc_expect_open()
  {
    ++m_expect_open;
  }


  long long get_expect_open() const
  {
    return m_expect_open.load();
  }


  void inc_expect_close()
  {
    ++m_expect_close;
  }


  long long get_expect_close() const
  {
    return m_expect_close.load();
  }


  void inc_stmt_create_collection()
  {
    ++m_stmt_create_collection;
  }


  void inc_stmt_ensure_collection()
  {
    ++m_stmt_ensure_collection;
  }


  long long get_stmt_create_collection() const
  {
    return m_stmt_create_collection.load();
  }


  long long get_stmt_ensure_collection() const
  {
    return m_stmt_ensure_collection.load();
  }


  void inc_stmt_create_collection_index()
  {
    ++m_stmt_create_collection_index;
  }


  long long get_stmt_create_collection_index() const
  {
    return m_stmt_create_collection_index.load();
  }


  void inc_stmt_drop_collection()
  {
    ++m_stmt_drop_collection;
  }


  long long get_stmt_drop_collection() const
  {
    return m_stmt_drop_collection.load();
  }


  void inc_stmt_drop_collection_index()
  {
    ++m_stmt_drop_collection_index;
  }


  long long get_stmt_drop_collection_index() const
  {
    return m_stmt_drop_collection_index.load();
  }


  void inc_stmt_list_objects()
  {
    ++m_stmt_list_objects;
  }


  long long get_stmt_list_objects() const
  {
    return m_stmt_list_objects.load();
  }


  void inc_stmt_enable_notices()
  {
    ++m_stmt_enable_notices;
  }


  long long get_stmt_enable_notices() const
  {
    return m_stmt_enable_notices.load();
  }


  void inc_stmt_disable_notices()
  {
    ++m_stmt_disable_notices;
  }


  long long get_stmt_disable_notices() const
  {
    return m_stmt_disable_notices.load();
  }


  void inc_stmt_list_notices()
  {
    ++m_stmt_list_notices;
  }


  long long get_stmt_list_notices() const
  {
    return m_stmt_list_notices.load();
  }


  void inc_stmt_list_clients()
  {
    ++m_stmt_list_clients;
  }


  long long get_stmt_list_clients() const
  {
    return m_stmt_list_clients.load();
  }


  void inc_stmt_kill_client()
  {
    ++m_stmt_kill_client;
  }


  long long get_stmt_kill_client() const
  {
    return m_stmt_kill_client.load();
  }

  void inc_stmt_ping()
  {
    ++m_stmt_ping;
  }


  long long get_stmt_ping() const
  {
    return m_stmt_ping.load();
  }


  void inc_bytes_sent(long bytes_sent)
  {
    m_bytes_sent += bytes_sent;
  }


  long long get_bytes_sent() const
  {
    return m_bytes_sent.load();
  }


  void inc_bytes_received(long bytes_received)
  {
    m_bytes_received += bytes_received;
  }


  long long get_bytes_received() const
  {
    return m_bytes_received.load();
  }


  void inc_errors_sent()
  {
    ++m_errors_sent;
  }


  long long get_errors_sent() const
  {
    return m_errors_sent.load();
  }


  void inc_rows_sent()
  {
    ++m_rows_sent;
  }


  long long get_rows_sent() const
  {
    return m_rows_sent.load();
  }


  void inc_notice_warning_sent()
  {
    ++m_notice_warning_sent;
  }


  long long get_notice_warning_sent() const
  {
    return m_notice_warning_sent.load();
  }


  void inc_notice_other_sent()
  {
    ++m_notice_other_sent;
  }


  long long get_notice_other_sent() const
  {
    return m_notice_other_sent.load();
  }


private:
  Common_status_variables(const Common_status_variables &);
  Common_status_variables &operator=(const Common_status_variables &);

  mutable volatile ngs::atomic<int64> m_stmt_execute_sql;
  mutable volatile ngs::atomic<int64> m_stmt_execute_xplugin;
  mutable volatile ngs::atomic<int64> m_stmt_execute_mysqlx;
  mutable volatile ngs::atomic<int64> m_crud_insert;
  mutable volatile ngs::atomic<int64> m_crud_update;
  mutable volatile ngs::atomic<int64> m_crud_find;
  mutable volatile ngs::atomic<int64> m_crud_delete;
  mutable volatile ngs::atomic<int64> m_expect_open;
  mutable volatile ngs::atomic<int64> m_expect_close;
  mutable volatile ngs::atomic<int64> m_stmt_create_collection;
  mutable volatile ngs::atomic<int64> m_stmt_ensure_collection;
  mutable volatile ngs::atomic<int64> m_stmt_create_collection_index;
  mutable volatile ngs::atomic<int64> m_stmt_drop_collection;
  mutable volatile ngs::atomic<int64> m_stmt_drop_collection_index;
  mutable volatile ngs::atomic<int64> m_stmt_list_objects;
  mutable volatile ngs::atomic<int64> m_stmt_enable_notices;
  mutable volatile ngs::atomic<int64> m_stmt_disable_notices;
  mutable volatile ngs::atomic<int64> m_stmt_list_notices;
  mutable volatile ngs::atomic<int64> m_stmt_list_clients;
  mutable volatile ngs::atomic<int64> m_stmt_kill_client;
  mutable volatile ngs::atomic<int64> m_stmt_ping;
  mutable volatile ngs::atomic<int64> m_bytes_sent;
  mutable volatile ngs::atomic<int64> m_bytes_received;
  mutable volatile ngs::atomic<int64> m_errors_sent;
  mutable volatile ngs::atomic<int64> m_rows_sent;
  mutable volatile ngs::atomic<int64> m_notice_warning_sent;
  mutable volatile ngs::atomic<int64> m_notice_other_sent;
};


} // namespace xpl


#endif // _XPL_COMMON_STATUS_VARIABLES_H_
