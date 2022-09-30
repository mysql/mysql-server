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

bool Oauth2Handler::can_process(HttpRequest *request) {
  return !request->get_input_headers().get(
      WwwAuthenticationHandler::kAuthorization);
}

void Oauth2Handler::mark_response(HttpRequest *) {}

uint64_t Oauth2Handler::get_service_id() const { return entry_.service_id; }

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
  if (!send_http_request(HttpMethod::Post, get_url_direct_auth(),
                         get_body_access_token_request(data),
                         get_request_handler_access_token(data).get())) {
    return false;
  }

  data->acquired_at = steady_clock::now();
  log_debug("acquired_access_token = %s", data->access_token.c_str());

  return true;
}

void Oauth2Handler::new_session_start_login(RequestContext *ctxt) {
  auto &request_uri = ctxt->request->get_uri();
  std::string uri = entry_.host + request_uri.get_path();

  if (request_uri.get_query().length()) {
    uri += "?" + request_uri.get_query();
  }

  auto data = sm_.new_session_data(ctxt->request);
  data->redirection = uri;

  // TODO(lkotula): escape get_id(), cookies doesn't support all characters.
  // For now its ok because ID is a number. (Shouldn't be in review)

  // Redirect the web-browser to `get_url_location` URL.
  http::redirect_and_throw(ctxt->request, get_url_location(ctxt));
}

bool Oauth2Handler::is_authorized(RequestContext *ctxt) {
  auto session = sm_.get_session_data(ctxt->request);
  log_debug("is_authorized session=%p, state=%i", session,
            session ? session->state : 0);

  if (!session) return false;
  if (session->state != GenericSessionData::kUserVerified) return false;

  ctxt->user.has_user_id = true;
  ctxt->user.user_id = session->user_id.value();
  log_debug("is_authorized session-user:%i", (int)session->user_id.value());

  if (!um_.user_get(&ctxt->user, &ctxt->sql_session_cache)) {
    ctxt->user.has_user_id = false;
    return false;
  }

  return true;
}

bool Oauth2Handler::unauthorize(RequestContext *ctxt) {
  sm_.remove_session_data(ctxt->request);
  return true;
}

bool Oauth2Handler::authorize(RequestContext *ctxt) {
  const static std::string kCode{"code"};
  const static std::string kState{"state"};
  const static std::string kError{"error"};
  const static std::string kToken{"token"};
  auto &request_uri = ctxt->request->get_uri();
  auto session = sm_.get_session_data(ctxt->request);
  http::Url::Parameaters query_parameters;
  http::Url::parse_query(request_uri.get_query().c_str(), &query_parameters);
  const bool token_in_parameters = 0 != query_parameters.count(kToken);
  const bool code_in_parameters = 0 != query_parameters.count(kCode);

  log_debug(
      "Oauth2FacebookHandler::authorize(id=%i, service_id=%i, session_id=%s) "
      "=> "
      "%s",
      static_cast<int>(entry_.id), static_cast<int>(entry_.service_id),
      session ? session->internal_session->get_id().c_str() : "null",
      helper::json::to_string(query_parameters).c_str());

  if (nullptr == session) {
    if (!token_in_parameters && !code_in_parameters) {
      log_debug("Session doesn't exists new-session");
      new_session_start_login(ctxt);
      return false;
    }

    session = sm_.new_session_data(ctxt->request);
    session->access_token = query_parameters[kToken];
    session->state = token_in_parameters ? GenericSessionData::kTokenVerified
                                         : GenericSessionData::kWaitingForCode;
  }

  if (session->state == GenericSessionData::kWaitingForCode &&
      token_in_parameters) {
    session->state = GenericSessionData::kTokenVerified;
  }
  switch (session->state) {
    case GenericSessionData::kWaitingForCode: {
      if (query_parameters.count(kError)) {
        sm_.remove_session_data(session);
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
        new_session_start_login(ctxt);
        return false;
      }

      session->auth_code = query_parameters[kCode];
      if (!http_acquire_access_token(session)) return false;

      session->state = GenericSessionData::kTokenVerified;
    }
      [[fallthrough]];
    case GenericSessionData::kTokenVerified:
      if (!http_verify_account(session, ctxt)) return false;
      session->state = GenericSessionData::kUserVerified;
      session->user_id = ctxt->user.user_id;

      return true;

    case GenericSessionData::kGettingTokken:
      break;

    case GenericSessionData::kUserVerified:
      ctxt->user.has_user_id = true;
      ctxt->user.user_id = session->user_id.value();
      return um_.user_get(&ctxt->user, &ctxt->sql_session_cache);
  }

  return false;
}

bool Oauth2Handler::http_verify_account(GenericSessionData *data,
                                        rest::RequestContext *ctxt) {
  std::string url = get_url_validation(data);

  log_debug("verify_user: %s", url.c_str());
  if (!send_http_request(
          HttpMethod::Get, url, {},
          get_request_handler_verify_account(data, ctxt).get())) {
    return false;
  }

  log_debug("user_id: %s", ctxt->user.vendor_user_id.c_str());
  ctxt->user.app_id = entry_.id;

  return um_.user_get(&ctxt->user, &ctxt->sql_session_cache);
}

const std::string &Oauth2Handler::get_host_alias() const {
  if (!entry_.host_alias.empty()) return entry_.host_alias;

  return entry_.host;
}

}  // namespace authentication
}  // namespace mrs
