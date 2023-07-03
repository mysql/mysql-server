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

#ifndef MYSQL_HARNESS_TLS_SERVER_CONTEXT_INCLUDED
#define MYSQL_HARNESS_TLS_SERVER_CONTEXT_INCLUDED

#include <array>
#include <bitset>
#include <string>
#include <vector>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_context.h"
#include "mysql/harness/tls_export.h"

namespace TlsVerifyOpts {
constexpr size_t kFailIfNoPeerCert = 1 << 0;
constexpr size_t kClientOnce = 1 << 1;
}  // namespace TlsVerifyOpts

/**
 * TLS Context for the server side.
 */
class HARNESS_TLS_EXPORT TlsServerContext : public TlsContext {
 public:
  /**
   * unacceptable ciphers.
   *
   * they are filtered out if set through cipher_list()
   */
  static constexpr std::array<const char *, 9> unacceptable_cipher_spec{
      "!aNULL", "!eNULL", "!EXPORT", "!MD5",  "!DES",
      "!RC2",   "!RC4",   "!PSK",    "!SSLv3"};

  /**
   * construct a TLS Context for server-side.
   */
  TlsServerContext(TlsVersion min_version = TlsVersion::TLS_1_2,
                   TlsVersion max_version = TlsVersion::AUTO);

  /**
   * load key and cert.
   *
   * cerifiticate is verified against the key
   *
   * @param cert_chain_file filename of a PEM file containing a certificate
   * @param private_key_file filename of a PEM file containing a key
   */
  stdx::expected<void, std::error_code> load_key_and_cert(
      const std::string &cert_chain_file, const std::string &private_key_file);

  /**
   * init temporary DH parameters.
   *
   * @param dh_params filename of a PEM file with DH parameters
   */
  stdx::expected<void, std::error_code> init_tmp_dh(
      const std::string &dh_params);

  /**
   * set cipher-list.
   *
   * list is filtered for unacceptable_cipher_spec
   *
   * @param ciphers colon separated list of ciphers
   *
   * @see openssl ciphers
   */
  stdx::expected<void, std::error_code> cipher_list(const std::string &ciphers);

  /**
   * set how cerifiticates should be verified.
   *
   * @param verify NONE or PEER
   * @param tls_opts extra options for PEER
   * @throws std::illegal_argument if verify is NONE and tls_opts is != 0
   */
  stdx::expected<void, std::error_code> verify(TlsVerify verify,
                                               std::bitset<2> tls_opts = 0);

  /**
   * default ciphers.
   */
  static std::vector<std::string> default_ciphers();
};

#endif
