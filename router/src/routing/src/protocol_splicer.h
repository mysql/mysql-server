/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates.

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
#include <functional>  // bind
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

  template <bool to_server>
  void transfer(std::error_code ec) {
    // cancel timers before they interrupt us.
    if (to_server) {
      client_read_timer_.cancel();
    } else {
      server_read_timer_.cancel();
    }
    if (ec == std::errc::operation_canceled) {
      if (splicer_->state() != State::DONE) splicer_->state(finish());
      return;
    }

    run<to_server>();
  }

  template <bool to_server>
  void handle_timeout(std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      return;
    }

    // timeout fired.
    if (to_server) {
      conn_->client_socket().cancel();
    } else {
      conn_->server_socket().cancel();
    }
  }

  /**
   * write the send-buffer from a channel to a socket.
   *
   * data that is successfully written to the socket and removed from the
   * send-buffer.
   *
   * @param sock a connected socket.
   * @param channel a channel with a send-buffer.
   */
  template <class Socket>
  static stdx::expected<size_t, std::error_code> send_to_socket(
      Socket &sock, Channel *channel) {
    size_t transferred{};

    while (!channel->send_buffer().empty()) {
      // if there is data to send to the client, send it.
      const auto write_res =
          net::write(sock, net::dynamic_buffer(channel->send_buffer()));
      if (!write_res) {
        if (write_res.error() ==
                make_error_condition(std::errc::operation_would_block) &&
            transferred != 0) {
          return transferred;
        }

        return write_res;
      } else {
        transferred += write_res.value();
      }
    }
    return transferred;
  }

  template <bool to_server>
  void run() {
#if 0
    if (to_server) {
      log_debug("%d: -- transfer to server: %d -> %d", __LINE__,
                conn_->client_socket().native_handle(),
                conn_->server_socket().native_handle());
    } else {
      log_debug("%d: -- transfer to client: %d <- %d", __LINE__,
                conn_->client_socket().native_handle(),
                conn_->server_socket().native_handle());
    }
#endif

    while (true) {
      // if there is anything to send to the client, send it now.
      if (!splicer_->client_channel()->send_buffer().empty()) {
        const auto send_res =
            send_to_socket(conn_->client_socket(), splicer_->client_channel());
        if (send_res) {
          conn_->transfered_to_client(send_res.value());
        } else if (send_res.error() ==
                   make_error_condition(std::errc::broken_pipe)) {
          splicer_->client_channel()->send_buffer().clear();
          splicer_->state(State::FINISH);
        } else {
          const auto ec = send_res.error();
          log_info("client::write() failed: %s (%s:%d)", ec.message().c_str(),
                   ec.category().name(), ec.value());
          splicer_->state(State::FINISH);
        }
      }

      // if there is anything to send to the server, send it now.
      if (!splicer_->server_channel()->send_buffer().empty()) {
        const auto send_res =
            send_to_socket(conn_->server_socket(), splicer_->server_channel());
        if (send_res) {
          conn_->transfered_to_server(send_res.value());
        } else if (send_res.error() ==
                   make_error_condition(std::errc::broken_pipe)) {
          splicer_->server_channel()->send_buffer().clear();
          splicer_->state(State::FINISH);
        } else {
          const auto ec = send_res.error();
          log_info("server::write() failed: %s (%s:%d)", ec.message().c_str(),
                   ec.category().name(), ec.value());
          splicer_->state(State::FINISH);
        }
      }

      // if we want to recv something from the client, read
      if (splicer_->client_channel()->want_recv() &&
          (!splicer_->client_waiting() || to_server)) {
#if 0
        log_debug("%d: reading from client %d", __LINE__,
                  conn_->client_socket().native_handle());
#endif
        splicer_->client_waiting(false);
        auto want_read = splicer_->client_channel()->want_recv();
        auto read_res = net::read(
            conn_->client_socket(),
            net::dynamic_buffer(splicer_->client_channel()->recv_buffer()),
            net::transfer_at_least(want_read));
        if (!read_res) {
#if 0
          log_debug("%d: read from client %d failed: %s", __LINE__,
                    conn_->client_socket().native_handle(),
                    read_res.error().message().c_str());
#endif
          if (read_res.error() ==
                  make_error_condition(
                      std::errc::resource_unavailable_try_again) ||
              read_res.error() ==
                  make_error_condition(std::errc::operation_would_block)) {
            async_wait_client();
            return;
          } else if (read_res.error() == net::stream_errc::eof ||
                     read_res.error() ==
                         make_error_condition(std::errc::connection_reset) ||
                     read_res.error() ==
                         make_error_condition(std::errc::connection_aborted)) {
            splicer_->state(State::FINISH);
          } else {
            const auto ec = read_res.error();
            log_info("client::recv() failed: %s (%s:%d)", ec.message().c_str(),
                     ec.category().name(), ec.value());
            splicer_->state(State::FINISH);
          }
        } else {
#if 0
          log_debug("%d: read from client %d: %zu", __LINE__,
                    conn_->client_socket().native_handle(), read_res.value());
#endif
          want_read -= std::min(want_read, read_res.value());
          splicer_->client_channel()->want_recv(want_read);
        }
      }

      if (splicer_->server_channel()->want_recv() &&
          (!splicer_->server_waiting() || !to_server)) {
        splicer_->server_waiting(false);
        auto want_read = splicer_->server_channel()->want_recv();
#if 0
        log_debug("server::recv(%d, %zu)",
                  conn_->server_socket().native_handle(), want_read);
#endif
        auto read_res = net::read(
            conn_->server_socket(),
            net::dynamic_buffer(splicer_->server_channel()->recv_buffer()),
            net::transfer_at_least(want_read));
        if (!read_res) {
#if 0
          log_debug("server::recv() failed: %s",
                    read_res.error().message().c_str());
#endif
          if (read_res.error() ==
                  make_error_condition(
                      std::errc::resource_unavailable_try_again) ||
              read_res.error() ==
                  make_error_condition(std::errc::operation_would_block)) {
            async_wait_server();
            return;
          } else if (read_res.error() == net::stream_errc::eof) {
            splicer_->state(State::FINISH);
          } else {
            const auto ec = read_res.error();
            log_info("server::recv() failed: %s (%s:%d)", ec.message().c_str(),
                     ec.category().name(), ec.value());
            splicer_->state(State::FINISH);
          }
        } else {
#if 0
          log_debug("%d: read from server: %zu", __LINE__, read_res.value());
#endif
          want_read -= std::min(want_read, read_res.value());

          splicer_->server_channel()->want_recv(want_read);
        }
      }

#if 0
      log_debug("%d: state: %s", __LINE__,
                BasicSplicer::state_to_string(splicer_->state()));
#endif
      switch (splicer_->state()) {
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

          splicer_->state(splicer_->splice<true>());
          splicer_->state(splicer_->splice<false>());

          // initiate the
          //
          // - move client to server and
          // - move server to client
          //
          // ... but as we are always called in the context of a "c->s" or
          // "c<-s" we have to initiate the other direction manually.
          //
          // When we are 'to_server', the io-section above will set the
          // client-channel to async_wait_client(), but nothing would start the
          // server side with async_wait_server().

          if (!to_server && !splicer_->client_waiting() &&
              splicer_->client_channel()->want_recv()) {
            async_wait_client();
          }

          if (to_server && !splicer_->server_waiting() &&
              splicer_->server_channel()->want_recv()) {
            async_wait_server();
          }

          break;
        case State::SPLICE:
          splicer_->state(splicer_->splice<to_server>());
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
    }
  }

  void async_wait_client() {
    using namespace std::chrono_literals;

#if 0
    log_debug("%d: waiting for the client::read(%d, ...)", __LINE__,
              conn_->client_socket().native_handle());
#endif
    splicer_->client_waiting(true);

    if (splicer_->state() == State::CLIENT_GREETING) {
      // wait for the client to respond within 100ms
      client_read_timer_.expires_after(
          conn_->context().get_client_connect_timeout());

      client_read_timer_.async_wait(std::bind(
          &Splicer<ClientProtocol, ServerProtocol>::handle_timeout<true>,
          this->shared_from_this(), std::placeholders::_1));
    }

    conn_->client_socket().async_wait(
        net::socket_base::wait_read,
        std::bind(&Splicer<ClientProtocol, ServerProtocol>::transfer<true>,
                  this->shared_from_this(), std::placeholders::_1));
  }

  void async_wait_server() {
    using namespace std::chrono_literals;

#if 0
    log_debug("%d: waiting for the server::read(%d, ...)", __LINE__,
              conn_->server_socket().native_handle());
#endif
    splicer_->server_waiting(true);

    if (splicer_->state() == State::SERVER_GREETING) {
      // should this timer include the time to connect() to the backend?
      server_read_timer_.expires_after(
          conn_->context().get_destination_connect_timeout());

      server_read_timer_.async_wait(std::bind(
          &Splicer<ClientProtocol, ServerProtocol>::handle_timeout<false>,
          this->shared_from_this(), std::placeholders::_1));
    }

    conn_->server_socket().async_wait(
        net::socket_base::wait_read,
        std::bind(&Splicer<ClientProtocol, ServerProtocol>::transfer<false>,
                  this->shared_from_this(), std::placeholders::_1));
  }

  void async_run() {
    conn_->connected();

    const bool from_client = splicer_->start();
    if (from_client) {
      run<true>();
    } else {
      run<false>();
    }
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
