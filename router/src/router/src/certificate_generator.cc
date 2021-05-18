/*
  Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#include "certificate_generator.h"

#include "harness_assert.h"
#include "mysql/harness/stdx/expected.h"

stdx::expected<CertificateGenerator::evp_key_unique_ptr_t, std::error_code>
CertificateGenerator::generate_evp_pkey() const {
  auto rsa = generate_rsa();
  if (!rsa) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::rsa_generation_failed));
  }
  evp_key_unique_ptr_t pkey{EVP_PKEY_new()};
  // pkey takes control over the rsa lifetime, assign_RSA function guarantees
  // that rsa will be freed on pkey destruction
  auto rsa_raw = rsa.value().release();
  if (!EVP_PKEY_assign_RSA(pkey.get(), rsa_raw)) {
    RSA_free(rsa_raw);
    return stdx::make_unexpected(
        make_error_code(cert_errc::evp_pkey_generation_failed));
  }

  return {std::move(pkey)};
}

std::string CertificateGenerator::pkey_to_string(
    const CertificateGenerator::evp_key_unique_ptr_t &pkey) const {
  rsa_unique_ptr_t rsa{EVP_PKEY_get1_RSA(pkey.get())};
  return write_custom_pem_to_string(PEM_write_bio_RSAPrivateKey, rsa.get(),
                                    nullptr, nullptr, 10, nullptr, nullptr);
}

std::string CertificateGenerator::cert_to_string(
    const CertificateGenerator::x509_unique_ptr_t &cert) const {
  return write_custom_pem_to_string(PEM_write_bio_X509, cert.get());
}

stdx::expected<CertificateGenerator::x509_unique_ptr_t, std::error_code>
CertificateGenerator::generate_x509(const evp_key_unique_ptr_t &pkey,
                                    const std::string &common_name,
                                    const uint32_t serial,
                                    const x509_unique_ptr_t &ca_cert,
                                    const evp_key_unique_ptr_t &ca_pkey,
                                    uint32_t notbefore,
                                    uint32_t notafter) const {
  harness_assert(serial != 0);
  harness_assert(common_name.length() <= k_max_cn_name_length);
  // Do not allow that either one of those is null when the second one is not
  harness_assert(!(static_cast<bool>(ca_cert) != static_cast<bool>(ca_pkey)));

  x509_unique_ptr_t cert{X509_new()};
  if (!cert)
    return stdx::make_unexpected(make_error_code(cert_errc::cert_alloc_failed));

  // Set certificate version
  if (!X509_set_version(cert.get(), 2)) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_version_failed));
  }

  // Set serial number
  if (!ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), serial)) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_serial_failed));
  }

  // Set certificate validity
  if (!X509_gmtime_adj(X509_get_notBefore(cert.get()), notbefore) ||
      !X509_gmtime_adj(X509_get_notAfter(cert.get()), notafter)) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_validity_failed));
  }

  // Set public key
  if (!X509_set_pubkey(cert.get(), pkey.get())) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_public_key_failed));
  }

  // Set CN value in subject
  auto name = X509_get_subject_name(cert.get());
  if (!name) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_cn_failed));
  }

  if (!X509_NAME_add_entry_by_txt(
          name, "CN", MBSTRING_ASC,
          reinterpret_cast<const unsigned char *>(common_name.c_str()), -1, -1,
          0)) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_cn_failed));
  }

  // Set Issuer
  if (!X509_set_issuer_name(
          cert.get(), ca_cert ? X509_get_subject_name(ca_cert.get()) : name)) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_issuer_failed));
  }

  // Add X509v3 extensions
  X509V3_CTX v3ctx;
  X509V3_set_ctx(&v3ctx, ca_cert ? ca_cert.get() : cert.get(), cert.get(),
                 nullptr, nullptr, 0);

  // Add CA:TRUE / CA:FALSE information
  x509_extension_unique_ptr_t ext{
      X509V3_EXT_conf_nid(nullptr, &v3ctx, NID_basic_constraints,
                          ca_cert ? const_cast<char *>("critical,CA:FALSE")
                                  : const_cast<char *>("critical,CA:TRUE"))};
  if (!ext) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_set_v3_extensions_failed));
  }
  X509_add_ext(cert.get(), ext.get(), -1);

  // Sign using SHA256
  if (!X509_sign(cert.get(), ca_cert ? ca_pkey.get() : pkey.get(),
                 EVP_sha256())) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::cert_could_not_be_signed));
  }

  return {std::move(cert)};
}

stdx::expected<CertificateGenerator::rsa_unique_ptr_t, std::error_code>
CertificateGenerator::generate_rsa(const uint32_t key_size,
                                   const uint32_t exponent) const {
  rsa_unique_ptr_t rsa{RSA_new()};
  bignum_unique_ptr_t bignum{BN_new()};
  if (!rsa || !bignum) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::rsa_generation_failed));
  }

  if (!BN_set_word(bignum.get(), exponent) ||
      !RSA_generate_key_ex(rsa.get(), key_size, bignum.get(), nullptr)) {
    return stdx::make_unexpected(
        make_error_code(cert_errc::rsa_generation_failed));
  }

  return {std::move(rsa)};
}

std::string CertificateGenerator::read_bio_to_string(
    const CertificateGenerator::bio_unique_ptr_t &bio) const {
  std::string result;
  const auto length = BIO_pending(bio.get());
  result.resize(length);
  BIO_read(bio.get(), &result[0], length);
  return result;
}
