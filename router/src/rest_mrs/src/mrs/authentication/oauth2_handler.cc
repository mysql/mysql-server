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

#include "mrs/authentication/oauth2_handler.h"

#include <stdexcept>

#include <my_rapidjson_size_t.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

#include <helper/json/text_to.h>
#include "helper/container/map.h"
#include "helper/json/rapid_json_to_map.h"
#include "helper/json/to_string.h"
#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/http/error.h"
#include "mrs/http/url.h"
#include "mrs/http/utilities.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/http_client.h"

// TODO(lkotula): Remove below. Temporary used to get following macro
// EVENT__HAVE_OPENSSL (Shouldn't be in review)
#include <event2/event.h>

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using GenericSessionData = Oauth2Handler::GenericSessionData;
using AuthApp = mrs::database::entry::AuthApp;

void Oauth2Handler::RequestHandlerJsonSimpleObject::before_send(HttpRequest *) {
}

bool Oauth2Handler::RequestHandlerJsonSimpleObject::response(
    const std::vector<uint8_t> &value) {
  using HandlerMapOfSimpleValues =
      helper::json::RapidReaderHandlerToMapOfSimpleValues;

  auto result = helper::json::text_to_handler<HandlerMapOfSimpleValues>(value);

  for (auto &e : output_) {
    auto &key = e.first;
    auto &out_value = e.second;
    if (!helper::container::get_value_other(result, key, out_value)) {
      log_debug("Getting key:'%s' from container failed.", key);
      return false;
    }
  }

  return true;
}

uint64_t Oauth2Handler::get_service_id() const { return entry_.service_id; }

const AuthApp &Oauth2Handler::get_entry() const { return entry_; }

uint64_t Oauth2Handler::get_id() const { return entry_.id; }

bool Oauth2Handler::send_http_request(HttpMethodType method,
                                      const std::string &url,
                                      const std::string &body,
                                      RequestHandler *request_handler) {
  IOContext io_ctx;
  TlsClientContext tls_ctx{TlsVerify::NONE};
  std::unique_ptr<HttpClient> http_client;
  HttpUri u{HttpUri::parse(url)};

  if (u.get_port() == 65535u) {
    if (u.get_scheme() == "http") {
      u.set_port(80);
    } else if (u.get_scheme() == "https") {
      u.set_port(443);
    }
  }

  if (u.get_scheme() == "https") {
    // TODO(lkotula): Configure CA file, CA dir, etc (Shouldn't be in review)
#ifdef EVENT__HAVE_OPENSSL
    http_client = std::make_unique<HttpsClient>(io_ctx, std::move(tls_ctx),
                                                u.get_host(), u.get_port());
#else
    throw std::runtime_error("HTTPS support disabled at buildtime");
#endif
  } else {
    http_client =
        std::make_unique<HttpClient>(io_ctx, u.get_host(), u.get_port());
  }

  log_debug("Oauth request:%s", url.c_str());
  log_debug(" - body:%s", body.c_str());

  HttpRequestImpl req{HttpRequestImpl::sync_callback, nullptr};
  auto &output_headers = req.get_output_headers();
  auto &output_buffer = req.get_output_buffer();

  output_headers.add("Connection", "close");
  output_headers.add("Host", http_client->hostname().c_str());

  if (method == HttpMethod::Post && !body.empty()) {
    output_headers.add("Content-Type", "application/x-www-form-urlencoded");
  }

  if (!body.empty()) output_buffer.add(body.c_str(), body.length());

  auto request_path = get_request_path(u);
  log_debug("Request: %s", request_path.c_str());

  if (request_handler) request_handler->before_send(&req);
  http_client->make_request_sync(&req, method, request_path);

  if (0 != req.error_code()) {
    log_debug("etap2 error_code=%i", (int)req.error_code());
    return false;
  }

  if (req.get_response_code() != HttpStatusCode::Ok) {
    log_debug("etap2 response_code=%i", (int)req.get_response_code());
    return false;
  }

  auto &buffer = req.get_input_buffer();
  auto response_data = buffer.pop_front(buffer.length());

  if (request_handler) return request_handler->response(response_data);

  return true;
}

std::string Oauth2Handler::get_request_path(HttpUri &u) {
  std::string result = u.get_path();
  auto q = u.get_query();
  auto f = u.get_fragment();

  if (q.length()) {
    result += "?" + q;
  }

  if (f.length()) {
    result += "#" + f;
  }

  return result;
}

bool Oauth2Handler::http_acquire_access_token(GenericSessionData *data) {
  log_debug("oauth2: redirection=%s", data->redirection.c_str());
  if (!send_http_request(HttpMethod::Post, get_url_direct_auth(),
                         get_body_access_token_request(data),
                         get_request_handler_access_token(data).get())) {
    return false;
  }

  data->acquired_at = steady_clock::now();
  log_debug("acquired_access_token = %s", data->access_token.c_str());

  return true;
}

void Oauth2Handler::new_session_start_login(Session *session, http::Url *url) {
  std::string uri = entry_.host + url->get_path();

  if (url->get_query().length()) {
    uri += "?" + url->get_query();
  }

  auto data = new GenericSessionData();
  session->set_data(data);
  data->redirection = uri;
  log_debug("Oauth2Handler new SessionData: redirection=%s",
            data->redirection.c_str());

  // TODO(lkotula): escape get_id(), cookies doesn't support all characters.
  // For now its ok because ID is a number. (Shouldn't be in review)

  // Redirect the web-browser to `get_url_location` URL.
  throw http::ErrorRedirect(get_url_location(data, url));
}

bool Oauth2Handler::is_authorized(Session *session, AuthUser *user) {
  log_debug("is_authorized session=%p, state=%i", session, session->state);

  if (session->state != Session::kUserVerified) return false;

  *user = session->user;
  log_debug("is_authorized session-user:%i", (int)user->user_id);

  return true;
}

bool Oauth2Handler::authorize(Session *session, http::Url *url,
                              SqlSessionCached *sql_session, HttpHeaders &,
                              AuthUser *out_user) {
  const static std::string kCode{"code"};
  const static std::string kState{"state"};
  const static std::string kError{"error"};
  const static std::string kToken{"token"};
  auto session_data = session->get_data<GenericSessionData>();

  http::Url::Parameaters &query_parameters = url->parameters_;
  const bool token_in_parameters = 0 != query_parameters.count(kToken);
  const bool code_in_parameters = 0 != query_parameters.count(kCode);

  log_debug(
      "Oauth2FacebookHandler::authorize(id=%i, service_id=%i, session_id=%s) "
      "=> "
      "%s",
      static_cast<int>(entry_.id), static_cast<int>(entry_.service_id),
      session_data ? session_data->internal_session->get_session_id().c_str()
                   : "null",
      helper::json::to_string(query_parameters).c_str());

  if (nullptr == session_data) {
    if (!token_in_parameters && !code_in_parameters) {
      log_debug("SessionData doesn't exist in new-session");
      new_session_start_login(session, url);
      return false;
    }

    session_data = new GenericSessionData();
    session->set_data(session_data);
    session_data->access_token = query_parameters[kToken];
    session->state = token_in_parameters ? Session::kTokenVerified
                                         : Session::kWaitingForCode;
  }

  if (session->state == Session::kWaitingForCode && token_in_parameters) {
    session->state = Session::kTokenVerified;
  }
  switch (session->state) {
    case Session::kUninitialized:
    case Session::kWaitingForCode: {
      if (query_parameters.count(kError)) {
        log_debug("Remote side returned and error.");
        // TODO(lkotula): forward the error ? (Shouldn't be in review)
        return false;
      }

      if (!query_parameters.count(kCode) || !query_parameters.count(kState)) {
        log_debug(
            "Remote side didn't return the code and state. Creating new "
            "session, and redirecting.");
        // TODO(lkotula): Limit somehow the number of retries (Shouldn't be in
        // review)
        new_session_start_login(session, url);
        return false;
      }

      session_data->auth_code = query_parameters[kCode];
      if (!http_acquire_access_token(session_data)) return false;

      session->state = Session::kTokenVerified;
    }
      [[fallthrough]];
    case Session::kTokenVerified:
      if (!http_verify_account(session, session_data, sql_session))
        return false;
      session->state = Session::kUserVerified;

      return true;

    case Session::kGettingTokken:
      break;

    case Session::kUserVerified:
      *out_user = session->user;
      return true;
  }

  return false;
}

bool Oauth2Handler::http_verify_account(Session *session,
                                        GenericSessionData *data,
                                        SqlSessionCached *sql_session) {
  std::string url = get_url_validation(data);

  log_debug("verify_user: %s", url.c_str());
  log_debug("oauth2: redirection=%s", data->redirection.c_str());
  if (!send_http_request(
          HttpMethod::Get, url, {},
          get_request_handler_verify_account(session, data).get())) {
    return false;
  }

  log_debug("user_id: %s", session->user.vendor_user_id.c_str());
  session->user.app_id = entry_.id;

  return um_.user_get(&session->user, sql_session);
}

const std::string &Oauth2Handler::get_host_alias() const {
  if (!entry_.host_alias.empty()) return entry_.host_alias;

  return entry_.host;
}

}  // namespace authentication
}  // namespace mrs
