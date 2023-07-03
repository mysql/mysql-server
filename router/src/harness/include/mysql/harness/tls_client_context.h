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

#ifndef MYSQL_HARNESS_TLS_CLIENT_CONTEXT_INCLUDED
#define MYSQL_HARNESS_TLS_CLIENT_CONTEXT_INCLUDED

#include "mysql/harness/tls_context.h"

#include <system_error>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_export.h"

/**
 * Client TLS Context.
 */
class HARNESS_TLS_EXPORT TlsClientContext : public TlsContext {
 public:
  TlsClientContext(TlsVerify mode = TlsVerify::PEER);

  /**
   * set cipher-list.
   *
   * for TLSv1.2-and-earlier ciphers.
   *
   * @param ciphers colon separated list of ciphers
   *
   * @note list is not filtered for unacceptable ciphers
   *
   * @see openssl ciphers
   * @see cipher_suites()
   */
  stdx::expected<void, std::error_code> cipher_list(const std::string &ciphers);

  /**
   * set cipher-suites of TLSv1.3.
   *
   * openssl 1.1.1 added support for TLSv1.3 and move setting those ciphers
   * to SSL_CTX_set_ciphersuites().
   *
   * @param ciphers colon separated list of ciphers. empty == empty, "DEFAULT"
   * is the default-set
   *
   * @note list is not filtered for unacceptable ciphers
   * @see openssl ciphers
   * @see has_set_cipher_suites()
   */
  stdx::expected<void, std::error_code> cipher_suites(
      const std::string &ciphers);

  /**
   * verification of certificates.
   */
  stdx::expected<void, std::error_code> verify(TlsVerify verify);

  /**
   * verify hostname.
   *
   * @param server_host hostname or ip-address to match in the certificate.
   */
  stdx::expected<void, std::error_code> verify_hostname(
      const std::string &server_host);
};

#endif
