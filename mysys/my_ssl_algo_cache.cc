/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

/**
  @file mysys/my_ssl_algo_cache.cc
*/

#include "my_ssl_algo_cache.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <cassert>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#endif

static int fips_mode = 0;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

// cached message digest algorithms
static EVP_MD *md_sha1 = nullptr;
static EVP_MD *md_sha224 = nullptr;
static EVP_MD *md_sha256 = nullptr;
static EVP_MD *md_sha384 = nullptr;
static EVP_MD *md_sha512 = nullptr;

// cached encryption algorithms
static EVP_CIPHER *crypt_aes_128_ecb = nullptr;
static EVP_CIPHER *crypt_aes_128_cbc = nullptr;
static EVP_CIPHER *crypt_aes_128_cfb1 = nullptr;
static EVP_CIPHER *crypt_aes_128_cfb8 = nullptr;
static EVP_CIPHER *crypt_aes_128_cfb128 = nullptr;
static EVP_CIPHER *crypt_aes_128_ofb = nullptr;
static EVP_CIPHER *crypt_aes_192_ecb = nullptr;
static EVP_CIPHER *crypt_aes_192_cbc = nullptr;
static EVP_CIPHER *crypt_aes_192_cfb1 = nullptr;
static EVP_CIPHER *crypt_aes_192_cfb8 = nullptr;
static EVP_CIPHER *crypt_aes_192_cfb128 = nullptr;
static EVP_CIPHER *crypt_aes_192_ofb = nullptr;
static EVP_CIPHER *crypt_aes_256_ecb = nullptr;
static EVP_CIPHER *crypt_aes_256_cbc = nullptr;
static EVP_CIPHER *crypt_aes_256_cfb1 = nullptr;
static EVP_CIPHER *crypt_aes_256_cfb8 = nullptr;
static EVP_CIPHER *crypt_aes_256_cfb128 = nullptr;
static EVP_CIPHER *crypt_aes_256_ofb = nullptr;
static EVP_CIPHER *crypt_aes_256_xts = nullptr;
static EVP_CIPHER *crypt_aes_256_wrap = nullptr;
static EVP_CIPHER *crypt_aes_256_ctr = nullptr;

#endif

const EVP_MD *my_EVP_sha1() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return md_sha1 ? md_sha1 : EVP_sha1();
#else
  return EVP_sha1();
#endif
}

const EVP_MD *my_EVP_sha224() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return md_sha224 ? md_sha224 : EVP_sha224();
#else
  return EVP_sha224();
#endif
}

const EVP_MD *my_EVP_sha256() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return md_sha256 ? md_sha256 : EVP_sha256();
#else
  return EVP_sha256();
#endif
}

const EVP_MD *my_EVP_sha384() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return md_sha384 ? md_sha384 : EVP_sha384();
#else
  return EVP_sha384();
#endif
}

const EVP_MD *my_EVP_sha512() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return md_sha512 ? md_sha512 : EVP_sha512();
#else
  return EVP_sha512();
#endif
}

const EVP_CIPHER *my_EVP_aes_128_ecb() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_128_ecb ? crypt_aes_128_ecb : EVP_aes_128_ecb();
#else
  return EVP_aes_128_ecb();
#endif
}

const EVP_CIPHER *my_EVP_aes_128_cbc() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_128_cbc ? crypt_aes_128_cbc : EVP_aes_128_cbc();
#else
  return EVP_aes_128_cbc();
#endif
}

const EVP_CIPHER *my_EVP_aes_128_cfb1() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_128_cfb1 ? crypt_aes_128_cfb1 : EVP_aes_128_cfb1();
#else
  return EVP_aes_128_cfb1();
#endif
}

const EVP_CIPHER *my_EVP_aes_128_cfb8() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_128_cfb8 ? crypt_aes_128_cfb8 : EVP_aes_128_cfb8();
#else
  return EVP_aes_128_cfb8();
#endif
}

const EVP_CIPHER *my_EVP_aes_128_cfb128() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_128_cfb128 ? crypt_aes_128_cfb128 : EVP_aes_128_cfb128();
#else
  return EVP_aes_128_cfb128();
#endif
}

const EVP_CIPHER *my_EVP_aes_128_ofb() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_128_ofb ? crypt_aes_128_ofb : EVP_aes_128_ofb();
#else
  return EVP_aes_128_ofb();
#endif
}

const EVP_CIPHER *my_EVP_aes_192_ecb() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_192_ecb ? crypt_aes_192_ecb : EVP_aes_192_ecb();
#else
  return EVP_aes_192_ecb();
#endif
}

const EVP_CIPHER *my_EVP_aes_192_cbc() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_192_cbc ? crypt_aes_192_cbc : EVP_aes_192_cbc();
#else
  return EVP_aes_192_cbc();
#endif
}

const EVP_CIPHER *my_EVP_aes_192_cfb1() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_192_cfb1 ? crypt_aes_192_cfb1 : EVP_aes_192_cfb1();
#else
  return EVP_aes_192_cfb1();
#endif
}

const EVP_CIPHER *my_EVP_aes_192_cfb8() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_192_cfb8 ? crypt_aes_192_cfb8 : EVP_aes_192_cfb8();
#else
  return EVP_aes_192_cfb8();
#endif
}

const EVP_CIPHER *my_EVP_aes_192_cfb128() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_192_cfb128 ? crypt_aes_192_cfb128 : EVP_aes_192_cfb128();
#else
  return EVP_aes_192_cfb128();
#endif
}

const EVP_CIPHER *my_EVP_aes_192_ofb() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_192_ofb ? crypt_aes_192_ofb : EVP_aes_192_ofb();
#else
  return EVP_aes_192_ofb();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_ecb() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_ecb ? crypt_aes_256_ecb : EVP_aes_256_ecb();
#else
  return EVP_aes_256_ecb();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_cbc() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_cbc ? crypt_aes_256_cbc : EVP_aes_256_cbc();
#else
  return EVP_aes_256_cbc();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_cfb1() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_cfb1 ? crypt_aes_256_cfb1 : EVP_aes_256_cfb1();
#else
  return EVP_aes_256_cfb1();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_cfb8() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_cfb8 ? crypt_aes_256_cfb8 : EVP_aes_256_cfb8();
#else
  return EVP_aes_256_cfb8();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_cfb128() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_cfb128 ? crypt_aes_256_cfb128 : EVP_aes_256_cfb128();
#else
  return EVP_aes_256_cfb128();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_ofb() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_ofb ? crypt_aes_256_ofb : EVP_aes_256_ofb();
#else
  return EVP_aes_256_ofb();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_xts() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_xts ? crypt_aes_256_xts : EVP_aes_256_xts();
#else
  return EVP_aes_256_xts();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_wrap() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_wrap ? crypt_aes_256_wrap : EVP_aes_256_wrap();
#else
  return EVP_aes_256_wrap();
#endif
}

const EVP_CIPHER *my_EVP_aes_256_ctr() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return crypt_aes_256_ctr ? crypt_aes_256_ctr : EVP_aes_256_ctr();
#else
  return EVP_aes_256_ctr();
#endif
}

void my_ssl_algorithm_cache_load() {
  // cache FIPS mode too
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  fips_mode = EVP_default_properties_is_fips_enabled(nullptr) &&
              OSSL_PROVIDER_available(nullptr, "fips");
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  fips_mode = FIPS_mode();
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  // warn if cache loaded multiple times
  assert(md_sha1 == nullptr);

  md_sha1 = EVP_MD_fetch(nullptr, "sha1", nullptr);
  assert(md_sha1 != nullptr);

  md_sha224 = EVP_MD_fetch(nullptr, "sha224", nullptr);
  assert(md_sha224 != nullptr);

  md_sha256 = EVP_MD_fetch(nullptr, "sha256", nullptr);
  assert(md_sha256 != nullptr);

  md_sha384 = EVP_MD_fetch(nullptr, "sha384", nullptr);
  assert(md_sha384 != nullptr);

  md_sha512 = EVP_MD_fetch(nullptr, "sha512", nullptr);
  assert(md_sha512 != nullptr);

  crypt_aes_128_ecb = EVP_CIPHER_fetch(nullptr, "AES-128-ECB", nullptr);
  assert(crypt_aes_128_ecb != nullptr);

  crypt_aes_128_cbc = EVP_CIPHER_fetch(nullptr, "AES-128-CBC", nullptr);
  assert(crypt_aes_128_cbc != nullptr);

  crypt_aes_128_cfb1 = EVP_CIPHER_fetch(nullptr, "AES-128-CFB1", nullptr);
  assert(crypt_aes_128_cfb1 != nullptr);

  crypt_aes_128_cfb8 = EVP_CIPHER_fetch(nullptr, "AES-128-CFB8", nullptr);
  assert(crypt_aes_128_cfb8 != nullptr);

  crypt_aes_128_cfb128 = EVP_CIPHER_fetch(nullptr, "AES-128-CFB", nullptr);
  assert(crypt_aes_128_cfb128 != nullptr);

  crypt_aes_128_ofb = EVP_CIPHER_fetch(nullptr, "AES-128-OFB", nullptr);
  assert(crypt_aes_128_ofb != nullptr);

  crypt_aes_192_ecb = EVP_CIPHER_fetch(nullptr, "AES-192-ECB", nullptr);
  assert(crypt_aes_192_ecb != nullptr);

  crypt_aes_192_cbc = EVP_CIPHER_fetch(nullptr, "AES-192-CBC", nullptr);
  assert(crypt_aes_192_cbc != nullptr);

  crypt_aes_192_cfb1 = EVP_CIPHER_fetch(nullptr, "AES-192-CFB1", nullptr);
  assert(crypt_aes_192_cfb1 != nullptr);

  crypt_aes_192_cfb8 = EVP_CIPHER_fetch(nullptr, "AES-192-CFB8", nullptr);
  assert(crypt_aes_192_cfb8 != nullptr);

  crypt_aes_192_cfb128 = EVP_CIPHER_fetch(nullptr, "AES-192-CFB", nullptr);
  assert(crypt_aes_192_cfb128 != nullptr);

  crypt_aes_192_ofb = EVP_CIPHER_fetch(nullptr, "AES-192-OFB", nullptr);
  assert(crypt_aes_192_ofb != nullptr);

  crypt_aes_256_ecb = EVP_CIPHER_fetch(nullptr, "AES-256-ECB", nullptr);
  assert(crypt_aes_256_ecb != nullptr);

  crypt_aes_256_cbc = EVP_CIPHER_fetch(nullptr, "AES-256-CBC", nullptr);
  assert(crypt_aes_256_cbc != nullptr);

  crypt_aes_256_cfb1 = EVP_CIPHER_fetch(nullptr, "AES-256-CFB1", nullptr);
  assert(crypt_aes_256_cfb1 != nullptr);

  crypt_aes_256_cfb8 = EVP_CIPHER_fetch(nullptr, "AES-256-CFB8", nullptr);
  assert(crypt_aes_256_cfb8 != nullptr);

  crypt_aes_256_cfb128 = EVP_CIPHER_fetch(nullptr, "AES-256-CFB", nullptr);
  assert(crypt_aes_256_cfb128 != nullptr);

  crypt_aes_256_ofb = EVP_CIPHER_fetch(nullptr, "AES-256-OFB", nullptr);
  assert(crypt_aes_256_ofb != nullptr);

  crypt_aes_256_xts = EVP_CIPHER_fetch(nullptr, "AES-256-XTS", nullptr);
  assert(crypt_aes_256_xts != nullptr);

  crypt_aes_256_wrap = EVP_CIPHER_fetch(nullptr, "aes256-wrap", nullptr);
  assert(crypt_aes_256_wrap != nullptr);

  crypt_aes_256_ctr = EVP_CIPHER_fetch(nullptr, "AES-256-CTR", nullptr);
  assert(crypt_aes_256_ctr != nullptr);
#endif

  // do not propagate possible errors
  ERR_clear_error();
}

void my_ssl_algorithm_cache_unload() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_MD_free(md_sha1);
  md_sha1 = nullptr;
  EVP_MD_free(md_sha224);
  md_sha224 = nullptr;
  EVP_MD_free(md_sha256);
  md_sha256 = nullptr;
  EVP_MD_free(md_sha384);
  md_sha384 = nullptr;
  EVP_MD_free(md_sha512);
  md_sha512 = nullptr;

  EVP_CIPHER_free(crypt_aes_128_ecb);
  crypt_aes_128_ecb = nullptr;
  EVP_CIPHER_free(crypt_aes_128_cbc);
  crypt_aes_128_cbc = nullptr;
  EVP_CIPHER_free(crypt_aes_128_cfb1);
  crypt_aes_128_cfb1 = nullptr;
  EVP_CIPHER_free(crypt_aes_128_cfb8);
  crypt_aes_128_cfb8 = nullptr;
  EVP_CIPHER_free(crypt_aes_128_cfb128);
  crypt_aes_128_cfb128 = nullptr;
  EVP_CIPHER_free(crypt_aes_128_ofb);
  crypt_aes_128_ofb = nullptr;
  EVP_CIPHER_free(crypt_aes_192_ecb);
  crypt_aes_192_ecb = nullptr;
  EVP_CIPHER_free(crypt_aes_192_cbc);
  crypt_aes_192_cbc = nullptr;
  EVP_CIPHER_free(crypt_aes_192_cfb1);
  crypt_aes_192_cfb1 = nullptr;
  EVP_CIPHER_free(crypt_aes_192_cfb8);
  crypt_aes_192_cfb8 = nullptr;
  EVP_CIPHER_free(crypt_aes_192_cfb128);
  crypt_aes_192_cfb128 = nullptr;
  EVP_CIPHER_free(crypt_aes_192_ofb);
  crypt_aes_192_ofb = nullptr;
  EVP_CIPHER_free(crypt_aes_256_ecb);
  crypt_aes_256_ecb = nullptr;
  EVP_CIPHER_free(crypt_aes_256_cbc);
  crypt_aes_256_cbc = nullptr;
  EVP_CIPHER_free(crypt_aes_256_cfb1);
  crypt_aes_256_cfb1 = nullptr;
  EVP_CIPHER_free(crypt_aes_256_cfb8);
  crypt_aes_256_cfb8 = nullptr;
  EVP_CIPHER_free(crypt_aes_256_cfb128);
  crypt_aes_256_cfb128 = nullptr;
  EVP_CIPHER_free(crypt_aes_256_ofb);
  crypt_aes_256_ofb = nullptr;
  EVP_CIPHER_free(crypt_aes_256_xts);
  crypt_aes_256_xts = nullptr;
  EVP_CIPHER_free(crypt_aes_256_wrap);
  crypt_aes_256_wrap = nullptr;
  EVP_CIPHER_free(crypt_aes_256_ctr);
  crypt_aes_256_ctr = nullptr;

  // do not propagate possible errors
  ERR_clear_error();
#endif
}

int my_get_fips_mode() { return fips_mode; }
