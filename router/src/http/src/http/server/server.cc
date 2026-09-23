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

#include "http/server/server.h"

#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

#include "http/server/http_counters.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/io_thread.h"

IMPORT_LOG_FUNCTIONS()

namespace http {
namespace server {

std::atomic<uint64_t> http_connections_created{0};
std::atomic<uint64_t> http_connections_closed{0};
std::atomic<uint64_t> http_connections_reused{0};

namespace {

template <typename Connection>
void disconnect(Connection &c) {
  auto &socket = c->get_socket();
  auto &io_ctx = socket.get_executor().context();

  // queue the cancel in the connections io-ctx to make it thread-safe.
  net::dispatch(io_ctx, [c]() { c->get_socket().cancel(); });
}

constexpr auto kRejectLogGracePeriod = std::chrono::seconds(5);
constexpr char kMaxHttpConnectionsRejectionMessage[]{
    "rejected connection due to max_http_connections limit"};
constexpr char kMaxRequestBodySizeRejectionMessage[]{
    "rejected request due to max_request_body_size limit"};
constexpr char kInvalidRequestBodyHeadersRejectionMessage[]{
    "rejected request due to invalid request body headers"};
constexpr char kMaxResponseBodySizeRejectionMessage[]{
    "rejected response due to max_response_body_size limit"};

uint64_t effective_limit(const uint64_t configured_limit,
                         const std::atomic<bool> &has_runtime_override,
                         const std::atomic<uint64_t> &runtime_override) {
  return has_runtime_override.load() ? runtime_override.load()
                                     : configured_limit;
}

void set_limit_override(std::atomic<bool> &has_runtime_override,
                        std::atomic<uint64_t> &runtime_override,
                        std::optional<uint64_t> value) {
  if (value.has_value()) {
    runtime_override.store(*value);
    has_runtime_override.store(true);
  } else {
    has_runtime_override.store(false);
  }
}

}  // namespace

Server::Server(TlsServerContext *tls_context, IoThreads *threads,
               server::Bind *bind_raw, server::Bind *bind_ssl,
               uint64_t max_http_connections, uint64_t max_request_body_size,
               uint64_t max_response_body_size,
               net::io_context *log_flush_context)
    : tls_context_{tls_context},
      threads_{threads},
      current_thread_{threads_->begin()},
      bind_raw_(bind_raw),
      bind_ssl_{bind_ssl},
      log_flush_context_{log_flush_context},
      configured_max_http_connections_{max_http_connections},
      configured_max_request_body_size_{max_request_body_size},
      configured_max_response_body_size_{max_response_body_size},
      has_max_http_connections_override_{false},
      has_max_request_body_size_override_{false},
      has_max_response_body_size_override_{false},
      max_http_connections_override_{0},
      max_request_body_size_override_{0},
      max_response_body_size_override_{0} {}

void Server::set_max_http_connections(std::optional<uint64_t> value) {
  set_limit_override(has_max_http_connections_override_,
                     max_http_connections_override_, value);
}

uint64_t Server::effective_max_http_connections() const {
  return effective_limit(configured_max_http_connections_,
                         has_max_http_connections_override_,
                         max_http_connections_override_);
}

void Server::set_max_request_body_size(std::optional<uint64_t> value) {
  set_limit_override(has_max_request_body_size_override_,
                     max_request_body_size_override_, value);
}

uint64_t Server::effective_max_request_body_size() const {
  return effective_limit(configured_max_request_body_size_,
                         has_max_request_body_size_override_,
                         max_request_body_size_override_);
}

void Server::set_max_response_body_size(std::optional<uint64_t> value) {
  set_limit_override(has_max_response_body_size_override_,
                     max_response_body_size_override_, value);
}

uint64_t Server::effective_max_response_body_size() const {
  return effective_limit(configured_max_response_body_size_,
                         has_max_response_body_size_override_,
                         max_response_body_size_override_);
}

void Server::clear_overrides() {
  set_max_http_connections(std::nullopt);
  set_max_request_body_size(std::nullopt);
  set_max_response_body_size(std::nullopt);
}

void Server::schedule_suppressed_log_summary(RateLimitedLogState &state,
                                             const char *summary_message) {
  if (log_flush_context_ == nullptr || state.summary_scheduled) return;

  if (!state.summary_timer.has_value()) {
    state.summary_timer.emplace(*log_flush_context_);
  }

  state.summary_scheduled = true;
  state.summary_timer->expires_at(state.next_log_time);
  state.summary_timer->async_wait(
      [this, &state, summary_message](const std::error_code &ec) {
        if (ec) return;

        std::lock_guard<std::mutex> lock(mutex_limit_log_);
        flush_suppressed_log_summary(state, summary_message);
      });
}

void Server::cancel_suppressed_log_summary(RateLimitedLogState &state) {
  if (state.summary_timer.has_value()) {
    state.summary_timer->cancel();
  }
  state.summary_scheduled = false;
}

void Server::flush_suppressed_log_summary(RateLimitedLogState &state,
                                          const char *summary_message) {
  const auto suppressed_logs = state.suppressed_logs;
  state.suppressed_logs = 0;
  state.summary_scheduled = false;

  if (suppressed_logs == 0) return;

  log_warning("%s (suppressed %llu similar messages in the last %llu seconds)",
              summary_message, static_cast<unsigned long long>(suppressed_logs),
              static_cast<unsigned long long>(kRejectLogGracePeriod.count()));
}

void Server::flush_all_suppressed_log_summaries() {
  flush_suppressed_log_summary(max_http_connections_reject_log_,
                               kMaxHttpConnectionsRejectionMessage);
  flush_suppressed_log_summary(max_request_body_size_reject_log_,
                               kMaxRequestBodySizeRejectionMessage);
  flush_suppressed_log_summary(invalid_request_body_headers_reject_log_,
                               kInvalidRequestBodyHeadersRejectionMessage);
  flush_suppressed_log_summary(max_response_body_size_reject_log_,
                               kMaxResponseBodySizeRejectionMessage);
}

void Server::cancel_all_suppressed_log_summaries() {
  cancel_suppressed_log_summary(max_http_connections_reject_log_);
  cancel_suppressed_log_summary(max_request_body_size_reject_log_);
  cancel_suppressed_log_summary(invalid_request_body_headers_reject_log_);
  cancel_suppressed_log_summary(max_response_body_size_reject_log_);
}

std::optional<uint64_t> Server::reject_max_http_connections_limit() const {
  const auto max_http_connections = effective_max_http_connections();

  const auto active_connections = connections_.size() + connections_ssl_.size();
  if (active_connections < max_http_connections) return std::nullopt;

  return max_http_connections;
}

void Server::log_max_http_connections_rejection(
    const uint64_t max_http_connections) {
  std::lock_guard<std::mutex> lock(mutex_limit_log_);
  const auto now = std::chrono::steady_clock::now();
  if (now < max_http_connections_reject_log_.next_log_time) {
    ++max_http_connections_reject_log_.suppressed_logs;
    schedule_suppressed_log_summary(max_http_connections_reject_log_,
                                    kMaxHttpConnectionsRejectionMessage);
    return;
  }

  if (max_http_connections_reject_log_.suppressed_logs != 0) {
    log_warning(
        "%s: %llu "
        "(suppressed %llu similar messages in the last %llu seconds)",
        kMaxHttpConnectionsRejectionMessage,
        static_cast<unsigned long long>(max_http_connections),
        static_cast<unsigned long long>(
            max_http_connections_reject_log_.suppressed_logs),
        static_cast<unsigned long long>(kRejectLogGracePeriod.count()));
  } else {
    log_warning("%s: %llu", kMaxHttpConnectionsRejectionMessage,
                static_cast<unsigned long long>(max_http_connections));
  }

  max_http_connections_reject_log_.suppressed_logs = 0;
  cancel_suppressed_log_summary(max_http_connections_reject_log_);
  max_http_connections_reject_log_.next_log_time = now + kRejectLogGracePeriod;
}

void Server::log_max_request_body_size_rejection(
    const uint64_t max_request_body_size,
    std::optional<uint64_t> content_length) {
  std::lock_guard<std::mutex> lock(mutex_limit_log_);
  const auto now = std::chrono::steady_clock::now();
  if (now < max_request_body_size_reject_log_.next_log_time) {
    ++max_request_body_size_reject_log_.suppressed_logs;
    schedule_suppressed_log_summary(max_request_body_size_reject_log_,
                                    kMaxRequestBodySizeRejectionMessage);
    return;
  }

  const auto suppressed_logs =
      max_request_body_size_reject_log_.suppressed_logs;
  if (content_length.has_value()) {
    if (suppressed_logs == 0) {
      log_warning("%s: limit=%llu, content-length=%llu",
                  kMaxRequestBodySizeRejectionMessage,
                  static_cast<unsigned long long>(max_request_body_size),
                  static_cast<unsigned long long>(*content_length));
    } else {
      log_warning(
          "%s: limit=%llu, content-length=%llu (suppressed %llu similar "
          "messages in the last %llu seconds)",
          kMaxRequestBodySizeRejectionMessage,
          static_cast<unsigned long long>(max_request_body_size),
          static_cast<unsigned long long>(*content_length),
          static_cast<unsigned long long>(suppressed_logs),
          static_cast<unsigned long long>(kRejectLogGracePeriod.count()));
    }
  } else {
    if (suppressed_logs == 0) {
      log_warning("%s: limit=%llu", kMaxRequestBodySizeRejectionMessage,
                  static_cast<unsigned long long>(max_request_body_size));
    } else {
      log_warning(
          "%s: limit=%llu (suppressed %llu similar messages in the last %llu "
          "seconds)",
          kMaxRequestBodySizeRejectionMessage,
          static_cast<unsigned long long>(max_request_body_size),
          static_cast<unsigned long long>(suppressed_logs),
          static_cast<unsigned long long>(kRejectLogGracePeriod.count()));
    }
  }

  max_request_body_size_reject_log_.suppressed_logs = 0;
  cancel_suppressed_log_summary(max_request_body_size_reject_log_);
  max_request_body_size_reject_log_.next_log_time = now + kRejectLogGracePeriod;
}

void Server::log_invalid_request_body_headers_rejection(
    std::string_view reason) {
  std::lock_guard<std::mutex> lock(mutex_limit_log_);
  const auto now = std::chrono::steady_clock::now();
  if (now < invalid_request_body_headers_reject_log_.next_log_time) {
    ++invalid_request_body_headers_reject_log_.suppressed_logs;
    schedule_suppressed_log_summary(invalid_request_body_headers_reject_log_,
                                    kInvalidRequestBodyHeadersRejectionMessage);
    return;
  }

  const auto suppressed_logs =
      invalid_request_body_headers_reject_log_.suppressed_logs;
  if (suppressed_logs == 0) {
    log_warning("%s: %.*s", kInvalidRequestBodyHeadersRejectionMessage,
                static_cast<int>(reason.size()), reason.data());
  } else {
    log_warning(
        "%s: %.*s "
        "(suppressed %llu similar messages in the last %llu seconds)",
        kInvalidRequestBodyHeadersRejectionMessage,
        static_cast<int>(reason.size()), reason.data(),
        static_cast<unsigned long long>(suppressed_logs),
        static_cast<unsigned long long>(kRejectLogGracePeriod.count()));
  }

  invalid_request_body_headers_reject_log_.suppressed_logs = 0;
  cancel_suppressed_log_summary(invalid_request_body_headers_reject_log_);
  invalid_request_body_headers_reject_log_.next_log_time =
      now + kRejectLogGracePeriod;
}

void Server::log_max_response_body_size_rejection(
    const uint64_t max_response_body_size, const uint64_t response_body_size) {
  std::lock_guard<std::mutex> lock(mutex_limit_log_);
  const auto now = std::chrono::steady_clock::now();
  if (now < max_response_body_size_reject_log_.next_log_time) {
    ++max_response_body_size_reject_log_.suppressed_logs;
    schedule_suppressed_log_summary(max_response_body_size_reject_log_,
                                    kMaxResponseBodySizeRejectionMessage);
    return;
  }

  const auto suppressed_logs =
      max_response_body_size_reject_log_.suppressed_logs;
  if (suppressed_logs == 0) {
    log_warning("%s: limit=%llu, response-body-size=%llu",
                kMaxResponseBodySizeRejectionMessage,
                static_cast<unsigned long long>(max_response_body_size),
                static_cast<unsigned long long>(response_body_size));
  } else {
    log_warning(
        "%s: limit=%llu, response-body-size=%llu (suppressed %llu similar "
        "messages in the last %llu seconds)",
        kMaxResponseBodySizeRejectionMessage,
        static_cast<unsigned long long>(max_response_body_size),
        static_cast<unsigned long long>(response_body_size),
        static_cast<unsigned long long>(suppressed_logs),
        static_cast<unsigned long long>(kRejectLogGracePeriod.count()));
  }

  max_response_body_size_reject_log_.suppressed_logs = 0;
  cancel_suppressed_log_summary(max_response_body_size_reject_log_);
  max_response_body_size_reject_log_.next_log_time =
      now + kRejectLogGracePeriod;
}

void Server::set_allowed_methods(const base::method::Bitset &methods) {
  allowed_methods_ = methods;
}

void Server::set_request_handler(RequestHandlerInterface *handler) {
  handler_ = handler;
}

void Server::start() {
  sync_state_.exchange(State::kInitializing, State::kRunning, [this]() {
    // After successful changing the state, just start listening.
    start_accepting();
  });
}

void Server::start_accepting() {
  if (bind_raw_) {
    bind_raw_->start_accepting_loop([this](auto sock) {
      this->on_new_connection(socket_move_to_io_thread(std::move(sock)));
    });
  }
  if (bind_ssl_) {
    bind_ssl_->start_accepting_loop([this](auto sock) {
      this->on_new_ssl_connection(socket_move_to_io_thread(std::move(sock)));
    });
  }
}

void Server::stop() {
  if (bind_raw_) bind_raw_->stop_accepting_loop();
  if (bind_ssl_) bind_ssl_->stop_accepting_loop();

  {
    std::lock_guard<std::mutex> lock(mutex_limit_log_);
    flush_all_suppressed_log_summaries();
    cancel_all_suppressed_log_summaries();
  }

  disconnect_all();

  sync_state_.wait(State::kStopped);
}

size_t Server::disconnect_all() {
  std::lock_guard<std::mutex> lock(mutex_connection_);
  auto count = connections_.size() + connections_ssl_.size();
  for (auto &c : connections_ssl_) {
    disconnect(c);
  }

  for (auto &c : connections_) {
    disconnect(c);
  }

  sync_state_.exchange({State::kInitializing, State::kRunning},
                       count ? State::kStopping : State::kStopped);

  return count;
}

// === SSL connections handling ===

void Server::on_new_ssl_connection(socket socket) {
  std::shared_ptr<ServerConnectionTls> connection;
  {
    std::lock_guard<std::mutex> lock(mutex_connection_);
    const auto max_http_connections = reject_max_http_connections_limit();
    if (max_http_connections.has_value()) {
      log_max_http_connections_rejection(*max_http_connections);
      socket.close();
      return;
    }

    connection = std::make_shared<ServerConnectionTls>(
        ConnectionTls::IO{tls_context_, std::move(socket)}, &allowed_methods_,
        handler_, this);
    connections_ssl_.push_back(connection);
  }

  http_connections_created++;

  connection->start();
}

void Server::on_connection_close(ConnectionTls *connection) {
  std::lock_guard<std::mutex> lock(mutex_connection_);

  for (auto it = connections_ssl_.begin(); it != connections_ssl_.end(); ++it) {
    if ((*it).get() == connection) {
      connections_ssl_.erase(it);
      break;
    }
  }

  http_connections_closed++;
  if (connections_.empty() && connections_ssl_.empty())
    sync_state_.exchange(State::kStopping, State::kStopped);
}

void Server::on_connection_io_error(ConnectionTls *, const std::error_code &) {
  // We do not need to do anything here,
  // the http::base::Connection, after the error closes the connection
  // which results in 'on_connection_close' call.

  // In future this callback may be used to count errors.
}

// === RAW connections handling ===

void Server::on_new_connection(socket socket) {
  std::shared_ptr<ServerConnectionRaw> connection;
  {
    std::lock_guard<std::mutex> lock(mutex_connection_);
    const auto max_http_connections = reject_max_http_connections_limit();
    if (max_http_connections.has_value()) {
      log_max_http_connections_rejection(*max_http_connections);
      socket.close();
      return;
    }

    connection = std::make_shared<ServerConnectionRaw>(
        std::move(socket), &allowed_methods_, handler_, this);
    connections_.push_back(connection);
  }

  http_connections_created++;

  connection->start();
}

void Server::on_connection_close(ConnectionRaw *connection) {
  std::lock_guard<std::mutex> lock(mutex_connection_);

  for (auto it = connections_.begin(); it != connections_.end(); ++it) {
    if ((*it).get() == connection) {
      connections_.erase(it);
      break;
    }
  }
  http_connections_closed++;

  if (connections_.empty() && connections_ssl_.empty())
    sync_state_.exchange(State::kStopping, State::kStopped);
}

void Server::on_connection_io_error(ConnectionRaw *, const std::error_code &) {
  // We do not need to do anything here,
  // the http::base::Connection, after the error closes the connection
  // which results in 'on_connection_close' call.

  // In future this callback may be used to count errors.
}

Server::socket Server::socket_move_to_io_thread(socket s) {
  auto protocol = s.local_endpoint().value().protocol();
  auto fd = s.release().value();

  Server::socket result{return_next_thread()->context()};
  result.assign(protocol, fd);

  return result;
}

IoThread *Server::return_next_thread() {
  if (threads_->end() == ++current_thread_) {
    current_thread_ = threads_->begin();
  }

  return std::addressof(*current_thread_);
}

}  // namespace server
}  // namespace http
