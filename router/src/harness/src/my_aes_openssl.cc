/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
  This is copy of mysys_ssl/my_aes_openssl.cc with some parts that
  we do not need removed.
  It's copied because the original file includes global my_aes_opmode_names
  which symbol is currently exposed from libmysqlclient. That is causing ODR
  violations.
  On the other hand we do not want to depend on my_aes_* functions
  being accessible from libmysqlclient, as this can change in the future.
*/

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <cstring>  // for memset

#include "my_aes.h"
#include "my_aes_impl.h"

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

/**
  Transforms an arbitrary long key into a fixed length AES key

  AES keys are of fixed length. This routine takes an arbitrary long key
  iterates over it in AES key length increment and XORs the bytes with the
  AES key buffer being prepared.
  The bytes from the last incomplete iteration are XORed to the start
  of the key until their depletion.
  Needed since crypto function routines expect a fixed length key.

  @param [in] key               Key to use for real key creation
  @param [in] key_length        Length of the key
  @param [out] rkey             Real key (used by OpenSSL/WolfSSL)
  @param [out] opmode           encryption mode
*/

void my_aes_create_key(const unsigned char *key, uint key_length, uint8 *rkey,
                       enum my_aes_opmode opmode) {
  const uint key_size = my_aes_opmode_key_sizes_impl[opmode] / 8;
  uint8 *rkey_end;                              /* Real key boundary */
  uint8 *ptr;                                   /* Start of the real key*/
  uint8 *sptr;                                  /* Start of the working key */
  uint8 *key_end = ((uint8 *)key) + key_length; /* Working key boundary*/

  rkey_end = rkey + key_size;

  memset(rkey, 0, key_size); /* Set initial key  */

  for (ptr = rkey, sptr = (uint8 *)key; sptr < key_end; ptr++, sptr++) {
    if (ptr == rkey_end) /*  Just loop over tmp_key until we used all key */
      ptr = rkey;
    *ptr ^= *sptr;
  }
}

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
      return NULL;
  }
}

int my_aes_encrypt(const unsigned char *source, uint32 source_length,
                   unsigned char *dest, const unsigned char *key,
                   uint32 key_length, enum my_aes_opmode mode,
                   const unsigned char *iv, bool padding) {
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
  my_aes_create_key(key, key_length, rkey, mode);

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
                   const unsigned char *iv, bool padding) {
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

  my_aes_create_key(key, key_length, rkey, mode);
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

int my_aes_get_size(uint32 source_length, my_aes_opmode opmode) {
  const EVP_CIPHER *cipher = aes_evp_type(opmode);
  size_t block_size;

  block_size = EVP_CIPHER_block_size(cipher);

  return block_size > 1 ? block_size * (source_length / block_size) + block_size
                        : source_length;
}
