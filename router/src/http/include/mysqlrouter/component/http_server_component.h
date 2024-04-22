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

#ifndef ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_COMPONENT_H_
#define ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_COMPONENT_H_

#include <memory>
#include <string>

#include "http/base/request_handler.h"
#include "mysqlrouter/http_server_lib_export.h"

#include "http/base/request.h"
#include "http/http_server_context.h"

class HTTP_SERVER_LIB_EXPORT HttpServerComponent {
 public:
  using HttpServerCtxtPtr = std::shared_ptr<http::HttpServerContext>;

 public:
  virtual ~HttpServerComponent() = default;

  static HttpServerComponent &get_instance();
  // Just for tests
  static void set_instance(std::unique_ptr<HttpServerComponent> component);

  virtual void init(HttpServerCtxtPtr srv) = 0;
  virtual void *add_route(const std::string &url_regex,
                          std::unique_ptr<http::base::RequestHandler> cb) = 0;
  virtual void remove_route(const std::string &url_regex) = 0;
  virtual void remove_route(const void *handler) = 0;

  virtual bool is_ssl_configured() = 0;
};

#endif  // ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_COMPONENT_H_
