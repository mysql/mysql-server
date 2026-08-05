/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COMMON_H_
#define COMMON_H_

#include <openssl/evp.h> /* EVP_* */

#include "my_inttypes.h" /* size_t */

extern EVP_MD *md_sha256;

/* Length of random salt in base64 format */
#define BASE64_SALT_LENGTH 45

/**
  Helper method to convert base64 string to url safe base64.
*/
void url_compatible_base64(char *url_compatible_str, size_t len,
                           char *base64_str);
/**
  Helper method to generate sha256 hash.
*/
bool generate_sha256(const unsigned char *in_key, unsigned int in_key_length,
                     unsigned char *hash, unsigned int &hash_length);
#endif  // COMMON_H_
