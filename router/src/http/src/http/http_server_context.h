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

#ifndef MYSQLROUTER_SERVER_CONTEXT_INCLUDED
#define MYSQLROUTER_SERVER_CONTEXT_INCLUDED

#include <cassert>
#include <list>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#include "mysql/harness/net_ts/io_context.h"

#include "http/server/bind.h"
#include "http/server/server.h"
#include "http_request_router.h"
#include "mysqlrouter/http_server_lib_export.h"

namespace http {

class HTTP_SERVER_LIB_EXPORT HttpServerContext {
 public:
  using IoThreads = std::list<IoThread>;

 public:
  HttpServerContext(net::io_context *context, IoThreads *io_threads,
                    TlsServerContext &&tls_context, const std::string &host,
                    const uint16_t port);

  HttpServerContext(net::io_context *context, IoThreads *io_threads,
                    const std::string &host, const uint16_t port);

  void start();
  void stop();
  void join_all();

  void add_route(const std::string &url_regex,
                 std::unique_ptr<http::base::RequestHandler> cb);
  void remove_route(const std::string &url_regex);
  void remove_route(const void *handler_id);

  bool is_ssl_configured();

  HttpRequestRouter &request_router();

 private:
  net::io_context *context_;
  TlsServerContext tls_context_;
  std::string host_;
  uint16_t port_;
  bool ssl_{false};
  server::Bind bind{context_, host_, port_};
  server::Server http;

 private:
  HttpRequestRouter request_handler_;
};

}  // namespace http

#endif  // MYSQLROUTER_SERVER_CONTEXT_INCLUDED
