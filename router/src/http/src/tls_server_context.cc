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

#include "tls_server_context.h"

#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/safestack.h>
#include <openssl/ssl.h>

#include "tls_error.h"

constexpr std::array<const char *, 9>
    TlsServerContext::unacceptable_cipher_spec;

TlsServerContext::TlsServerContext(const std::string &cert_chain_file,
                                   const std::string &private_key_file,
                                   const std::string &ciphers,
                                   const std::string &dh_params)
    : ssl_ctx_(SSL_CTX_new(
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
                   TLS_server_method()
#else
                   SSLv23_server_method()
#endif
                       ),
               &SSL_CTX_free) {
  set_min_version();
  load_key_and_cert(cert_chain_file, private_key_file);
  init_tmp_ecdh();
  init_tmp_dh(dh_params);
  // ensure DH keys are only used once
  SSL_CTX_set_options(ssl_ctx_.get(),
                      SSL_OP_SINGLE_DH_USE | SSL_OP_SINGLE_ECDH_USE);

  set_cipher_list(ciphers);
}

void TlsServerContext::set_min_version() {
// set min TLS version
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  if (1 != SSL_CTX_set_min_proto_version(ssl_ctx_.get(), TLS1_2_VERSION)) {
    throw TlsError("set min-TLS-version failed");
  }
#else
  if (1 != SSL_CTX_set_options(ssl_ctx_.get(),
                               SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                                   SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1)) {
    throw TlsError("set min-TLS-version failed");
  }

#endif
}

void TlsServerContext::load_key_and_cert(const std::string &cert_chain_file,
                                         const std::string &private_key_file) {
  // load cert and key
  if (!cert_chain_file.empty()) {
    if (1 != SSL_CTX_use_certificate_chain_file(ssl_ctx_.get(),
                                                cert_chain_file.c_str())) {
      throw TlsError("use certificate " + cert_chain_file + " failed");
    }
  }
  if (1 != SSL_CTX_use_PrivateKey_file(ssl_ctx_.get(), private_key_file.c_str(),
                                       SSL_FILETYPE_PEM)) {
    throw TlsError("using private key-file " + private_key_file + " failed");
  }
  if (1 != SSL_CTX_check_private_key(ssl_ctx_.get())) {
    throw TlsError("check-private-key");
  }
}

// setup ellyptic curve for EDH
void TlsServerContext::init_tmp_ecdh() {
  std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)> ecdh(
      EC_KEY_new_by_curve_name(NID_secp384r1), &EC_KEY_free);
  if (nullptr == ecdh) {
    throw TlsError("ec-key-new-by-curve-name");
  }
  if (1 != SSL_CTX_set_tmp_ecdh(ssl_ctx_.get(), ecdh.get())) {
    throw TlsError("ssl: set-tmp-ecdh");
  }
}

// load DH params
void TlsServerContext::init_tmp_dh(const std::string &dh_params) {
  std::unique_ptr<DH, decltype(&DH_free)> dh2048(nullptr, &DH_free);
  if (!dh_params.empty()) {
    std::unique_ptr<FILE, decltype(&fclose)> f(::fopen(dh_params.c_str(), "r"),
                                               &fclose);
    if (nullptr == f) {
      throw std::runtime_error("failed to open dh-param file");
    }
    dh2048.reset(PEM_read_DHparams(f.get(), NULL, NULL, NULL));
    if (nullptr == dh2048.get()) {
      throw std::runtime_error("failed to open dh-param file");
    }
  } else {
    dh2048.reset(DH_get_2048_256());
  }

  if (1 != SSL_CTX_set_tmp_dh(ssl_ctx_.get(), dh2048.get())) {
    throw TlsError("ssl: set-tmp-dh");
  }
}

void TlsServerContext::set_cipher_list(const std::string &ciphers) {
  // NEVER allow weak ciphers

  std::string ci(ciphers);
  for (const auto &s : unacceptable_cipher_spec) {
    if (!ciphers.empty()) ci += ":";

    ci += s;
  }

  // load the cipher-list
  if (1 != SSL_CTX_set_cipher_list(ssl_ctx_.get(), ci.c_str())) {
    throw TlsError("ssl: set-cipher-list");
  }
}

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
std::vector<std::string> TlsServerContext::cipher_list() const {
  // dump the cipher-list we actually have
  STACK_OF(SSL_CIPHER) *st = SSL_CTX_get_ciphers(ssl_ctx_.get());
  size_t num_ciphers = sk_SSL_CIPHER_num(st);

  std::vector<std::string> out(num_ciphers);
  for (size_t ndx = 0; ndx < num_ciphers; ++ndx) {
    auto *cipher = sk_SSL_CIPHER_value(st, ndx);
    out.emplace_back(SSL_CIPHER_get_name(cipher));
  }

  return out;
}
#endif

std::vector<std::string> Tls::get_default_ciphers() {
  // as TLSv1.2 is the minimum version, only TLSv1.2+ ciphers are set by
  // default

  // TLSv1.2 with PFS using SHA2, encrypted by AES in GCM or CBC mode
  const std::vector<std::string> mandatory_p1{
      // clang-format off
        "ECDHE-ECDSA-AES128-GCM-SHA256",
        "ECDHE-ECDSA-AES256-GCM-SHA384",
        "ECDHE-RSA-AES128-GCM-SHA256",
        "ECDHE-ECDSA-AES128-SHA256",
        "ECDHE-RSA-AES128-SHA256"
      // clang-format on
  };

  // TLSv1.2+ with PFS using SHA2, encrypted by AES in GCM or CBC mode
  const std::vector<std::string> optional_p1{
      // clang-format off

        // TLSv1.3
        "TLS_AES_128_GCM_SHA256",
        "TLS_AES_256_GCM_SHA384",
        "TLS_CHACHA20_POLY1305_SHA256",
        "TLS_AES_128_CCM_SHA256",
        "TLS_AES_128_CCM_8_SHA256",

        // TLSv1.2
        "ECDHE-RSA-AES256-GCM-SHA384",
        "ECDHE-RSA-AES256-SHA384",
        "ECDHE-ECDSA-AES256-SHA384",
        "DHE-RSA-AES128-GCM-SHA256",
        "DHE-DSS-AES128-GCM-SHA256",
        "DHE-RSA-AES128-SHA256",
        "DHE-DSS-AES128-SHA256",
        "DHE-DSS-AES256-GCM-SHA384",
        "DHE-RSA-AES256-SHA256",
        "DHE-DSS-AES256-SHA256",
        "DHE-RSA-AES256-GCM-SHA384",
        "ECDHE-ECDSA-CHACHA20-POLY1305",
        "ECDHE-RSA-CHACHA20-POLY1305"
      // clang-format on
  };

  // TLSv1.2+ with DH, ECDH, RSA using SHA2
  // encrypted by AES in GCM or CBC mode
  const std::vector<std::string> optional_p2{
      // clang-format off
        "DH-DSS-AES128-GCM-SHA256",
        "ECDH-ECDSA-AES128-GCM-SHA256",
        "DH-DSS-AES256-GCM-SHA384",
        "ECDH-ECDSA-AES256-GCM-SHA384",
        "AES128-GCM-SHA256",
        "AES256-GCM-SHA384",
        "AES128-SHA256",
        "DH-DSS-AES128-SHA256",
        "ECDH-ECDSA-AES128-SHA256",
        "AES256-SHA256",
        "DH-DSS-AES256-SHA256",
        "ECDH-ECDSA-AES256-SHA384",
        "DH-RSA-AES128-GCM-SHA256",
        "ECDH-RSA-AES128-GCM-SHA256",
        "DH-RSA-AES256-GCM-SHA384",
        "ECDH-RSA-AES256-GCM-SHA384",
        "DH-RSA-AES128-SHA256",
        "ECDH-RSA-AES128-SHA256",
        "DH-RSA-AES256-SHA256",
        "ECDH-RSA-AES256-SHA384",
      // clang-format on
  };

  // required by RFC5246, but quite likely removed by the !SSLv3 filter
  const std::vector<std::string> optional_p3{"AES128-SHA"};

  std::vector<std::string> out(mandatory_p1.size() + optional_p1.size() +
                               optional_p2.size() + optional_p3.size());
  for (const auto &a : {mandatory_p1, optional_p1, optional_p2, optional_p3}) {
    out.insert(out.end(), a.begin(), a.end());
  }

  return out;
}
