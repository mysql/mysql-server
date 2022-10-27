/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mrs/authentication/www_authentication_handler.h"

#include "mrs/http/error.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

const static char *kBasicSchema = "basic";

using AuthApp = mrs::database::entry::AuthApp;
using AuthUser = mrs::database::entry::AuthUser;
using Session = mrs::http::SessionManager::Session;

struct WwwAuthSessionData : mrs::http::SessionManager::Session::SessionData {};

bool WwwAuthenticationHandler::is_authorized(Session *session, AuthUser *user) {
  log_debug("WwwAuthenticationHandler::is_authorized");
  // TODO(lkotula): Right now we do not need to get the session_data (Shouldn't
  // be in review)
  auto session_data = session->get_data<WwwAuthSessionData>();
  if (!session_data) return false;
  if (session->state != Session::kUserVerified) {
    log_debug("WwwAuth: user not verified");
    return false;
  }

  log_debug("is_authorized returned true");
  *user = session->user;

  return true;
}

bool WwwAuthenticationHandler::authorize(Session *session, http::Url *url,
                                         SqlSessionCached *sql_session,
                                         HttpHeaders &input_headers,
                                         AuthUser *out_user) {
  log_debug("WwwAuth: Authorize user");
  if (session->state == Session::kUserVerified) {
    log_debug("WwwAuth: user already verified");
    *out_user = session->user;
    return true;
  }

  url->get_if_query_parameter("onCompletionRedirect",
                              &session->users_on_complete_url_redirection);
  url->get_if_query_parameter("onCompletionClose",
                              &session->users_on_complete_timeout);

  auto authorization_cstr = input_headers.get(kAuthorization);
  if (nullptr == authorization_cstr) {
    log_debug("WwwAuth: no authorization selected, retry?");
    add_www_authenticate(kBasicSchema);
  }
  std::string authorization = authorization_cstr ? authorization_cstr : "";
  auto args = mysql_harness::split_string(authorization, ' ', false);
  std::string value = args.size() > 1 ? args[1] : "";
  log_debug("WwwAuth: execute");
  auto result = www_authorize(value, sql_session, out_user);

  if (result) {
    session->user = *out_user;
    session->state = Session::kUserVerified;
    return true;
  }
  add_www_authenticate(kBasicSchema);

  return false;
}

const AuthApp &WwwAuthenticationHandler::get_entry() const { return entry_; }

void WwwAuthenticationHandler::add_www_authenticate(const char *schema) {
  class ErrorAddWwwBasicAuth : public http::ErrorChangeResponse {
   public:
    ErrorAddWwwBasicAuth(const std::string &schema) : schema_{schema} {}

    bool retry() const override { return true; }
    http::Error change_response(HttpRequest *request) const override {
      request->get_output_headers().add(kWwwAuthenticate, schema_.c_str());

      return http::Error{HttpStatusCode::Unauthorized};
    }

    const std::string schema_;
  };

  throw ErrorAddWwwBasicAuth(schema);
}

}  // namespace authentication
}  // namespace mrs
