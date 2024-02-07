/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "sha2.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/cache_based_verification.h"
#include "plugin/x/src/sha256_password_cache.h"

namespace xpl {

/**
  Convert given asciiz string of hex (0..9 a..f) characters to octet
  sequence.

  @param[out] to Buffer to place result; must be at least len/2 bytes
  @param[in] str Input buffer; 'str' and 'to' may not overlap;
  @param[in] len length for character string; len % 2 == 0
 */
void Cache_based_verification::hex2octet(uint8_t *to, const char *str,
                                         uint32_t len) const {
  auto char_val = [](uint8_t X) {
    return (X >= '0' && X <= '9'
                ? X - '0'
                : X >= 'A' && X <= 'Z' ? X - 'A' + 10 : X - 'a' + 10);
  };

  const char *str_end = str + len;
  while (str < str_end) {
    char tmp = char_val(*str++);
    *to++ = (tmp << 4) | char_val(*str++);
  }
}

/**
  Verify user credentials based on information stored in the sha256 password
  cache, salt which was provided to the user and client response containing
  SHA256(SHA256(password))

  @param[in] user Username
  @param[in] host Hostname
  @param[in] client_string_hex Client response provided by the user in the
  second phase of challenge response authentication, provided in hex format

  @return Result of verification
    @retval true Verification successful
    @retval false Verification unsuccessful
*/
bool Cache_based_verification::verify_authentication_string(
    const std::string &user, const std::string &host,
    const std::string &client_string_hex,
    const std::string & /* unused */) const {
  if (client_string_hex.empty()) return false;

  if (!m_sha256_password_cache) return false;

  auto stored_hash = m_sha256_password_cache->get_entry(user, host);
  if (!stored_hash.first) return false;

  uint8_t client_string[SHA256_DIGEST_LENGTH];
  hex2octet(client_string, client_string_hex.c_str(), SHA256_DIGEST_LENGTH * 2);

  sha2_password::Validate_scramble validate_scramble(
      client_string,
      reinterpret_cast<const unsigned char *>(stored_hash.second.c_str()),
      reinterpret_cast<const unsigned char *>(get_salt().c_str()),
      get_salt().length());

  return (validate_scramble.validate() == 0);
}

}  // namespace xpl
