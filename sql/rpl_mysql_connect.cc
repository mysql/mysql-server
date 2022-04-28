/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "sql/rpl_mysql_connect.h"
#include "include/mysql.h"
#include "include/sql_common.h"
#include "mysql/components/services/log_builtins.h"
#include "sql-common/net_ns.h"
#include "sql/rpl_replica.h"

Mysql_connection::Mysql_connection(THD *thd, Master_info *mi, std::string host,
                                   uint port, std::string network_namespace,
                                   bool is_io_thread)
    : m_conn(nullptr),
      m_init(false),
      m_thd(thd),
      m_mi(mi),
      m_host(host),
      m_port(port),
      m_network_namespace(network_namespace),
      m_is_io_thread(is_io_thread) {
  if (!(m_conn = mysql_init(nullptr))) {
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
               ER_THD(thd, ER_SLAVE_FATAL_ERROR), "error in mysql_init()");
    return;
  }
  m_init = true;
  m_connected = safe_connect(thd, mi, host, port, network_namespace);
}

Mysql_connection::~Mysql_connection() {
  if (m_init && m_conn != nullptr) {
    mysql_close(m_conn);
    m_conn = nullptr;
  }
  m_init = false;
  m_connected = false;
}

bool Mysql_connection::is_connected() { return m_connected; }

bool Mysql_connection::safe_connect(THD *thd, Master_info *mi, std::string host,
                                    uint port, std::string network_namespace) {
  DBUG_TRACE;
  bool successfully_connected{false};

  std::string tmp_net_ns =
      (network_namespace.empty()) ? mi->network_namespace : network_namespace;
  if (!tmp_net_ns.empty()) {
#ifdef HAVE_SETNS
    if (set_network_namespace(tmp_net_ns)) return false;
#else
    // Network namespace not supported by the platform. Report error.
    LogErr(ERROR_LEVEL, ER_NETWORK_NAMESPACES_NOT_SUPPORTED);
    return true;
#endif
  }

  successfully_connected = !connect_to_master(
      thd, m_conn, mi, false, true, host.c_str(), port, m_is_io_thread);

#ifdef HAVE_SETNS
  if (!tmp_net_ns.empty()) {
    // Restore original network namespace used to be before connection has
    // been created
    successfully_connected =
        restore_original_network_namespace() | successfully_connected;
  }
#endif

  return successfully_connected;
}

bool Mysql_connection::safe_reconnect(THD *thd, Master_info *mi,
                                      bool suppress_warnings, std::string host,
                                      uint port) {
  DBUG_TRACE;
  bool successfully_connected =
      !connect_to_master(thd, m_conn, mi, true, suppress_warnings, host.c_str(),
                         port, m_is_io_thread);
  return successfully_connected;
}

bool Mysql_connection::reconnect() {
  if (!m_init) return false;

  if (!m_connected)
    m_connected = !safe_reconnect(m_thd, m_mi, true, m_host, m_port);

  return m_connected;
}

MYSQL_RES_TUPLE
Mysql_connection::execute_query(std::string query) const {
  uint error = 0;
  std::vector<std::vector<std::string>> rs;

  if (!m_init || !m_connected) return make_pair(error, rs);

  if (m_conn) error = mysql_real_query(m_conn, query.c_str(), query.length());

  if (error) return make_pair(mysql_errno(m_conn), rs);

  if (m_conn->field_count > 0) {
    MYSQL_RES *result = mysql_store_result(m_conn);
    int num_fields = mysql_num_fields(result);
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result))) {
      std::vector<std::string> n_row;
      for (int i = 0; i < num_fields; i++) {
        if (row[i])
          n_row.push_back(row[i]);
        else
          n_row.push_back("");
        error = 0;
      }
      rs.push_back(n_row);
    }
    mysql_free_result(result);
  }

  return make_pair(error, rs);
}
