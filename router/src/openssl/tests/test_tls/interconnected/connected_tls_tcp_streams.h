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

#ifndef ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TLS_TCP_STREAMS_H_
#define ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TLS_TCP_STREAMS_H_

#include <memory>
#include <string>

#include "test_tls/interconnected/connected_tcp_streams.h"
#include "test_tls/tls/tls_test_contextes.h"
#include "tls/tls_stream.h"
#ifdef CONNECTION_TLS_TCP_STREAM_MONITOR
#include "test_tls/trace_stream.h"
#endif

class ConnectedTlsTcpStreams {
 private:
  TlsTestContext tls_context_;

#ifdef CONNECTION_TLS_TCP_STREAM_MONITOR
  class NameRawLayer {
   public:
    const char *get_name() { return value_.c_str(); }
    void set_name(std::string value) { value_ = std::move(value); }

   private:
    std::string value_{"RawLayer"};
  };

  class NameSslLayer {
   public:
    const char *get_name() { return value_.c_str(); }
    void set_name(std::string value) { value_ = std::move(value); }

   private:
    std::string value_{"SslLayer"};
  };
#endif  // CONNECTION_TLS_TCP_STREAM_MONITOR

 public:
#ifdef CONNECTION_TLS_TCP_STREAM_MONITOR
  using Stream =
      TraceStream<NameSslLayer,
                  net::tls::TlsStream<
                      TraceStream<NameRawLayer, ConnectedTcpStreams::Stream>>>;
#else
  using Stream = net::tls::TlsStream<ConnectedTcpStreams::Stream>;
#endif  // CONNECTION_TLS_TCP_STREAM_MONITOR
  using StreamPtr = std::unique_ptr<Stream>;

  void create_interconnected(net::io_context &context, StreamPtr &out_server,
                             StreamPtr &out_client) {
    create_interconnected(context, context, out_server, out_client);
  }

  void create_interconnected(net::io_context &context1,
                             net::io_context &context2, StreamPtr &out_server,
                             StreamPtr &out_client) {
    ConnectedTcpStreams::StreamPtr socket1;
    ConnectedTcpStreams::StreamPtr socket2;

    tcp_stream_.create_interconnected(context1, context2, socket1, socket2);

#ifdef CONNECTION_TLS_TCP_STREAM_MONITOR
    out_server.reset(new Stream(out_, &tls_context_.ssl_ctxt_server_, out_,
                                std::move(*socket1)));
    out_client.reset(new Stream(out_, &tls_context_.ssl_ctxt_client_, out_,
                                std::move(*socket2)));

    out_server->set_name("Server/Tls");
    out_server->lower_layer().lower_layer().set_name("Server/Raw");
    out_client->set_name("Client/Tls");
    out_client->lower_layer().lower_layer().set_name("Client/Raw");
#else
    out_server.reset(
        new Stream(&tls_context_.ssl_ctxt_server_, std::move(*socket1)));
    out_client.reset(
        new Stream(&tls_context_.ssl_ctxt_client_, std::move(*socket2)));
#endif  // CONNECTION_TLS_TCP_STREAM_MONITOR
  }

  void change_output(std::ostream *out) { out_ = out; }

  void change_non_blocking(const bool non_blocking) {
    tcp_stream_.change_non_blocking(non_blocking);
  }

 private:
  std::ostream *out_ = &std::cout;
  ConnectedTcpStreams tcp_stream_;
};

#endif  // ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TLS_TCP_STREAMS_H_
