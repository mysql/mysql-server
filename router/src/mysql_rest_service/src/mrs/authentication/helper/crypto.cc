/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mrs/authentication/helper/crypto.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace mrs {
namespace authentication {

std::string crypto_hmac(const std::string &key, const std::string &data) {
  std::string output;
  output.resize(EVP_MAX_MD_SIZE);
  unsigned int len = output.length();

  if (nullptr == HMAC(EVP_sha256(), key.c_str(), key.length(),
                      reinterpret_cast<const uint8_t *>(data.c_str()),
                      data.length(), reinterpret_cast<uint8_t *>(output.data()),
                      &len))
    return {};

  output.resize(len);

  return output;
}

std::string crypto_xor(const std::string &key, const std::string &data) {
  auto min_length = std::min(key.length(), data.length());
  auto max_length = std::max(key.length(), data.length());
  std::string result(max_length, ' ');

  for (size_t i = 0; i < max_length; ++i) {
    if (i >= min_length) {
      if (i < data.length())
        result[i] = data[i];
      else
        result[i] = key[i];
      continue;
    }

    result[i] = key[i] ^ data[i];
  }

  return result;
}

std::string crypto_sha256(const std::string &data) {
  auto md_context = EVP_MD_CTX_create();

  if (!md_context) return {};

  if (!static_cast<bool>(EVP_DigestInit_ex(md_context, EVP_sha256(), nullptr)))
    return {};

  bool ok = EVP_DigestUpdate(md_context, data.c_str(), data.length());

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_length = sizeof(digest);
  if (ok) ok = EVP_DigestFinal_ex(md_context, digest, &digest_length);

  EVP_MD_CTX_destroy(md_context);
  return ok ? std::string(reinterpret_cast<char *>(digest), digest_length)
            : std::string{};
}

}  // namespace authentication
}  // namespace mrs
