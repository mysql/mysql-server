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

#ifndef PLUGIN_X_SRC_SERVER_AUTHENTICATION_CONTAINER_H_
#define PLUGIN_X_SRC_SERVER_AUTHENTICATION_CONTAINER_H_

#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/authentication_container.h"
#include "plugin/x/src/interface/client.h"

namespace xpl {

class Authentication_container : public iface::Authentication_container {
 public:
  Authentication_container();

  std::unique_ptr<iface::Authentication> get_auth_handler(
      const std::string &name, iface::Session *session) override;
  std::vector<std::string> get_authentication_mechanisms(
      iface::Client *client) override;

 private:
  template <typename Mech>
  void add_authentication_mechanism(const std::string &name,
                                    const bool tls_required) {
    m_auth_handlers.emplace_back(name, tls_required, Mech::create);
  }

  class Auth_entry {
   public:
    using Create = std::unique_ptr<iface::Authentication> (*)(
        iface::Session *, iface::SHA256_password_cache *);

   public:
    Auth_entry(const std::string &name, const bool is_secure,
               const Create create)
        : m_name(name),
          m_must_be_secure_connection(is_secure),
          m_create(create) {}

    const std::string m_name;
    const bool m_must_be_secure_connection;
    const Create m_create;
  };

  using Auth_entries = std::vector<Auth_entry>;

  Auth_entries m_auth_handlers;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVER_AUTHENTICATION_CONTAINER_H_
