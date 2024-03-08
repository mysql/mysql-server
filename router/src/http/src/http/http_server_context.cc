/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "http/http_server_context.h"

#include <memory>
#include <string>
#include <utility>

namespace http {

HttpServerContext::HttpServerContext(net::io_context *context,
                                     IoThreads *io_threads,
                                     TlsServerContext &&tls_context,
                                     const std::string &host,
                                     const uint16_t port)
    : context_{context},
      tls_context_{std::move(tls_context)},
      host_(host),
      port_(port),
      ssl_{true},
      http{&tls_context_, io_threads, ssl_ ? nullptr : &bind,
           ssl_ ? &bind : nullptr} {}

HttpServerContext::HttpServerContext(net::io_context *context,
                                     IoThreads *io_threads,
                                     const std::string &host,
                                     const uint16_t port)
    : context_{context},
      host_(host),
      port_(port),
      http{&tls_context_, io_threads, ssl_ ? nullptr : &bind,
           ssl_ ? &bind : nullptr} {}

void HttpServerContext::start() {
  http.set_allowed_methods({0xFFFFFFFF});
  http.set_request_handler(&request_handler_);
  http.start();
}

void HttpServerContext::stop() { http.stop(); }

void HttpServerContext::join_all() {}

void HttpServerContext::add_route(
    const std::string &url_regex,
    std::unique_ptr<http::base::RequestHandler> cb) {
  if (url_regex.empty()) {
    request_handler_.set_default_route(std::move(cb));
  } else {
    request_handler_.append(url_regex, std::move(cb));
  }
}

void HttpServerContext::remove_route(const std::string &url_regex) {
  if (url_regex.empty()) {
    request_handler_.clear_default_route();
  } else {
    request_handler_.remove(url_regex);
  }
}

void HttpServerContext::remove_route(const void *handler_id) {
  request_handler_.remove(handler_id);
}

bool HttpServerContext::is_ssl_configured() { return ssl_; }

HttpRequestRouter &HttpServerContext::request_router() {
  return request_handler_;
}

}  // namespace http
