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

#ifndef MYSQLROUTER_TLS_CLIENT_CONTEXT_INCLUDED
#define MYSQLROUTER_TLS_CLIENT_CONTEXT_INCLUDED

#include "mysqlrouter/tls_context.h"

#include "mysqlrouter/http_client_export.h"

/**
 * Client TLS Context.
 */
class HTTP_CLIENT_EXPORT TlsClientContext : public TlsContext {
 public:
  TlsClientContext();

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
  void cipher_list(const std::string &ciphers);

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
   * @throws std::invalid_argument if the API doesn't support it
   * @see has_set_cipher_suites()
   */
  void cipher_suites(const std::string &ciphers);

  /**
   * verification of certificates.
   */
  void verify(TlsVerify verify);
};

#endif
