/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "http_request_router.h"

#include <regex>
#include <string>
#include <utility>

#include "mysqlrouter/component/http_auth_realm_component.h"
#include "mysqlrouter/component/http_server_auth.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

using BaseRequestHandlerPtr = HttpRequestRouter::BaseRequestHandlerPtr;

/**
 * Request router
 *
 * send requests for a path of the URI to a handler callback
 *
 * if no handler is found, reply with 404 not found
 */
void HttpRequestRouter::append(const std::string &url_regex_str,
                               std::unique_ptr<http::base::RequestHandler> cb) {
  log_debug("adding route for regex: %s", url_regex_str.c_str());
  std::lock_guard<std::mutex> lock(route_mtx_);
  request_handlers_.emplace_back(RouterData{
      url_regex_str, std::regex{url_regex_str, std::regex_constants::extended},
      std::move(cb)});
}

void HttpRequestRouter::remove(const void *handler_id) {
  std::lock_guard<std::mutex> lock(route_mtx_);

  for (auto it = request_handlers_.begin(); it != request_handlers_.end();) {
    if (it->handler.get() == handler_id) {
      log_debug("removing route for regex: %s", it->url_regex_str.c_str());
      it = request_handlers_.erase(it);
    } else {
      ++it;
    }
  }
}

void HttpRequestRouter::remove(const std::string &url_regex_str) {
  log_debug("removing route for regex: %s", url_regex_str.c_str());
  std::lock_guard<std::mutex> lock(route_mtx_);
  for (auto it = request_handlers_.begin(); it != request_handlers_.end();) {
    if (it->url_regex_str == url_regex_str) {
      it = request_handlers_.erase(it);
    } else {
      it++;
    }
  }
}

// if no routes are specified, return 404
void HttpRequestRouter::handler_not_found(http::base::Request &req) {
  if (!require_realm_.empty()) {
    if (auto realm =
            HttpAuthRealmComponent::get_instance().get(require_realm_)) {
      if (HttpAuth::require_auth(req, realm)) {
        // request is already handled, nothing to do
        return;
      }

      // access granted, fall through
    }
  }
  req.send_error(HttpStatusCode::NotFound);
}

void HttpRequestRouter::set_default_route(
    std::unique_ptr<http::base::RequestHandler> cb) {
  log_debug("adding default route");
  std::lock_guard<std::mutex> lock(route_mtx_);
  default_route_ = std::move(cb);
}

void HttpRequestRouter::clear_default_route() {
  log_debug("removing default route");
  std::lock_guard<std::mutex> lock(route_mtx_);
  default_route_ = nullptr;
}

void HttpRequestRouter::route(http::base::Request &req) {
  const auto &uri = req.get_uri();

  // CONNECT can't be routed to the request handlers as it doesn't have a "path"
  // part.
  //
  // If the client Accepts "application/problem+json", send it a RFC7807 error
  // otherwise a classic text/html one.
  if (req.get_method() == HttpMethod::Connect) {
    auto hdr_accept = req.get_input_headers().find("Accept");
    if (hdr_accept &&
        hdr_accept->find("application/problem+json") != std::string::npos) {
      req.get_output_headers().add("Content-Type", "application/problem+json");
      std::string json_problem(R"({
  "title": "Method Not Allowed",
  "status": 405
})");
      int status_code = HttpStatusCode::MethodNotAllowed;
      req.send_reply(status_code,
                     HttpStatusCode::get_default_status_text(status_code),
                     json_problem);
    } else {
      req.send_error(HttpStatusCode::MethodNotAllowed);
    }
    return;
  }

  auto handler = find_route_handler(uri.get_path());

  if (handler) {
    handler->handle_request(req);
    return;
  }

  handler_not_found(req);
}

BaseRequestHandlerPtr HttpRequestRouter::find_route_handler(
    const std::string &path) {
  std::lock_guard<std::mutex> lock(route_mtx_);

  for (auto &request_handler : request_handlers_) {
    if (std::regex_search(path, request_handler.url_regex)) {
      return request_handler.handler;
    }
  }

  return default_route_;
}
