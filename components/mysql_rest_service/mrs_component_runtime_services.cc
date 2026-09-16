/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "components/mysql_rest_service/mrs_component_runtime_services.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "components/mysql_rest_service/mrs_component.h"
#include "components/mysql_rest_service/mrs_component_embedded_mysql_cache.h"
#include "components/mysql_rest_service/mrs_component_mysql_command_service_session.h"
#include "router/src/mysql_rest_service/include/collector/mysql_cache_manager.h"
#include "scope_guard.h"
#include "secure_string.h"  // NOLINT(build/include_subdir)

namespace mrs_component {

ComponentEmbeddedRuntimeServices::ComponentEmbeddedRuntimeServices(
    std::string local_address, std::string metadata_user, std::string data_user,
    std::string jwt_secret)
    : local_address_{std::move(local_address)},
      metadata_auth_id_{std::move(metadata_user) + "@localhost"},
      data_auth_id_{std::move(data_user) + "@localhost"},
      jwt_secret_{std::move(jwt_secret)} {}

ComponentEmbeddedRuntimeServices::~ComponentEmbeddedRuntimeServices() {
  if (!jwt_secret_.empty()) {
    mysql_harness::detail::secure_wipe(jwt_secret_.data(), jwt_secret_.size());
  }
}

std::unique_ptr<collector::MysqlCacheManager>
ComponentEmbeddedRuntimeServices::create_mysql_cache_manager(
    const mrs::Configuration &configuration) {
  return create_embedded_mysql_cache_manager(configuration, local_address_,
                                             metadata_auth_id_, data_auth_id_);
}

std::optional<std::string>
ComponentEmbeddedRuntimeServices::authenticate_mysql_user(
    const std::string &user, const mysql_harness::SecureString &password) {
  Mysql_command_service_session session;
  session.connect_authenticated(user, password, CLIENT_FOUND_ROWS);

  constexpr char current_user_query[] =
      "SELECT CURRENT_USER(), @@session.external_user";
  if (cmd_query->query(session.handle(), current_user_query,
                       sizeof(current_user_query) - 1) != 0) {
    throw std::runtime_error(session.last_error());
  }

  MYSQL_RES_H result = nullptr;
  const auto store_result_error =
      cmd_query_result->store_result(session.handle(), &result);
  const auto result_cleanup = create_scope_guard([result] {
    if (result != nullptr) cmd_query_result->free_result(result);
  });

  if (store_result_error != 0 || result == nullptr) {
    throw std::runtime_error("Failed to retrieve authenticated MySQL user");
  }

  unsigned int field_count = 0;
  MYSQL_ROW_H row = nullptr;
  if (cmd_field_info->num_fields(result, &field_count) != 0 ||
      field_count != 2 || cmd_query_result->fetch_row(result, &row) != 0 ||
      row == nullptr || row[0] == nullptr) {
    return std::nullopt;
  }

  // external_user is also set by plugins that can authenticate over TCP.
  if (row[1] != nullptr) {
    unsigned int protocol = MYSQL_PROTOCOL_DEFAULT;
    if (cmd_options->get(session.handle(), MYSQL_OPT_PROTOCOL, &protocol) !=
        0) {
      throw std::runtime_error(
          "Failed to retrieve MySQL authentication transport");
    }
    if (protocol == MYSQL_PROTOCOL_SOCKET) {
      // Socket authentication may trust mysqld's OS identity, not the HTTP
      // caller. Reject it; retrying would charge another hourly connection
      // admission.
      return std::nullopt;
    }
  }

  ulong *lengths = nullptr;
  if (cmd_query_result->fetch_lengths(result, &lengths) != 0 ||
      lengths == nullptr) {
    throw std::runtime_error("Failed to retrieve authenticated MySQL user");
  }

  return std::string{row[0], lengths[0]};
}

std::shared_ptr<collector::MysqlFixedPoolManager>
ComponentEmbeddedRuntimeServices::create_mysql_user_session_pool(
    uint32_t passthrough_pool_size, const std::string &auth_id) {
  return create_embedded_mysql_user_session_pool(passthrough_pool_size,
                                                 local_address_, auth_id);
}

}  // namespace mrs_component
