/*
  Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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

#include "mysql/harness/tls_context.h"

#include <array>
#include <string>
#include <vector>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysql/harness/tls_types.h"
#include "openssl_version.h"

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
#include <openssl/core_names.h>  // OSSL_PKEY_...
#include <openssl/decoder.h>     // OSSL_DECODER...
#endif

constexpr int kMinRsaKeySize{2048};

TlsLibraryContext::TlsLibraryContext() {
  OPENSSL_init_ssl(0, nullptr);
  SSL_load_error_strings();
  ERR_load_crypto_strings();
}

TlsLibraryContext::~TlsLibraryContext() {
// in case any of this is needed for cleanup
#if 0
  FIPS_mode_set(0);

  SSL_COMP_free_compression_methods();

  ENGINE_cleanup();

  CONF_modules_free();
  CONF_modules_unload(1);

  COMP_zlib_cleanup();

  ERR_free_strings();
  EVP_cleanup();

  CRYPTO_cleanup_all_ex_data();
#endif
}

TlsContext::TlsContext(const SSL_METHOD *method)
    : ssl_ctx_{SSL_CTX_new(const_cast<SSL_METHOD *>(method)), &SSL_CTX_free} {
  // SSL_CTX_new may fail if ciphers aren't loaded.
}

stdx::expected<void, std::error_code> TlsContext::ssl_ca(
    const std::string &ca_file, const std::string &ca_path) {
  if (!ssl_ctx_) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  if (1 != SSL_CTX_load_verify_locations(
               ssl_ctx_.get(), ca_file.empty() ? nullptr : ca_file.c_str(),
               ca_path.empty() ? nullptr : ca_path.c_str())) {
    return stdx::unexpected(make_tls_error());
  }
  return {};
}

stdx::expected<void, std::error_code> TlsContext::crl(
    const std::string &crl_file, const std::string &crl_path) {
  if (!ssl_ctx_) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  auto *store = SSL_CTX_get_cert_store(ssl_ctx_.get());

  if (1 != X509_STORE_load_locations(
               store, crl_file.empty() ? nullptr : crl_file.c_str(),
               crl_path.empty() ? nullptr : crl_path.c_str())) {
    return stdx::unexpected(make_tls_error());
  }

  if (1 != X509_STORE_set_flags(
               store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL)) {
    return stdx::unexpected(make_tls_error());
  }

  return {};
}

stdx::expected<void, std::error_code> TlsContext::curves_list(
    const std::string &curves) {
  if (curves.empty()) return {};

  if (1 != SSL_CTX_set1_curves_list(ssl_ctx_.get(),
                                    const_cast<char *>(curves.c_str()))) {
    return stdx::unexpected(make_tls_error());
  }
  return {};
}

static int o11x_version(TlsVersion version) {
  switch (version) {
    case TlsVersion::AUTO:
      return 0;
    case TlsVersion::SSL_3:
      return SSL3_VERSION;
    case TlsVersion::TLS_1_0:
      return TLS1_VERSION;
    case TlsVersion::TLS_1_1:
      return TLS1_1_VERSION;
    case TlsVersion::TLS_1_2:
      return TLS1_2_VERSION;
    case TlsVersion::TLS_1_3:
      return TLS1_3_VERSION;
    default:
      throw std::invalid_argument("version out of range");
  }
}

stdx::expected<void, std::error_code> TlsContext::version_range(
    TlsVersion min_version, TlsVersion max_version) {
  // set min TLS version
  if (1 != SSL_CTX_set_min_proto_version(ssl_ctx_.get(),
                                         o11x_version(min_version))) {
    return stdx::unexpected(make_tls_error());
  }
  if (1 != SSL_CTX_set_max_proto_version(ssl_ctx_.get(),
                                         o11x_version(max_version))) {
    return stdx::unexpected(make_tls_error());
  }
  return {};
}

TlsVersion TlsContext::min_version() const {
  switch (auto v = SSL_CTX_get_min_proto_version(ssl_ctx_.get())) {
    case SSL3_VERSION:
      return TlsVersion::SSL_3;
    case TLS1_VERSION:
      return TlsVersion::TLS_1_0;
    case TLS1_1_VERSION:
      return TlsVersion::TLS_1_1;
    case TLS1_2_VERSION:
      return TlsVersion::TLS_1_2;
    case TLS1_3_VERSION:
      return TlsVersion::TLS_1_3;
    case 0:
      return TlsVersion::AUTO;
    default:
      throw std::invalid_argument("unknown min-proto-version: " +
                                  std::to_string(v));
  }
}

std::vector<std::string> TlsContext::cipher_list() const {
  // dump the cipher-list we actually have - using the SSL_* functions
  mysql_harness::Ssl ssl{SSL_new(ssl_ctx_.get())};

  std::vector<std::string> out;
  int prio = 0;
  while (auto cipher = SSL_get_cipher_list(ssl.get(), prio++)) {
    out.emplace_back(cipher);
  }

  return out;
}

void TlsContext::info_callback(TlsContext::InfoCallback cb) {
  SSL_CTX_set_info_callback(ssl_ctx_.get(), cb);
}

TlsContext::InfoCallback TlsContext::info_callback() const {
  return SSL_CTX_get_info_callback(ssl_ctx_.get());
}

/**
 * get the key size of an RSA key.
 *
 * @param x509 a non-null pointer to RSA-key wrapped in a X509 struct.
 *
 * @returns a key-size of RSA key on success, a std::error_code on failure.
 */
static stdx::expected<int, std::error_code> get_rsa_key_size(X509 *x509) {
  EVP_PKEY *public_key = X509_get0_pubkey(x509);
  if (public_key == nullptr) {
    return stdx::unexpected(make_error_code(TlsCertErrc::kNotACertificate));
  }

  if (EVP_PKEY_base_id(public_key) != EVP_PKEY_RSA) {
    return stdx::unexpected(make_error_code(TlsCertErrc::kNoRSACert));
  }

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
  int key_bits;
  if (!EVP_PKEY_get_int_param(public_key, OSSL_PKEY_PARAM_BITS, &key_bits)) {
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  return key_bits;
#else
  RSA *rsa_key = EVP_PKEY_get0_RSA(public_key);
  if (!rsa_key) {
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
  return RSA_bits(rsa_key);
#endif
}

stdx::expected<void, std::error_code> TlsContext::load_key_and_cert(
    const std::string &private_key_file, const std::string &cert_chain_file) {
  // load cert and key
  if (!cert_chain_file.empty()) {
    if (1 != SSL_CTX_use_certificate_chain_file(ssl_ctx_.get(),
                                                cert_chain_file.c_str())) {
      return stdx::unexpected(make_tls_error());
    }
  }
  // internal pointer, don't free
  if (X509 *x509 = SSL_CTX_get0_certificate(ssl_ctx_.get())) {
    auto key_size_res = get_rsa_key_size(x509);
    if (!key_size_res) {
      auto ec = key_size_res.error();

      if (ec != TlsCertErrc::kNoRSACert) {
        return stdx::unexpected(key_size_res.error());
      }

      // if it isn't a RSA Key ... just continue.
    } else {
      const auto key_size = *key_size_res;

      if (key_size < kMinRsaKeySize) {
        return stdx::unexpected(
            make_error_code(TlsCertErrc::kRSAKeySizeToSmall));
      }
    }
  } else {
    // doesn't exist
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
  if (1 != SSL_CTX_use_PrivateKey_file(ssl_ctx_.get(), private_key_file.c_str(),
                                       SSL_FILETYPE_PEM)) {
    return stdx::unexpected(make_tls_error());
  }
  if (1 != SSL_CTX_check_private_key(ssl_ctx_.get())) {
    return stdx::unexpected(make_tls_error());
  }

  return {};
}

int TlsContext::security_level() const {
  return SSL_CTX_get_security_level(ssl_ctx_.get());
}

long TlsContext::session_cache_hits() const {
  return SSL_CTX_sess_hits(ssl_ctx_.get());
}
