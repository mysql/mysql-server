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

#include "mysqlrouter/rest_client.h"
#include "mysqlrouter/base64.h"

using Request = RestClient::Request;

static std::string format_address_for_url(const std::string &address) {
  using namespace std::string_literals;  // NOLINT(build/namespaces)
  if (address.empty()) return address;

  // Assume that the address is IPv6, that is already prepared for URL.
  if (*address.begin() == '[' && *address.rbegin() == ']') return address;

  // Check if its IPv6, if so prepare if for URL.
  if (std::all_of(address.begin(), address.end(),
                  [](char c) { return c == ':' || std::isxdigit(c); }))
    return "["s + address + "]";

  return address;
}

RestClient::RestClient(IOContext &io_ctx, const std::string &address,
                       uint16_t port, const std::string &username,
                       const std::string &password, const bool use_http2)
    : io_context_{io_ctx},
      http_client_{std::make_unique<http::client::Client>(io_ctx, use_http2)},
      use_http2_{use_http2} {
  uri_.set_port(port);
  uri_.set_host(format_address_for_url(address));

  if (!username.empty() || !password.empty()) {
    uri_.set_userinfo(make_userinfo(username, password));
  }
}

Request RestClient::request_sync(http::base::method::key_type method,
                                 const std::string &path,
                                 const std::string &request_body,
                                 const std::string &content_type) {
  http::base::Uri uri_path{path};
  // Check if 'path' parameter contains full URL.
  if (!uri_path.get_host().empty())
    return request_sync(method, uri_path, request_body, content_type);

  // User scheme, host, port from default URL.
  uri_.set_path(uri_path.get_path());
  uri_.set_query(uri_path.get_query());
  uri_.set_fragment(uri_path.get_fragment());

  return request_sync(method, uri_, request_body, content_type);
}

Request RestClient::request_sync(http::base::method::key_type method,
                                 const HttpUri &uri,
                                 const std::string &request_body,
                                 const std::string &content_type) {
  Request req{uri, method};

  io_context_.restart();

  // TRACE forbids a request-body
  if (!request_body.empty()) {
    if (method == http::base::method::Trace) {
      throw std::logic_error("TRACE can't have request-body");
    }
    req.get_output_headers().add("content-type", content_type.c_str());
    auto &out_buf = req.get_output_buffer();
    out_buf.add(request_body.data(), request_body.size());
  }

  auto ui = uri.get_userinfo();
  if (!ui.empty()) {
    // Correct the layout of data, from URI format to Header Basic format.
    if (ui.find(':') == std::string::npos) ui.append(":");

    req.get_output_headers().add(
        "authorization",
        ("Basic " + Base64::encode(std::vector<uint8_t>{ui.begin(), ui.end()}))
            .c_str());
  }

  // ask the server to close the connection after this request
  if (!use_http2_) {
    req.get_output_headers().add("connection", "close");
  }
  req.get_output_headers().add("host", uri.get_host().c_str());

  // tell the server that we would accept error-messages as problem+json
  req.get_output_headers().add("accept",
                               "application/problem+json, application/json");
  http_client_->send_request(&req);

  return req;
}
