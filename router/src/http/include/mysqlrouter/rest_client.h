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
#ifndef MYSQL_ROUTER_REST_CLIENT_H_INCLUDED
#define MYSQL_ROUTER_REST_CLIENT_H_INCLUDED

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "http/base/method.h"
#include "http/base/uri.h"
#include "http/client/client.h"
#include "http/client/request.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/tls_client_context.h"

#include "mysqlrouter/http_client_export.h"

constexpr const char kRestAPIVersion[] = "20190715";

using IOContext = net::io_context;

class HTTP_CLIENT_EXPORT RestClient {
 public:
  using Request = http::client::Request;
  using HttpUri = http::base::Uri;

 public:
  RestClient(IOContext &io_ctx, const std::string &default_host,
             uint16_t default_port, const std::string &default_username = {},
             const std::string &default_password = {},
             const bool use_http2 = false);

  // The path, query, fragment parts of the URI, are ignored
  // (overwritten when specifying the request).
  RestClient(IOContext &io_ctx,
             const HttpUri &default_uri = HttpUri{"http://127.0.0.1"},
             const bool use_http2 = false)
      : io_context_{io_ctx},
        uri_{default_uri},
        http_client_{std::make_unique<http::client::Client>(io_ctx, use_http2)},
        use_http2_{use_http2} {}

  RestClient(IOContext &io_ctx, TlsClientContext &&tls_context,
             const HttpUri &default_uri = HttpUri{"http://127.0.0.1"},
             const bool use_http2 = false)
      : io_context_{io_ctx},
        uri_{default_uri},
        http_client_{std::make_unique<http::client::Client>(
            io_ctx, std::move(tls_context), use_http2)},
        use_http2_{use_http2} {}

  // Request might be send to different host than the default one.
  // 'uri' parameter overrides default uri settings.
  Request request_sync(http::base::method::key_type method, const HttpUri &uri,
                       const std::string &request_body = {},
                       const std::string &content_type = "application/json");

  // Use default host, for this request.
  Request request_sync(http::base::method::key_type method,
                       const std::string &path,
                       const std::string &request_body = {},
                       const std::string &content_type = "application/json");

  operator bool() const { return http_client_->operator bool(); }

  std::string error_msg() const { return http_client_->error_message(); }

 private:
  static std::string make_userinfo(const std::string &user,
                                   const std::string &password) {
    if (password.empty()) return user;

    std::string result = user;

    result += ':';
    result += password;
    return result;
  }

  net::io_context &io_context_;
  HttpUri uri_{"/"};
  std::unique_ptr<http::client::Client> http_client_;
  bool use_http2_;
};

#endif  // MYSQL_ROUTER_REST_CLIENT_H_INCLUDED
