/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "certificate_generator.h"

#include <array>
#include <stdexcept>

#include <openssl/evp.h>

#include "harness_assert.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_server_context.h"
#include "openssl_version.h"
#include "scope_guard.h"

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
#include <openssl/decoder.h>  // OSSL_DECODER...
#include <openssl/encoder.h>  // OSSL_ENCODER...
#endif

namespace {

// RSA key-sizes per sec-level
constexpr const std::array rsa_key_sizes{2048, 2048, 2048, 3072, 7680, 15360};

template <class T>
struct OsslDeleter;

template <class T>
using OsslUniquePtr = std::unique_ptr<T, OsslDeleter<T>>;

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
template <>
struct OsslDeleter<OSSL_DECODER_CTX> {
  void operator()(OSSL_DECODER_CTX *ctx) { OSSL_DECODER_CTX_free(ctx); }
};

template <>
struct OsslDeleter<OSSL_ENCODER_CTX> {
  void operator()(OSSL_ENCODER_CTX *ctx) { OSSL_ENCODER_CTX_free(ctx); }
};
#endif

template <>
struct OsslDeleter<BIO> {
  void operator()(BIO *bio) { BIO_free(bio); }
};

#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(3, 0, 0)
template <>
struct OsslDeleter<RSA> {
  void operator()(RSA *rsa) { RSA_free(rsa); }
};

template <>
struct OsslDeleter<BIGNUM> {
  void operator()(BIGNUM *num) { BN_free(num); }
};
#endif

template <>
struct OsslDeleter<X509_EXTENSION> {
  void operator()(X509_EXTENSION *num) { X509_EXTENSION_free(num); }
};

using EvpPkey = CertificateGenerator::EvpPkey;
using X509Cert = CertificateGenerator::X509Cert;

#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(3, 0, 0)
/**
 * Generate RSA key pair of a given length.
 *
 * @param[in] key_size Bit length of a RSA key to be generated.
 * @param[in] exponent Public exponent used for modulus operations.
 *
 * @return RSA public/private key pair on success or std::error_code on
 * failure.
 */
stdx::expected<OsslUniquePtr<RSA>, std::error_code> generate_rsa(
    const unsigned int key_size, const unsigned int exponent) {
  OsslUniquePtr<RSA> rsa{RSA_new()};
  OsslUniquePtr<BIGNUM> bignum{BN_new()};
  if (!rsa || !bignum) {
    return stdx::unexpected(make_error_code(cert_errc::rsa_generation_failed));
  }

  if (!BN_set_word(bignum.get(), exponent) ||
      !RSA_generate_key_ex(rsa.get(), key_size, bignum.get(), nullptr)) {
    return stdx::unexpected(make_error_code(cert_errc::rsa_generation_failed));
  }

  return {std::move(rsa)};
}
#endif

std::string read_bio_to_string(BIO *bio) {
  const auto length = BIO_pending(bio);

  std::string result;
  result.resize(length);
  BIO_read(bio, result.data(), length);

  return result;
}

/**
 * Get string representation of a PEM (certificate or key) object.
 *
 * @param[in] pem_to_bio_func Callback that will be used to convert PEM to
 * BIO.
 * @param[in] args Argument pack that will be forwarded to the
 * pem_to_bio_func.
 *
 * @throws std::runtime_error PEM to string conversion failed.
 *
 * @returns PEM object string representation.
 */
template <typename F, typename... Args>
std::string write_custom_pem_to_string(F &&pem_to_bio_func, Args &&...args) {
  OsslUniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
  if (!pem_to_bio_func(bio.get(), std::forward<Args>(args)...)) {
    throw std::runtime_error{"Could not convert PEM to string"};
  }

  return read_bio_to_string(bio.get());
}
}  // namespace

stdx::expected<EvpPkey, std::error_code>
CertificateGenerator::generate_evp_pkey() {
  const int sec_level = TlsServerContext().security_level();
  const unsigned int default_rsa_key_size = 2048;

  const unsigned int key_size =
      sec_level >= 0 && sec_level < static_cast<int>(rsa_key_sizes.size())
          ? rsa_key_sizes[sec_level]
          : default_rsa_key_size;

#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(3, 0, 0)
  const unsigned int exponent = RSA_F4;

  auto rsa_res = generate_rsa(key_size, exponent);
  if (!rsa_res) return stdx::unexpected(rsa_res.error());

  auto rsa = std::move(*rsa_res);

  EvpPkey pkey{EVP_PKEY_new()};
  // pkey takes control over the rsa lifetime, assign_RSA function guarantees
  // that rsa will be freed on pkey destruction
  if (!EVP_PKEY_assign_RSA(pkey.get(), rsa.get())) {
    return stdx::unexpected(
        make_error_code(cert_errc::evp_pkey_generation_failed));
  }
  (void)rsa.release();

  return {std::move(pkey)};
#else
  return EvpPkey{EVP_RSA_gen(key_size)};
#endif
}

std::string CertificateGenerator::pkey_to_string(EVP_PKEY *pkey) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
  OsslUniquePtr<OSSL_ENCODER_CTX> encoder_ctx(OSSL_ENCODER_CTX_new_for_pkey(
      pkey, OSSL_KEYMGMT_SELECT_KEYPAIR | OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS,
      "PEM", "type-specific", nullptr));

  unsigned char *data{};
  size_t data_size{};

  if (1 != OSSL_ENCODER_to_data(encoder_ctx.get(), &data, &data_size)) {
    throw std::runtime_error("encode failed :(");
  }

  Scope_guard exit_guard([&data]() { OPENSSL_free(data); });

  return std::string{reinterpret_cast<char *>(data), data_size};
#else
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  RSA *rsa = EVP_PKEY_get0_RSA(pkey);
#else
  OsslUniquePtr<RSA> rsa_storage{EVP_PKEY_get1_RSA(pkey)};

  RSA *rsa = rsa_storage.get();
#endif
  return write_custom_pem_to_string(PEM_write_bio_RSAPrivateKey, rsa, nullptr,
                                    nullptr, 10, nullptr, nullptr);
#endif
}

std::string CertificateGenerator::cert_to_string(X509 *cert) {
  return write_custom_pem_to_string(PEM_write_bio_X509, cert);
}

stdx::expected<X509Cert, std::error_code> CertificateGenerator::generate_x509(
    EVP_PKEY *pkey, const std::string &common_name, const uint32_t serial,
    X509 *ca_cert, EVP_PKEY *ca_pkey, uint32_t notbefore,
    uint32_t notafter) const {
  harness_assert(serial != 0);
  harness_assert(common_name.length() <= k_max_cn_name_length);
  // Do not allow that either one of those is null when the second one is not
  harness_assert(!(static_cast<bool>(ca_cert) != static_cast<bool>(ca_pkey)));

  X509Cert cert{X509_new()};
  if (!cert) {
    return stdx::unexpected(make_error_code(cert_errc::cert_alloc_failed));
  }

  // Set certificate version
  if (!X509_set_version(cert.get(), 2)) {
    return stdx::unexpected(
        make_error_code(cert_errc::cert_set_version_failed));
  }

  // Set serial number
  if (!ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), serial)) {
    return stdx::unexpected(make_error_code(cert_errc::cert_set_serial_failed));
  }

  // Set certificate validity
  if (!X509_gmtime_adj(X509_get_notBefore(cert.get()), notbefore) ||
      !X509_gmtime_adj(X509_get_notAfter(cert.get()), notafter)) {
    return stdx::unexpected(
        make_error_code(cert_errc::cert_set_validity_failed));
  }

  // Set public key
  if (!X509_set_pubkey(cert.get(), pkey)) {
    return stdx::unexpected(
        make_error_code(cert_errc::cert_set_public_key_failed));
  }

  // Set CN value in subject
  auto name = X509_get_subject_name(cert.get());
  if (!name) {
    return stdx::unexpected(make_error_code(cert_errc::cert_set_cn_failed));
  }

  if (!X509_NAME_add_entry_by_txt(
          name, "CN", MBSTRING_ASC,
          reinterpret_cast<const unsigned char *>(common_name.c_str()), -1, -1,
          0)) {
    return stdx::unexpected(make_error_code(cert_errc::cert_set_cn_failed));
  }

  // Set Issuer
  if (!X509_set_issuer_name(cert.get(),
                            ca_cert ? X509_get_subject_name(ca_cert) : name)) {
    return stdx::unexpected(make_error_code(cert_errc::cert_set_issuer_failed));
  }

  // Add X509v3 extensions
  X509V3_CTX v3ctx;
  X509V3_set_ctx(&v3ctx, ca_cert ? ca_cert : cert.get(), cert.get(), nullptr,
                 nullptr, 0);

  // Add CA:TRUE / CA:FALSE information
  OsslUniquePtr<X509_EXTENSION> ext{
      X509V3_EXT_conf_nid(nullptr, &v3ctx, NID_basic_constraints,
                          ca_cert ? const_cast<char *>("critical,CA:FALSE")
                                  : const_cast<char *>("critical,CA:TRUE"))};
  if (!ext) {
    return stdx::unexpected(
        make_error_code(cert_errc::cert_set_v3_extensions_failed));
  }
  X509_add_ext(cert.get(), ext.get(), -1);

  // Sign using SHA256
  if (!X509_sign(cert.get(), ca_cert ? ca_pkey : pkey, EVP_sha256())) {
    return stdx::unexpected(
        make_error_code(cert_errc::cert_could_not_be_signed));
  }

  return {std::move(cert)};
}
