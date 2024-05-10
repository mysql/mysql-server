/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/src/server/authentication_container.h"

#include <memory>
#include <string>

#include "plugin/x/src/auth_challenge_response.h"
#include "plugin/x/src/auth_legacy.h"
#include "plugin/x/src/auth_plain.h"
#include "plugin/x/src/module_mysqlx.h"

namespace xpl {

Authentication_container::Authentication_container() {
  const bool tls_enabled = true, tls_disabled = false;

  add_legacy_authentication_mechanism<Sasl_legacy_auth>("MYSQL41", tls_enabled);
  add_legacy_authentication_mechanism<Sasl_legacy_auth>("MYSQL41",
                                                        tls_disabled);
  add_authentication_mechanism<Sasl_plain_auth>("PLAIN", tls_enabled);
  add_authentication_mechanism<Sasl_sha256_memory_auth>("SHA256_MEMORY",
                                                        tls_enabled);
  add_authentication_mechanism<Sasl_sha256_memory_auth>("SHA256_MEMORY",
                                                        tls_disabled);
}

std::unique_ptr<iface::Authentication>
Authentication_container::get_auth_handler(const std::string &name,
                                           iface::Session *session) {
  const auto type = session->client().connection().get_type();
  const auto is_secure = Connection_type_helper::is_secure_type(type);

  const auto auth_handler =
      std::find_if(m_auth_handlers.begin(), m_auth_handlers.end(),
                   [name, is_secure](const Auth_entry &entry) {
                     if (is_secure != entry.m_must_be_secure_connection)
                       return false;
                     return name == entry.m_name;
                   });

  if (auth_handler == m_auth_handlers.end()) return {};

  return auth_handler->m_create(
      session,
      modules::Module_mysqlx::get_instance_sha256_password_cache().container());
}

std::vector<std::string>
Authentication_container::get_authentication_mechanisms(iface::Client *client) {
  std::vector<std::string> result;
  const auto type = client->connection().get_type();
  const bool is_secure = Connection_type_helper::is_secure_type(type);

  result.reserve(m_auth_handlers.size());

  for (const auto &item : m_auth_handlers) {
    if (item.m_must_be_secure_connection != is_secure) continue;
    if (item.m_legacy) continue;
    result.push_back(item.m_name);
  }

  return result;
}

}  // namespace xpl
