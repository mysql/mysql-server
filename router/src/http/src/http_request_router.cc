/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysqlrouter/component/http_auth_realm_component.h"
#include "mysqlrouter/component/http_server_auth.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

using BaseRequestHandlerPtr = HttpRequestRouter::BaseRequestHandlerPtr;

bool HttpRequestRouter::RouteRegexMatcher::matches(
    std::string_view input) const {
  return matcher_->find(std::string(input));
}

/*static*/ HttpRequestRouter::RouteDirectMatcher::UrlPathKey
HttpRequestRouter::RouteDirectMatcher::path_key_from_matcher(
    const ::http::base::UriPathMatcher &url_path_matcher) {
  UrlPathKey result;

  http::base::Uri uri{url_path_matcher.path};
  for (const auto &e : uri.get_path_elements()) {
    result.path_elements.push_back(e);
  }

  if (url_path_matcher.allow_id_element) {
    result.path_elements.push_back(std::nullopt);
  }

  result.allow_trailing_slash = url_path_matcher.allow_trailing_slash;

  return result;
}

bool HttpRequestRouter::RouteDirectMatcher::UrlPathKey::operator<(
    const UrlPathKey &other) const {
  for (auto [pos, el] : stdx::views::enumerate(this->path_elements)) {
    if (pos < other.path_elements.size()) {
      auto &other_el = other.path_elements[pos];
      // std::nullopt means optional and match id
      if (!el || !other_el) continue;

      const auto comp_res = (el->compare(*other_el));
      if (comp_res != 0) {
        return comp_res < 0;
      }
    }
    // we have more elements than 'other'
    else {
      // std::nullopt means optional so it's a match
      if (!el) continue;
      // if there is trailing slash the last element will be empty
      if (el->empty() && other.allow_trailing_slash) continue;

      // matching elements are equal but we have more elements so we are not <
      return false;
    }
  }

  // if 'other' has more elements
  for (auto i = this->path_elements.size(); i < other.path_elements.size();
       i++) {
    // std::nullopt means optional so it's a match
    if (!other.path_elements[i]) continue;
    // if there is trailing slash the last element will be empty
    if (other.path_elements[i]->empty() && this->allow_trailing_slash) continue;

    // matching elements are equal but we have less elements so we are <
    return true;
  }

  return false;
}

/**
 * Request router
 *
 * send requests for a path of the URI to a handler callback
 *
 * if no handler is found, reply with 404 not found
 */
void HttpRequestRouter::register_regex_handler(
    const std::string &url_host, const std::string &url_regex_str,
    std::unique_ptr<http::base::RequestHandler> cb) {
  log_debug("adding route for regex: %s, url_host: '%s'", url_regex_str.c_str(),
            url_host.c_str());

  RouteRegexMatcher matcher(url_regex_str, std::move(cb));

  std::unique_lock lock(route_mtx_);
  auto req_it = request_regex_handlers_.find(url_host);
  if (req_it == request_regex_handlers_.end()) {
    std::vector<RouteRegexMatcher> route_matchers;
    route_matchers.emplace_back(std::move(matcher));

    request_regex_handlers_.emplace(url_host, std::move(route_matchers));
  } else {
    req_it->second.emplace_back(std::move(matcher));
  }
}

void HttpRequestRouter::register_direct_match_handler(
    const std::string &url_host,
    const ::http::base::UriPathMatcher &uri_path_matcher,
    std::unique_ptr<RequestHandler> cb) {
  log_debug("adding route for path: %s, url_host: '%s'",
            uri_path_matcher.path.c_str(), url_host.c_str());

  auto path_key = RouteDirectMatcher::path_key_from_matcher(uri_path_matcher);

  std::unique_lock lock(route_mtx_);
  auto host_it = request_direct_handlers_.find(url_host);
  if (host_it == request_direct_handlers_.end()) {
    std::map<RouteDirectMatcher::UrlPathKey, RouteDirectMatcher> route_matchers;
    RouteDirectMatcher matcher({uri_path_matcher, std::move(cb)});
    route_matchers.emplace(std::move(path_key), std::move(matcher));

    request_direct_handlers_.emplace(url_host, std::move(route_matchers));

    return;
  }

  auto &requests = host_it->second;
  auto req_it = requests.find(path_key);
  if (req_it == requests.end()) {
    RouteDirectMatcher matcher({uri_path_matcher, std::move(cb)});
    host_it->second.emplace(std::move(path_key), std::move(matcher));
    return;
  }

  if (uri_path_matcher.allow_id_element) {
    // A "wildcard" matcher is being added. Need to merge all the existing
    // non-wildcard ones that are matching the wildcard. For example if
    // matcher for the following path already exists: `/svc/db/ob/_metadata`
    // and the following matcher is being added: `/svc/db/ob/[id]`, they
    // need to be merged into one, with the second one at the end of the
    // list.
    std::remove_reference_t<decltype(req_it->second.handlers())> all_handlers;
    while (req_it != requests.end()) {
      for (auto &h : req_it->second.handlers()) {
        all_handlers.push_back(std::move(h));
      }
      requests.erase(req_it);
      req_it = requests.find(path_key);
    }

    RouteDirectMatcher matcher({uri_path_matcher, std::move(cb)});
    auto r = requests.emplace(std::move(path_key), std::move(matcher));
    for (auto &h : all_handlers) {
      r.first->second.add_handler(std::move(h));
    }

  } else {
    req_it->second.add_handler({uri_path_matcher, std::move(cb)});
  }
}

void HttpRequestRouter::unregister_handler(const void *handler_id) {
  std::unique_lock lock(route_mtx_);

  unregister_regex_handler(handler_id);
  unregister_direct_match_handler(handler_id);
}

void HttpRequestRouter::unregister_regex_handler(const void *handler_id) {
  for (auto map_it = request_regex_handlers_.begin();
       map_it != request_regex_handlers_.end();) {
    auto &[url_host, request_handlers] = *map_it;
    for (auto it = request_handlers.begin(); it != request_handlers.end();) {
      if (it->handler().get() == handler_id) {
        log_debug("removing route for regex: %s, url_host: '%s'",
                  it->url_pattern().c_str(), url_host.c_str());
        it = request_handlers.erase(it);
      } else {
        ++it;
      }
    }

    if (request_handlers.empty()) {
      // if there are no more request-handlers for a hostname, remove the entry
      // in the hostname map.
      map_it = request_regex_handlers_.erase(map_it);
    } else {
      ++map_it;
    }
  }
}

void HttpRequestRouter::unregister_direct_match_handler(
    const void *handler_id) {
  for (auto map_it = request_direct_handlers_.begin();
       map_it != request_direct_handlers_.end();) {
    auto &[url_host, request_handlers] = *map_it;
    for (auto it = request_handlers.begin(); it != request_handlers.end();) {
      if (it->second.has_handler(handler_id)) {
        log_debug("removing route for direct path: %s, url_host: '%s'",
                  it->second.get_handler_path(handler_id).c_str(),
                  url_host.c_str());
        if (it->second.remove_handler(handler_id) == 0) {
          // if it was a last handler for this path/key
          it = request_handlers.erase(it);
        } else {
          ++it;
        }
      } else {
        ++it;
      }
    }

    if (request_handlers.empty()) {
      // if there are no more request-handlers for a hostname, remove the entry
      // in the hostname map.
      map_it = request_direct_handlers_.erase(map_it);
    } else {
      ++map_it;
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
  std::unique_lock lock(route_mtx_);

  default_route_ = std::move(cb);
}

void HttpRequestRouter::clear_default_route() {
  log_debug("removing default route");
  std::unique_lock lock(route_mtx_);

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

  std::string url_host;
  auto hdr_host = req.get_input_headers().find(":authority");
  if (hdr_host) {
    url_host = *hdr_host;
  }

  auto handler = find_route_handler(url_host, uri.get_path());

  if (handler) {
    handler->handle_request(req);
    return;
  }

  handler_not_found(req);
}

namespace {

// If the argument sent as a parameter is in "<hostname>:<port>" format
// (matches "^(.*):[0-9]+$" regex) returns <hostname> part, otherwise returns
// std::nullopt
std::optional<std::string_view> get_host_if_host_and_port(
    const std::string_view &url_host) {
  auto last_colon = url_host.find_last_of(':');

  // no colon or colon at the end
  if (last_colon == std::string_view::npos ||
      last_colon == url_host.size() - 1) {
    return std::nullopt;
  }

  // some non-digit after the colon
  if (std::find_if(url_host.begin() + last_colon + 1, url_host.end(),
                   [](const auto &c) { return !std::isdigit(c); }) !=
      url_host.end()) {
    return std::nullopt;
  }

  return url_host.substr(0, last_colon);
}

}  // namespace

BaseRequestHandlerPtr HttpRequestRouter::find_route_handler(
    std::string_view url_host, std::string_view path) {
  {
    const auto handler_ptr = find_direct_match_route_handler(url_host, path);
    if (handler_ptr) return handler_ptr;
  }

  {
    const auto handler_ptr = find_regex_route_handler(url_host, path);
    if (handler_ptr) return handler_ptr;
  }

  return default_route_;
}

BaseRequestHandlerPtr HttpRequestRouter::find_direct_match_route_handler(
    std::string_view url_host, std::string_view path) {
  const auto path_key = RouteDirectMatcher::path_key_from_matcher(
      {std::string(path), false, false});

  std::shared_lock lock(route_mtx_);

  if (!url_host.empty()) {
    // Check if we have a handler with a hostname that exactly matches the one
    // from the request
    auto req_it = request_direct_handlers_.find(url_host);
    // No exact match. Check if the url_host in the request is in the
    // <hostname>:<port> format. If that is the case we still accept it if the
    // <hostname> part matches the handler.
    if (req_it == request_direct_handlers_.end()) {
      auto hostname = get_host_if_host_and_port(url_host);
      if (hostname) {
        // Currently the SDK's "CREATE SERVICE" command does not support ipv6 so
        // it is good enough to do the exact match of the host part here. If
        // that ever changes we need something smarter to also match the names
        // with and without enclosing []'s
        req_it = request_direct_handlers_.find(*hostname);
      }
    }

    if (req_it != request_direct_handlers_.end()) {
      auto &request_handlers = req_it->second;
      const auto &handler = request_handlers.find(path_key);

      if (handler != request_handlers.end()) {
        return handler->second.handler(path);
      }
    }
  }

  // no handler with matching host found, try the one with empty host
  auto req_it = request_direct_handlers_.find("");
  if (req_it != request_direct_handlers_.end()) {
    auto &request_handlers = req_it->second;

    const auto &handler = request_handlers.find(path_key);

    if (handler != request_handlers.end()) {
      return handler->second.handler(path);
    }
  }

  return nullptr;
}

BaseRequestHandlerPtr HttpRequestRouter::find_regex_route_handler(
    std::string_view url_host, std::string_view path) {
  std::shared_lock lock(route_mtx_);

  if (!url_host.empty()) {
    // Check if we have a handler with a hostname that exactly matches the one
    // from the request
    auto req_it = request_regex_handlers_.find(url_host);
    // No exact match. Check if the url_host in the request is in the
    // <hostname>:<port> format. If that is the case we still accept it if the
    // <hostname> part matches the handler.
    if (req_it == request_regex_handlers_.end()) {
      auto hostname = get_host_if_host_and_port(url_host);
      if (hostname) {
        // Currently the SDK's "CREATE SERVICE" command does not support ipv6 so
        // it is good enough to do the exact match of the host part here. If
        // that ever changes we need something smarter to also match the names
        // with and without enclosing []'s
        req_it = request_regex_handlers_.find(*hostname);
      }
    }

    if (req_it != request_regex_handlers_.end()) {
      auto &request_handlers = req_it->second;

      for (auto &request_handler : request_handlers) {
        if (request_handler.matches(path)) {
          return request_handler.handler();
        }
      }
    }
  }

  // no handler with matching host found, try the one with empty host
  auto req_it = request_regex_handlers_.find("");
  if (req_it != request_regex_handlers_.end()) {
    auto &request_handlers = req_it->second;

    for (auto &request_handler : request_handlers) {
      if (request_handler.matches(path)) {
        return request_handler.handler();
      }
    }
  }

  return nullptr;
}

BaseRequestHandlerPtr HttpRequestRouter::RouteDirectMatcher::handler(
    std::string_view path) const {
  for (auto &h : handlers_) {
    if (h.path_matcher.allow_id_element) return h.handler;
    if (h.path_matcher.path == path) return h.handler;
    if (h.path_matcher.allow_trailing_slash &&
        ((h.path_matcher.path + "/") == path))
      return h.handler;
  }
  return BaseRequestHandlerPtr{};
}

bool HttpRequestRouter::RouteDirectMatcher::has_handler(
    const void *handler_id) const {
  for (auto &h : handlers_) {
    if (h.handler.get() == handler_id) {
      return true;
    }
  }

  return false;
}

std::string HttpRequestRouter::RouteDirectMatcher::get_handler_path(
    const void *handler_id) const {
  for (auto &h : handlers_) {
    if (h.handler.get() == handler_id) {
      return h.path_matcher.str();
    }
  }

  return "";
}

size_t HttpRequestRouter::RouteDirectMatcher::remove_handler(
    const void *handler_id) {
  for (auto it = handlers_.begin(); it != handlers_.end();) {
    if (it->handler.get() == handler_id) {
      it = handlers_.erase(it);
    } else {
      ++it;
    }
  }

  return handlers_.size();
}

void HttpRequestRouter::RouteDirectMatcher::add_handler(
    const PathHandler &path_handler) {
  // a matcher with allow_id_element has to be at the end
  if (path_handler.path_matcher.allow_id_element) {
    handlers_.push_back(path_handler);
  } else {
    auto it = std::find_if(handlers_.begin(), handlers_.end(), [](auto &h) {
      return h.path_matcher.allow_id_element;
    });

    handlers_.insert(it, path_handler);
  }
}