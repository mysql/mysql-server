/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_HTTP_AUTH_REALM_COMPONENT_INCLUDED
#define MYSQLROUTER_HTTP_AUTH_REALM_COMPONENT_INCLUDED

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>

#include "mysqlrouter/http_auth_realm_lib_export.h"

class HttpAuthRealm;

/**
 * public API of HttpAuthRealm plugin.
 */
class HTTP_AUTH_REALM_LIB_EXPORT HttpAuthRealmComponent {
 public:
  using value_type = std::map<std::string, std::shared_ptr<HttpAuthRealm>>;

  /**
   * get singleton instance of the component.
   */
  static HttpAuthRealmComponent &get_instance();

  /**
   * register a realm with a handler.
   */
  void add_realm(const std::string &name, std::shared_ptr<HttpAuthRealm> realm);

  /**
   * unregister a realm.
   */
  void remove_realm(const std::string &name);

  /**
   * authenticate user with authdata against realm.
   *
   * @param inst instance name of realm
   * @param username name of user to authenticate
   * @param authdata credentials of user
   * @returns error
   * @retval false authentication without error.
   */
  std::error_code authenticate(const std::string &inst,
                               const std::string &username,
                               const std::string &authdata);

  /**
   * get realm by instance name.
   */
  std::shared_ptr<HttpAuthRealm> get(const std::string &inst);

 private:
  // disable copy, as we are a single-instance
  HttpAuthRealmComponent(HttpAuthRealmComponent const &) = delete;
  void operator=(HttpAuthRealmComponent const &) = delete;

  std::mutex realms_m_;
  value_type auth_realms_;

  HttpAuthRealmComponent() = default;
};

#endif
