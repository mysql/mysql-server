/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "collector/mysql_cache_manager.h"

#include "mysqlrouter/mysql_session.h"

namespace collector {

using Object = MysqlCacheManager::Object;

Object MysqlCacheManager::MysqlCacheCallbacks::object_allocate() {
  std::unique_ptr<::mysqlrouter::MySQLSession> obj{
      new ::mysqlrouter::MySQLSession()};

  obj->connect_and_set_opts(new_connection_params());
  return obj.release();
}

void MysqlCacheManager::MysqlCacheCallbacks::object_remove(Object obj) {
  delete obj;
}

bool MysqlCacheManager::MysqlCacheCallbacks::object_before_cache(Object obj) {
  // If we are at other server, then just drop such connection,
  // we only need to cache the connections to default server.
  if (!is_default_server(obj)) return false;

  try {
    object_restore_defaults(obj);
  } catch (...) {
    return false;
  }

  return true;
}

void MysqlCacheManager::MysqlCacheCallbacks::object_retrived_from_cache(
    Object) {}

void MysqlCacheManager::MysqlCacheCallbacks::object_restore_defaults(
    Object &obj) {
  if (!is_default_user(obj)) {
    obj->change_user(configuration_.mysql_user_, configuration_.mysql_password_,
                     "");
    return;
  }

  obj->reset();
}

bool MysqlCacheManager::MysqlCacheCallbacks::is_default_server(
    Object &obj) const {
  const auto &active_params = obj->get_connection_parameters();

  if (!active_params.conn_opts.unix_socket.empty()) return false;

  const auto &nodes = configuration_.nodes_;
  const bool any_node_matches = std::any_of(
      nodes.begin(), nodes.end(), [&active_params](const mrs::Node &n) {
        if (n.host_ != active_params.conn_opts.host) return false;
        return n.port_ == active_params.conn_opts.port;
      });

  return any_node_matches;
}

bool MysqlCacheManager::MysqlCacheCallbacks::is_default_user(
    Object &obj) const {
  const auto &active_params = obj->get_connection_parameters();

  if (active_params.conn_opts.username != configuration_.mysql_user_)
    return false;

  return active_params.conn_opts.password == configuration_.mysql_password_;
}

const ConnectionConfiguration &
MysqlCacheManager::MysqlCacheCallbacks::get_connection_configuration() const {
  return configuration_;
}

MysqlCacheManager::ConnectionParameters
MysqlCacheManager::MysqlCacheCallbacks::new_connection_params() {
  MysqlCacheManager::ConnectionParameters result;
  const auto number_of_nodes = configuration_.nodes_.size();
  const auto &node =
      configuration_.nodes_[node_rount_robin_++ % number_of_nodes];

  //  result.ssl_opts.ssl_mode = SSL_MODE_PREFERRED;
  result.conn_opts.username = configuration_.mysql_user_;
  result.conn_opts.password = configuration_.mysql_password_;
  result.conn_opts.host = node.host_;
  result.conn_opts.port = node.port_;

  result.ssl_opts.ssl_mode = configuration_.ssl_.ssl_mode_;
  result.ssl_opts.ca = configuration_.ssl_.ssl_ca_file_;
  result.ssl_opts.capath = configuration_.ssl_.ssl_ca_path_;
  result.ssl_opts.crl = configuration_.ssl_.ssl_crl_file_;
  result.ssl_opts.crlpath = configuration_.ssl_.ssl_crl_path_;
  result.ssl_opts.ssl_cipher = configuration_.ssl_.ssl_ciphers_;

  return result;
}

}  // namespace collector
