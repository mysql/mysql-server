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

#ifndef ROUTER_HTTP_AUTH_REALM_INCLUDED
#define ROUTER_HTTP_AUTH_REALM_INCLUDED

#include "mysqlrouter/http_auth_realm_export.h"

#include <string>
#include <system_error>

/**
 * Authentication Realm.
 *
 * multiple areas in the HTTP server may have the same required authentication.
 * They are in the same 'realm'.
 *
 * A realm has
 *
 * - a name
 * - a requirement
 * - a backend
 * - a required authentication method
 */
class HTTP_AUTH_REALM_EXPORT HttpAuthRealm {
 public:
  HttpAuthRealm(const std::string &name, const std::string &require,
                const std::string &method, const std::string &backend)
      : name_{name}, require_{require}, method_{method}, backend_{backend} {}

  /**
   * name of the auth realm.
   */
  std::string name() const { return name_; }

  /**
   * require notitation of the realm
   */
  std::string require() const { return require_; }

  /**
   * Authentication Method name.
   *
   * - basic
   */
  std::string method() const { return method_; }

  /**
   * Authentication Backend name.
   *
   * - file
   */
  std::string backend() const { return backend_; }

  /**
   * authenticate a user in a realm.
   *
   * @returns error-code
   * @retval 0 authentication without error
   */
  std::error_code authenticate(const std::string &username,
                               const std::string &password) const;

 private:
  std::string name_;
  std::string require_;
  std::string method_;
  std::string backend_;
};

#endif
