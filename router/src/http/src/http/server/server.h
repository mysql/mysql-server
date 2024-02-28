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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_SERVER_SERVER_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_SERVER_SERVER_H_

#include <list>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <vector>

#include "helper/wait_variable.h"
#include "mysqlrouter/http_server_export.h"

#include "http/base/connection.h"
#include "http/base/connection_status_callbacks.h"
#include "http/base/method.h"
#include "http/server/bind.h"
#include "http/server/connection.h"
#include "http/server/request_handler_interface.h"
#include "tls/tls_stream.h"

class IoThread;

namespace http {
namespace server {

using Socket = net::ip::tcp::socket;
using TlsSocket = net::tls::TlsStream<Socket>;
using ServerConnectionRaw = ServerConnection<net::ip::tcp::socket>;
using ServerConnectionTls = ServerConnection<TlsSocket>;
using ConnectionRaw = ServerConnectionRaw::Parent;
using ConnectionTls = ServerConnectionTls::Parent;
using ConnectionStatusCallbacksRaw = ConnectionRaw::ConnectionStatusCallbacks;
using ConnectionStatusCallbacksTls = ConnectionTls::ConnectionStatusCallbacks;

class HTTP_SERVER_EXPORT Server : public ConnectionStatusCallbacksRaw,
                                  public ConnectionStatusCallbacksTls {
 public:
  using native_handle_type = net::impl::socket::native_handle_type;
  using io_context = net::io_context;
  using socket = net::ip::tcp::socket;
  using Methods = base::method::Bitset;
  using IoThreads = std::list<IoThread>;
  using IoIterator = IoThreads::iterator;

 public:
  Server(TlsServerContext *tls_context, IoThreads *threads, Bind *bind_raw,
         Bind *bind_ssl);

  void set_allowed_methods(const Methods &methods);
  void set_request_handler(RequestHandlerInterface *handler);

  void start();
  void stop();

 private:  // Ssl connections handling
  void on_new_ssl_connection(socket socket);
  void on_connection_close(ConnectionTls *connection) override;
  void on_connection_io_error(ConnectionTls *connection,
                              const std::error_code &ec) override;

 private:  // Raw connections handling
  void on_new_connection(socket socket);
  void on_connection_close(ConnectionRaw *connection) override;
  void on_connection_io_error(ConnectionRaw *connection,
                              const std::error_code &ec) override;

 private:
  enum class State { kInitializing, kRunning, kStopping, kStopped };

  size_t disconnect_all();

  void start_accepting();
  socket socket_move_to_io_thread(socket socket);
  IoThread *return_next_thread();
  TlsServerContext *tls_context_;
  std::list<IoThread> *threads_;
  IoIterator current_thread_;
  Bind *bind_raw_;
  Bind *bind_ssl_;
  base::method::Bitset allowed_methods_;
  RequestHandlerInterface *handler_ = nullptr;

  std::mutex mutex_connection_;
  std::vector<std::shared_ptr<ServerConnectionRaw>> connections_;
  std::vector<std::shared_ptr<ServerConnectionTls>> connections_ssl_;
  WaitableVariable<State> sync_state_{State::kInitializing};
};

}  // namespace server
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_SERVER_SERVER_H_
