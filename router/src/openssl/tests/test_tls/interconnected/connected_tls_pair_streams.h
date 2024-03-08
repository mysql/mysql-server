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

#ifndef ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TLS_PAIR_STREAMS_H_
#define ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TLS_PAIR_STREAMS_H_

#include <memory>

#include "mysql/harness/net_ts.h"
#include "test_tls/pair_stream.h"
#include "test_tls/tls/tls_test_contextes.h"
#include "tls/tls_stream.h"

class ConnectedTlsPairStreams {
 private:
  TlsTestContext tls_context_;

 public:
  using Stream = net::tls::TlsStream<Pair_stream>;
  using StreamPtr = std::unique_ptr<Stream>;

  void create_interconnected(net::io_context &context, StreamPtr &out_server,
                             StreamPtr &out_client) {
    Pair_stream socket1_{context};
    Pair_stream socket2_{context, &socket1_};

    out_server.reset(
        new Stream(&tls_context_.ssl_ctxt_server_, std::move(socket1_)));
    out_client.reset(
        new Stream(&tls_context_.ssl_ctxt_client_, std::move(socket2_)));
  }

  void change_non_blocking(const bool /*non_blocking*/) {
    // The Pair_stream doesn't support blocking/non blocking mode.
  }
};

#endif  // ROUTER_SRC_HTTP_TESTS_TEST_TLS_INTERCONNECTED_CONNECTED_TLS_PAIR_STREAMS_H_
