/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/**
  @file

  @brief
  Wrapper functions for OpenSSL, WolfSSL implementations. Also provides a
  Compatibility layer to make available YaSSL's SHA1 implementation.
*/

#include "mysqlrouter/sha1.h"
#include "openssl/sha.h"

namespace my_sha1 {

#if defined(HAVE_WOLFSSL) || defined(HAVE_OPENSSL)

namespace {
int mysql_sha1_reset(SHA_CTX *context) { return SHA1_Init(context); }

int mysql_sha1_input(SHA_CTX *context, const uint8_t *message_array,
                     unsigned length) {
  return SHA1_Update(context, message_array, length);
}

int mysql_sha1_result(SHA_CTX *context,
                      uint8_t Message_Digest[SHA1_HASH_SIZE]) {
  return SHA1_Final(Message_Digest, context);
}
}  // namespace

/**
  Wrapper function to compute SHA1 message digest.

  @param digest [out]  Computed SHA1 digest
  @param buf    [in]   Message to be computed
  @param len    [in]   Length of the message
*/
void compute_sha1_hash(uint8_t *digest, const char *buf, size_t len) {
  SHA_CTX sha1_context;

  mysql_sha1_reset(&sha1_context);
  mysql_sha1_input(&sha1_context, (const uint8_t *)buf, len);
  mysql_sha1_result(&sha1_context, digest);
}

#endif

}  // namespace my_sha1
