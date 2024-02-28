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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_REQUEST_ROUTER_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_REQUEST_ROUTER_H_

#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "http/base/request.h"
#include "http/base/request_handler.h"
#include "http/server/request_handler_interface.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/stdx/monitor.h"

class HttpRequestRouter : public http::server::RequestHandlerInterface {
 public:
  using RequestHandler = http::base::RequestHandler;
  using BaseRequestHandlerPtr = std::shared_ptr<http::base::RequestHandler>;

 public:
  void append(const std::string &url_regex_str,
              std::unique_ptr<RequestHandler> cb);
  void remove(const void *handler_id);
  void remove(const std::string &url_regex_str);

  void set_default_route(std::unique_ptr<RequestHandler> cb);
  void clear_default_route();
  void route(http::base::Request &req) override;

  void require_realm(const std::string &realm) { require_realm_ = realm; }

 private:
  struct RouterData {
    std::string url_regex_str;
    std::regex url_regex;
    BaseRequestHandlerPtr handler;
  };

  // if no routes are specified, return 404
  void handler_not_found(http::base::Request &req);
  BaseRequestHandlerPtr find_route_handler(const std::string &path);

  std::vector<RouterData> request_handlers_;

  BaseRequestHandlerPtr default_route_;
  std::string require_realm_;

  std::mutex route_mtx_;
};

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_REQUEST_ROUTER_H_
