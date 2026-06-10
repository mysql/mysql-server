/*
 Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mrs/authentication/oauth2_oidc_handler.h"

#include <chrono>
#include <string_view>

#include "helper/container/map.h"
#include "helper/http/url.h"
#include "helper/json/to_string.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/http_client.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using RequestHandler = Oauth2Handler::RequestHandler;
using RequestHandlerPtr = Oauth2Handler::RequestHandlerPtr;
using std::chrono::seconds;
using std::chrono::steady_clock;

static void modify_oci_path(std::vector<std::string> &path_elements,
                            const char *last_path_element) {
  if (path_elements.empty()) {
    path_elements.emplace_back("oauth2");
    path_elements.emplace_back("v1");
  }
  path_elements.emplace_back(last_path_element);
}

const std::string k_oauth_scope = "openid profile email phone";

void Oauth2OidcHandler::RequestHandlerJsonSimpleObjectWithBearer::before_send(
    Request *request) {
  std::string bearer = "Bearer " + token_;
  request->get_output_headers().add("Authorization", bearer.c_str());
}

Oauth2OidcHandler::Oauth2OidcHandler(const AuthApp &entry, QueryFactory *qf)
    : Oauth2Handler{entry, qf} {
  log_debug("Oauth2OidcHandler for service %s", to_string(entry_).c_str());
}

Oauth2OidcHandler::~Oauth2OidcHandler() {
  log_debug("~Oauth2OidcHandler for service %s", to_string(entry_).c_str());
}

const std::string &Oauth2OidcHandler::get_handler_name() const {
  static const std::string k_app_name{"OIDC authentication application"};

  return k_app_name;
}

std::string Oauth2OidcHandler::get_url_location(GenericSessionData *session,
                                                Url *) const {
  ::http::base::Uri result{entry_.url};

  modify_oci_path(result.get_path_elements(), "authorize");

  auto &qe = result.get_query_elements();

  qe["response_type"] = "code";
  qe["client_id"] = entry_.app_id;
  qe["state"] = "first";
  qe["scope"] = k_oauth_scope;

  // Unescaped redirect_uri & last
  return result.join() + "&redirect_uri=" + session->redirection;
}

std::string Oauth2OidcHandler::get_url_direct_auth() const {
  ::http::base::Uri result{entry_.url};

  modify_oci_path(result.get_path_elements(), "token");

  return result.join();
}

std::string Oauth2OidcHandler::get_url_validation(GenericSessionData *) const {
  ::http::base::Uri result{entry_.url};

  modify_oci_path(result.get_path_elements(), "userinfo");

  return result.join();
}

std::string Oauth2OidcHandler::get_body_access_token_request(
    GenericSessionData *session_data) const {
  std::string body =
      "grant_type=authorization_code&code=" + session_data->auth_code +
      "&client_id=" + entry_.app_id + "&client_secret=" + entry_.app_token +
      "&redirect_uri=" + session_data->redirection;

  return body;
}

RequestHandlerPtr Oauth2OidcHandler::get_request_handler_access_token(
    GenericSessionData *session_data) {
  RequestHandler *result = new RequestHandlerJsonSimpleObject{
      {{"access_token", &session_data->access_token},
       {"expires_in", &session_data->expires}}};
  return RequestHandlerPtr{result};
}

RequestHandlerPtr Oauth2OidcHandler::get_request_handler_verify_account(
    Session *session, GenericSessionData *session_data) {
  // todo: email_verified
  RequestHandler *result = new RequestHandlerJsonSimpleObjectWithBearer{
      {{"sub", &session->user.vendor_user_id},
       {"name", &session->user.name},
       {"email", &session->user.email}},
      session_data->access_token};

  return RequestHandlerPtr{result};
}

}  // namespace authentication
}  // namespace mrs
