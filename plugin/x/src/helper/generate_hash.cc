/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

#include <cstring>

#include "mysql_com.h"  // octet2hex
#include "plugin/x/src/helper/generate_hash.h"
#include "sha1.h"  // for SHA1_HASH_SIZE

static void compute_two_stage_hash(const char *input, size_t input_len,
                                   uint8 *output) {
  uint8 hash_stage1[SHA1_HASH_SIZE];
  /* Stage 1: hash password */
  compute_sha1_hash(hash_stage1, input, input_len);

  /* Stage 2 : hash first stage's output. */
  compute_sha1_hash(output, (const char *)hash_stage1, SHA1_HASH_SIZE);
}

static void scrambled_input(char *output, const char *input,
                            size_t input_length) {
  uint8 hash_stage2[SHA1_HASH_SIZE];

  /* Two stage SHA1 hash of the password. */
  compute_two_stage_hash(input, input_length, hash_stage2);

  octet2hex(output, (const char *)hash_stage2, SHA1_HASH_SIZE);
}

namespace xpl {

std::string generate_hash(const std::string &input) {
  std::string hash(2 * SHA1_HASH_SIZE + 2, '\0');
  ::scrambled_input(&hash[0], input.c_str(), input.length());
  hash.resize(2 * SHA1_HASH_SIZE);  // strip the \0
  return hash;
}

}  // namespace xpl
