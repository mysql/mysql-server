/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "client/authentication.h"

#include "helper/container/map.h"
#include "helper/http/url.h"
#include "helper/string/hex.h"
#include "helper/string/random.h"

#include "mysqlrouter/base64.h"
#include "mysqlrouter/component/http_auth_method_basic.h"

namespace mrs_client {

static void authenticate(HttpClientRequest *request, const std::string &user,
                         const std::string &password) {
  HttpAuthMethodBasic basic;
  const char *kAuthorization = "Authorization";
  std::string auth_string{basic.kMethodName};
  auth_string += " " + basic.encode_authorization({user, password});
  request->add_header(kAuthorization, auth_string.c_str());
}

Result Authentication::do_basic_flow(HttpClientRequest *request,
                                     std::string url, const std::string &user,
                                     const std::string &password,
                                     const SessionType st) {
  HttpAuthMethodBasic basic;
  const char *kAuthorization = "Authorization";
  std::string auth_string{basic.kMethodName};
  auth_string += " " + basic.encode_authorization({user, password});
  request->add_header(kAuthorization, auth_string.c_str());

  authenticate(request, user, password);
  if (st == SessionType::kJWT) {
    url = url + "?sessionType=bearer";
  }

  bool set_new_cookies = st == SessionType::kCookie;
  auto result = request->do_request(HttpMethod::Get, url, {}, set_new_cookies);

  if (result.status == HttpStatusCode::NotFound) return result;

  if (result.status != HttpStatusCode::TemporaryRedirect) {
    return result;
    //    throw std::runtime_error(
    //        "Expected redirection flow, received other status code.");
  }

  auto location = find_in_headers(result.headers, "Location");
  if (location.empty())
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `Location` header.");

  // Parameter value
  std::string pvalue;
  http::base::Uri u{location};
  std::map<std::string, std::string> parameters;

  helper::http::Url helper_uri(u);

  if (!helper_uri.get_if_query_parameter("login", &pvalue))
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `login` query parameter.");

  if (pvalue != "success")
    throw std::runtime_error("HTTP redirect, points that login failed.");

  if (st == SessionType::kJWT) {
    pvalue.clear();
    if (!helper_uri.get_if_query_parameter("accessToken", &pvalue)) {
      throw std::runtime_error(
          "HTTP redirect, doesn't contain `accessToken` query parameter.");
    }

    if (pvalue.empty())
      throw std::runtime_error(
          "HTTP redirect, doesn't contain valid JWT token.");
    std::string header{"Authorization:Bearer "};
    header += pvalue;
    request->get_session()->add_header(&header[0]);
  }

  return {HttpStatusCode::Ok, {}, {}};
}

class Scram {
 public:
  using Base64NoPadd =
      Base64Base<Base64Alphabet::Base64Url, Base64Endianess::BIG, true, '='>;

  std::string get_initial_auth_data(const std::string &user) {
    const static std::string kParameterAuthData = "data";
    using namespace std::string_literals;
    client_first_ = "n="s + user + ",r=" + generate_nonce(10);
    return kParameterAuthData + "=" +
           Base64NoPadd::encode(as_array("n,," + client_first_));
  }

  std::string as_string(const std::vector<unsigned char> &c) {
    return std::string(c.begin(), c.end());
  }

  std::vector<uint8_t> as_array(const std::string &s) {
    return std::vector<uint8_t>(s.begin(), s.end());
  }

  std::string generate_nonce(std::size_t size) {
    return helper::string::hex(
        helper::generate_string<helper::Generator8bitsValues>(size));
  }

  std::string client_first_;
};

Result Authentication::do_scram_flow(HttpClientRequest *request,
                                     std::string url, const std::string &user,
                                     const std::string &,
                                     const SessionType st) {
  Scram scram;

  url = url + "?" + scram.get_initial_auth_data(user);
  //  if (st == SessionType::kJWT) {
  //    url = url + "?sessionType=bearer";
  //  }

  bool set_new_cookies = st == SessionType::kCookie;
  auto result = request->do_request(HttpMethod::Get, url, {}, set_new_cookies);

  if (result.status == HttpStatusCode::NotFound) return result;

  if (result.status != HttpStatusCode::TemporaryRedirect)
    throw std::runtime_error(
        "Expected redirection flow, received other status code.");

  auto location = find_in_headers(result.headers, "Location");
  if (location.empty())
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `Location` header.");

  // Parameter value
  std::string pvalue;
  http::base::Uri u(location);
  std::map<std::string, std::string> parameters;

  helper::http::Url helper_uri(u);

  if (!helper_uri.get_if_query_parameter("login", &pvalue))
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `login` query parameter.");

  if (pvalue != "success")
    throw std::runtime_error("HTTP redirect, points that login failed.");

  if (st == SessionType::kJWT) {
    pvalue.clear();
    if (!helper_uri.get_if_query_parameter("accessToken", &pvalue)) {
      throw std::runtime_error(
          "HTTP redirect, doesn't contain `accessToken` query parameter.");
    }

    if (pvalue.empty())
      throw std::runtime_error(
          "HTTP redirect, doesn't contain valid JWT token.");
    std::string header{"Authorization:Bearer "};
    header += pvalue;
    request->get_session()->add_header(&header[0]);
  }

  return {HttpStatusCode::Ok, {}, {}};
}

}  // namespace mrs_client
