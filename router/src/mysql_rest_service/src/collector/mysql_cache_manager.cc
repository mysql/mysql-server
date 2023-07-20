/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mrs/router_observation_entities.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/mysql_session.h"

IMPORT_LOG_FUNCTIONS()

namespace collector {

using Object = MysqlCacheManager::Object;

Object MysqlCacheManager::MysqlCacheCallbacks::object_allocate(bool wait) {
  using namespace std::literals::string_literals;
  std::unique_ptr<CountedMySQLSession> obj{new CountedMySQLSession()};

  obj->connect_and_set_opts(new_connection_params(wait));
  mrs::Counter<kEntityCounterMySQLConnectionsCreated>::increment();

  if (!role_.empty()) {
    obj->execute("SET ROLE "s + role_);
  }
  return obj.release();
}

void MysqlCacheManager::MysqlCacheCallbacks::object_remove(Object obj) {
  mrs::Counter<kEntityCounterMySQLConnectionsClosed>::increment();
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

bool MysqlCacheManager::MysqlCacheCallbacks::object_retrived_from_cache(
    Object connection) {
  auto can_be_used = connection->ping();

  if (can_be_used)
    mrs::Counter<kEntityCounterMySQLConnectionsReused>::increment();

  return can_be_used;
}

void MysqlCacheManager::MysqlCacheCallbacks::object_restore_defaults(
    Object &obj) {
  if (!is_default_user(obj)) {
    obj->change_user(connection_configuration_.mysql_user_,
                     connection_configuration_.mysql_password_, "");
    return;
  }

  obj->reset();
}

bool MysqlCacheManager::MysqlCacheCallbacks::is_default_server(
    Object &obj) const {
  const auto &active_params = obj->get_connection_parameters();

  if (!active_params.conn_opts.unix_socket.empty()) return false;

  // Drop the server is its not on the providers list,
  // Some server was either removed from it or the connection was
  // moved from some other cache.
  return connection_configuration_.provider_->is_node_supported(
      {active_params.conn_opts.host,
       static_cast<uint16_t>(active_params.conn_opts.port)});
}

bool MysqlCacheManager::MysqlCacheCallbacks::is_default_user(
    Object &obj) const {
  const auto &active_params = obj->get_connection_parameters();

  if (active_params.conn_opts.username != connection_configuration_.mysql_user_)
    return false;

  return active_params.conn_opts.password ==
         connection_configuration_.mysql_password_;
}

const ConnectionConfiguration &
MysqlCacheManager::MysqlCacheCallbacks::get_connection_configuration() const {
  return connection_configuration_;
}

MysqlCacheManager::ConnectionParameters
MysqlCacheManager::MysqlCacheCallbacks::new_connection_params(bool wait) {
  using collector::DestinationProvider;
  MysqlCacheManager::ConnectionParameters result;
  const auto node = connection_configuration_.provider_->get_node(
      wait ? DestinationProvider::kWaitUntilAvaiable
           : DestinationProvider::kNoWait);

  if (!node.has_value())
    throw std::runtime_error(
        "Connection to MySQL is impossible, there are not destinations "
        "configured.");
  log_debug("MysqlCacheManager::new_connection_params address:%s, port:%i",
            node->address().c_str(), static_cast<int>(node->port()));
  //  result.ssl_opts.ssl_mode = SSL_MODE_PREFERRED;
  result.conn_opts.username = connection_configuration_.mysql_user_;
  result.conn_opts.password = connection_configuration_.mysql_password_;
  result.conn_opts.host = node->address();
  result.conn_opts.port = node->port();
  result.conn_opts.extra_client_flags = CLIENT_FOUND_ROWS;

  const auto &ssl =
      connection_configuration_.provider_->get_ssl_configuration();
  result.ssl_opts.ssl_mode = ssl.ssl_mode_;
  result.ssl_opts.ca = ssl.ssl_ca_file_;
  result.ssl_opts.capath = ssl.ssl_ca_path_;
  result.ssl_opts.crl = ssl.ssl_crl_file_;
  result.ssl_opts.crlpath = ssl.ssl_crl_path_;
  result.ssl_opts.ssl_cipher = ssl.ssl_ciphers_;

  return result;
}

}  // namespace collector
