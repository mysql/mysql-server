/* Copyright (c) 2007, 2026, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_sha2.cc
  A compatibility layer to our built-in SSL implementation, to mimic the
  oft-used external library, OpenSSL.
*/

#include <stddef.h>

#include "my_compiler.h"
#include "my_ssl_algo_cache.h"  // IWYU pragma: keep
#include "sha2.h"

#include <openssl/opensslv.h>  // for OPENSSL_VERSION_NUMBER
#include <openssl/sha.h>       // for SHA224_Final, SHA224_Init, SHA224_Update
#if OPENSSL_VERSION_NUMBER >= 0x30000000L

// OpenSSL3.x EVP APIs are 6x slower than these (deprecated) APIs
MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wdeprecated-declarations")
MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wdeprecated-declarations")
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4996)

unsigned char *SHA_EVP512(const unsigned char *input_ptr, size_t input_length,
                          char unsigned *output_ptr) {
  SHA512_CTX ctx;
  SHA512_Init(&ctx);
  SHA512_Update(&ctx, input_ptr, input_length);
  SHA512_Final(output_ptr, &ctx);
  return output_ptr;
}

unsigned char *SHA_EVP384(const unsigned char *input_ptr, size_t input_length,
                          char unsigned *output_ptr) {
  SHA512_CTX ctx;
  SHA384_Init(&ctx);
  SHA384_Update(&ctx, input_ptr, input_length);
  SHA384_Final(output_ptr, &ctx);
  return output_ptr;
}

unsigned char *SHA_EVP256(const unsigned char *input_ptr, size_t input_length,
                          char unsigned *output_ptr) {
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, input_ptr, input_length);
  SHA256_Final(output_ptr, &ctx);
  return output_ptr;
}

unsigned char *SHA_EVP224(const unsigned char *input_ptr, size_t input_length,
                          char unsigned *output_ptr) {
  SHA256_CTX ctx;
  SHA224_Init(&ctx);
  SHA224_Update(&ctx, input_ptr, input_length);
  SHA224_Final(output_ptr, &ctx);
  return output_ptr;
}

// restore clang/gcc checks for -Wdeprecated-declarations
MY_COMPILER_DIAGNOSTIC_POP()

#else /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
#define GEN_OPENSSL_EVP_SHA2_BRIDGE(size)                          \
  unsigned char *SHA_EVP##size(const unsigned char *input_ptr,     \
                               size_t input_length,                \
                               char unsigned *output_ptr) {        \
    EVP_MD_CTX *md_ctx = EVP_MD_CTX_create();                      \
    EVP_DigestInit_ex(md_ctx, my_EVP_sha##size(), NULL);           \
    EVP_DigestUpdate(md_ctx, input_ptr, input_length);             \
    EVP_DigestFinal_ex(md_ctx, (unsigned char *)output_ptr, NULL); \
    EVP_MD_CTX_destroy(md_ctx);                                    \
    return output_ptr;                                             \
  }

/*
  @fn SHA_EVP512
  @fn SHA_EVP384
  @fn SHA_EVP256
  @fn SHA_EVP224
*/

GEN_OPENSSL_EVP_SHA2_BRIDGE(512)
GEN_OPENSSL_EVP_SHA2_BRIDGE(384)
GEN_OPENSSL_EVP_SHA2_BRIDGE(256)
GEN_OPENSSL_EVP_SHA2_BRIDGE(224)
#undef GEN_OPENSSL_EVP_SHA2_BRIDGE

#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
