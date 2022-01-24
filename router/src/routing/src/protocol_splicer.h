/*
  Copyright (c) 2018, 2021, Oracle and/or its affiliates.

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

#ifndef ROUTING_PROTOCOL_SPLICER_INCLUDED
#define ROUTING_PROTOCOL_SPLICER_INCLUDED

#include <chrono>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "basic_protocol_splicer.h"
#include "channel.h"
#include "classic_protocol_splicer.h"
#include "context.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_client_context.h"
#include "mysql/harness/tls_error.h"
#include "mysql/harness/tls_server_context.h"
#include "mysql_router_thread.h"
#include "mysqlrouter/classic_protocol.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/utils.h"  // to_string
#include "protocol/base_protocol.h"
#include "tcp_address.h"
#include "x_protocol_splicer.h"

// #define DEBUG_SSL
// #define DEBUG_IO
// #define DEBUG_STATE

IMPORT_LOG_FUNCTIONS()

template <class ClientProtocol, class ServerProtocol>
class MySQLRoutingConnection;

/**
 * prepare the socket specific connection attributes.
 */
template <class ClientProtocol>
std::vector<std::pair<std::string, std::string>> initial_connection_attributes(
    const typename ClientProtocol::endpoint &);

/**
 * TCP/IP socket related connection attributes.
 *
 * - client-ip (IPv4 and IPv6)
 * - client-port
 */
template <>
inline std::vector<std::pair<std::string, std::string>>
initial_connection_attributes<net::ip::tcp>(const net::ip::tcp::endpoint &ep) {
  return {
      {"_client_ip", ep.address().to_string()},
      {"_client_port", std::to_string(ep.port())},
  };
}

#ifdef NET_TS_HAS_UNIX_SOCKET
/**
 * UNIX domain socket related connection attributes.
 *
 * - client-socket
 */
template <>
inline std::vector<std::pair<std::string, std::string>>
initial_connection_attributes<local::stream_protocol>(
    const local::stream_protocol::endpoint &ep) {
  return {
      {"_client_socket", ep.path()},
  };
}
#endif

template <class ClientProtocol, class ServerProtocol>
std::unique_ptr<BasicSplicer> make_splicer(
    MySQLRoutingConnection<ClientProtocol, ServerProtocol> *conn) {
  switch (conn->context().get_protocol()) {
    case BaseProtocol::Type::kClassicProtocol:
      return std::make_unique<ClassicProtocolSplicer>(
          conn->context().source_ssl_mode(), conn->context().dest_ssl_mode(),
          [conn]() { return conn->context().source_ssl_ctx()->get(); },
          [conn]() -> SSL_CTX * {
            auto make_res =
                mysql_harness::make_tcp_address(conn->get_destination_id());
            if (!make_res) {
              return nullptr;
            }

            return conn->context().dest_ssl_ctx(make_res->address())->get();
          },
          initial_connection_attributes<ClientProtocol>(
              conn->client_endpoint()));
    case BaseProtocol::Type::kXProtocol:
      return std::make_unique<XProtocolSplicer>(
          conn->context().source_ssl_mode(), conn->context().dest_ssl_mode(),
          [conn]() { return conn->context().source_ssl_ctx()->get(); },
          [conn]() -> SSL_CTX * {
            auto make_res =
                mysql_harness::make_tcp_address(conn->get_destination_id());
            if (!make_res) {
              return nullptr;
            }

            return conn->context().dest_ssl_ctx(make_res->address())->get();
          },
          initial_connection_attributes<ClientProtocol>(
              conn->client_endpoint()));
  }

  return {};
}

template <class ClientProtocol, class ServerProtocol>
class Splicer : public std::enable_shared_from_this<
                    Splicer<ClientProtocol, ServerProtocol>> {
 public:
  using client_protocol_type = ClientProtocol;
  using server_protocol_type = ServerProtocol;

  using State = BasicSplicer::State;

  Splicer(MySQLRoutingConnection<ClientProtocol, ServerProtocol> *conn,
          const size_t net_buffer_size)
      : conn_{conn},
        splicer_{make_splicer(conn)},
        max_read_size_{net_buffer_size} {}

  ~Splicer() {
    if (splicer_->state() != BasicSplicer::State::DONE) {
      std::cerr << __LINE__ << ": invalid final state" << std::endl;
      std::terminate();
    }

    conn_->disassociate();
  }

  void client_recv_ready(const std::error_code ec) {
    // cancel timers before they interrupt us.
    client_read_timer_.cancel();
    if (ec == std::errc::operation_canceled) {
      if (splicer_->state() != State::DONE) splicer_->state(finish());
      return;
    }

    // not waiting anymore.
    splicer_->client_waiting_recv(false);

    const bool finished = recv_client_channel();

    if (!finished) return;

    run();
  }

  void server_recv_ready(const std::error_code ec) {
    // cancel timers before they interrupt us.
    server_read_timer_.cancel();

    if (ec == std::errc::operation_canceled) {
      if (splicer_->state() != State::DONE) splicer_->state(finish());
      return;
    }

    // not waiting anymore.
    splicer_->server_waiting_recv(false);

    const bool finished = recv_server_channel();

    if (!finished) return;

    run();
  }

  void client_send_ready(const std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      if (splicer_->state() != State::DONE) splicer_->state(finish());
      return;
    }

    // not waiting anymore.
    splicer_->client_waiting_send(false);

    const bool finished = send_client_channel();
    if (!finished) return;

    run();
  }

  void server_send_ready(const std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      if (splicer_->state() != State::DONE) splicer_->state(finish());
      return;
    }

    // not waiting anymore.
    splicer_->server_waiting_send(false);

    const bool finished = send_server_channel();
    if (!finished) return;

    run();
  }

  void handle_client_read_timeout(const std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      return;
    }

    // timeout fired, interrupt client socket wait.
    conn_->client_socket().cancel();
  }

  void handle_server_read_timeout(const std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      return;
    }

    // timeout fired, interrupt server socket wait
    conn_->server_socket().cancel();
  }

  enum class ToDirection { SERVER, CLIENT };
  enum class FromDirection { SERVER, CLIENT };

  /**
   * write the send-buffer from a channel to a socket.
   *
   * success -> track bytes written
   * would-block -> wait for writable
   * connection close -> FINISH
   * failure -> log it -> FINISH
   *
   * @param sock a connected socket.
   * @param channel a channel with a send-buffer.
   */
  template <ToDirection direction, class Socket>
  bool send_channel(Socket &sock, Channel *channel) {
    if (channel->send_buffer().empty()) return true;

    const char *to = (direction == ToDirection::SERVER) ? "server" : "client";

    const auto send_res =
        net::write(sock, net::dynamic_buffer(channel->send_buffer()));
    if (send_res) {
#if defined(DEBUG_IO)
      log_debug("%d: %s::send() = %zu", __LINE__, to, send_res.value());
#endif
      if ((direction == ToDirection::SERVER)) {
        conn_->transfered_to_server(send_res.value());
      } else {
        conn_->transfered_to_client(send_res.value());
      }

      if (channel->send_buffer().empty()) return true;

      // there is still data in the send-buffer.
      if ((direction == ToDirection::SERVER)) {
        async_wait_server_send();
      } else {
        async_wait_client_send();
      }

      // not finished yet, we need to send more.
      return false;
    } else {
      const auto ec = send_res.error();
#if defined(DEBUG_IO)
      log_debug("%d: %s::send() = %s", __LINE__, to,
                send_res.error().message().c_str());
#endif

      if (ec == make_error_condition(std::errc::operation_would_block)) {
        if ((direction == ToDirection::SERVER)) {
          async_wait_server_send();
        } else {
          async_wait_client_send();
        }

        // not finished yet, we need to send more.
        return false;
      } else {
        if (ec == make_error_condition(std::errc::broken_pipe)) {
          // the connection got closed by the other side.
          channel->send_buffer().clear();
        } else {
          // connection reset? abort? network?
          log_warning("%s::write() failed: %s (%s:%d). Aborting connection.",
                      to, ec.message().c_str(), ec.category().name(),
                      ec.value());
        }
        splicer_->state(State::FINISH);
      }
    }

    return true;
  }

  bool send_server_channel() {
    return send_channel<ToDirection::SERVER>(conn_->server_socket(),
                                             splicer_->server_channel());
  }

  bool send_client_channel() {
    return send_channel<ToDirection::CLIENT>(conn_->client_socket(),
                                             splicer_->client_channel());
  }

  // @retval true finished.
  // @retval false would-block
  template <FromDirection direction, class Socket>
  bool recv_channel(Socket &sock, Channel *channel) {
    if (channel->want_recv() == 0) return true;
    if (direction == FromDirection::SERVER ? splicer_->server_waiting_recv()
                                           : splicer_->client_waiting_recv()) {
      // already waiting to receive something, don't try again.
      return true;
    }

    const char *from =
        (direction == FromDirection::SERVER) ? "server" : "client";

    auto want_read = channel->want_recv();
#if defined(DEBUG_IO)
    log_debug("%s::recv(%d, %zu)", from, sock.native_handle(), want_read);
#endif
    const auto read_res =
        net::read(sock, net::dynamic_buffer(channel->recv_buffer()),
                  net::transfer_at_least(want_read));
    if (!read_res) {
      const auto ec = read_res.error();
#if defined(DEBUG_IO)
      log_debug("%s::recv() failed: %s", from, ec.message().c_str());
#endif
      if (ec ==
              make_error_condition(std::errc::resource_unavailable_try_again) ||
          ec == make_error_condition(std::errc::operation_would_block)) {
        if ((direction == FromDirection::SERVER)) {
          async_wait_server_recv();
        } else {
          async_wait_client_recv();
        }

        // not finished yet, we need to read more.
        return false;
      } else {
        if (ec != net::stream_errc::eof &&
            ec != make_error_condition(std::errc::connection_reset) &&
            ec != make_error_condition(std::errc::connection_aborted)) {
          log_info("%s::recv() failed: %s (%s:%d)", from, ec.message().c_str(),
                   ec.category().name(), ec.value());
        }
        splicer_->state(State::FINISH);
      }
    } else {
#if defined(DEBUG_IO)
      log_debug("%s::recv() = %zu", from, read_res.value());
#endif
      want_read -= std::min(want_read, read_res.value());

      channel->want_recv(want_read);
    }

    return true;
  }

  bool recv_server_channel() {
    return recv_channel<FromDirection::SERVER>(conn_->server_socket(),
                                               splicer_->server_channel());
  }

  bool recv_client_channel() {
    return recv_channel<FromDirection::CLIENT>(conn_->client_socket(),
                                               splicer_->client_channel());
  }

  void run() {
    while (true) {
#if defined(DEBUG_STATE)
      log_debug("%d: state: %s", __LINE__,
                BasicSplicer::state_to_string(splicer_->state()));
#endif

      auto before_state = splicer_->state();

      switch (before_state) {
        case State::SERVER_GREETING:
          splicer_->state(splicer_->server_greeting());
          break;
        case State::CLIENT_GREETING:
          splicer_->state(splicer_->client_greeting());
          break;
        case State::TLS_ACCEPT:
          splicer_->state(splicer_->tls_accept());
          break;
        case State::TLS_CLIENT_GREETING:
          splicer_->state(splicer_->tls_client_greeting());
          break;
        case State::TLS_CLIENT_GREETING_RESPONSE:
          splicer_->state(splicer_->tls_client_greeting_response());
          break;
        case State::TLS_CONNECT:
          splicer_->state(splicer_->tls_connect());
          break;
        case State::SPLICE_INIT:
          // handshake is really finished.
          conn_->context().template clear_error_counter<ClientProtocol>(
              conn_->client_endpoint());

          splicer_->state(State::SPLICE);
          // adjust the "before_state" to get the circuit-breaker working
          // correctly below.
          //
          // not adjusting state would lead to an infinite loop.
          before_state = splicer_->state();

          splicer_->state(splicer_->splice<true>());
          splicer_->state(splicer_->splice<false>());

          break;
        case State::SPLICE:
          if (splicer_->client_channel()->want_recv() == 0 &&
              !splicer_->client_channel()->recv_buffer().empty()) {
            splicer_->state(splicer_->splice<true>());
          }

          if (splicer_->server_channel()->want_recv() == 0 &&
              !splicer_->server_channel()->recv_buffer().empty()) {
            splicer_->state(splicer_->splice<false>());
          }
          break;
        case State::ERROR:
          return;
        case State::TLS_SHUTDOWN:
          splicer_->state(splicer_->tls_shutdown());
          break;
        case State::FINISH:
          splicer_->state(finish());
          return;
        case State::DONE:
          return;
      }

      // send_buffer -> send() -> would_block -> async_wait ->
      //   transfer() -> run()
      const auto send_client_finished = send_client_channel();
      const auto send_server_finished = send_server_channel();

      if (!send_client_finished) {
        return;
      }
      if (!send_server_finished) {
        return;
      }

      // want_read() -> recv() -> would_block -> async_wait ->
      //   transfer() -> run()
      //
      if (splicer_->client_channel()->want_recv() != 0 ||
          splicer_->server_channel()->want_recv() != 0) {
        // some receive is wanted. Try to read and check if we can make some
        // progress.

        bool some_recv_finished{false};
        if (splicer_->client_channel()->want_recv() &&
            !splicer_->client_waiting_recv()) {
          const bool finished = recv_client_channel();
          some_recv_finished |= finished;
        }

        if (splicer_->server_channel()->want_recv() &&
            !splicer_->server_waiting_recv()) {
          const bool finished = recv_server_channel();
          some_recv_finished |= finished;
        }

        // no progress made even though it was requested, let's wait for
        // readiness.
        if (!some_recv_finished && before_state == splicer_->state()) {
#if defined(DEBUG_STATE)
          log_debug(
              "%d: sent everything, received nothing, no state-change (%s). "
              "Leaving.",
              __LINE__, BasicSplicer::state_to_string(splicer_->state()));
#endif
          return;
        }
      }
    }
  }

  void async_wait_client_recv() {
    using namespace std::chrono_literals;

#if defined(DEBUG_IO)
    log_debug("%d: waiting for the client::read(%d, ...)", __LINE__,
              conn_->client_socket().native_handle());
#endif
    splicer_->client_waiting_recv(true);

    if (splicer_->state() == State::CLIENT_GREETING) {
      // wait for the client to respond within 100ms
      client_read_timer_.expires_after(
          conn_->context().get_client_connect_timeout());

      client_read_timer_.async_wait(
          [self = this->shared_from_this()](std::error_code ec) {
            self->handle_client_read_timeout(ec);
          });
    }

    conn_->client_socket().async_wait(
        net::socket_base::wait_read,
        [self = this->shared_from_this()](std::error_code ec) {
          self->client_recv_ready(ec);
        });
  }

  void async_wait_server_recv() {
    using namespace std::chrono_literals;

#if defined(DEBUG_IO)
    log_debug("%d: waiting for the server::read(%d, ...)", __LINE__,
              conn_->server_socket().native_handle());
#endif
    splicer_->server_waiting_recv(true);

    if (splicer_->state() == State::SERVER_GREETING) {
      // should this timer include the time to connect() to the backend?
      server_read_timer_.expires_after(
          conn_->context().get_destination_connect_timeout());

      server_read_timer_.async_wait(
          [self = this->shared_from_this()](std::error_code ec) {
            self->handle_server_read_timeout(ec);
          });
    }

    conn_->server_socket().async_wait(
        net::socket_base::wait_read,
        [self = this->shared_from_this()](std::error_code ec) {
          self->server_recv_ready(ec);
        });
  }

  void async_wait_client_send() {
    using namespace std::chrono_literals;

#if defined(DEBUG_IO)
    log_debug("%d: waiting for the client::read(%d, ...)", __LINE__,
              conn_->client_socket().native_handle());
#endif
    splicer_->client_waiting_send(true);

    conn_->client_socket().async_wait(
        net::socket_base::wait_write,
        [self = this->shared_from_this()](std::error_code ec) {
          self->client_send_ready(ec);
        });
  }

  void async_wait_server_send() {
    using namespace std::chrono_literals;

#if defined(DEBUG_IO)
    log_debug("%d: waiting for the server::read(%d, ...)", __LINE__,
              conn_->server_socket().native_handle());
#endif
    splicer_->server_waiting_send(true);

    conn_->server_socket().async_wait(
        net::socket_base::wait_write,
        [self = this->shared_from_this()](std::error_code ec) {
          self->server_send_ready(ec);
        });
  }

  void async_run() {
    conn_->connected();

    // set the initial state of the state-machine.
    splicer_->start();

    net::defer(conn_->client_socket().get_executor(),
               [self = this->shared_from_this()]() { self->run(); });
  }

 private:
  /**
   * one side of the conneciton closed.
   */
  State finish() {
    if (!splicer_->handshake_done()) {
      log_info("[%s] %s closed connection before finishing handshake",
               conn_->context().get_name().c_str(),
               mysqlrouter::to_string(conn_->client_endpoint()).c_str());
      conn_->context().template block_client_host<ClientProtocol>(
          conn_->client_endpoint());

      if (conn_->client_socket().is_open()) {
        std::vector<uint8_t> buf;
        const auto encode_res = splicer_->on_block_client_host(buf);

        if (!encode_res) {
          log_debug("[%s] fd=%d -- %d: encoding final-handshake failed: %s",
                    conn_->context().get_name().c_str(),
                    conn_->client_socket().native_handle(),
                    conn_->server_socket().native_handle(),
                    encode_res.error().message().c_str());
        } else {
          const auto write_res =
              net::write(conn_->server_socket(), net::buffer(buf));

          if (!write_res) {
            log_debug("[%s] fd=%d -- %d: writing final-handshake failed: %s",
                      conn_->context().get_name().c_str(),
                      conn_->client_socket().native_handle(),
                      conn_->server_socket().native_handle(),
                      write_res.error().message().c_str());
          }
        }
      }
    }

    // Either client or server terminated

    log_debug("[%s] fd=%d -- %d: connection closed (up: %zub; down: %zub)",
              conn_->context().get_name().c_str(),
              conn_->client_socket().native_handle(),
              conn_->server_socket().native_handle(), conn_->get_bytes_up(),
              conn_->get_bytes_down());

    if (conn_->client_socket().is_open()) {
      conn_->client_socket().shutdown(net::socket_base::shutdown_send);
      conn_->client_socket().close();
    }

    if (conn_->server_socket().is_open()) {
      conn_->server_socket().shutdown(net::socket_base::shutdown_send);
      conn_->server_socket().close();
    }

    conn_->context().decrease_info_active_routes();

    return State::DONE;
  }

  MySQLRoutingConnection<client_protocol_type, server_protocol_type> *conn_;
  std::unique_ptr<BasicSplicer> splicer_;

  size_t max_read_size_;

  net::steady_timer client_read_timer_{
      conn_->client_socket().get_executor().context()};
  net::steady_timer server_read_timer_{
      conn_->server_socket().get_executor().context()};
};

#endif
