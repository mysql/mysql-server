/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/tls_client_context.h"

#include <openssl/ssl.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "openssl_version.h"

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
#define TLS_CLIENT_METHOD() TLS_client_method()
#else
#define TLS_CLIENT_METHOD() SSLv23_client_method()
#endif

TlsClientContext::TlsClientContext(TlsVerify mode)
    : TlsContext(TLS_CLIENT_METHOD()) {
  verify(mode);
}

stdx::expected<void, std::error_code> TlsClientContext::verify(
    TlsVerify verify) {
  int mode = 0;
  switch (verify) {
    case TlsVerify::NONE:
      mode = SSL_VERIFY_NONE;
      break;
    case TlsVerify::PEER:
      mode = SSL_VERIFY_PEER;
      break;
  }

  SSL_CTX_set_verify(ssl_ctx_.get(), mode, nullptr);

  return {};
}

stdx::expected<void, std::error_code> TlsClientContext::cipher_suites(
    const std::string &ciphers) {
// TLSv1.3 ciphers are controlled via SSL_CTX_set_ciphersuites()
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
  if (1 != SSL_CTX_set_ciphersuites(ssl_ctx_.get(), ciphers.c_str())) {
    return stdx::make_unexpected(make_tls_error());
  }
#else
  (void)ciphers;
  return stdx::make_unexpected(
      make_error_code(std::errc::function_not_supported));
#endif

  return {};
}

stdx::expected<void, std::error_code> TlsClientContext::cipher_list(
    const std::string &ciphers) {
  // TLSv1.3 ciphers are controlled via SSL_CTX_set_ciphersuites()
  if (1 != SSL_CTX_set_cipher_list(ssl_ctx_.get(), ciphers.c_str())) {
    return stdx::make_unexpected(make_tls_error());
  }

  return {};
}

stdx::expected<void, std::error_code> TlsClientContext::verify_hostname(
    const std::string &server_host) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  // get0_param() is added in openssl 1.0.2
  auto *ssl_ctx = ssl_ctx_.get();

  X509_VERIFY_PARAM *param = SSL_CTX_get0_param(ssl_ctx);
  /*
    As we don't know if the server_host contains IP addr or hostname
    call X509_VERIFY_PARAM_set1_ip_asc() first and if it returns an error
    (not valid IP address), call X509_VERIFY_PARAM_set1_host().
  */
  if (1 != X509_VERIFY_PARAM_set1_ip_asc(param, server_host.c_str())) {
    if (1 != X509_VERIFY_PARAM_set1_host(param, server_host.c_str(), 0)) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }
  }
#else
  (void)server_host;
  return stdx::make_unexpected(
      make_error_code(std::errc::function_not_supported));
#endif
  return {};
}
