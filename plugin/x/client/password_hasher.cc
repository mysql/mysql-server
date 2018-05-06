/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "plugin/x/client/password_hasher.h"

#include <sys/types.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include <openssl/rand.h>
#include <openssl/sha.h>

#include "my_dbug.h"
#include "plugin/x/client/mysql41_hash.h"

#define PVERSION41_CHAR '*'
#define SCRAMBLE_LENGTH 20

namespace xcl {
namespace password_hasher {
namespace {

const char *_dig_vec_upper = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void compute_two_stage_mysql41_hash(const char *password, size_t pass_len,
                                    uint8_t *hash_stage1,
                                    uint8_t *hash_stage2) {
  /* Stage 1: hash pwd */
  compute_mysql41_hash(hash_stage1, password, static_cast<unsigned>(pass_len));

  /* Stage 2 : hash first stage's output. */
  compute_mysql41_hash(hash_stage2, (const char *)hash_stage1,
                       MYSQL41_HASH_SIZE);
}

void my_crypt(char *to, const uint8_t *s1, const uint8_t *s2, size_t len) {
  const uint8_t *s1_end = s1 + len;

  while (s1 < s1_end) *to++ = *s1++ ^ *s2++;
}

}  // namespace

/**
  Convert given octet sequence to asciiz string of hex characters;
  str..str+len and 'to' may not overlap.

  @param [out] to Output buffer. Must be at least 2*len+1 bytes
  @param [in] str Input string
  @param [in] len Length of the input string

  @return End of output buffer at position buf+len*2
 */
char *octet2hex(char *to, const char *str, size_t len) {
  const char *str_end = str + len;
  for (; str != str_end; ++str) {
    *to++ = _dig_vec_upper[((uint8_t)*str) >> 4];
    *to++ = _dig_vec_upper[((uint8_t)*str) & 0x0F];
  }
  *to = '\0';
  return to;
}

/** Generate human readable string from the binary
 *  result from hashing function.
 *
 *  @return empty string when invalid hash was set, else
 *          human readable version of hash_stage2.
 */
std::string get_password_from_salt(const std::string &hash_stage2) {
  const std::uint8_t result_size =
      2 * MYSQL41_HASH_SIZE + 1 /* '\0' sign */ + 1 /* '*' sign */;
  char result[result_size] = {0};

  if (hash_stage2.length() != MYSQL41_HASH_SIZE) return "";

  result[0] = PVERSION41_CHAR;
  octet2hex(&result[1], &hash_stage2[0], MYSQL41_HASH_SIZE);

  // Skip the additional \0 sign added by octet2hex
  return {std::begin(result), std::end(result) - 1};
}

std::string generate_user_salt() {
  std::string result(SCRAMBLE_LENGTH, '\0');
  char *buffer = &result[0];
  char *end = buffer + result.length() - 1;

  RAND_bytes((unsigned char *)buffer, SCRAMBLE_LENGTH);

  /* Sequence must be a legal UTF8 string */
  for (; buffer < end; buffer++) {
    *buffer &= 0x7f;
    if (*buffer == '\0' || *buffer == '$') *buffer = *buffer + 1;
  }

  return result;
}

bool check_scramble_mysql41_hash(const char *scramble_arg, const char *message,
                                 const uint8_t *hash_stage2) {
  char buf[MYSQL41_HASH_SIZE];
  uint8_t hash_stage2_reassured[MYSQL41_HASH_SIZE];

  DBUG_ASSERT(MYSQL41_HASH_SIZE == SCRAMBLE_LENGTH);
  /* create key to encrypt scramble */
  compute_mysql41_hash_multi(reinterpret_cast<uint8_t *>(buf), message,
                             SCRAMBLE_LENGTH, (const char *)hash_stage2,
                             MYSQL41_HASH_SIZE);

  /* encrypt scramble */
  my_crypt(buf, (const uint8_t *)buf, (const uint8_t *)scramble_arg,
           SCRAMBLE_LENGTH);

  /* now buf supposedly contains hash_stage1: so we can get hash_stage2 */
  compute_mysql41_hash(reinterpret_cast<uint8_t *>(hash_stage2_reassured),
                       (const char *)buf, MYSQL41_HASH_SIZE);

  return 0 == memcmp(hash_stage2, hash_stage2_reassured, MYSQL41_HASH_SIZE);
}

std::string scramble(const char *message, const char *password) {
  uint8_t hash_stage1[MYSQL41_HASH_SIZE];
  uint8_t hash_stage2[MYSQL41_HASH_SIZE];
  std::string result(SCRAMBLE_LENGTH, '\0');

  result.at(SCRAMBLE_LENGTH - 1) = '\0';

  DBUG_ASSERT(MYSQL41_HASH_SIZE == SCRAMBLE_LENGTH);

  /* Two stage SHA1 hash of the pwd */
  compute_two_stage_mysql41_hash(password, strlen(password),
                                 reinterpret_cast<uint8_t *>(hash_stage1),
                                 reinterpret_cast<uint8_t *>(hash_stage2));

  /* create crypt string as sha1(message, hash_stage2) */
  compute_mysql41_hash_multi(reinterpret_cast<uint8_t *>(&result[0]), message,
                             SCRAMBLE_LENGTH, (const char *)hash_stage2,
                             MYSQL41_HASH_SIZE);
  my_crypt(&result[0], (const uint8_t *)&result[0], hash_stage1,
           SCRAMBLE_LENGTH);

  return result;
}

}  // namespace password_hasher
}  // namespace xcl
