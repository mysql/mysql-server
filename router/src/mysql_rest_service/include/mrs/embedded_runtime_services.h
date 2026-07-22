/* Copyright (c) 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef ROUTER_SRC_REST_MRS_INCLUDE_MRS_EMBEDDED_RUNTIME_SERVICES_H_
#define ROUTER_SRC_REST_MRS_INCLUDE_MRS_EMBEDDED_RUNTIME_SERVICES_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "mrs/configuration.h"
#include "secure_string.h"  // NOLINT(build/include_subdir)

namespace collector {
class MysqlCacheManager;
class MysqlFixedPoolManager;
}  // namespace collector

namespace mrs {

/**
 * Component-mode overrides for selected MRS runtime services.
 *
 * Standalone Router does not install these services and continues to use the
 * normal network/password/keyring-backed implementation. The component backend
 * installs a non-owning services instance before starting the embedded Router
 * loader and clears it during shutdown.
 *
 * The services object must outlive all MRS objects created while it is
 * installed.
 */
class EmbeddedRuntimeServices {
 public:
  virtual ~EmbeddedRuntimeServices() = default;

  virtual std::unique_ptr<collector::MysqlCacheManager>
  create_mysql_cache_manager(const Configuration &configuration) = 0;

  /**
   * Validate HTTP-supplied MySQL credentials through the server authentication
   * path and return the authenticated CURRENT_USER() identity. The returned
   * auth id is used for embedded sessions created after authentication; this
   * call itself must not attach an embedded security context for a user that
   * has not been authenticated yet.
   */
  virtual std::optional<std::string> authenticate_mysql_user(
      const std::string &user, const mysql_harness::SecureString &password) = 0;

  virtual std::shared_ptr<collector::MysqlFixedPoolManager>
  create_mysql_user_session_pool(uint32_t passthrough_pool_size,
                                 const std::string &auth_id) = 0;

  virtual const std::string &jwt_secret() const = 0;
};

void set_embedded_runtime_services(EmbeddedRuntimeServices *services);
EmbeddedRuntimeServices *get_embedded_runtime_services();

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_INCLUDE_MRS_EMBEDDED_RUNTIME_SERVICES_H_
