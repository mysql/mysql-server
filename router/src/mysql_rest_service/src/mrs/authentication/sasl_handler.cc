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

#include "mrs/authentication/sasl_handler.h"

#include <stdexcept>

#include <my_rapidjson_size_t.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

#include "helper/container/map.h"
#include "helper/http/url.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "mrs/http/error.h"
#include "mrs/http/utilities.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/base64.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

namespace {

class JsonGetState : public helper::json::RapidReaderHandlerToStruct<
                         std::pair<std::string, bool>> {
 public:
  bool String(const Ch *cstr, rapidjson::SizeType clen, bool) override {
    if (!is_object_path()) return true;

    if ("state" == get_current_key()) {
      result_.first.assign(cstr, clen);
      return true;
    }

    result_.second = true;
    return true;
  }
};

}  // namespace

using AuthApp = mrs::database::entry::AuthApp;
using AuthenticationState = SaslHandler::AuthenticationState;
using SaslData = SaslHandler::SaslData;
using Url = helper::http::Url;

SaslHandler::SaslHandler(const AuthApp &entry) : entry_{entry} {
  log_debug("SaslHandler for service %s", to_string(entry_).c_str());
}

UniversalId SaslHandler::get_service_id() const { return entry_.service_id; }

const AuthApp &SaslHandler::get_entry() const { return entry_; }

UniversalId SaslHandler::get_id() const { return entry_.id; }

bool SaslHandler::redirects() const {
  log_debug("SaslHandler::redirects - false");
  return false;
}

bool SaslHandler::is_authorized(Session *session, AuthUser *user) {
  log_debug("is_authorized session=%p, state=%i", session, session->state);

  if (session->state != Session::kUserVerified) return false;

  *user = session->user;
  log_debug("is_authorized session-user:%s", user->user_id.to_string().c_str());

  return true;
}

const static std::string kParameterAuthData = "data";

AuthenticationState get_authentication_state_impl(const std::string &s) {
  const std::map<std::string, AuthenticationState> allowed_values{
      {"exchange", AuthenticationState::AuthenticationStateExchange},
      {"initial", AuthenticationState::AuthenticationStateInitialResponse},
      {"response", AuthenticationState::AuthenticationStateResponse}};

  auto it = allowed_values.find(s);
  if (allowed_values.end() == it) {
    it = allowed_values.find(mysql_harness::make_lower(s));
    if (allowed_values.end() == it)
      return AuthenticationState::AuthenticationStateExchange;
  }

  return it->second;
}

AuthenticationState SaslHandler::get_authentication_state(
    const Url::Parameters &parameters, const bool has_auth_data) {
  const static std::string kState{"state"};
  auto state = get_authentication_state_impl(
      helper::container::get_value_default(parameters, kState, ""));

  if (state == AuthenticationStateExchange && has_auth_data) {
    return AuthenticationStateInitialResponse;
  }

  return state;
}

template <typename T>
std::string as_string(const std::vector<T> &v) {
  return std::string(v.begin(), v.end());
}

SaslData SaslHandler::get_authorize_data(RequestContext &ctxt) {
  if (ctxt.request->get_method() == HttpMethod::Post) {
    auto &ib = ctxt.request->get_input_buffer();
    auto ib_len = ib.length();
    if (0 == ib_len) return {AuthenticationStateInvalid, "", false};
    std::string data = as_string(ib.pop_front(ib_len));
    auto [state_name, has_other_data] =
        helper::json::text_to_handler<JsonGetState>(data);
    auto state = get_authentication_state_impl(state_name);

    if (state == AuthenticationStateExchange && has_other_data) {
      state = AuthenticationStateInitialResponse;
    }
    return {state, data, true};
  }

  auto url = ctxt.get_http_url();
  bool has_auth_data = url.is_query_parameter(kParameterAuthData);
  auto state =
      get_authentication_state(url.get_query_elements(), has_auth_data);

  auto auth_data = helper::container::get_value_default(
      url.get_query_elements(), kParameterAuthData, "");
  auth_data = as_string(Base64Url::decode(auth_data));

  return {state, auth_data, false};
}

bool SaslHandler::authorize(RequestContext &ctxt, Session *session,
                            AuthUser *out_user) {
  log_debug("SaslHandler::authorize");
  auto session_data = session->get_data<SaslSessionData>();
  SaslResult response;

  if (!session_data) {
    log_debug("Creating session data");
    session_data = dynamic_cast<SaslSessionData *>(allocate_session_data());
    session->set_data(session_data);
  }

  auto [state, auth_data, is_json] = get_authorize_data(ctxt);

  session_data->sasl_state = state;

  switch (state) {
    case AuthenticationStateExchange:
      response =
          client_request_authentication_exchange(ctxt, session, out_user);
      break;
    case AuthenticationStateInitialResponse:
      response =
          client_initial_response(ctxt, session, out_user, auth_data, is_json);
      break;
    case AuthenticationStateResponse:
      response = client_response(ctxt, session, out_user, auth_data, is_json);
      break;
    case AuthenticationStateInvalid:
      throw http::Error(HttpStatusCode::BadRequest);
  }

  if (response.response_type == SaslResult::SaslHttpStatusCode)
    throw response.http_result;

  if (response.response_type == SaslResult::SaslOk) {
    session->state = Session::kUserVerified;
    *out_user = session->user;
    return true;
  }

  return false;
}

}  // namespace authentication
}  // namespace mrs
