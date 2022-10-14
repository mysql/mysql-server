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

namespace mrs {
namespace authentication {

using AuthApp = mrs::database::entry::AuthApp;
using AuthUser = mrs::database::entry::AuthUser;
using Session = mrs::http::SessionManager::Session;

struct WwwAuthSessionData : mrs::http::SessionManager::Session::SessionData {
  enum State { kWaitingForToken, kUserVerified };

  State state{kWaitingForToken};
  AuthUser user;
};

bool WwwAuthenticationHandler::is_authorized(Session *session, AuthUser *user) {
  auto session_data = session->get_data<WwwAuthSessionData>();
  if (!session_data) return false;
  if (session_data->state != WwwAuthSessionData::kUserVerified) return false;

  *user = session_data->user;

  return true;
}

static WwwAuthSessionData *get_session_data(Session *session) {
  auto session_data = session->get_data<WwwAuthSessionData>();
  if (session_data) return session_data;

  session_data = new WwwAuthSessionData();
  session->set_data(session_data);

  return session_data;
}

bool WwwAuthenticationHandler::authorize(Session *session, http::Url *,
                                         SqlSessionCached *sql_session,
                                         HttpHeaders &input_headers,
                                         AuthUser *out_user) {
  auto session_data = get_session_data(session);

  if (session_data->state == WwwAuthSessionData::kUserVerified) {
    *out_user = session_data->user;
    return true;
  }

  auto authorization = input_headers.get(kAuthorization);
  auto args = mysql_harness::split_string(authorization, ' ', false);
  std::string value = args.size() > 1 ? args[1] : "";
  auto result = www_authorize(value, sql_session, out_user);

  if (result) {
    session_data->user = *out_user;
  }

  return result;
}

const AuthApp &WwwAuthenticationHandler::get_entry() const { return entry_; }

void WwwAuthenticationHandler::add_www_authenticate(const char *schema) {
  class ErrorAddWwwBasicAuth : public http::ErrorChangeResponse {
   public:
    ErrorAddWwwBasicAuth(const std::string &schema) : schema_{schema} {}

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
