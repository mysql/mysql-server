/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "client/basic_authentication.h"

#include "helper/http/url.h"

#include "mysqlrouter/http_auth_method_basic.h"
#include "mysqlrouter/http_request.h"

namespace mrs_client {

std::string find_in_headers(const Headers &h, const std::string &key) {
  for (const auto &[k, v] : h) {
    if (k == key) return v;
  }

  return {};
}

static void authenticate(HttpClientRequest &request, const std::string &user,
                         const std::string &password) {
  HttpAuthMethodBasic basic;
  const char *kAuthorization = "Authorization";
  std::string auth_string{basic.kMethodName};
  auth_string += " " + basic.encode_authorization({user, password});
  request.add_header(kAuthorization, auth_string.c_str());
}

Result BasicAuthentication::do_basic_flow_with_session(
    HttpClientRequest &request, const std::string &url, const std::string &user,
    const std::string &password) {
  HttpAuthMethodBasic basic;
  const char *kAuthorization = "Authorization";
  std::string auth_string{basic.kMethodName};
  auth_string += " " + basic.encode_authorization({user, password});
  request.add_header(kAuthorization, auth_string.c_str());

  authenticate(request, user, password);

  auto result = request.do_request(HttpMethod::Get, url, {});

  if (result.status != HttpStatusCode::TemporaryRedirect)
    throw std::runtime_error(
        "Expected redirection flow, received other status code.");

  auto location = find_in_headers(result.headers, "Location");
  if (location.empty())
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `Location` header.");

  // Parameter value
  std::string pvalue;
  auto u = HttpUri::parse(location);
  helper::http::Url query_parse(u);
  if (!query_parse.get_if_query_parameter("login", &pvalue))
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `login` query parameter.");

  if (pvalue != "success")
    throw std::runtime_error("HTTP redirect, points that login failed.");

  return {HttpStatusCode::Ok, {}, {}};
}

}  // namespace mrs_client
