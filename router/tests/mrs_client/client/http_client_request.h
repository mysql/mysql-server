/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_REQUEST_H_
#define ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mysql/harness/net_ts/io_context.h"
#include "mysqlrouter/http_client.h"
#include "tls/tls_keylog_dumper.h"

#include "client/session.h"
#include "configuration/request.h"

namespace mrs_client {

using Headers = std::vector<std::pair<std::string, std::string>>;

inline std::optional<std::string> try_find_in_headers(const Headers &h,
                                                      const std::string &key) {
  for (const auto &[k, v] : h) {
    if (k == key) {
      return v;
    }
  }

  return {};
}

inline std::string find_in_headers(const Headers &h, const std::string &key) {
  auto result = try_find_in_headers(h, key);

  return result.has_value() ? result.value() : std::string{};
}

struct Result {
  HttpStatusCode::key_type status;
  Headers headers;
  std::string body;
  bool ok{false};
};

class HttpClientRequest {
 public:
  /**
   * Object that manages HTTP/HTTPS connection..
   *
   * The `url` parameter describes: scheme, host, port, other parts of
   * the URL are ignored.
   */
  HttpClientRequest(net::io_context *conext, HttpClientSession *session,
                    const http::base::Uri &uri, const bool use_http2);

  void add_header(const char *name, const char *value);
  const Headers &get_input_headers() const;
  Result do_request(http::base::method::key_type type, const std::string &path,
                    const std::string &body, bool set_new_cookies = true);

  HttpClientSession *get_session() { return session_; }

 private:
  http::base::Uri uri_;
  net::io_context *context_;
  std::unique_ptr<tls::TlsKeylogDumper> key_dump_;
  HttpClientSession *session_{};
  std::unique_ptr<http::client::Client> client_;
  std::string response_;
  Headers one_shot_headers_;
  Headers input_headers_;
};

}  // namespace mrs_client

#endif  // ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_REQUEST_H_
