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
#ifndef MYSQLROUTER_KDF_PBKDF2_INCLUDED
#define MYSQLROUTER_KDF_PBKDF2_INCLUDED

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "mcf_error.h"
#include "mysqlrouter/http_auth_backend_lib_export.h"

/**
 * Key Derivation Function for PBKDF2.
 *
 * See: RFC 2898
 *
 * while PBKDF2 support several hash-functions, only the most commonly used,
 * secure variants are exposed:
 *
 * - SHA256
 * - SHA512
 *
 * while the insecure ones are not offered:
 *
 * - SHA1
 *
 * Other HMACs of (https://tools.ietf.org/html/rfc8018#appendix-B.1.2)
 * may be added:
 *
 * - SHA224
 * - SHA384
 * - SHA512-224
 * - SHA512-256
 */
class HTTP_AUTH_BACKEND_LIB_EXPORT Pbkdf2 {
 public:
  enum class Type { Sha_256, Sha_512 };
  static std::vector<uint8_t> salt();
  static std::vector<uint8_t> derive(Type type, unsigned long rounds,
                                     const std::vector<uint8_t> &salt,
                                     const std::string &key);
};

/**
 * map the MCF name to internal types.
 *
 * MCF-name are taking from passlib:
 *
 * https://passlib.readthedocs.io/en/stable/modular_crypt_format.html#application-defined-hashes
 */
class Pbkdf2McfType {
 public:
  using Type = Pbkdf2::Type;
  static constexpr char kTypeSha256[] = "pbkdf2-sha256";
  static constexpr char kTypeSha512[] = "pbkdf2-sha512";

  static std::pair<bool, std::string> name(Type type) noexcept {
    switch (type) {
      case Type::Sha_256:
        return std::make_pair(true, kTypeSha256);
      case Type::Sha_512:
        return std::make_pair(true, kTypeSha512);
    }

    return std::make_pair(false, std::string{});
  }

  static std::pair<bool, Type> type(const std::string &name) noexcept {
    if (name == kTypeSha256) {
      return std::make_pair(true, Type::Sha_256);
    } else if (name == kTypeSha512) {
      return std::make_pair(true, Type::Sha_512);
    }

    return std::make_pair(false, Type{});
  }

  static bool supports_name(const std::string &name) noexcept {
    if (name == kTypeSha256) {
      return true;
    } else if (name == kTypeSha512) {
      return true;
    }

    return false;
  }
};

/**
 * MCF reader/writer for PBKDF2.
 */
class HTTP_AUTH_BACKEND_LIB_EXPORT Pbkdf2McfAdaptor {
 public:
  using mcf_type = Pbkdf2McfType;
  using kdf_type = Pbkdf2;
  using Type = mcf_type::Type;

  /**
   * rounds if no rounds was specified in from_mcf().
   */
  static constexpr unsigned long kDefaultRounds = 1000;
  /**
   * minimum rounds.
   */
  static constexpr unsigned long kMinRounds = 1;
  /**
   * maximum rounds.
   */
  static constexpr unsigned long kMaxRounds = 999999999;
  /**
   * maximum length of the salt.
   *
   * only the first kMaxSaltLength bytes of the salt will be used.
   */
  static constexpr size_t kMaxSaltLength = 16;

  Pbkdf2McfAdaptor(Type type, unsigned long rounds,
                   const std::vector<uint8_t> &salt,
                   const std::vector<uint8_t> &checksum)
      : type_{type}, rounds_{rounds}, salt_{salt}, checksum_{checksum} {
    // limit salt
    if (salt_.size() > kMaxSaltLength) {
      salt_.resize(kMaxSaltLength);
    }

    // limit rounds to allow range
    if (rounds_ < kMinRounds) rounds_ = kMinRounds;
    if (rounds_ > kMaxRounds) rounds_ = kMaxRounds;
  }

  /**
   * name of the digest according to MCF.
   *
   * - pbkdf2_sha256 for SHA256
   * - pbkdf2_sha512 for SHA512
   */
  std::string mcf_id() const {
    auto r = mcf_type::name(digest());
    if (r.first) return r.second;

    throw std::invalid_argument("failed to map digest to a name");
  }

  /**
   * checksum.
   *
   * RFC4648 base64 encoded
   */
  std::vector<uint8_t> checksum() const { return checksum_; }

  /**
   * salt.
   *
   * @pre must be [a-z0-9]*
   */
  std::vector<uint8_t> salt() const { return salt_; }

  /**
   *
   */
  Type digest() const { return type_; }

  /**
   * rounds.
   *
   * rounds the hash will be applied on itself.
   */
  unsigned long rounds() const { return rounds_; }

  /**
   * build PBKDF2 from a MCF notation.
   *
   * - ${prefix}${rounds}${salt}${checksum}
   * - ${prefix}${rounds}${salt}
   *
   * prefix
   * :  pbkdf2_sha256|pbkdf2_sha512
   *
   * rounds
   * :  [1-9][0-9]*
   *
   * salt
   * :  [^$]*
   *
   * checksum
   * :  [./a-zA-Z0-0]*
   */
  static Pbkdf2McfAdaptor from_mcf(const std::string &data);

  /**
   * encode to MCF.
   *
   * MCF (Modular Crypt Format)
   */
  std::string to_mcf() const;

  /**
   * Base64 encode.
   *
   * Variant of RFC... with a different alphabet
   *
   * - no whitespace
   * - no padding
   * - . and / as altchars instead of + and /
   */
  static std::vector<uint8_t> base64_decode(const std::string &binary);

  /**
   * Base64 decode.
   *
   * Variant of RFC... with a different alphabet
   */
  static std::string base64_encode(const std::vector<uint8_t> &encoded);

  /**
   * derive a checksum from a key.
   *
   * updates checksum
   */
  void derive(const std::string &key) {
    checksum_ = kdf_type::derive(type_, rounds(), salt(), key);
  }

  static bool supports_mcf_id(const std::string mcf_id) {
    return mcf_type::supports_name(mcf_id);
  }

  static std::error_code validate(const std::string &mcf_line,
                                  const std::string &password) {
    try {
      auto mcf = from_mcf(mcf_line);
      if (kdf_type::derive(mcf.digest(), mcf.rounds(), mcf.salt(), password) ==
          mcf.checksum()) {
        return {};
      } else {
        return make_error_code(McfErrc::kPasswordNotMatched);
      }
    } catch (const std::exception &) {
      // whatever the exception was, make it a parse-error
      return make_error_code(McfErrc::kParseError);
    }
  }

 private:
  Type type_;
  unsigned long rounds_;
  std::vector<uint8_t> salt_;
  std::vector<uint8_t> checksum_;
};

#endif
