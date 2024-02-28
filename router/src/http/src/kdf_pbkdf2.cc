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

#include "kdf_pbkdf2.h"

#include <algorithm>
#include <cstdint>  // uint8_t
#include <cstdlib>  // std::strtol
#include <cstring>
#include <iterator>  // std::distance
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>  // std::tie
#include <vector>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "mysqlrouter/base64.h"

std::vector<uint8_t> Pbkdf2::salt() {
  std::vector<uint8_t> out(16);

  if (0 == RAND_bytes(out.data(), static_cast<int>(out.size()))) {
    throw std::runtime_error("getting random bytes failed");
  }

  return out;
}

Pbkdf2McfAdaptor Pbkdf2McfAdaptor::from_mcf(const std::string &crypt_data) {
  if (crypt_data.size() == 0 || crypt_data.at(0) != '$') {
    throw std::invalid_argument("no $ at the start");
  }
  auto algo_begin = crypt_data.begin() + 1;
  auto algo_end = std::find(algo_begin, crypt_data.end(), '$');

  if (std::distance(algo_end, crypt_data.end()) == 0) {
    throw std::invalid_argument("no $ after prefix");
  }
  auto algorithm = std::string{algo_begin, algo_end};

  Type type;
  bool success;

  std::tie(success, type) = Pbkdf2McfType::type(algorithm);
  if (!success) {
    throw std::runtime_error("algorithm-id " + algorithm + " is not supported");
  }

  auto rounds_begin = algo_end + 1;
  auto rounds_end = std::find(rounds_begin, crypt_data.end(), '$');

  // if a $ was found and there is enough data for rounds=
  // at least one $ try to find rounds=<uint>
  if (rounds_end == crypt_data.end())
    throw std::invalid_argument("missing $ after rounds");
  if (std::distance(rounds_begin, rounds_end) == 0)
    throw std::invalid_argument("rounds is empty");

  unsigned int rounds{0};
  {
    auto rounds_str = std::string{rounds_begin, rounds_end};

    char *endp = nullptr;
    auto r = std::strtol(rounds_str.c_str(), &endp, 10);

    if ((*endp != '\0') || (r < 0)) {
      throw std::invalid_argument("invalid rounds");
    }

    rounds = r;
  }
  auto salt_begin = rounds_end + 1;
  auto salt_end = std::find(salt_begin, crypt_data.end(), '$');
  auto salt = std::distance(salt_begin, salt_end) > 0
                  ? std::string{salt_begin, salt_end}
                  : "";

  // may be empty
  auto checksum_b64 = salt_end < crypt_data.end()
                          ? std::string{salt_end + 1, crypt_data.end()}
                          : "";

  return {type, rounds, base64_decode(salt), base64_decode(checksum_b64)};
}

std::string Pbkdf2McfAdaptor::to_mcf() const {
  return std::string("$") + mcf_id() + "$" + std::to_string(rounds()) + "$" +
         base64_encode(salt()) + "$" + base64_encode(checksum());
}

std::vector<uint8_t> Pbkdf2::derive(Pbkdf2::Type type, unsigned long rounds,
                                    const std::vector<uint8_t> &salt,
                                    const std::string &key) {
  const EVP_MD *func = type == Type::Sha_256 ? EVP_sha256() : EVP_sha512();
  std::vector<uint8_t> derived(EVP_MD_size(func));
  if (1 != PKCS5_PBKDF2_HMAC(key.data(), key.size(), salt.data(), salt.size(),
                             rounds, func, derived.capacity(),
                             derived.data())) {
    throw std::runtime_error("PKCS5_PBKDF2_HMAC failed");
  }

  return derived;
}

std::vector<uint8_t> Pbkdf2McfAdaptor::base64_decode(
    const std::string &encoded) {
  return Radix64Mcf::decode(encoded);
}

std::string Pbkdf2McfAdaptor::base64_encode(
    const std::vector<uint8_t> &binary) {
  return Radix64Mcf::encode(binary);
}
