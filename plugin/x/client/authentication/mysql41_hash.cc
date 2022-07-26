/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/client/authentication/mysql41_hash.h"

#include <openssl/sha.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/evp.h>
#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

#if OPENSSL_VERSION_NUMBER < 0x30000000L
int mysql_mysql41_hash_reset(SHA_CTX *context) { return SHA1_Init(context); }

int mysql_mysql41_hash_input(SHA_CTX *context, const uint8_t *message_array,
                             unsigned length) {
  return SHA1_Update(context, message_array, length);
}

int mysql_mysql41_hash_result(SHA_CTX *context,
                              uint8_t Message_Digest[MYSQL41_HASH_SIZE]) {
  return SHA1_Final(Message_Digest, context);
}
#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

/**
  Wrapper function to compute mysql41_hash message digest.

  @param[out] digest   Computed mysql41_hash digest
  @param[in]  buf      Message to be computed
  @param[in]  len      Length of the message
*/
void compute_mysql41_hash(uint8_t *digest, const char *buf, unsigned len) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  /*
    EVP_Digest() is a wrapper around the EVP_DigestInit_ex(),
    EVP_Update() and EVP_Final_ex() functions.
  */
  EVP_Digest(buf, len, digest, nullptr, EVP_sha1(), nullptr);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  SHA_CTX mysql41_hash_context;

  mysql_mysql41_hash_reset(&mysql41_hash_context);
  mysql_mysql41_hash_input(&mysql41_hash_context, (const uint8_t *)buf, len);
  mysql_mysql41_hash_result(&mysql41_hash_context, digest);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
}

/**
  Wrapper function to compute mysql41_hash message digest for
  two messages in order to emulate mysql41_hash(msg1, msg2).

  @param[out] digest   Computed mysql41_hash digest
  @param[in]  buf1     First message
  @param[in]  len1     Length of first message
  @param[in]  buf2     Second message
  @param[in]  len2     Length of second message
*/
void compute_mysql41_hash_multi(uint8_t *digest, const char *buf1,
                                unsigned len1, const char *buf2,
                                unsigned len2) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(md_ctx, EVP_sha1(), nullptr);
  EVP_DigestUpdate(md_ctx, buf1, len1);
  EVP_DigestUpdate(md_ctx, buf2, len2);
  EVP_DigestFinal_ex(md_ctx, digest, nullptr);
  EVP_MD_CTX_free(md_ctx);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  SHA_CTX mysql41_hash_context;

  mysql_mysql41_hash_reset(&mysql41_hash_context);
  mysql_mysql41_hash_input(&mysql41_hash_context, (const uint8_t *)buf1, len1);
  mysql_mysql41_hash_input(&mysql41_hash_context, (const uint8_t *)buf2, len2);
  mysql_mysql41_hash_result(&mysql41_hash_context, digest);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
}
