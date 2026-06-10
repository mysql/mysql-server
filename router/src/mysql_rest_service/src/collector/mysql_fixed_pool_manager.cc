/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "collector/mysql_fixed_pool_manager.h"

#include <vector>

#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "mrs/router_observation_entities.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/mysql_session.h"

IMPORT_LOG_FUNCTIONS()

namespace collector {

using Object = MysqlFixedPoolManager::Object;

Object MysqlFixedPoolManager::MysqlCacheCallbacks::object_allocate(bool) {
  throw db_pool_exhausted();
}

void MysqlFixedPoolManager::MysqlCacheCallbacks::object_remove(Object obj) {
  mrs::Counter<kEntityCounterMySQLConnectionsClosed>::increment();
  delete obj;
}

bool MysqlFixedPoolManager::MysqlCacheCallbacks::object_before_cache(
    Object obj, [[maybe_unused]] bool dirty) {
  const auto last_error = obj->last_errno();
  if (last_error >= CR_ERROR_FIRST && last_error <= CR_ERROR_LAST) return false;

  return true;
}

bool MysqlFixedPoolManager::MysqlCacheCallbacks::object_retrived_from_cache(
    Object connection) {
  const auto can_be_used = !connection->has_data_on_socket();

  if (can_be_used) {
    mrs::Counter<kEntityCounterMySQLConnectionsReused>::increment();
    connection->allow_failure_at_next_query();
  }

  return can_be_used;
}

namespace {

MysqlFixedPoolManager::ConnectionParameters new_connection_params(
    DestinationProvider *destination, const std::string &user,
    const mysql_harness::SecureString &password) {
  MysqlFixedPoolManager::ConnectionParameters result;
  const auto node =
      destination->get_node(DestinationProvider::kWaitUntilAvaiable);

  if (!node.has_value())
    throw std::runtime_error(
        "Connection to MySQL is impossible, there are no destinations "
        "available.");
  log_debug("MysqlFixedPoolManager::new_connection_params address:%s",
            node->str().c_str());

  result.conn_opts.username = user;
  result.conn_opts.password = password;

  result.conn_opts.destination = *node;

  result.conn_opts.extra_client_flags = CLIENT_FOUND_ROWS;

  const auto &ssl = destination->get_ssl_configuration();
  result.ssl_opts.ssl_mode = ssl.ssl_mode_;
  result.ssl_opts.ca = ssl.ssl_ca_file_;
  result.ssl_opts.capath = ssl.ssl_ca_path_;
  result.ssl_opts.crl = ssl.ssl_crl_file_;
  result.ssl_opts.crlpath = ssl.ssl_crl_path_;
  result.ssl_opts.ssl_cipher = ssl.ssl_ciphers_;
  result.ssl_opts.tls_version = ssl.tls_version_;

  return result;
}

}  // namespace

void MysqlFixedPoolManager::init(DestinationProvider *destination,
                                 const std::string &user,
                                 const mysql_harness::SecureString &password) {
  auto conn_params = new_connection_params(destination, user, password);

  for (size_t i = 0; i < num_instances_; i++) {
    std::unique_ptr<CountedMySQLSession> obj{new CountedMySQLSession()};
    obj->connect(conn_params);
    mrs::Counter<kEntityCounterMySQLConnectionsCreated>::increment();

    // enable all roles, in case necessary ones are not enabled by default
    obj->execute("SET ROLE ALL");

    CachedObject cached{&cache_manager_, false, obj.release()};

    cache_manager_.return_instance(cached);
  }
}

}  // namespace collector
