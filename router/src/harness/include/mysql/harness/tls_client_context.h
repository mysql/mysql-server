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

#ifndef MYSQL_HARNESS_TLS_CLIENT_CONTEXT_INCLUDED
#define MYSQL_HARNESS_TLS_CLIENT_CONTEXT_INCLUDED

#include "mysql/harness/tls_context.h"

#include <chrono>
#include <list>
#include <mutex>
#include <system_error>
#include <vector>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_export.h"

/**
 * Client TLS Context.
 */
class HARNESS_TLS_EXPORT TlsClientContext : public TlsContext {
 public:
  struct SslSessionDeleter {
    void operator()(SSL_SESSION *sess) { SSL_SESSION_free(sess); }
  };
  using SslSession = std::unique_ptr<SSL_SESSION, SslSessionDeleter>;

  TlsClientContext(
      TlsVerify mode = TlsVerify::PEER, bool session_cache_mode = false,
      size_t session_cache_size = 0,
      std::chrono::seconds session_cache_timeout = std::chrono::seconds(0));

  TlsClientContext(const TlsClientContext &) = delete;
  TlsClientContext(TlsClientContext &&) = default;
  TlsClientContext &operator=(const TlsClientContext &) = delete;
  TlsClientContext &operator=(TlsClientContext &&) = default;
  ~TlsClientContext();

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

  /**
   * add session.
   */
  stdx::expected<void, std::error_code> add_session(SSL_SESSION *sess);
  /**
   * remove session.
   */
  stdx::expected<void, std::error_code> remove_session(SSL_SESSION *sess);
  /**
   * get session.
   */
  stdx::expected<SSL_SESSION *, std::error_code> get_session();

 private:
  struct Sessions {
    using SessionId = std::vector<uint8_t>;
    using SessionData = std::pair<SessionId, SslSession>;

    std::list<SessionData> sessions_;
    std::mutex mtx_;
  };
  std::unique_ptr<Sessions> sessions_;

  bool session_cache_mode_;
  size_t session_cache_size_;
  std::chrono::seconds session_cache_timeout_;
};

#endif
