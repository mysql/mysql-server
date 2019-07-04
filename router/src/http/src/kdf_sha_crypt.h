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
#ifndef ROUTER_KDF_SHA_CRYPT_INCLUDED
#define ROUTER_KDF_SHA_CRYPT_INCLUDED

#include <string>
#include <utility>  // std::pair
#include <vector>

#include "digest.h"
#include "mcf_error.h"

/**
 * SHA based crypt() key derivation function.
 *
 * - sha256_crypt
 * - sha512_crypt
 *
 * @see  https://www.akkadia.org/drepper/SHA-crypt.txt
 */
class ShaCrypt {
 public:
  enum class Type { Sha256, Sha512 };
  static std::string salt();
  static std::string derive(Type digest, unsigned long rounds,
                            const std::string &salt,
                            const std::string &password);

 private:
  /**
   * crypt specific base64 encode.
   *
   * different alphabet than RFC4648
   */
  static std::string base64_encode(const std::vector<uint8_t> &data);
};

class ShaCryptMcfType {
 public:
  using Type = ShaCrypt::Type;

  static std::pair<bool, std::string> name(Type type) noexcept {
    switch (type) {
      case Type::Sha256:
        return std::make_pair(true, kTypeSha256);
      case Type::Sha512:
        return std::make_pair(true, kTypeSha512);
    }

    return std::make_pair(false, std::string{});
  }

  static std::pair<bool, Type> type(const std::string &name) noexcept {
    if (name == kTypeSha256) {
      return std::make_pair(true, Type::Sha256);
    } else if (name == kTypeSha512) {
      return std::make_pair(true, Type::Sha512);
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

 private:
  static constexpr char kTypeSha256[] = "5";
  static constexpr char kTypeSha512[] = "6";
};

/**
 * MCF reader/writer for ShaCrypt
 */
class ShaCryptMcfAdaptor {
 public:
  using mcf_type = ShaCryptMcfType;
  using kdf_type = ShaCrypt;

  using Type = mcf_type::Type;

  /**
   * number of rounds if no rounds was specified in from_mcf().
   */
  static constexpr unsigned long kDefaultRounds = 5000;
  /**
   * minimum rounds.
   */
  static constexpr unsigned long kMinRounds = 1000;
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

  ShaCryptMcfAdaptor(Type digest, unsigned long rounds, const std::string &salt,
                     const std::string &checksum)
      : digest_{digest}, rounds_{rounds}, salt_{salt}, checksum_{checksum} {
    // limit salt
    if (salt_.size() > kMaxSaltLength) {
      salt_.resize(kMaxSaltLength);
    }

    // limit rounds to allowed range
    if (rounds_ < kMinRounds) rounds_ = kMinRounds;
    if (rounds_ > kMaxRounds) rounds_ = kMaxRounds;
  }

  /**
   * name of the digest according to MCF.
   *
   * - 5 for SHA256
   * - 6 for SHA512
   */
  std::string mcf_digest_name() const {
    auto r = mcf_type::name(digest());
    if (r.first) return r.second;

    throw std::invalid_argument("failed to map digest to a name");
  }

  /**
   * checkum.
   *
   * in crypt-specific base64 encoding
   */
  std::string checksum() const { return checksum_; }

  /**
   * salt.
   *
   * @pre must be [a-z0-9]*
   */
  std::string salt() const { return salt_; }

  /**
   *
   */
  Type digest() const { return digest_; }

  /**
   * rounds.
   *
   * number of rounds the hash will be applied
   */
  unsigned long rounds() const { return rounds_; }

  /**
   * build ShaCrypt from a MCF notation.
   *
   * - ${prefix}$rounds={rounds}${salt}${checksum}
   * - ${prefix}$rounds={rounds}${salt}
   * - ${prefix}${salt}${checksum}
   * - ${prefix}${salt}
   *
   * prefix
   * :  [56] (5 is SHA256, 6 is SHA512)
   *
   * rounds
   * :  [0-9]+
   *
   * salt
   * :  [^$]*
   *
   * checksum
   * :  [./a-zA-Z0-0]*
   */
  static ShaCryptMcfAdaptor from_mcf(const std::string &data);

  /**
   * encode to MCF.
   *
   * MCF (Modular Crypt Format)
   */
  std::string to_mcf() const;

  /**
   * hash a password into checksum.
   *
   * updates checksum
   */
  void hash(const std::string &password) {
    checksum_ = kdf_type::derive(digest_, rounds_, salt_, password);
  }

  static bool supports_mcf_id(const std::string mcf_id) {
    return mcf_type::supports_name(mcf_id);
  }

  static std::error_code validate(const std::string &mcf_line,
                                  const std::string &password) {
    try {
      auto me = from_mcf(mcf_line);
      if (kdf_type::derive(me.digest(), me.rounds(), me.salt(), password) ==
          me.checksum()) {
        return {};
      } else {
        return std::make_error_code(McfErrc::kPasswordNotMatched);
      }
    } catch (const std::exception &) {
      // treat all exceptions as parse-errors
      return std::make_error_code(McfErrc::kParseError);
    }
  }

 private:
  Type digest_;
  unsigned long rounds_;
  std::string salt_;
  std::string checksum_;
};

#endif
