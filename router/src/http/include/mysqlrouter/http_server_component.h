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

#ifndef MYSQLROUTER_HTTP_SERVER_COMPONENT_INCLUDED
#define MYSQLROUTER_HTTP_SERVER_COMPONENT_INCLUDED

#include <mutex>

#include "mysqlrouter/http_common.h"
#include "mysqlrouter/http_server_export.h"

class HTTP_SERVER_EXPORT BaseRequestHandler {
 public:
  void call(HttpRequest &req, void *me) {
    auto *my = static_cast<BaseRequestHandler *>(me);
    my->handle_request(req);
  }

  virtual void handle_request(HttpRequest &req) = 0;

  virtual ~BaseRequestHandler();
};

class HTTP_SERVER_EXPORT HttpServerComponent {
 public:
  static HttpServerComponent &getInstance();
  void init(std::shared_ptr<HttpServer> srv);
  void add_route(const std::string &url_regex,
                 std::unique_ptr<BaseRequestHandler> cb);
  void remove_route(const std::string &url_regex);

 private:
  // disable copy, as we are a single-instance
  HttpServerComponent(HttpServerComponent const &) = delete;
  void operator=(HttpServerComponent const &) = delete;

  struct RouterData {
    std::string url_regex_str;
    std::unique_ptr<BaseRequestHandler> handler;
  };

  std::mutex rh_mu;  // request handler mutex
  std::vector<RouterData> request_handlers_;

  std::weak_ptr<HttpServer> srv_;

  HttpServerComponent() = default;
};

#endif
