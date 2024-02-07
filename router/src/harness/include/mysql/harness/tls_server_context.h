/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_TLS_SERVER_CONTEXT_INCLUDED
#define MYSQL_HARNESS_TLS_SERVER_CONTEXT_INCLUDED

#include <array>
#include <string>
#include <vector>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/flags.h"
#include "mysql/harness/tls_context.h"
#include "mysql/harness/tls_export.h"

enum class TlsVerifyOpts {
  kFailIfNoPeerCert = 0,
  kClientOnce = 1,
};

namespace stdx {
template <>
struct is_flags<TlsVerifyOpts> : std::true_type {};
}  // namespace stdx

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
  static constexpr std::array<const char *, 12> unacceptable_cipher_spec{
      "!aNULL", "!eNULL", "!EXPORT", "!LOW", "!MD5", "!DES",
      "!3DES",  "!RC2",   "!RC4",    "!PSK", "!kDH", "!SSLv3"};

  /**
   * construct a TLS Context for server-side.
   */
  TlsServerContext(TlsVersion min_version = TlsVersion::TLS_1_2,
                   TlsVersion max_version = TlsVersion::AUTO,
                   bool session_cache_mode = false,
                   size_t session_cache_size = 0,
                   unsigned int session_cache_timeout = 0);

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
  stdx::expected<void, std::error_code> verify(
      TlsVerify verify, stdx::flags<TlsVerifyOpts> tls_opts = {});

  /**
   * get the security level.
   *
   * | sec-level | RSA-min-key-size |
   * +-----------+------------------+
   * |         1 |             1024 |
   * |         2 |             2048 |
   * |         3 |             3072 |
   * |         4 |             7680 |
   * |         5 |            15360 |
   *
   * @see SSL_CTX_get_security_level()
   *
   * @returns the security level of the ssl-ctx.
   */
  int security_level() const;

  /**
   * default ciphers.
   */
  static std::vector<std::string> default_ciphers();

  /**
   * set the session-id context for ssl-context reuse.
   *
   * unique identifier of the ssl-ctx.
   *
   * @param sid_ctx opaque string of size sid_ctx_len
   * @param sid_ctx_len length of sid_ctx_len
   */
  stdx::expected<void, std::error_code> session_id_context(
      const unsigned char *sid_ctx, unsigned int sid_ctx_len);
};

#endif
