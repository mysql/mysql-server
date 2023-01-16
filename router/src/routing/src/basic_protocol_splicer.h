/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_BASIC_PROTOCOL_SPLICER_INCLUDED
#define ROUTING_BASIC_PROTOCOL_SPLICER_INCLUDED

#include <cstdint>  // size_t
#include <cstdio>
#include <functional>  // function
#include <sstream>
#include <string>

#ifdef _WIN32
// include winsock2.h before openssl/ssl.h
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <openssl/ssl.h>  // SSL_CTX

#include "blocked_endpoints.h"
#include "channel.h"
#include "harness_assert.h"
#include "initial_connection_attributes.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"  // net::ip::tcp
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"  // local::stream_protocol
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/connection_base.h"
#include "ssl_mode.h"

enum class TlsContentType {
  kChangeCipherSpec = 0x14,
  kAlert,
  kHandshake,
  kApplication,
  kHeartbeat
};

inline std::string tls_content_type_to_string(TlsContentType v) {
  switch (v) {
    case TlsContentType::kChangeCipherSpec:
      return "change-cipher-spec";
    case TlsContentType::kAlert:
      return "alert";
    case TlsContentType::kHandshake:
      return "handshake";
    case TlsContentType::kApplication:
      return "application";
    case TlsContentType::kHeartbeat:
      return "heartbeat";
  }

  return "unknown-" + std::to_string(static_cast<int>(v));
}

class TlsSwitchable {
 public:
  using ssl_ctx_gettor_type = std::function<SSL_CTX *(const std::string &id)>;

  TlsSwitchable(SslMode ssl_mode) : ssl_mode_{ssl_mode} {}

  [[nodiscard]] SslMode ssl_mode() const { return ssl_mode_; }

 private:
  SslMode ssl_mode_;
};

class RoutingConnectionBase {
 public:
  virtual ~RoutingConnectionBase() = default;

  [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>>
  initial_connection_attributes() const = 0;

  virtual uint64_t reset_error_count(BlockedEndpoints &blocked_endpoints) = 0;
  virtual uint64_t increment_error_count(
      BlockedEndpoints &blocked_endpoints) = 0;
};

template <class Protocol>
struct IsTransportSecure : std::false_type {};

#ifdef NET_TS_HAS_UNIX_SOCKET
template <>
struct IsTransportSecure<local::stream_protocol> : std::true_type {};
#endif

/**
 * basic connection which wraps a net-ts Protocol.
 *
 * knows about mysql-protocol specifics like:
 *
 * - session attributes
 * - connection error-tracking.
 *
 * @tparam Protocol a protocol like net::ip::tcp or local::stream_protocol
 */
template <class Protocol>
class BasicConnection : public ConnectionBase {
 public:
  using protocol_type = Protocol;
  using socket_type = typename protocol_type::socket;
  using endpoint_type = typename protocol_type::endpoint;

  using recv_buffer_type = ConnectionBase::recv_buffer_type;

  BasicConnection(socket_type sock, endpoint_type ep)
      : sock_{std::move(sock)}, ep_{std::move(ep)} {}

  net::io_context &io_ctx() override { return sock_.get_executor().context(); }

  stdx::expected<void, std::error_code> set_io_context(
      net::io_context &new_ctx) override {
    // nothing to do.
    if (sock_.get_executor() == new_ctx.get_executor()) return {};

    return sock_.release().and_then(
        [this, &new_ctx](
            auto native_handle) -> stdx::expected<void, std::error_code> {
          socket_type new_sock(new_ctx);

          auto assign_res = new_sock.assign(ep_.protocol(), native_handle);
          if (!assign_res) return assign_res;

          std::swap(sock_, new_sock);

          return {};
        });
  }

  void async_recv(recv_buffer_type &buf,
                  std::function<void(std::error_code ec, size_t transferred)>
                      completion) override {
    net::async_read(sock_, net::dynamic_buffer(buf), net::transfer_at_least(1),
                    std::move(completion));
  }

  void async_send(recv_buffer_type &buf,
                  std::function<void(std::error_code ec, size_t transferred)>
                      completion) override {
    if (sock_.native_non_blocking()) {
      // if the socket is non-blocking try to send directly as the send-buffer
      // is usually empty
      auto write_res = net::write(sock_, net::dynamic_buffer(buf),
                                  net::transfer_at_least(1));
      if (write_res) {
        net::defer(sock_.get_executor(), [completion = std::move(completion),
                                          transferred = *write_res]() {
          completion({}, transferred);
        });
        return;
      }

      const auto ec = write_res.error();

      if (ec != make_error_condition(std::errc::operation_would_block) &&
          ec !=
              make_error_condition(std::errc::resource_unavailable_try_again)) {
        net::defer(sock_.get_executor(), [completion = std::move(completion),
                                          ec]() { completion(ec, 0); });
        return;
      }

      // if it would-block, use the normal async-write.
    }

    net::async_write(sock_, net::dynamic_buffer(buf), net::transfer_at_least(1),
                     std::move(completion));
  }

  void async_wait_send(
      std::function<void(std::error_code ec)> completion) override {
    sock_.async_wait(net::socket_base::wait_write, std::move(completion));
  }

  void async_wait_recv(
      std::function<void(std::error_code ec)> completion) override {
    sock_.async_wait(net::socket_base::wait_read, std::move(completion));
  }

  void async_wait_error(
      std::function<void(std::error_code ec)> completion) override {
    sock_.async_wait(net::socket_base::wait_error, std::move(completion));
  }

  [[nodiscard]] bool is_open() const override { return sock_.is_open(); }

  [[nodiscard]] net::impl::socket::native_handle_type native_handle()
      const override {
    return sock_.native_handle();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> close() override {
    return sock_.close();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> cancel() override {
    return sock_.cancel();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> shutdown(
      net::socket_base::shutdown_type st) override {
    return sock_.shutdown(st);
  }

  [[nodiscard]] std::string endpoint() const override {
    std::ostringstream oss;

    oss << ep_;

    return oss.str();
  }

  template <class GettableSocketOption>
  stdx::expected<void, std::error_code> get_option(
      GettableSocketOption &opt) const {
    return sock_.get_option(opt);
  }

  /**
   * check if the underlying transport is secure.
   *
   * - unix-socket, shared-memory, ... are secure.
   */
  [[nodiscard]] bool is_secure_transport() const override {
    return IsTransportSecure<Protocol>::value;
  }

 protected:
  socket_type sock_;
  endpoint_type ep_;
};

template <class Protocol>
class RoutingConnection : public RoutingConnectionBase {
 public:
  using protocol_type = Protocol;
  using endpoint_type = typename protocol_type::endpoint;

  RoutingConnection(endpoint_type ep) : ep_{std::move(ep)} {}

  [[nodiscard]] std::vector<std::pair<std::string, std::string>>
  initial_connection_attributes() const override {
    return ::initial_connection_attributes(ep_);
  }

  uint64_t reset_error_count(BlockedEndpoints &blocked_endpoints) override {
    return blocked_endpoints.reset_error_count(ep_);
  }

  uint64_t increment_error_count(BlockedEndpoints &blocked_endpoints) override {
    return blocked_endpoints.increment_error_count(ep_);
  }

 private:
  endpoint_type ep_;
};

using TcpConnection = BasicConnection<net::ip::tcp>;
#ifdef NET_TS_HAS_UNIX_SOCKET
using UnixDomainConnection = BasicConnection<local::stream_protocol>;
#endif

class ProtocolStateBase {
 public:
  virtual ~ProtocolStateBase() = default;
};

/**
 * a Connection that can be switched to TLS.
 *
 * wraps
 *
 * - a low-level connections (conn)
 * - a routing connection (endpoints, destinations, ...)
 * - a tls switchable (a SSL_CTX * wrapper)
 * - protocol state (classic, xproto)
 */
class TlsSwitchableConnection {
 public:
  //    16kb per buffer
  //     2   buffers per channel (send/recv)
  //     2   channels per connection
  // 10000   connections
  // = 640MByte
  static constexpr size_t kRecvBufferSize{16UL * 1024};

  TlsSwitchableConnection(std::unique_ptr<ConnectionBase> conn,
                          std::unique_ptr<RoutingConnectionBase> routing_conn,
                          SslMode ssl_mode,
                          std::unique_ptr<ProtocolStateBase> state)
      : conn_{std::move(conn)},
        routing_conn_{std::move(routing_conn)},
        ssl_mode_{std::move(ssl_mode)},
        channel_{std::make_unique<Channel>()},
        protocol_{std::move(state)} {
    channel_->recv_buffer().reserve(kRecvBufferSize);
  }

  TlsSwitchableConnection(std::unique_ptr<ConnectionBase> conn,
                          std::unique_ptr<RoutingConnectionBase> routing_conn,
                          SslMode ssl_mode, std::unique_ptr<Channel> channel,
                          std::unique_ptr<ProtocolStateBase> state)
      : conn_{std::move(conn)},
        routing_conn_{std::move(routing_conn)},
        ssl_mode_{std::move(ssl_mode)},
        channel_{std::move(channel)},
        protocol_{std::move(state)} {
    channel_->recv_buffer().reserve(kRecvBufferSize);
  }

  [[nodiscard]] std::vector<std::pair<std::string, std::string>>
  initial_connection_attributes() const {
    return routing_conn_->initial_connection_attributes();
  }

  /**
   * assign a low-level connection.
   */
  void assign_connection(std::unique_ptr<ConnectionBase> conn) {
    conn_ = std::move(conn);
  }

  /**
   * async receive data from connection into the channel's receive buffer.
   *
   * calls func when async operation is completed.
   */
  template <class Func>
  void async_recv(Func &&func) {
    harness_assert(conn_ != nullptr);
    harness_assert(channel_ != nullptr);

    // discard everything that has been marked as 'consumed'
    channel_->view_discard_raw();

    conn_->async_recv(channel_->recv_buffer(),
                      [this, func = std::forward<Func>(func)](
                          std::error_code ec, size_t transferred) {
                        channel_->view_sync_raw();

                        func(ec, transferred);
                      });
  }

  /**
   * async send data from the channel's send buffer to the connection.
   *
   * calls func when async operation is completed.
   */
  template <class Func>
  void async_send(Func &&func) {
    conn_->async_send(channel_->send_buffer(), std::forward<Func>(func));
  }

  /**
   * async wait until connection allows to send data.
   *
   * calls func when async operation is completed.
   */
  template <class Func>
  void async_wait_send(Func &&func) {
    conn_->async_wait_send(std::forward<Func>(func));
  }

  template <class Func>
  void async_wait_error(Func &&func) {
    conn_->async_wait_error(std::forward<Func>(func));
  }

  [[nodiscard]] Channel *channel() { return channel_.get(); }

  [[nodiscard]] const Channel *channel() const { return channel_.get(); }

  [[nodiscard]] SslMode ssl_mode() const { return ssl_mode_; }

  [[nodiscard]] bool is_open() const { return conn_ && conn_->is_open(); }

  [[nodiscard]] net::impl::socket::native_handle_type native_handle() const {
    return conn_->native_handle();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> close() const {
    if (!conn_) {
      return stdx::make_unexpected(make_error_code(std::errc::not_connected));
    }
    return conn_->close();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> shutdown(
      net::socket_base::shutdown_type st) const {
    if (!conn_) {
      return stdx::make_unexpected(make_error_code(std::errc::not_connected));
    }
    return conn_->shutdown(st);
  }

  [[nodiscard]] std::string endpoint() const {
    if (!is_open()) return "";

    return conn_->endpoint();
  }

  [[nodiscard]] uint64_t reset_error_count(
      BlockedEndpoints &blocked_endpoints) {
    return routing_conn_->reset_error_count(blocked_endpoints);
  }

  [[nodiscard]] uint64_t increment_error_count(
      BlockedEndpoints &blocked_endpoints) {
    return routing_conn_->increment_error_count(blocked_endpoints);
  }

  [[nodiscard]] stdx::expected<void, std::error_code> cancel() {
    if (!conn_) return {};

    return conn_->cancel();
  }

  [[nodiscard]] ProtocolStateBase *protocol() { return protocol_.get(); }

  [[nodiscard]] const ProtocolStateBase *protocol() const {
    return protocol_.get();
  }

  std::unique_ptr<ConnectionBase> &connection() { return conn_; }

  /**
   * check if the channel is secure.
   *
   * - if TLS is enabled, it the transport is secure
   * - if transport is secure, the channel is secure
   */
  [[nodiscard]] bool is_secure_transport() const {
    return conn_->is_secure_transport() || channel_->ssl();
  }

 private:
  // tcp/unix-socket
  std::unique_ptr<ConnectionBase> conn_;
  std::unique_ptr<RoutingConnectionBase> routing_conn_;

  SslMode ssl_mode_;

  // socket buffers
  std::unique_ptr<Channel> channel_;

  // higher-level protocol
  std::unique_ptr<ProtocolStateBase> protocol_;
};

/**
 * splices two connections together.
 */
class ProtocolSplicerBase {
 public:
  ProtocolSplicerBase(TlsSwitchableConnection client_conn,
                      TlsSwitchableConnection server_conn)
      : client_conn_{std::move(client_conn)},
        server_conn_{std::move(server_conn)} {}

  template <class Func>
  void async_wait_send_server(Func &&func) {
    server_conn_.async_wait_send(std::forward<Func>(func));
  }

  template <class Func>
  void async_recv_server(Func &&func) {
    server_conn_.async_recv(std::forward<Func>(func));
  }

  template <class Func>
  void async_send_server(Func &&func) {
    server_conn_.async_send(std::forward<Func>(func));
  }

  template <class Func>
  void async_recv_client(Func &&func) {
    client_conn_.async_recv(std::forward<Func>(func));
  }

  template <class Func>
  void async_send_client(Func &&func) {
    client_conn_.async_send(std::forward<Func>(func));
  }

  template <class Func>
  void async_client_wait_error(Func &&func) {
    client_conn_.async_wait_error(std::forward<Func>(func));
  }

  [[nodiscard]] TlsSwitchableConnection &client_conn() { return client_conn_; }

  [[nodiscard]] const TlsSwitchableConnection &client_conn() const {
    return client_conn_;
  }

  [[nodiscard]] TlsSwitchableConnection &server_conn() { return server_conn_; }

  [[nodiscard]] const TlsSwitchableConnection &server_conn() const {
    return server_conn_;
  }

  [[nodiscard]] SslMode source_ssl_mode() const {
    return client_conn().ssl_mode();
  }

  [[nodiscard]] SslMode dest_ssl_mode() const {
    return server_conn().ssl_mode();
  }

  [[nodiscard]] Channel *client_channel() { return client_conn().channel(); }

  [[nodiscard]] const Channel *client_channel() const {
    return client_conn().channel();
  }

  [[nodiscard]] Channel *server_channel() { return server_conn().channel(); }

  /**
   * accept a TLS connection from the client_channel_.
   */
  [[nodiscard]] stdx::expected<void, std::error_code> tls_accept() {
    // write socket data to SSL struct
    auto *channel = client_conn_.channel();

    {
      const auto flush_res = channel->flush_from_recv_buf();
      if (!flush_res) return flush_res.get_unexpected();
    }

    if (!channel->tls_init_is_finished()) {
      const auto res = channel->tls_accept();

      // flush the TLS message to the send-buffer.
      {
        const auto flush_res = channel->flush_to_send_buf();
        if (!flush_res) {
          const auto ec = flush_res.error();
          if (ec != make_error_code(std::errc::operation_would_block)) {
            return flush_res.get_unexpected();
          }
        }
      }

      if (!res) {
        return res.get_unexpected();
      }
    }

    return {};
  }

 protected:
  TlsSwitchableConnection client_conn_;
  TlsSwitchableConnection server_conn_;
};

#endif
