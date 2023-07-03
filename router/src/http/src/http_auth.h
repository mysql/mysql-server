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

#ifndef MYSQLROUTER_HTTP_AUTH_INCLUDED
#define MYSQLROUTER_HTTP_AUTH_INCLUDED

#include <string>
#include <system_error>

#include "mysqlrouter/http_request.h"
#include "mysqlrouter/http_server_export.h"

#include "http_auth_realm.h"

/**
 * Quoted String.
 *
 * @see https://tools.ietf.org/html/rfc7230#section-3.2.6
 */
class HTTP_SERVER_EXPORT HttpQuotedString {
 public:
  static std::string quote(const std::string &str);
};

/**
 * Authentication Challenge.
 *
 * sent by server to client when it asks client to authenticate.
 *
 * payload of the WWW-Authenticate header.
 *
 * @see https://tools.ietf.org/html/rfc7235#section-4.1
 */
class HTTP_SERVER_EXPORT HttpAuthChallenge {
 public:
  /**
   * construct challenge message.
   */
  HttpAuthChallenge(
      const std::string &scheme, const std::string &token,
      const std::vector<std::pair<std::string, std::string>> params)
      : scheme_{scheme}, token_{token}, params_{params} {}

  /**
   * convert challenge message to payload of WWW-Authenticate.
   *
   * @returns `{scheme} {token} {params}`
   */
  std::string str() const;

  /**
   * authentication scheme.
   *
   * e.g.: Basic
   */
  std::string scheme() const { return scheme_; }

  /**
   * token of the challenge message.
   *
   * @note valid according to RFC 7235, but usually unused.
   */
  std::string token() const { return token_; }

  /**
   * parameters of the challenge message.
   *
   * e.g.: realm="secret"
   */
  std::vector<std::pair<std::string, std::string>> params() { return params_; }

 private:
  std::string scheme_;
  std::string token_;
  std::vector<std::pair<std::string, std::string>> params_;
};

/**
 * Authorization message.
 *
 * sent from client to server.
 *
 * @see https://tools.ietf.org/html/rfc7235#section-4.2
 */
class HTTP_SERVER_EXPORT HttpAuthCredentials {
 public:
  /**
   * construct Authorization message from fields.
   */
  HttpAuthCredentials(
      const std::string &scheme, const std::string &token,
      const std::vector<std::pair<std::string, std::string>> params)
      : scheme_{scheme}, token_{token}, params_{params} {}

  /**
   * parse a 'credentials' field.
   *
   * ec MUST be checked before using the return-value.
   *
   * @param hdr content of Authorization header
   * @param ec error code
   * @returns a HttpAuthCredentials message ... and error_code
   */
  static HttpAuthCredentials from_header(const std::string &hdr,
                                         std::error_code &ec);

  /**
   * string representation of 'credentials'.
   *
   * according to RFC 7235
   */
  std::string str() const;

  /**
   * authentication scheme of the Authorization message.
   *
   * e.g.: Basic
   */
  std::string scheme() const { return scheme_; }

  /**
   * token part of the Authorization message.
   *
   * for Basic this is a Base64 encoded strings.
   */
  std::string token() const { return token_; }

  /**
   * params part of the Authorization message.
   *
   * for Bearer this is a list of params
   */
  std::vector<std::pair<std::string, std::string>> params() { return params_; }

 private:
  std::string scheme_;
  std::string token_;
  std::vector<std::pair<std::string, std::string>> params_;
};

#endif
