/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <cstring>
#include <string>

#include "common.h"
#include "connection.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql_router_thread.h"
#include "mysql_routing_common.h"
#include "mysqlrouter/routing.h"
#include "utils.h"
IMPORT_LOG_FUNCTIONS()

MySQLRoutingConnection ::MySQLRoutingConnection(
    MySQLRoutingContext &context, int client_socket,
    const sockaddr_storage &client_addr, int server_socket,
    const mysql_harness::TCPAddress &server_address,
    std::function<void(MySQLRoutingConnection *)> remove_callback)
    : context_(context),
      remove_callback_(remove_callback),
      client_socket_(client_socket),
      client_addr_(client_addr),
      server_socket_(server_socket),
      server_address_(server_address),
      client_address_(make_client_address(client_socket, context,
                                          context_.get_socket_operations())) {}

void MySQLRoutingConnection::start(bool detached) {
  try {
    // both lines can throw std::runtime_error
    mysql_harness::MySQLRouterThread connect_thread(
        context_.get_thread_stack_size());
    connect_thread.run(&run_thread, this, detached);
  } catch (std::runtime_error &err) {
    context_.get_protocol().send_error(
        client_socket_, 1040,
        "Router couldn't spawn a new thread to service new client connection",
        "08004", context_.get_name());
    context_.get_socket_operations()->close(
        client_socket_);  // no shutdown() before close()

    // we only want to log this message once, because in a low-resource
    // situation, this would lead do a DoS against ourselves (heavy I/O and disk
    // full)
    static bool logged_this_before = false;
    if (!logged_this_before) {
      logged_this_before = true;
      log_error(
          "Couldn't spawn a new thread to service new client connection from "
          "%s."
          " This message will not be logged again until Router restarts, "
          "error=%s",
          client_address_.c_str(), err.what());
    }
  }
}

void *MySQLRoutingConnection::run_thread(void *context) {
  MySQLRoutingConnection *connection(
      static_cast<MySQLRoutingConnection *>(context));
  connection->run();
  return nullptr;
}

bool MySQLRoutingConnection::check_sockets() {
  if ((server_socket_ == routing::kInvalidSocket) ||
      (client_socket_ == routing::kInvalidSocket)) {
    std::stringstream os;
    os << "Can't connect to remote MySQL server for client connected to '"
       << context_.get_bind_address().addr << ":"
       << context_.get_bind_address().port << "'";

    log_warning("[%s] fd=%d %s", context_.get_name().c_str(), client_socket_,
                os.str().c_str());

    // at this point, it does not matter whether client gets the error
    context_.get_protocol().send_error(client_socket_, 2003, os.str(), "HY000",
                                       context_.get_name());

    if (client_socket_ != routing::kInvalidSocket)
      context_.get_socket_operations()->shutdown(client_socket_);
    if (server_socket_ != routing::kInvalidSocket)
      context_.get_socket_operations()->shutdown(server_socket_);

    if (client_socket_ != routing::kInvalidSocket) {
      context_.get_socket_operations()->close(client_socket_);
    }
    if (server_socket_ != routing::kInvalidSocket) {
      context_.get_socket_operations()->close(server_socket_);
    }
    return false;
  }

  return true;
}

void MySQLRoutingConnection::run() {
  mysql_harness::rename_thread(
      get_routing_thread_name(context_.get_name(), "RtC")
          .c_str());  // "Rt client thread" would be too long :(

  context_.increase_active_thread_counter();
  std::shared_ptr<void> thread_exit_guard(nullptr, [&](void *) {
    context_.decrease_active_thread_counter();

    // remove callback has to be executed as a last thing in connection
    remove_callback_(this);
  });

  std::size_t bytes_down = 0;
  std::size_t bytes_up = 0;
  std::size_t bytes_read = 0;
  std::string extra_msg = "";
  RoutingProtocolBuffer buffer(context_.get_net_buffer_length());
  bool handshake_done = false;

  if (!check_sockets()) {
    return;
  }

  log_debug("[%s] fd=%d connected %s -> %s as fd=%d",
            context_.get_name().c_str(), client_socket_,
            client_address_.c_str(), get_server_address().str().c_str(),
            server_socket_);

  context_.increase_info_active_routes();
  context_.increase_info_handled_routes();

  int pktnr = 0;

  bool connection_is_ok = true;
  while (connection_is_ok && !disconnect_) {
    const size_t kClientEventIndex = 0;
    const size_t kServerEventIndex = 1;

    struct pollfd fds[] = {
        {routing::kInvalidSocket, POLLIN, 0},
        {routing::kInvalidSocket, POLLIN, 0},
    };

    fds[kClientEventIndex].fd = client_socket_;
    fds[kServerEventIndex].fd = server_socket_;

    const std::chrono::milliseconds poll_timeout_ms =
        handshake_done ? std::chrono::milliseconds(1000)
                       : context_.get_client_connect_timeout();
    int res = context_.get_socket_operations()->poll(
        fds, sizeof(fds) / sizeof(fds[0]), poll_timeout_ms);

    if (res < 0) {
      const int last_errno = context_.get_socket_operations()->get_errno();
      switch (last_errno) {
        case EINTR:
        case EAGAIN:
          // got interrupted. Just retry
          break;
        default:
          // break the loop, something ugly happened
          connection_is_ok = false;
          extra_msg = std::string(
              "poll() failed: " +
              mysqlrouter::to_string(get_message_error(last_errno)));
          break;
      }

      continue;
    } else if (res == 0) {
      // timeout
      if (!handshake_done) {
        connection_is_ok = false;
        extra_msg = std::string("client auth timed out");

        break;
      } else {
        continue;
      }
    }

    // something happened on the socket: either we have data or the socket was
    // closed.
    //
    // closed sockets are signalled in two ways:
    //
    // * Linux: POLLIN + read() == 0
    // * Windows: POLLHUP

    const bool client_is_readable =
        (fds[kClientEventIndex].revents & (POLLIN | POLLHUP)) != 0;
    const bool server_is_readable =
        (fds[kServerEventIndex].revents & (POLLIN | POLLHUP)) != 0;

    // Handle traffic from Server to Client
    // Note: In classic protocol Server _always_ talks first
    if (context_.get_protocol().copy_packets(
            server_socket_, client_socket_, server_is_readable, buffer, &pktnr,
            handshake_done, &bytes_read, true) == -1) {
      const int last_errno = context_.get_socket_operations()->get_errno();
      if (last_errno > 0) {
        // if read() against closed socket, errno will be 0. Don't log that.
        extra_msg =
            std::string("Copy server->client failed: " +
                        mysqlrouter::to_string(get_message_error(last_errno)));
      }

      connection_is_ok = false;
    } else {
      bytes_up += bytes_read;
    }

    // Handle traffic from Client to Server
    if (context_.get_protocol().copy_packets(
            client_socket_, server_socket_, client_is_readable, buffer, &pktnr,
            handshake_done, &bytes_read, false) == -1) {
      const int last_errno = context_.get_socket_operations()->get_errno();
      if (last_errno > 0) {
        extra_msg =
            std::string("Copy client->server failed: " +
                        mysqlrouter::to_string(get_message_error(last_errno)));
      } else if (!handshake_done) {
        extra_msg = std::string(
            "Copy client->server failed: unexpected connection close");
      }
      // client close on us.
      connection_is_ok = false;
    } else {
      bytes_down += bytes_read;
    }

  }  // while (connection_is_ok && !disconnect_.load())

  if (!handshake_done) {
    log_info("[%s] fd=%d Pre-auth socket failure %s: %s",
             context_.get_name().c_str(), client_socket_,
             client_address_.c_str(), extra_msg.c_str());
    auto ip_array = in_addr_to_array(client_addr_);
    context_.block_client_host(ip_array, client_address_.c_str(),
                               server_socket_);
  }

  // Either client or server terminated
  context_.get_socket_operations()->shutdown(client_socket_);
  context_.get_socket_operations()->shutdown(server_socket_);
  context_.get_socket_operations()->close(client_socket_);
  context_.get_socket_operations()->close(server_socket_);

  context_.decrease_info_active_routes();
#ifndef _WIN32
  log_debug("[%s] fd=%d connection closed (up: %zub; down: %zub) %s",
            context_.get_name().c_str(), client_socket_, bytes_up, bytes_down,
            extra_msg.c_str());
#else
  log_debug("[%s] fd=%d connection closed (up: %Iub; down: %Iub) %s",
            context_.get_name().c_str(), client_socket_, bytes_up, bytes_down,
            extra_msg.c_str());
#endif
}

void MySQLRoutingConnection::disconnect() noexcept { disconnect_ = true; }

const mysql_harness::TCPAddress &MySQLRoutingConnection::get_server_address()
    const noexcept {
  return server_address_;
}

const std::string &MySQLRoutingConnection::get_client_address() const {
  return client_address_;
}

std::string MySQLRoutingConnection::make_client_address(
    int client_socket, const MySQLRoutingContext &context,
    mysql_harness::SocketOperationsBase *sock_op) {
  std::pair<std::string, int> c_ip = get_peer_name(client_socket, sock_op);

  if (c_ip.second == 0) {
    // Unix socket/Windows Named pipe
    return context.get_bind_named_socket().c_str();
  } else {
    std::ostringstream oss;
    oss << c_ip.first.c_str() << ":" << c_ip.second;
    return oss.str();
  }
}
