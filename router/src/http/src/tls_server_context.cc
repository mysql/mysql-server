/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <string>
#include <vector>

#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#if !defined(LIBWOLFSSL_VERSION_HEX)
#include <openssl/safestack.h>
#endif
#include <openssl/ssl.h>

#include "mysql/harness/utility/string.h"

#include "openssl_version.h"
#include "tls_error.h"

#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(1, 1, 0)
#define RSA_bits(rsa) BN_num_bits(rsa->n)
#define DH_bits(dh) BN_num_bits(dh->p)
#endif

// type == decltype(BN_num_bits())
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
constexpr int kMinRsaKeySize{2048};
#endif
constexpr int kMinDhKeySize{1024};

constexpr std::array<const char *, 9>
    TlsServerContext::unacceptable_cipher_spec;

static const SSL_METHOD *server_method =
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
    TLS_server_method()
#else
    SSLv23_server_method()
#endif
    ;

TlsServerContext::TlsServerContext(TlsVersion min_ver, TlsVersion max_ver)
    : TlsContext(server_method) {
  version_range(min_ver, max_ver);
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  SSL_CTX_set_ecdh_auto(ssl_ctx_.get(), 1);
#endif
  SSL_CTX_set_options(ssl_ctx_.get(), SSL_OP_NO_COMPRESSION);
  cipher_list("ALL");  // ALL - unacceptable ciphers
}

void TlsServerContext::load_key_and_cert(const std::string &cert_chain_file,
                                         const std::string &private_key_file) {
  // load cert and key
  if (!cert_chain_file.empty()) {
    if (1 != SSL_CTX_use_certificate_chain_file(ssl_ctx_.get(),
                                                cert_chain_file.c_str())) {
      throw TlsError("using SSL certificate file '" + cert_chain_file +
                     "' failed");
    }
  }
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  // openssl 1.0.1 has no SSL_CTX_get0_certificate() and doesn't allow
  // to access ctx->cert->key->x509 as cert_st is opaque to us.

  // internal pointer, don't free
  if (X509 *x509 = SSL_CTX_get0_certificate(ssl_ctx_.get())) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> public_key(
        X509_get_pubkey(x509), &EVP_PKEY_free);
    if (public_key) {
      switch (EVP_PKEY_base_id(public_key.get())) {
        case EVP_PKEY_RSA: {
          std::unique_ptr<RSA, decltype(&RSA_free)> rsa_key(
              EVP_PKEY_get1_RSA(public_key.get()), &RSA_free);
          auto key_size = RSA_bits(rsa_key.get());

          if (key_size < kMinRsaKeySize) {
            throw std::runtime_error(
                "keylength of RSA public-key of certificate " +
                cert_chain_file + " is too small. Expected at least " +
                std::to_string(kMinRsaKeySize) + ", got " +
                std::to_string(key_size));
          }
          break;
        }
        default:
          throw std::runtime_error("not an RSA certificate?");
      }
    } else {
      throw std::runtime_error(
          "expected to find a publickey in the certificate");
    }
  } else {
    throw std::runtime_error("expected to find a certificate in SSL_CTx");
  }
#endif
  if (1 != SSL_CTX_use_PrivateKey_file(ssl_ctx_.get(), private_key_file.c_str(),
                                       SSL_FILETYPE_PEM)) {
    throw TlsError("using SSL key file '" + private_key_file + "' failed");
  }
  if (1 != SSL_CTX_check_private_key(ssl_ctx_.get())) {
    throw TlsError("checking SSL key file '" + private_key_file +
                   "' against SSL certificate file '" + cert_chain_file +
                   "' failed");
  }
}

// load DH params
void TlsServerContext::init_tmp_dh(const std::string &dh_params) {
  std::unique_ptr<DH, decltype(&DH_free)> dh2048(nullptr, &DH_free);
  if (!dh_params.empty()) {
    std::unique_ptr<BIO, decltype(&BIO_free)> pem_bio(
        BIO_new_file(dh_params.c_str(), "r"), &BIO_free);
    if (!pem_bio) {
      throw std::runtime_error("failed to open dh-param file '" + dh_params +
                               "'");
    }
    dh2048.reset(PEM_read_bio_DHparams(pem_bio.get(), NULL, NULL, NULL));
    if (!dh2048) {
      throw TlsError("failed to parse dh-param file");
    }

#if !defined(LIBWOLFSSL_VERSION_HEX)
    int codes = 0;
    if (1 != DH_check(dh2048.get(), &codes)) {
      throw TlsError("DH_check() failed");
    }

    if (codes != 0) {
      throw std::runtime_error("check of DH params failed: ");
    }
#endif

    if (DH_bits(dh2048.get()) < kMinDhKeySize) {
      throw std::runtime_error("key size of DH param " + dh_params +
                               " too small. Expected " +
                               std::to_string(kMinDhKeySize) + ", got " +
                               std::to_string(DH_bits(dh2048.get())));
    }

  } else {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
    dh2048.reset(DH_get_2048_256());
#else
    /*
       Diffie-Hellman key.
       Generated using: >openssl dhparam -5 -C 2048
    */
    const char dh_2048[]{
        "-----BEGIN DH PARAMETERS-----\n"
        "MIIBCAKCAQEAil36wGZ2TmH6ysA3V1xtP4MKofXx5n88xq/aiybmGnReZMviCPEJ\n"
        "46+7VCktl/RZ5iaDH1XNG1dVQmznt9pu2G3usU+k1/VB4bQL4ZgW4u0Wzxh9PyXD\n"
        "glm99I9Xyj4Z5PVE4MyAsxCRGA1kWQpD9/zKAegUBPLNqSo886Uqg9hmn8ksyU9E\n"
        "BV5eAEciCuawh6V0O+Sj/C3cSfLhgA0GcXp3OqlmcDu6jS5gWjn3LdP1U0duVxMB\n"
        "h/neTSCSvtce4CAMYMjKNVh9P1nu+2d9ZH2Od2xhRIqMTfAS1KTqF3VmSWzPFCjG\n"
        "mjxx/bg6bOOjpgZapvB6ABWlWmRmAAWFtwIBBQ==\n"
        "-----END DH PARAMETERS-----"};

    std::unique_ptr<BIO, decltype(&BIO_free)> bio{
        BIO_new_mem_buf(const_cast<char *>(dh_2048), sizeof(dh_2048) - 1),
        &BIO_free};

    dh2048.reset(PEM_read_bio_DHparams(bio.get(), NULL, NULL, NULL));
#endif
  }

  if (1 != SSL_CTX_set_tmp_dh(ssl_ctx_.get(), dh2048.get())) {
    throw TlsError("set-tmp-dh failed");
  }
  // ensure DH keys are only used once
  SSL_CTX_set_options(ssl_ctx_.get(),
                      SSL_OP_SINGLE_DH_USE | SSL_OP_SINGLE_ECDH_USE);
}

void TlsServerContext::verify(TlsVerify verify, std::bitset<2> tls_opts) {
  int mode = 0;
  switch (verify) {
    case TlsVerify::NONE:
      mode = SSL_VERIFY_NONE;

      if (tls_opts.to_ulong() != 0) {
        throw std::invalid_argument("tls_opts MUST be zero if verify is NONE");
      }
      break;
    case TlsVerify::PEER:
      mode = SSL_VERIFY_PEER;
      break;
  }
  if (tls_opts.test(TlsVerifyOpts::kFailIfNoPeerCert)) {
    mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  }
  SSL_CTX_set_verify(ssl_ctx_.get(), mode, NULL);
}

void TlsServerContext::cipher_list(const std::string &ciphers) {
  // append the "unacceptable_cipher_spec" to ensure to NEVER allow weak ciphers

  std::string ci(ciphers);
  if (!ci.empty()) ci += ":";

  ci += mysql_harness::join(unacceptable_cipher_spec, ":");

  // load the cipher-list
  if (1 != SSL_CTX_set_cipher_list(ssl_ctx_.get(), ci.c_str())) {
    throw TlsError("set-cipher-list failed");
  }
}

std::vector<std::string> TlsServerContext::default_ciphers() {
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
  for (const std::vector<std::string> &a :
       std::vector<std::vector<std::string>>{mandatory_p1, optional_p1,
                                             optional_p2, optional_p3}) {
    out.insert(out.end(), a.begin(), a.end());
  }

  return out;
}
