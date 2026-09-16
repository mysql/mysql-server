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

#ifndef ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_COMPONENT_H_
#define ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_COMPONENT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "http/base/request_handler.h"
#include "mysqlrouter/http_server_lib_export.h"

#include "http/base/request.h"
#include "http/base/uri_path_matcher.h"
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
  virtual void *add_regex_route(
      const std::string &url_host, const std::string &url_regex,
      std::unique_ptr<http::base::RequestHandler> cb) = 0;
  virtual void *add_direct_match_route(
      const std::string &url_host, const ::http::base::UriPathMatcher &url_path,
      std::unique_ptr<http::base::RequestHandler> cb) = 0;
  virtual void remove_route(const void *handler) = 0;

  virtual bool is_ssl_configured() = 0;

  // Runtime overrides are intentionally not constrained by mysqlrouter.conf
  // option bounds. `std::nullopt` clears an override; otherwise the full
  // uint64_t range is accepted and callers that expose user/config input must
  // validate their own policy before calling these setters.
  virtual void set_max_http_connections(std::optional<uint64_t> value) = 0;
  virtual uint64_t get_effective_max_http_connections() = 0;
  virtual void set_max_request_body_size(std::optional<uint64_t> value) = 0;
  virtual uint64_t get_effective_max_request_body_size() = 0;
  virtual void set_max_response_body_size(std::optional<uint64_t> value) = 0;
  virtual uint64_t get_effective_max_response_body_size() = 0;
  virtual void clear_overrides() = 0;
};

#endif  // ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_COMPONENT_H_
