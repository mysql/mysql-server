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

#ifndef MYSQL_HARNESS_TLS_CONTEXT_INCLUDED
#define MYSQL_HARNESS_TLS_CONTEXT_INCLUDED

#include "mysql/harness/tls_export.h"

#include <memory>  // unique_ptr
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
// include windows headers before openssl/ssl.h
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <openssl/ssl.h>  // SSL_METHOD

#include "mysql/harness/stdx/expected.h"

/**
 * TLS Versions.
 *
 * used for set_min_version.
 *
 * @note for now own TLS1.2 is used, but others may be added later.
 */
enum class TlsVersion { AUTO, SSL_3, TLS_1_0, TLS_1_1, TLS_1_2, TLS_1_3 };

/**
 * Verification of Cerifiticates.
 *
 * NONE no certificate is verified
 * PEER verify the cert of the peer
 */
enum class TlsVerify { NONE, PEER };

class HARNESS_TLS_EXPORT TlsLibraryContext {
 public:
  TlsLibraryContext();
  TlsLibraryContext(const TlsLibraryContext &) = delete;
  TlsLibraryContext(TlsLibraryContext &&) = delete;
  TlsLibraryContext &operator=(const TlsLibraryContext &) = delete;
  TlsLibraryContext &operator=(TlsLibraryContext &&) = delete;
  ~TlsLibraryContext();
};

/**
 * wraps SSL_CTX.
 *
 * TODO:
 * - SSL_CTX_set_session_cache_mode()
 * - SSL_CTX_set_alpn_select_cb()
 * - SSL_CTX_set_tlsext_ticket_key_cb()
 * - SSL_CTX_set_session_id_context()
 * - SSL_CTX_set_tlsext_servername_callback() for SNI
 * - SSL_CTX_set_cert_verify_callback() vs. SSL_CTX_set_verify()
 *
 */
class HARNESS_TLS_EXPORT TlsContext {
 public:
  /**
   * if TLS context allows to change elliptic curves list.
   *
   * @returns if curves_list() is supported.
   * @retval false curves_list() is not supported
   */
  static constexpr bool has_set_curves_list() {
    // 1.0.2 and later
    return OPENSSL_VERSION_NUMBER >= 0x1000200f;
  }

  /**
   * if TLS context allows setting cipher-suites (TLSv1.3 and later).
   *
   * @returns if cipher_suites() is supported.
   * @retval false cipher_suites() is not supported
   */
  static constexpr bool has_set_cipher_suites() {
    // 1.1.1 and later
    return OPENSSL_VERSION_NUMBER >= 0x1010100f;
  }

  /**
   * if TLS context allows getting cipher-lists.
   *
   * @returns if cipher_list() is supported.
   * @retval false cipher_list() is not supported
   */
  static constexpr bool has_get_cipher_list() {
    // 1.1.0 and later
    return OPENSSL_VERSION_NUMBER >= 0x1010000f;
  }

  /**
   * construct a TlsContext based on the SSL_METHODs provided by openssl.
   */
  explicit TlsContext(const SSL_METHOD *method);

  /**
   * set CA file and CA directory.
   *
   * Search-order:
   *
   * 1. ca_file (if not empty)
   * 2. all PEMs in ca_dir (if not empty)
   *
   * @see SSL_CTX_load_verify_locations
   *
   * @param ca_file path to a PEM file containing a certificate of a CA, ignored
   * if empty()
   * @param ca_path path to a directory of PEM files containing certifications,
   * ignored if empty() of CAs
   *
   * @returns success
   * @retval false if both ca_file and ca_path are empty
   */
  stdx::expected<void, std::error_code> ssl_ca(const std::string &ca_file,
                                               const std::string &ca_path);

  /**
   * set CRL file and CRL directory.
   *
   * Search-order:
   *
   * 1. crl_file (if not empty)
   * 2. all PEMs in crl_dir (if not empty)
   *
   * @see X509_STORE_load_locations
   *
   * @param crl_file path to a PEM file containing CRL file,
   * ignored if empty()
   * @param crl_path path to a directory of PEM files containing CRL files,
   * ignored if empty()
   *
   * @returns success
   * @retval false if both ca_file and ca_path are empty
   */
  stdx::expected<void, std::error_code> crl(const std::string &crl_file,
                                            const std::string &crl_path);

  /**
   * get non-owning pointer to SSL_CTX.
   */
  SSL_CTX *get() const { return ssl_ctx_.get(); }

  /**
   * set the supported TLS version range.
   */
  stdx::expected<void, std::error_code> version_range(TlsVersion min_version,
                                                      TlsVersion max_version);

  /**
   * get the min TLS version.
   */
  TlsVersion min_version() const;

  /**
   * init elliptic curves for DH ciphers for Perfect Forward Security.
   *
   * @note uses P-512, P-384 or P-256
   * @see RFC 5480
   * @see has_curves()
   *
   * @param curves colon-separated names of curves
   * @throws TlsError
   * @throws std::invalid_argument if API isn't supported
   * @see has_set_curves_list()
   */
  stdx::expected<void, std::error_code> curves_list(const std::string &curves);

  /**
   * get current cipher-list.
   *
   * @throws std::invalid_argument if API isn't supported
   * @see has_get_cipher_list()
   */
  std::vector<std::string> cipher_list() const;

  using InfoCallback = void (*)(const SSL *, int, int);

  /**
   * set info callback.
   */
  void info_callback(InfoCallback);

  /**
   * get info callback
   */
  InfoCallback info_callback() const;

  /**
   * get security_level.
   */
  int security_level() const;

 protected:
  std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> ssl_ctx_{nullptr,
                                                             &SSL_CTX_free};
};

#endif
