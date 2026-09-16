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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_HTTP_SERVER_COMPONENT_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_HTTP_SERVER_COMPONENT_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "mysqlrouter/component/http_server_component.h"

class MockHttpServerComponent : public HttpServerComponent {
 public:
  MOCK_METHOD(void, init, (HttpServerCtxtPtr srv), (override));
  MOCK_METHOD(void *, add_regex_route,
              (const std::string &url_host, const std::string &url_regex,
               std::unique_ptr<http::base::RequestHandler> cb),
              (override));
  MOCK_METHOD(void *, add_direct_match_route,
              (const std::string &url_host,
               const ::http::base::UriPathMatcher &url_path,
               std::unique_ptr<http::base::RequestHandler> cb),
              (override));
  MOCK_METHOD(void, remove_route, (const void *handler), (override));
  MOCK_METHOD(bool, is_ssl_configured, (), (override));
  MOCK_METHOD(void, set_max_http_connections, (std::optional<uint64_t> value),
              (override));
  MOCK_METHOD(uint64_t, get_effective_max_http_connections, (), (override));
  MOCK_METHOD(void, set_max_request_body_size, (std::optional<uint64_t> value),
              (override));
  MOCK_METHOD(uint64_t, get_effective_max_request_body_size, (), (override));
  MOCK_METHOD(void, set_max_response_body_size, (std::optional<uint64_t> value),
              (override));
  MOCK_METHOD(uint64_t, get_effective_max_response_body_size, (), (override));
  MOCK_METHOD(void, clear_overrides, (), (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_HTTP_SERVER_COMPONENT_H_
