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

#ifndef ROUTER_CERTIFICATE_GENERATOR_INCLUDED
#define ROUTER_CERTIFICATE_GENERATOR_INCLUDED

#include <memory>
#include <string>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

#include "dim.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/stdx/expected.h"

enum class cert_errc {
  rsa_generation_failed,
  evp_pkey_generation_failed,
  cert_alloc_failed,
  cert_set_version_failed,
  cert_set_serial_failed,
  cert_set_validity_failed,
  cert_set_public_key_failed,
  cert_set_cn_failed,
  cert_set_issuer_failed,
  cert_set_v3_extensions_failed,
  cert_could_not_be_signed,
};

namespace std {
template <>
struct is_error_code_enum<cert_errc> : public std::true_type {};
}  // namespace std

inline const std::error_category &cert_err_category() noexcept {
  class cert_err_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override {
      return "certificate generator";
    }
    std::string message(int ev) const override {
      switch (static_cast<cert_errc>(ev)) {
        case cert_errc::rsa_generation_failed:
          return "RSA generation failed";
        case cert_errc::evp_pkey_generation_failed:
          return "EVP_PKEY generation failed";
        case cert_errc::cert_alloc_failed:
          return "Could not create X.509 certificate";
        case cert_errc::cert_set_version_failed:
          return "Failed to set version for the X.509 certificate";
        case cert_errc::cert_set_serial_failed:
          return "Failed to set serial number for the X.509 certificate";
        case cert_errc::cert_set_validity_failed:
          return "Failed to set validity period for the X.509 certificate";
        case cert_errc::cert_set_public_key_failed:
          return "Failed to set X.509 certificate public key";
        case cert_errc::cert_set_cn_failed:
          return "Failed to set X.509 certificate CN field";
        case cert_errc::cert_set_issuer_failed:
          return "Failed to set X.509 certificate issuer field";
        case cert_errc::cert_set_v3_extensions_failed:
          return "Failed to set X.509 certificate v3 extensions";
        case cert_errc::cert_could_not_be_signed:
          return "Failed to sign X.509 certificate";
        default:
          return "unknown";
      }
    }
  };

  static cert_err_category_impl instance;
  return instance;
}

inline std::error_code make_error_code(cert_errc e) noexcept {
  return {static_cast<int>(e), cert_err_category()};
}

class CertificateGenerator {
 private:
  struct EvpPkeyDeleter {
    void operator()(EVP_PKEY *pkey) { EVP_PKEY_free(pkey); }
  };

  struct X509Deleter {
    void operator()(X509 *x509) { X509_free(x509); }
  };

 public:
  using EvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
  using X509Cert = std::unique_ptr<X509, X509Deleter>;

  /**
   * Generate EVP_PKEY containing public and private keys.
   *
   * @returns Unique pointer to EVP_PKEY object on success or std::error_code if
   * key generation failed.
   */
  static stdx::expected<EvpPkey, std::error_code> generate_evp_pkey();

  /**
   * Get string representation of a private key.
   *
   * @param[in] pkey Private key.
   *
   * @returns Private key string representation.
   */
  static std::string pkey_to_string(EVP_PKEY *pkey);

  /**
   * Get string representation of a X.509 certificate.
   *
   * @param[in] cert X.509 certificate
   *
   * @returns X.509 certificate string representation.
   */
  static std::string cert_to_string(X509 *cert);

  /**
   * Generate X.509 cerificate.
   *
   * Generate X.509 cerificate that could be either self-signed or signed by
   * some provided CA certificate. Certificate will be by default valid for
   * 10 years.
   *
   * @param[in] pkey EVP_PKEY object containing public/private key pair.
   * @param[in] common_name Common name that will be used in certificate Subject
   * name section.
   * @param[in] serial Serial number that will be encoded into the certificate.
   * @param[in] ca_cert Certificate that will be used to sign certificate
   * returned by this method. If ca_cert is nullptr then returned certificate
   * will be self-signed.
   * @param[in] ca_pkey CA private key that will be used to sign the
   * certificate, for a self signed certificate 'pkey' argument will be used.
   * @param[in] notbefore Certificate validity period start.
   * @param[in] notafter Certificate validity period end.
   *
   * @return X.509 certificate on success or std::error_code if
   * certificate generation failed.
   */
  stdx::expected<X509Cert, std::error_code> generate_x509(
      EVP_PKEY *pkey, const std::string &common_name, const uint32_t serial,
      X509 *ca_cert, EVP_PKEY *ca_pkey, uint32_t notbefore = 0,
      uint32_t notafter = 10 * k_year) const;

 private:
  constexpr static uint32_t k_year = 365 * 24 * 60 * 60;
  constexpr static uint32_t k_max_cn_name_length = 64;
};

#endif  // ROUTER_CERTIFICATE_GENERATOR_INCLUDED
