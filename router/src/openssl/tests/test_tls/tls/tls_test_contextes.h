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

#ifndef ROUTER_SRC_HTTP_TESTS_ASIO_TLS_SSL_TEST_CONTEXTES_H_
#define ROUTER_SRC_HTTP_TESTS_ASIO_TLS_SSL_TEST_CONTEXTES_H_

#include "mysql/harness/tls_client_context.h"
#include "mysql/harness/tls_server_context.h"

extern std::string g_data_dir;

class TlsTestContext {
 public:
  TlsTestContext() {
    auto result = ssl_ctxt_server_.load_key_and_cert(
        g_data_dir + "/server-key.pem", g_data_dir + "/server-cert.pem");
    if (!result) {
      throw std::runtime_error(result.error().message());
    }
  }

  TlsServerContext ssl_ctxt_server_;
  TlsClientContext ssl_ctxt_client_{TlsVerify::NONE};
};

#endif  // ROUTER_SRC_HTTP_TESTS_ASIO_TLS_SSL_TEST_CONTEXTES_H_
