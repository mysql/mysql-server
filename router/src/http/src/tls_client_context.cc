/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "mysqlrouter/tls_client_context.h"

#include <openssl/ssl.h>

#include "openssl_version.h"
#include "tls_error.h"

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
#define TLS_CLIENT_METHOD() TLS_client_method()
#else
#define TLS_CLIENT_METHOD() SSLv23_client_method()
#endif

TlsClientContext::TlsClientContext() : TlsContext(TLS_CLIENT_METHOD()) {
  verify(TlsVerify::PEER);
}

void TlsClientContext::verify(TlsVerify verify) {
  int mode = 0;
  switch (verify) {
    case TlsVerify::NONE:
      mode = SSL_VERIFY_NONE;
      break;
    case TlsVerify::PEER:
      mode = SSL_VERIFY_PEER;
      break;
  }
  SSL_CTX_set_verify(ssl_ctx_.get(), mode, NULL);
}

void TlsClientContext::cipher_suites(const std::string &ciphers) {
// TLSv1.3 ciphers are controlled via SSL_CTX_set_ciphersuites()
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
  if (1 != SSL_CTX_set_ciphersuites(ssl_ctx_.get(), ciphers.c_str())) {
    throw TlsError("set-cipher-suites");
  }
#else
  throw std::invalid_argument(
      "::cipher_suites(" + ciphers +
      ") isn't implemented. Use .has_set_cipher_suites() "
      "to check before calling");
#endif
}

void TlsClientContext::cipher_list(const std::string &ciphers) {
  // TLSv1.3 ciphers are controlled via SSL_CTX_set_ciphersuites()
  if (1 != SSL_CTX_set_cipher_list(ssl_ctx_.get(), ciphers.c_str())) {
    throw TlsError("set-cipher-list");
  }
}
