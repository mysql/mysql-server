/*
  Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "http/http_server_context.h"
#include "mysqlrouter/component/http_server_component.h"

namespace impl {

class HTTP_SERVER_LIB_EXPORT HttpServerComponentImpl
    : public HttpServerComponent {
 public:
  HttpServerComponentImpl() = default;

  void init(std::shared_ptr<http::HttpServerContext> srv) override;

  void *add_regex_route(
      const std::string &url_host, const std::string &url_regex,
      std::unique_ptr<http::base::RequestHandler> cb) override;
  void *add_direct_match_route(
      const std::string &url_host, const ::http::base::UriPathMatcher &url_path,
      std::unique_ptr<http::base::RequestHandler> cb) override;
  void remove_route(const void *handler) override;

  bool is_ssl_configured() override;

 private:
  // disable copy, as we are a single-instance
  HttpServerComponentImpl(HttpServerComponentImpl const &) = delete;
  void operator=(HttpServerComponent const &) = delete;

  struct RouteData {
    std::string url_host;
    std::unique_ptr<http::base::RequestHandler> handler;
  };

  struct RegexRouteData : public RouteData {
    std::string url_regex_str;
  };

  struct DirectMatchRouteData : public RouteData {
    ::http::base::UriPathMatcher url_path_matcher;
  };

  std::mutex rh_mu;  // request handler mutex
  std::vector<RegexRouteData> regex_request_handlers_;
  std::vector<DirectMatchRouteData> direct_match_request_handlers_;

  std::weak_ptr<http::HttpServerContext> srv_;
};

//
// HTTP Server's public API
//
void *HttpServerComponentImpl::add_regex_route(
    const std::string &url_host, const std::string &url_regex,
    std::unique_ptr<http::base::RequestHandler> handler) {
  std::lock_guard<std::mutex> lock(rh_mu);

  void *result_id = handler.get();
  // if srv_ already points to the http_server forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->add_regex_route(url_host, url_regex, std::move(handler));
  } else {
    regex_request_handlers_.emplace_back(
        RegexRouteData{{url_host, std::move(handler)}, url_regex});
  }

  return result_id;
}

void *HttpServerComponentImpl::add_direct_match_route(
    const std::string &url_host, const ::http::base::UriPathMatcher &url_path,
    std::unique_ptr<http::base::RequestHandler> cb) {
  std::lock_guard<std::mutex> lock(rh_mu);

  void *result_id = cb.get();
  // if srv_ already points to the http_server forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->add_direct_match_route(url_host, url_path, std::move(cb));
  } else {
    direct_match_request_handlers_.emplace_back(
        DirectMatchRouteData{{url_host, std::move(cb)}, url_path});
  }

  return result_id;
}

void HttpServerComponentImpl::remove_route(const void *handler) {
  std::lock_guard<std::mutex> lock(rh_mu);

  // if srv_ already points to the http_server forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->remove_route(handler);
  } else {
    for (auto it = regex_request_handlers_.begin();
         it != regex_request_handlers_.end(); ++it) {
      if (it->handler.get() == handler) {
        regex_request_handlers_.erase(it);
        return;
      }
    }

    for (auto it = direct_match_request_handlers_.begin();
         it != direct_match_request_handlers_.end(); ++it) {
      if (it->handler.get() == handler) {
        // direct_match_request_handlers_.erase(it);
        return;
      }
    }
  }
}

void HttpServerComponentImpl::init(
    std::shared_ptr<http::HttpServerContext> srv) {
  std::lock_guard<std::mutex> lock(rh_mu);

  srv_ = srv;

  for (auto &route : regex_request_handlers_) {
    srv->add_regex_route(route.url_host, route.url_regex_str,
                         std::move(route.handler));
  }

  regex_request_handlers_.clear();

  for (auto &route : direct_match_request_handlers_) {
    srv->add_direct_match_route(route.url_host, route.url_path_matcher,
                                std::move(route.handler));
  }

  direct_match_request_handlers_.clear();
}

bool HttpServerComponentImpl::is_ssl_configured() {
  std::lock_guard<std::mutex> lock(rh_mu);
  if (auto srv = srv_.lock()) {
    return srv->is_ssl_configured();
  }

  return false;
}

}  // namespace impl

std::unique_ptr<HttpServerComponent> g_http_server_custom_component;

HttpServerComponent &HttpServerComponent::get_instance() {
  if (g_http_server_custom_component) return *g_http_server_custom_component;

  static impl::HttpServerComponentImpl instance;

  return instance;
}

void HttpServerComponent::set_instance(
    std::unique_ptr<HttpServerComponent> component) {
  g_http_server_custom_component = std::move(component);
}
