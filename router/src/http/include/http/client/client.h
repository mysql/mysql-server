/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_CLIENT_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_CLIENT_H_

#include <list>
#include <memory>
#include <string>
#include <system_error>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)

#include "http/base/connection_interface.h"
#include "http/base/method.h"
#include "http/client/request.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/tls_client_context.h"

#include "mysqlrouter/http_client_export.h"

namespace http {
namespace client {

class HTTP_CLIENT_EXPORT Client {
 public:
  using io_context = net::io_context;
  using HttpMethodType = http::base::method::key_type;
  struct Endpoint {
    bool is_tls;
    uint16_t port;
    std::string host;
    bool operator==(const Endpoint &other) const {
      if (other.is_tls != is_tls) return false;
      if (other.port != port) return false;

      return (other.host == host);
    }
  };

  struct Statistics {
    uint64_t connected{0};
    uint64_t reused{0};
    uint64_t connected_tls{0};
  };

 public:
  Client(io_context &io_context, TlsClientContext &&tls_context);
  explicit Client(io_context &io_context);

  ~Client();

  void async_send_request(Request *request);
  void send_request(Request *request);

  operator bool() const;
  int error_code() const;
  std::string error_message() const;
  const Statistics &statistics() const;

 private:
  class CallbacksPrivateImpl;
  bool is_connected_{false};
  std::error_code error_code_;
  Endpoint connected_endpoint_;
  io_context &io_context_;
  TlsClientContext tls_context_;
  std::unique_ptr<http::base::ConnectionInterface> connection_;
  std::unique_ptr<CallbacksPrivateImpl> callbacks_;
  Request *fill_request_by_callback_{nullptr};
  Statistics statistics_;
};

}  // namespace client
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_CLIENT_H_
