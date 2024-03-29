/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_aes_openssl.cc
*/

#include <assert.h>
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "m_string.h"
#include "my_aes.h"
#include "my_aes_impl.h"
#include "mysys/my_kdf.h"

/*
  xplugin needs BIO_new_bio_pair, but the server does not.
  Add an explicit dependency here, so that it is available when loading
  the plugin.
 */
int dummy_function_needed_by_xplugin() {
  BIO *bio1;
  BIO *bio2;
  return BIO_new_bio_pair(&bio1, 42U, &bio2, 42U);
}

/* keep in sync with enum my_aes_opmode in my_aes.h */
const char *my_aes_opmode_names[] = {
    "aes-128-ecb",    "aes-192-ecb",    "aes-256-ecb",    "aes-128-cbc",
    "aes-192-cbc",    "aes-256-cbc",    "aes-128-cfb1",   "aes-192-cfb1",
    "aes-256-cfb1",   "aes-128-cfb8",   "aes-192-cfb8",   "aes-256-cfb8",
    "aes-128-cfb128", "aes-192-cfb128", "aes-256-cfb128", "aes-128-ofb",
    "aes-192-ofb",    "aes-256-ofb",    nullptr /* needed for the type
                                                   enumeration */
};

/* keep in sync with enum my_aes_opmode in my_aes.h */
static uint my_aes_opmode_key_sizes_impl[] = {
    128 /* aes-128-ecb */,    192 /* aes-192-ecb */,
    256 /* aes-256-ecb */,    128 /* aes-128-cbc */,
    192 /* aes-192-cbc */,    256 /* aes-256-cbc */,
    128 /* aes-128-cfb1 */,   192 /* aes-192-cfb1 */,
    256 /* aes-256-cfb1 */,   128 /* aes-128-cfb8 */,
    192 /* aes-192-cfb8 */,   256 /* aes-256-cfb8 */,
    128 /* aes-128-cfb128 */, 192 /* aes-192-cfb128 */,
    256 /* aes-256-cfb128 */, 128 /* aes-128-ofb */,
    192 /* aes-192-ofb */,    256 /* aes-256-ofb */
};

uint *my_aes_opmode_key_sizes = my_aes_opmode_key_sizes_impl;

static const EVP_CIPHER *aes_evp_type(const my_aes_opmode mode) {
  switch (mode) {
    case my_aes_128_ecb:
      return EVP_aes_128_ecb();
    case my_aes_128_cbc:
      return EVP_aes_128_cbc();
    case my_aes_128_cfb1:
      return EVP_aes_128_cfb1();
    case my_aes_128_cfb8:
      return EVP_aes_128_cfb8();
    case my_aes_128_cfb128:
      return EVP_aes_128_cfb128();
    case my_aes_128_ofb:
      return EVP_aes_128_ofb();
    case my_aes_192_ecb:
      return EVP_aes_192_ecb();
    case my_aes_192_cbc:
      return EVP_aes_192_cbc();
    case my_aes_192_cfb1:
      return EVP_aes_192_cfb1();
    case my_aes_192_cfb8:
      return EVP_aes_192_cfb8();
    case my_aes_192_cfb128:
      return EVP_aes_192_cfb128();
    case my_aes_192_ofb:
      return EVP_aes_192_ofb();
    case my_aes_256_ecb:
      return EVP_aes_256_ecb();
    case my_aes_256_cbc:
      return EVP_aes_256_cbc();
    case my_aes_256_cfb1:
      return EVP_aes_256_cfb1();
    case my_aes_256_cfb8:
      return EVP_aes_256_cfb8();
    case my_aes_256_cfb128:
      return EVP_aes_256_cfb128();
    case my_aes_256_ofb:
      return EVP_aes_256_ofb();
    default:
      return nullptr;
  }
}

/**
  Creates required length of AES key,
  Input key size can be smaller or bigger in length, we need exact AES key
  size. If KDF options are valid and given, use KDF functionality. otherwise
  use previously used method.

  @param [out] rkey Output key
  @param key Input key
  @param key_length input key length
  @param mode AES mode
  @param kdf_options  KDF function options

  @return 0 on success and 1 on failure
*/
int my_create_key(unsigned char *rkey, const unsigned char *key,
                  uint32 key_length, enum my_aes_opmode mode,
                  vector<string> *kdf_options) {
  if (kdf_options) {
    if (kdf_options->size() < 1) {
      return 1;
    }
    const uint key_size = my_aes_opmode_key_sizes[mode] / 8;
    return create_kdf_key(key, key_length, rkey, key_size, kdf_options);
  }

  my_aes_create_key(key, key_length, rkey, mode);
  return 0;
}

int my_aes_encrypt(const unsigned char *source, uint32 source_length,
                   unsigned char *dest, const unsigned char *key,
                   uint32 key_length, enum my_aes_opmode mode,
                   const unsigned char *iv, bool padding,
                   vector<string> *kdf_options) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX stack_ctx;
  EVP_CIPHER_CTX *ctx = &stack_ctx;
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  const EVP_CIPHER *cipher = aes_evp_type(mode);
  int u_len, f_len;
  /* The real key to be used for encryption */
  unsigned char rkey[MAX_AES_KEY_LENGTH / 8];

  if (my_create_key(rkey, key, key_length, mode, kdf_options)) {
    return MY_AES_BAD_DATA;
  }
  if (!ctx || !cipher || (EVP_CIPHER_iv_length(cipher) > 0 && !iv))
    return MY_AES_BAD_DATA;

  if (!EVP_EncryptInit(ctx, cipher, rkey, iv)) goto aes_error;   /* Error */
  if (!EVP_CIPHER_CTX_set_padding(ctx, padding)) goto aes_error; /* Error */
  if (!EVP_EncryptUpdate(ctx, dest, &u_len, source, source_length))
    goto aes_error; /* Error */

  if (!EVP_EncryptFinal(ctx, dest + u_len, &f_len)) goto aes_error; /* Error */

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX_cleanup(ctx);
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX_free(ctx);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  return u_len + f_len;

aes_error:
  /* need to explicitly clean up the error if we want to ignore it */
  ERR_clear_error();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX_cleanup(ctx);
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX_free(ctx);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  return MY_AES_BAD_DATA;
}

int my_aes_decrypt(const unsigned char *source, uint32 source_length,
                   unsigned char *dest, const unsigned char *key,
                   uint32 key_length, enum my_aes_opmode mode,
                   const unsigned char *iv, bool padding,
                   vector<string> *kdf_options) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX stack_ctx;
  EVP_CIPHER_CTX *ctx = &stack_ctx;
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  const EVP_CIPHER *cipher = aes_evp_type(mode);
  int u_len, f_len;

  /* The real key to be used for decryption */
  unsigned char rkey[MAX_AES_KEY_LENGTH / 8];

  if (my_create_key(rkey, key, key_length, mode, kdf_options)) {
    return MY_AES_BAD_DATA;
  }

  if (!ctx || !cipher || (EVP_CIPHER_iv_length(cipher) > 0 && !iv))
    return MY_AES_BAD_DATA;

  if (!EVP_DecryptInit(ctx, aes_evp_type(mode), rkey, iv))
    goto aes_error;                                              /* Error */
  if (!EVP_CIPHER_CTX_set_padding(ctx, padding)) goto aes_error; /* Error */
  if (!EVP_DecryptUpdate(ctx, dest, &u_len, source, source_length))
    goto aes_error; /* Error */
  if (!EVP_DecryptFinal_ex(ctx, dest + u_len, &f_len))
    goto aes_error; /* Error */

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX_cleanup(ctx);
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX_free(ctx);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

  return u_len + f_len;

aes_error:
  /* need to explicitly clean up the error if we want to ignore it */
  ERR_clear_error();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX_cleanup(ctx);
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX_free(ctx);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  return MY_AES_BAD_DATA;
}

longlong my_aes_get_size(uint32 source_length, my_aes_opmode opmode) {
  const EVP_CIPHER *cipher = aes_evp_type(opmode);
  size_t block_size;

  block_size = EVP_CIPHER_block_size(cipher);

  if (block_size <= 1) return source_length;
  return block_size * (static_cast<ulonglong>(source_length) / block_size) +
         block_size;
}

bool my_aes_needs_iv(my_aes_opmode opmode) {
  const EVP_CIPHER *cipher = aes_evp_type(opmode);
  int iv_length;
  iv_length = EVP_CIPHER_iv_length(cipher);
  assert(iv_length == 0 || iv_length == MY_AES_IV_SIZE);
  return iv_length != 0 ? true : false;
}
