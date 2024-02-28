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

#ifndef ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TCP_STREAMS_H_
#define ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TCP_STREAMS_H_

#include <memory>

#include "helpers/tcp_port_pool.h"
#include "mysql/harness/net_ts.h"

class ConnectedTcpStreams {
 public:
  using Stream = net::ip::tcp::socket;
  using StreamPtr = std::unique_ptr<Stream>;

  void create_interconnected(net::io_context &context, StreamPtr &out_server,
                             StreamPtr &out_client) {
    create_interconnected(context, context, out_server, out_client);
  }

  void create_interconnected(net::io_context &context1,
                             net::io_context &context2, StreamPtr &out_server,
                             StreamPtr &out_client) {
    auto endpoint_listen_addr = net::ip::make_address("127.0.0.1").value();
    net::ip::tcp::endpoint endpoint_listen{endpoint_listen_addr,
                                           port_pool_.get_next_available()};
    net::ip::tcp::acceptor acceptror(context2);
    Stream socket1{context1};
    Stream socket2{context2};

    socket1.open(net::ip::tcp::v4());
    acceptror.open(net::ip::tcp::v4());
    acceptror.local_endpoint();

    if (!acceptror.bind(endpoint_listen))
      throw std::runtime_error("ConnectedTcpStreams: bind failed");

    if (!acceptror.listen(10))
      throw std::runtime_error("ConnectedTcpStreams: listen failed");

    acceptror.async_accept(
        [&context2, &socket2](std::error_code ec, net::ip::tcp::socket s) {
          if (!ec) {
            socket2 = std::move(s);
          }
          context2.stop();
        });

    std::thread thread_accept{[&context2]() {
      auto guard = net::make_work_guard(context2.get_executor());
      while (!context2.stopped()) context2.run();
    }};

    if (!socket1.connect(acceptror.local_endpoint().value())) {
      throw std::runtime_error(
          "ConnectedTcpStreams: can't connect to other endpoint");
    }

    thread_accept.join();
    context2.restart();

    out_server.reset(new Stream(std::move(socket1)));
    out_client.reset(new Stream(std::move(socket2)));

    out_client->native_non_blocking(non_blocking_);
    out_server->native_non_blocking(non_blocking_);
  }

  void change_non_blocking(const bool non_blocking) {
    non_blocking_ = non_blocking;
  }
  void change_output(std::ostream *) {}

 private:
  bool non_blocking_{true};
  TcpPortPool port_pool_;
};

#endif  // ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TCP_STREAMS_H_
