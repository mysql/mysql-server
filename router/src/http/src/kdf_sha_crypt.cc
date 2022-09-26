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

#include "kdf_sha_crypt.h"

#include <algorithm>
#include <cstddef>  // size_t
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

#include "base64.h"

std::string ShaCrypt::salt() {
  // 12 byte input, generate 16 byte output
  std::vector<uint8_t> out(12);

  if (0 == RAND_bytes(out.data(), out.size())) {
    throw std::runtime_error("getting random bytes failed");
  }

  return base64_encode(out);
}

ShaCryptMcfAdaptor ShaCryptMcfAdaptor::from_mcf(const std::string &crypt_data) {
  if (crypt_data.size() == 0 || crypt_data.at(0) != '$') {
    throw std::invalid_argument("no $ at the start");
  }
  const auto algo_begin = crypt_data.begin() + 1;
  const auto algo_end = std::find(algo_begin, crypt_data.end(), '$');

  if (std::distance(algo_end, crypt_data.end()) == 0) {
    throw std::invalid_argument("no $ after prefix");
  }
  const auto algorithm = std::string{algo_begin, algo_end};
  if (algorithm == "A")
    return CachingSha2Adaptor::from_mcf({algo_end + 1, crypt_data.end()});

  Type type;
  bool success;
  std::tie(success, type) = ShaCryptMcfType::type(algorithm);
  if (!success) {
    throw std::runtime_error("algorithm-id $" + algorithm +
                             "$ is not supported");
  }

  const auto rounds_begin = algo_end + 1;
  const auto rounds_end = std::find(rounds_begin, crypt_data.end(), '$');
  unsigned long rounds = kDefaultRounds;

  auto salt_begin = rounds_begin;
  // if a $ was found and there is enough data for rounds=
  // at least one $ try to find rounds=<uint>
  if ((rounds_end != crypt_data.end()) &&
      (std::distance(rounds_begin, rounds_end) > 7)) {
    auto rounds_str = std::string{algo_end + 1, rounds_end};

    if (rounds_str.substr(0, 7) == "rounds=") {
      char *endp = nullptr;
      const auto num =
          rounds_str.substr(7);  // keep alive as long as endp is alive,
                                 // otherwise stack-use-after-scope
      const auto r = std::strtol(num.c_str(), &endp, 10);

      if ((*endp == '\0') && (r >= 0)) {
        rounds = r;

        // salt begins after round's $
        salt_begin = rounds_end + 1;
      }
    }
  }
  const auto salt_end = std::find(salt_begin, crypt_data.end(), '$');
  const auto salt = std::distance(salt_begin, salt_end) > 0
                        ? std::string{salt_begin, salt_end}
                        : "";

  // may be empty
  const auto checksum_b64 = salt_end < crypt_data.end()
                                ? std::string{salt_end + 1, crypt_data.end()}
                                : "";

  return {type, rounds, salt, checksum_b64};
}

std::string ShaCryptMcfAdaptor::to_mcf() const {
  return std::string("$") + mcf_digest_name() +
         ((rounds() != kDefaultRounds) ? ("$rounds=" + std::to_string(rounds()))
                                       : "") +
         "$" + salt() + "$" + checksum();
}

ShaCryptMcfAdaptor CachingSha2Adaptor::from_mcf(const std::string &crypt_data) {
  unsigned long rounds = kDefaultRounds;
  const auto rounds_begin = crypt_data.begin();
  const auto rounds_end = std::find(rounds_begin, crypt_data.end(), '$');
  if (rounds_end != crypt_data.end()) {
    rounds = std::stoi(std::string{rounds_begin, rounds_end});
    rounds *= 1000;  // caching_sha2 encodes rounds/1000 (e.g. 5000 as 005)
  }

  auto salt_begin = rounds_end + 1;
  if (std::distance(salt_begin, std::end(crypt_data)) <
      static_cast<long>(kCachingSha2SaltLength)) {
    throw std::runtime_error("invalid MCF for caching_sha2_password");
  }

  const auto salt_end = salt_begin + kCachingSha2SaltLength;
  const auto salt = std::string{salt_begin, salt_end};

  // may be empty
  const auto checksum_b64 = salt_end < std::end(crypt_data)
                                ? std::string{salt_end, std::end(crypt_data)}
                                : "";

  return {Type::CachingSha2Password, rounds, salt, checksum_b64};
}

/**
 * get Digest::Type for ShaCrypt::Type.
 */
static Digest::Type get_digest_type(ShaCrypt::Type type) {
  switch (type) {
    case ShaCrypt::Type::Sha256:
      return Digest::Type::Sha256;
    case ShaCrypt::Type::Sha512:
      return Digest::Type::Sha512;
    case ShaCrypt::Type::CachingSha2Password:
      return Digest::Type::Sha256;
  }

  throw std::invalid_argument("unreachable: type wasn't part of Type");
}

std::string ShaCrypt::derive(ShaCrypt::Type type, unsigned long rounds,
                             const std::string &salt,
                             const std::string &password) {
  // see https://www.akkadia.org/drepper/SHA-crypt.txt "Algorithm for crypt
  // using SHA-256/SHA-512" for explanation of the comment numbers below.
  Digest::Type md = get_digest_type(type);

  std::vector<std::uint8_t> a_out(Digest::digest_size(md));
  {
    // 1.
    Digest a(md);
    // 2.
    a.update(password);
    // 3.
    a.update(salt);

    std::vector<std::uint8_t> b_out(Digest::digest_size(md));
    {
      // 4.
      Digest b(md);
      // 5.
      b.update(password);
      // 6.
      b.update(salt);
      // 7.
      b.update(password);
      // 8.
      b.finalize(b_out);
    }

    {
      // 9.
      size_t cnt;
      const size_t step_size = Digest::digest_size(md);

      for (cnt = password.size(); cnt > step_size; cnt -= step_size) {
        a.update(b_out);
      }
      // 10.
      a.update(std::vector<uint8_t>(b_out.begin(), b_out.begin() + cnt));
    }

    // 11.
    for (size_t cnt = password.size(); cnt > 0; cnt >>= 1) {
      if ((cnt & 1) != 0) {
        a.update(b_out);
      } else {
        a.update(password);
      }
    }

    // 12.
    a.finalize(a_out);
  }

  std::vector<std::uint8_t> tmp_out(Digest::digest_size(md));
  {
    // 13.
    Digest dp(md);

    // 14.
    for (size_t cnt = 0; cnt < password.size(); ++cnt) {
      dp.update(password);
    }

    // 15.
    dp.finalize(tmp_out);
  }

  // P
  std::vector<uint8_t> p_bytes(password.size());
  {
    size_t cnt;
    const size_t step_size = Digest::digest_size(md);

    // 16.
    p_bytes.resize(0);
    for (cnt = password.size(); cnt >= step_size; cnt -= step_size) {
      p_bytes.insert(p_bytes.end(), tmp_out.begin(), tmp_out.end());
    }
    p_bytes.insert(p_bytes.end(), tmp_out.begin(), tmp_out.begin() + cnt);
  }

  // S
  {
    // 17.
    Digest ds(md);

    // 18.
    for (size_t cnt = 0; cnt < 16u + a_out[0]; ++cnt) {
      ds.update(salt);
    }

    // 19.
    ds.finalize(tmp_out);
  }

  // 20.
  std::vector<uint8_t> s_bytes(salt.size());
  {
    size_t cnt;
    const size_t step_size = Digest::digest_size(md);

    s_bytes.resize(0);
    for (cnt = salt.size(); cnt >= step_size; cnt -= step_size) {
      s_bytes.insert(s_bytes.end(), tmp_out.begin(), tmp_out.end());
    }
    s_bytes.insert(s_bytes.end(), tmp_out.begin(), tmp_out.begin() + cnt);
  }

  // 21.
  for (unsigned int cnt = 0; cnt < rounds; ++cnt) {
    Digest c(md);

    if ((cnt & 1) != 0) {
      // b
      c.update(p_bytes);
    } else {
      // c
      c.update(a_out);
    }

    if (cnt % 3 != 0) {
      // d
      c.update(s_bytes);
    }
    if (cnt % 7 != 0) {
      // e
      c.update(p_bytes);
    }
    if ((cnt & 1) != 0) {
      // f
      c.update(a_out);
    } else {
      // g
      c.update(p_bytes);
    }
    // h
    c.finalize(a_out);
  }

  // shuffle
  std::vector<uint8_t> shuffled(Digest::digest_size(md));

  if (get_digest_type(type) == Digest::Type::Sha256) {
    const std::array<std::uint8_t, 32> shuffle_ndxes{
        20, 10, 0,  11, 1, 21, 2, 22, 12, 23, 13, 3,  14, 4, 24, 5,
        25, 15, 26, 16, 6, 17, 7, 27, 8,  28, 18, 29, 19, 9, 30, 31};
    auto shuffled_it = shuffled.begin();
    for (auto const &ndx : shuffle_ndxes) {
      *(shuffled_it++) = a_out[ndx];
    }

  } else {
    const std::array<std::uint8_t, 64> shuffle_ndxes{
        42, 21, 0,  1,  43, 22, 23, 2,  44, 45, 24, 3,  4,  46, 25, 26,
        5,  47, 48, 27, 6,  7,  49, 28, 29, 8,  50, 51, 30, 9,  10, 52,
        31, 32, 11, 53, 54, 33, 12, 13, 55, 34, 35, 14, 56, 57, 36, 15,
        16, 58, 37, 38, 17, 59, 60, 39, 18, 19, 61, 40, 41, 20, 62, 63};
    auto shuffled_it = shuffled.begin();
    for (auto const &ndx : shuffle_ndxes) {
      *(shuffled_it++) = a_out[ndx];
    }
  }

  return base64_encode(shuffled);
}

std::string ShaCrypt::base64_encode(const std::vector<uint8_t> &data) {
  return Radix64Crypt::encode(data);
}
