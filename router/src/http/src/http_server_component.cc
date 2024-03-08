/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

class HTTP_SERVER_EXPORT HttpServerComponentImpl : public HttpServerComponent {
 public:
  HttpServerComponentImpl() = default;

  void init(std::shared_ptr<http::HttpServerContext> srv) override;

  void *add_route(const std::string &url_regex,
                  std::unique_ptr<http::base::RequestHandler> cb) override;
  void remove_route(const std::string &url_regex) override;
  void remove_route(const void *handler) override;

  bool is_ssl_configured() override;

 private:
  // disable copy, as we are a single-instance
  HttpServerComponentImpl(HttpServerComponentImpl const &) = delete;
  void operator=(HttpServerComponent const &) = delete;

  struct RouterData {
    std::string url_regex_str;
    std::unique_ptr<http::base::RequestHandler> handler;
  };

  std::mutex rh_mu;  // request handler mutex
  std::vector<RouterData> request_handlers_;

  std::weak_ptr<http::HttpServerContext> srv_;
};

//
// HTTP Server's public API
//
void *HttpServerComponentImpl::add_route(
    const std::string &url_regex,
    std::unique_ptr<http::base::RequestHandler> handler) {
  std::lock_guard<std::mutex> lock(rh_mu);

  void *result_id = handler.get();
  // if srv_ already points to the http_server forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->add_route(url_regex, std::move(handler));
  } else {
    request_handlers_.emplace_back(RouterData{url_regex, std::move(handler)});
  }

  return result_id;
}

void HttpServerComponentImpl::remove_route(const std::string &url_regex) {
  std::lock_guard<std::mutex> lock(rh_mu);

  // if srv_ already points to the http_server forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->remove_route(url_regex);
  } else {
    for (auto it = request_handlers_.begin(); it != request_handlers_.end();) {
      if (it->url_regex_str == url_regex) {
        it = request_handlers_.erase(it);
      } else {
        it++;
      }
    }
  }
}

void HttpServerComponentImpl::remove_route(const void *handler) {
  std::lock_guard<std::mutex> lock(rh_mu);

  // if srv_ already points to the http_server forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->remove_route(handler);
  } else {
    for (auto it = request_handlers_.begin(); it != request_handlers_.end();) {
      if (it->handler.get() == handler) {
        request_handlers_.erase(it);
        break;
      }
    }
  }
}

void HttpServerComponentImpl::init(
    std::shared_ptr<http::HttpServerContext> srv) {
  std::lock_guard<std::mutex> lock(rh_mu);

  srv_ = srv;

  for (auto &route : request_handlers_) {
    srv->add_route(route.url_regex_str, std::move(route.handler));
  }

  request_handlers_.clear();
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
