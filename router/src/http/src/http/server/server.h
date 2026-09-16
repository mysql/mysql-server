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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_SERVER_SERVER_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_SERVER_SERVER_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <optional>
#include <string_view>
#include <vector>

#include "mysql/harness/utility/wait_variable.h"
#include "mysqlrouter/http_server_lib_export.h"

#include "http/base/connection.h"
#include "http/base/connection_status_callbacks.h"
#include "http/base/method.h"
#include "http/server/bind.h"
#include "http/server/connection.h"
#include "http/server/request_handler_interface.h"
#include "mysql/harness/net_ts/timer.h"
#include "tls/tls_stream.h"

class IoThread;

namespace http {
namespace server {

constexpr uint64_t kDefaultMaxHttpConnections{1024};
constexpr uint64_t kDefaultMaxRequestBodySize{16 * 1024 * 1024};
constexpr uint64_t kDefaultMaxResponseBodySize{16 * 1024 * 1024};
constexpr uint64_t kMaxHttpConnectionsUpperBound{100000};
constexpr uint64_t kMaxBodySizeUpperBound{2147483648ULL};

using Socket = net::ip::tcp::socket;
using TlsSocket = net::tls::TlsStream<Socket>;
using ServerConnectionRaw = ServerConnection<net::ip::tcp::socket>;
using ServerConnectionTls = ServerConnection<TlsSocket>;
using ConnectionRaw = ServerConnectionRaw::Parent;
using ConnectionTls = ServerConnectionTls::Parent;
using ConnectionStatusCallbacksRaw =
    ServerConnectionRaw::ConnectionStatusCallbacks;
using ConnectionStatusCallbacksTls =
    ServerConnectionTls::ConnectionStatusCallbacks;

class HTTP_SERVER_LIB_EXPORT Server : public ConnectionStatusCallbacksRaw,
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
         Bind *bind_ssl,
         uint64_t max_http_connections = kDefaultMaxHttpConnections,
         uint64_t max_request_body_size = kDefaultMaxRequestBodySize,
         uint64_t max_response_body_size = kDefaultMaxResponseBodySize,
         net::io_context *log_flush_context = nullptr);

  void set_allowed_methods(const Methods &methods);
  void set_request_handler(RequestHandlerInterface *handler);

  // Runtime overrides are policy-free. Callers that expose user/config input
  // must validate their own bounds before setting an override.
  void set_max_http_connections(std::optional<uint64_t> value);
  uint64_t effective_max_http_connections() const;

  void set_max_request_body_size(std::optional<uint64_t> value);
  uint64_t effective_max_request_body_size() const;

  void set_max_response_body_size(std::optional<uint64_t> value);
  uint64_t effective_max_response_body_size() const;

  void clear_overrides();

  void start();
  void stop();

 private:  // Ssl connections handling
  void on_new_ssl_connection(socket socket);
  uint64_t max_request_body_size() const override {
    return effective_max_request_body_size();
  }
  uint64_t max_response_body_size() const override {
    return effective_max_response_body_size();
  }
  void log_max_request_body_size_rejection(
      uint64_t max_request_body_size,
      std::optional<uint64_t> content_length) override;
  void log_invalid_request_body_headers_rejection(
      std::string_view reason) override;
  void log_max_response_body_size_rejection(
      uint64_t max_response_body_size, uint64_t response_body_size) override;
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
  struct RateLimitedLogState {
    std::chrono::steady_clock::time_point next_log_time{};
    uint64_t suppressed_logs{0};
    std::optional<net::steady_timer> summary_timer;
    bool summary_scheduled{false};
  };

  size_t disconnect_all();
  std::optional<uint64_t> reject_max_http_connections_limit() const;
  void log_max_http_connections_rejection(const uint64_t max_http_connections);
  void schedule_suppressed_log_summary(RateLimitedLogState &state,
                                       const char *summary_message);
  void cancel_suppressed_log_summary(RateLimitedLogState &state);
  void flush_suppressed_log_summary(RateLimitedLogState &state,
                                    const char *summary_message);
  void flush_all_suppressed_log_summaries();
  void cancel_all_suppressed_log_summaries();

  void start_accepting();
  socket socket_move_to_io_thread(socket socket);
  IoThread *return_next_thread();
  TlsServerContext *tls_context_;
  std::list<IoThread> *threads_;
  IoIterator current_thread_;
  Bind *bind_raw_;
  Bind *bind_ssl_;
  net::io_context *log_flush_context_;
  base::method::Bitset allowed_methods_;
  RequestHandlerInterface *handler_ = nullptr;

  std::mutex mutex_connection_;
  std::vector<std::shared_ptr<ServerConnectionRaw>> connections_;
  std::vector<std::shared_ptr<ServerConnectionTls>> connections_ssl_;
  mysql_harness::utility::WaitableVariable<State> sync_state_{
      State::kInitializing};
  uint64_t configured_max_http_connections_{kDefaultMaxHttpConnections};
  uint64_t configured_max_request_body_size_{kDefaultMaxRequestBodySize};
  uint64_t configured_max_response_body_size_{kDefaultMaxResponseBodySize};
  std::atomic<bool> has_max_http_connections_override_;
  std::atomic<bool> has_max_request_body_size_override_;
  std::atomic<bool> has_max_response_body_size_override_;
  std::atomic<uint64_t> max_http_connections_override_;
  std::atomic<uint64_t> max_request_body_size_override_;
  std::atomic<uint64_t> max_response_body_size_override_;
  std::mutex mutex_limit_log_;
  RateLimitedLogState max_http_connections_reject_log_;
  RateLimitedLogState max_request_body_size_reject_log_;
  RateLimitedLogState invalid_request_body_headers_reject_log_;
  RateLimitedLogState max_response_body_size_reject_log_;
};

}  // namespace server
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_SERVER_SERVER_H_
