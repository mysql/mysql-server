/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mrs/authentication/basic_handler.h"

#include <utility>

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/base64.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

static bool extract_user_credentials_from_token(
    [[maybe_unused]] const std::string &token,
    [[maybe_unused]] std::string *user,
    [[maybe_unused]] std::string *password) {
  auto result = Base64::decode(token.c_str());

  auto it = std::find(result.begin(), result.end(), ':');
  if (it == result.end()) return false;

  *user = std::string(result.begin(), it);
  *password = std::string(it + 1, result.end());

  return true;
}

BasicHandler::BasicHandler(const AuthApp &entry,
                           collector::MysqlCacheManager *cache_manager)
    : WwwAuthenticationHandler(entry), cache_manager_{cache_manager} {
  log_debug("BasicHandler for service %s, %s",
            entry.service_id.to_string().c_str(), to_string(entry).c_str());
}

UniversalId BasicHandler::get_service_id() const { return entry_.service_id; }

UniversalId BasicHandler::get_id() const { return entry_.id; }

bool BasicHandler::www_authorize(const std::string &token,
                                 SqlSessionCached *out_cache,
                                 AuthUser *out_user) {
  try {
    database::entry::AuthUser mrds_user;
    std::string auth_user;
    std::string auth_password;

    if (!extract_user_credentials_from_token(token, &auth_user, &auth_password))
      throw std::runtime_error("extraction failed");

    // The MySQL account may be different for different host,
    // even if they use the same user-name.
    //
    // This potential problem should be documented.
    pre_authorize_account(this, auth_user);

    auto default_auth_user =
        out_cache->get()->get_connection_parameters().conn_opts;
    out_cache->get()->change_user(auth_user, auth_password, "");

    out_user->vendor_user_id =
        (*out_cache->get()->query_one("SELECT CURRENT_USER();"))[0];

    // Restore default user
    out_cache->get()->change_user(default_auth_user.username,
                                  default_auth_user.password, "");

    out_user->app_id = entry_.id;

    return um_.user_get(out_user, out_cache);
  } catch (const std::exception &e) {
    return false;
  }
}

}  // namespace authentication
}  // namespace mrs
