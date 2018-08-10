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

#ifndef ROUTING_CONNECTION_INCLUDED
#define ROUTING_CONNECTION_INCLUDED

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "context.h"
#include "mysql_router_thread.h"
#include "protocol/base_protocol.h"
#include "tcp_address.h"

class MySQLRouting;
class MySQLRoutingContext;

namespace mysql_harness {
class PluginFuncEnv;
}

/**
 * @brief MySQLRoutingConnection represents a connection to MYSQL Server.
 *
 * The MySQLRoutingConnection wraps thread that handles traffic from one socket
 * to another.
 *
 * When instance of MySQLRoutingConnection object is created, remove callback
 * is passed, which is called when connection thread completes.
 */
class MySQLRoutingConnection {
 public:
  /**
   * @brief Creates and initializes connection object. It doesn't create
   *        new thread of execution. In order to create new thread of
   *        execution call start().
   *
   * @param context wrapper for common data used by all connection threads
   * @param client_socket socket used to send/receive data to/from client
   * @param client_addr address of the socket used to send/receive data to/from
   * client
   * @param server_socket socket used to send/receive data to/from server
   * @param server_address IP address and TCP port of MySQL Server
   * @param remove_callback called when thread finishes its execution to remove
   *        associated MySQLRoutingConnection from container. It must be called
   *        at the very end of thread execution
   */
  MySQLRoutingConnection(
      MySQLRoutingContext &context, int client_socket,
      const sockaddr_storage &client_addr, int server_socket,
      const mysql_harness::TCPAddress &server_address,
      std::function<void(MySQLRoutingConnection *)> remove_callback);

  /**
   * @brief Verify if client socket and server socket are valid.
   *
   * @return true if both client socket and server socket are valid, false
   * otherwise
   */
  bool check_sockets();

  /**
   * @brief creates new thread of execution which calls run()
   *
   * @param detached true if start() should not block until run thread is
   * completed, false otherwise
   */
  void start(bool detached = true);

  /** @brief Worker function for thread
   *
   * Worker function handling incoming connection from a MySQL client using
   * the poll-method.
   *
   * Errors are logged.
   *
   */
  void run();

  /**
   * @brief mark connection to disconnect as soon as possible
   */
  void disconnect() noexcept;

  /**
   * @brief Returns address of server to which connection is established.
   *
   * @return address of server
   */
  const mysql_harness::TCPAddress &get_server_address() const noexcept;

  /**
   * @brief Returns address of client which connected to router
   *
   * @return address of client
   */
  const std::string &get_client_address() const;

 private:
  /** @brief wrapper for common data used by all routing threads */
  MySQLRoutingContext &context_;
  /** @brief callback that is called when thread of execution completes */
  std::function<void(MySQLRoutingConnection *)> remove_callback_;
  /** @brief socket used to communicate with client */
  int client_socket_;
  /** @brief client's address */
  const sockaddr_storage client_addr_;
  /** @brief socket used to communicate with server */
  int server_socket_;
  mysql_harness::TCPAddress server_address_;
  /** @brief true if connection should be disconnected */
  std::atomic<bool> disconnect_{false};
  /** @brief address of the client */
  std::string client_address_;
  /** @brief run client thread which will service this new connection */
  static void *run_thread(void *context);
  /** @brief make address of client */
  static std::string make_client_address(
      int client_socket, const MySQLRoutingContext &context,
      mysql_harness::SocketOperationsBase *sock_op);
};

#endif /* ROUTING_CONNECTION_INCLUDED */
