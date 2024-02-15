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

#ifndef ROUTER_CONNECTION_POOL_CONNECTION_BASE_INCLUDED
#define ROUTER_CONNECTION_POOL_CONNECTION_BASE_INCLUDED

#include <cstdint>  // size_t
#include <functional>
#include <system_error>  // error_code
#include <vector>

#ifdef _WIN32
// include winsock2.h before openssl/ssl.h
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <openssl/ssl.h>  // SSL_CTX

#include "harness_assert.h"
#include "mysql/harness/default_init_allocator.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/channel.h"
#include "mysqlrouter/ssl_mode.h"

/**
 * virtual base-class of BasicConnection.
 */
class ConnectionBase {
 public:
  virtual ~ConnectionBase() = default;

  using recv_buffer_type =
      std::vector<uint8_t, default_init_allocator<uint8_t>>;

  virtual net::io_context &io_ctx() = 0;

  virtual void async_recv(
      recv_buffer_type &,
      std::function<void(std::error_code ec, size_t transferred)>) = 0;

  virtual void async_send(
      recv_buffer_type &,
      std::function<void(std::error_code ec, size_t transferred)>) = 0;

  virtual void async_wait_send(std::function<void(std::error_code ec)>) = 0;
  virtual void async_wait_recv(std::function<void(std::error_code ec)>) = 0;
  virtual void async_wait_error(std::function<void(std::error_code ec)>) = 0;

  [[nodiscard]] virtual bool is_open() const = 0;

  [[nodiscard]] virtual net::impl::socket::native_handle_type native_handle()
      const = 0;

  [[nodiscard]] virtual stdx::expected<void, std::error_code> close() = 0;
  [[nodiscard]] virtual stdx::expected<void, std::error_code> shutdown(
      net::socket_base::shutdown_type st) = 0;

  [[nodiscard]] virtual std::string endpoint() const = 0;

  [[nodiscard]] virtual stdx::expected<void, std::error_code> cancel() = 0;

  [[nodiscard]] virtual bool is_secure_transport() const = 0;

  [[nodiscard]] virtual stdx::expected<void, std::error_code> set_io_context(
      net::io_context &new_ctx) = 0;
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

using TcpConnection = BasicConnection<net::ip::tcp>;
#ifdef NET_TS_HAS_UNIX_SOCKET
using UnixDomainConnection = BasicConnection<local::stream_protocol>;
#endif

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
template <class T>
class TlsSwitchableConnection {
 public:
  //    16kb per buffer
  //     2   buffers per channel (send/recv)
  //     2   channels per connection
  // 10000   connections
  // = 640MByte
  static constexpr size_t kRecvBufferSize{16UL * 1024};
  using protocol_state_type = T;

  TlsSwitchableConnection(std::unique_ptr<ConnectionBase> conn,
                          SslMode ssl_mode, protocol_state_type state)
      : conn_{std::move(conn)},
        ssl_mode_(ssl_mode),
        channel_{},
        protocol_{std::move(state)} {
    channel_.recv_buffer().reserve(kRecvBufferSize);
  }

  TlsSwitchableConnection(std::unique_ptr<ConnectionBase> conn,
                          SslMode ssl_mode, Channel channel,
                          protocol_state_type state)
      : conn_{std::move(conn)},
        ssl_mode_(ssl_mode),
        channel_{std::move(channel)},
        protocol_{std::move(state)} {
    channel_.recv_buffer().reserve(kRecvBufferSize);
  }

  /**
   * assign a low-level connection.
   */
  void assign_connection(std::unique_ptr<ConnectionBase> conn) {
    conn_ = std::move(conn);
  }

  void prepare_for_pool() {
    if (auto *ssl = channel_.ssl()) {
      SSL_set_info_callback(ssl, nullptr);
      SSL_set_msg_callback_arg(ssl, nullptr);
    }

    // reset the recv and send buffers and the pool shouldn't care about the
    // content of those buffers.
    channel_.clear();
  }

  /**
   * async receive data from connection into the channel's receive buffer.
   *
   * calls func when async operation is completed.
   */
  template <class Func>
  void async_recv(Func &&func) {
    harness_assert(conn_ != nullptr);

    // discard everything that has been marked as 'consumed'
    channel_.view_discard_raw();

    conn_->async_recv(channel_.recv_buffer(),
                      [this, func = std::forward<Func>(func)](
                          std::error_code ec, size_t transferred) {
                        if (ec == std::error_code()) {
                          channel_.view_sync_raw();
                        }

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
    conn_->async_send(channel_.send_buffer(), std::forward<Func>(func));
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

  [[nodiscard]] Channel &channel() { return channel_; }

  [[nodiscard]] const Channel &channel() const { return channel_; }

  [[nodiscard]] SslMode ssl_mode() const { return ssl_mode_; }

  [[nodiscard]] bool is_open() const { return conn_ && conn_->is_open(); }

  [[nodiscard]] net::impl::socket::native_handle_type native_handle() const {
    return conn_->native_handle();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> close() const {
    if (!conn_) {
      return stdx::unexpected(make_error_code(std::errc::not_connected));
    }
    return conn_->close();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> shutdown(
      net::socket_base::shutdown_type st) const {
    if (!conn_) {
      return stdx::unexpected(make_error_code(std::errc::not_connected));
    }
    return conn_->shutdown(st);
  }

  [[nodiscard]] std::string endpoint() const {
    if (!is_open()) return "";

    return conn_->endpoint();
  }

  [[nodiscard]] stdx::expected<void, std::error_code> cancel() {
    if (!conn_) return {};

    return conn_->cancel();
  }

  [[nodiscard]] protocol_state_type &protocol() { return protocol_; }

  [[nodiscard]] const protocol_state_type &protocol() const {
    return protocol_;
  }

  std::unique_ptr<ConnectionBase> &connection() { return conn_; }

  const std::unique_ptr<ConnectionBase> &connection() const { return conn_; }

  /**
   * check if the channel is secure.
   *
   * - if TLS is enabled, it the transport is secure
   * - if transport is secure, the channel is secure
   */
  [[nodiscard]] bool is_secure_transport() const {
    return conn_->is_secure_transport() || (channel_.ssl() != nullptr);
  }

 private:
  // tcp/unix-socket
  std::unique_ptr<ConnectionBase> conn_;

  SslMode ssl_mode_;

  // socket buffers
  Channel channel_;

  // higher-level protocol
  protocol_state_type protocol_;
};

#endif
