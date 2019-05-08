/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_HTTP_AUTH_BACKEND_COMPONENT_INCLUDED
#define MYSQLROUTER_HTTP_AUTH_BACKEND_COMPONENT_INCLUDED

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>

#include "mysqlrouter/http_auth_backend_export.h"

class HttpAuthBackend;

/**
 * public API of the HttpAuthBackend plugin.
 */
class HTTP_AUTH_BACKEND_EXPORT HttpAuthBackendComponent {
 public:
  using value_type = std::map<std::string, std::shared_ptr<HttpAuthBackend>>;

  /**
   * get instance of Component.
   */
  static HttpAuthBackendComponent &get_instance();

  /**
   * initialize component from backends.
   */
  void init(std::shared_ptr<value_type> backends);

  /**
   * authenticate user against auth-backend.
   *
   * @returns error
   * @retval false authentication without error.
   */
  std::error_code authenticate(const std::string &inst,
                               const std::string &username,
                               const std::string &authdata);

 private:
  // disable copy, as we are a single-instance
  HttpAuthBackendComponent(HttpAuthBackendComponent const &) = delete;
  void operator=(HttpAuthBackendComponent const &) = delete;

  std::mutex mu;  // request handler mutex
  std::weak_ptr<value_type> auth_backends_;

  HttpAuthBackendComponent() = default;
};

#endif
