/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <memory>
#include <mutex>
#include <string>

#include "http_server_plugin.h"
#include "mysqlrouter/http_server_component.h"

// must be declared in .cc file as otherwise each plugin
// gets its own class-instance of BaseRequestHandler which leads
// to undefined behaviour (ubsan -> vptr)
BaseRequestHandler::~BaseRequestHandler() = default;

//
// HTTP Server's public API
//
void HttpServerComponent::add_route(
    const std::string &url_regex, std::unique_ptr<BaseRequestHandler> handler) {
  std::lock_guard<std::mutex> lock(rh_mu);

  // if srv_ already points to the http_server forward the
  // route directly, otherwise add it to the delayed backlog
  if (auto srv = srv_.lock()) {
    srv->add_route(url_regex, std::move(handler));
  } else {
    request_handlers_.emplace_back(RouterData{url_regex, std::move(handler)});
  }
}

void HttpServerComponent::remove_route(const std::string &url_regex) {
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

void HttpServerComponent::init(std::shared_ptr<HttpServer> srv) {
  std::lock_guard<std::mutex> lock(rh_mu);

  srv_ = srv;

  for (auto &route : request_handlers_) {
    srv->add_route(route.url_regex_str, std::move(route.handler));
  }

  request_handlers_.clear();
}

HttpServerComponent &HttpServerComponent::getInstance() {
  static HttpServerComponent instance;

  return instance;
}
