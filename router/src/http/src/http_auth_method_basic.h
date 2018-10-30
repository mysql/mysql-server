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

#ifndef ROUTER_HTTP_AUTH_METHOD_BASIC_INCLUDED
#define ROUTER_HTTP_AUTH_METHOD_BASIC_INCLUDED

#include <string>
#include <system_error>

#include "http_auth_method.h"
#include "mysqlrouter/http_server_export.h"

/**
 * Basic Authentication for HTTP.
 *
 * Credentials (username:password) are wrapped in Base64. Not encrypted, must be
 * over secure channel.
 *
 * @see RFC 7235
 *
 * @startuml
 * participant C
 * participant S
 *
 * C->S: GET / HTTP/1.1
 * S->C: HTTP/1.1 401 Unauthed\nWWW-Authenticate: Basic realm="..."
 *
 * C->S: GET / HTTP/1.1\nAuthorization: Basic 34850872634
 * alt success
 * S->C: HTTP/1.1 200 Ok
 * else failed
 * S->C: HTTP/1.1 403 Forbidden
 * end
 * @enduml
 */
class HTTP_SERVER_EXPORT HttpAuthMethodBasic : public HttpAuthMethod {
 public:
  static constexpr char kMethodName[] = "Basic";
  struct AuthData {
    std::string username;
    std::string password;
  };

  static AuthData decode_authorization(const std::string &http_auth_data,
                                       std::error_code &ec);

  static std::string encode_authorization(const AuthData &auth_data);
};

#endif
