/*
 Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mrs/authentication/oauth2_google_handler.h"

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

const std::string k_oauth_scope =
    "https://www.googleapis.com/auth/userinfo.email "
    "https://www.googleapis.com/auth/userinfo.profile";

Oauth2GoogleHandler::Oauth2GoogleHandler(const AuthApp &entry)
    : Oauth2Handler{entry} {
  log_debug("Oauth2GoogleHandler for service %s", to_string(entry_).c_str());
}

Oauth2GoogleHandler::~Oauth2GoogleHandler() {
  log_debug("~Oauth2GoogleHandler for service %s", to_string(entry_).c_str());
}

std::string Oauth2GoogleHandler::get_url_location(GenericSessionData *,
                                                  Url *url) const {
  ::http::base::Uri result{
      !entry_.url.empty() ? entry_.url
                          : "https://accounts.google.com/o/oauth2/v2/auth"};

  auto &requests_uri = url->uri_;
  std::string redirect_uri = entry_.host + requests_uri.get_path();

  if (requests_uri.get_query().length()) {
    redirect_uri += "?" + requests_uri.get_query();
  }

  auto &qe = result.get_query_elements();

  qe["response_type"] = "code";
  qe["client_id"] = entry_.app_id;
  qe["state"] = "first";
  qe["scope"] = k_oauth_scope;

  // Unescaped redirect_uri & last
  return result.join() + "&redirect_uri=" + redirect_uri;
}

std::string Oauth2GoogleHandler::get_url_direct_auth() const {
  if (!entry_.url_access_token.empty()) return entry_.url_access_token;

  const static std::string result{"https://oauth2.googleapis.com/token"};
  return result;
}

std::string Oauth2GoogleHandler::get_url_validation(
    GenericSessionData *data) const {
  std::string result{entry_.url_validation.empty()
                         ? "https://www.googleapis.com/oauth2/v3/userinfo"
                         : entry_.url_validation};
  result += "?access_token=" + data->access_token;
  return result;
}

std::string Oauth2GoogleHandler::get_body_access_token_request(
    GenericSessionData *session_data) const {
  std::string body =
      "grant_type=authorization_code&code=" + session_data->auth_code +
      "&client_id=" + entry_.app_id + "&client_secret=" + entry_.app_token +
      "&redirect_uri=" + session_data->redirection;

  return body;
}

RequestHandlerPtr Oauth2GoogleHandler::get_request_handler_access_token(
    GenericSessionData *session_data) {
  RequestHandler *result = new RequestHandlerJsonSimpleObject{
      {{"access_token", &session_data->access_token},
       {"expires_in", &session_data->expires}}};
  return RequestHandlerPtr{result};
}

RequestHandlerPtr Oauth2GoogleHandler::get_request_handler_verify_account(
    Session *session, GenericSessionData *) {
  RequestHandler *result = new RequestHandlerJsonSimpleObject{
      {{"sub", &session->user.vendor_user_id},
       {"name", &session->user.name},
       {"email", &session->user.email}}};

  return RequestHandlerPtr{result};
}

}  // namespace authentication
}  // namespace mrs
