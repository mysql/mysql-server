/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_TLS_ERROR_H_INCLUDED
#define MYSQL_HARNESS_TLS_ERROR_H_INCLUDED

#include <system_error>

#include <openssl/ssl.h>

#include "mysql/harness/tls_export.h"

static_assert(SSL_ERROR_WANT_READ != 0);

enum class TlsErrc {
  kWantRead = SSL_ERROR_WANT_READ,
  kWantWrite = SSL_ERROR_WANT_WRITE,
  kZeroReturn = SSL_ERROR_ZERO_RETURN,
};

enum class TlsCertErrc {
  kNoRSACert = 1,
  kNotACertificate,
  kRSAKeySizeToSmall,
};

namespace std {
template <>
struct is_error_code_enum<TlsErrc> : std::true_type {};

template <>
struct is_error_code_enum<TlsCertErrc> : std::true_type {};
}  // namespace std

/**
 * make std::error_code from TlsCertErrc.
 */
HARNESS_TLS_EXPORT std::error_code make_error_code(TlsCertErrc ec);

/**
 * make std::error_code from TlsErrc.
 */
HARNESS_TLS_EXPORT std::error_code make_error_code(TlsErrc ec);

/**
 * make a std::error_code from ERR_get_error().
 */
HARNESS_TLS_EXPORT std::error_code make_tls_error();

/**
 * make a std::error_code from SSL_get_error().
 *
 * @param ssl a SSL connection
 * @param res result of a SSL_ function.
 */
HARNESS_TLS_EXPORT std::error_code make_tls_ssl_error(const SSL *ssl, int res);

#endif
