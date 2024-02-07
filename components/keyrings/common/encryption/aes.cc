/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <algorithm>
#include <locale>
#include <memory>

#include <include/scope_guard.h>

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <openssl/sha.h>

#include "aes.h"

namespace keyring_common {
namespace aes_encryption {

const Known_block_mode_map Aes_operation_context::s_blockmodes = {
    {std::make_pair("ecb", 256), Keyring_aes_opmode::keyring_aes_256_ecb},
    {std::make_pair("cbc", 256), Keyring_aes_opmode::keyring_aes_256_cbc},
    {std::make_pair("cfb1", 256), Keyring_aes_opmode::keyring_aes_256_cfb1},
    {std::make_pair("cfb8", 256), Keyring_aes_opmode::keyring_aes_256_cfb8},
    {std::make_pair("cfb128", 256), Keyring_aes_opmode::keyring_aes_256_cfb128},
    {std::make_pair("ofb", 256), Keyring_aes_opmode::keyring_aes_256_ofb}};

Aes_operation_context::Aes_operation_context(const std::string data_id,
                                             const std::string auth_id,
                                             const std::string mode,
                                             size_t block_size)
    : data_id_(data_id),
      auth_id_(auth_id),
      opmode_(Keyring_aes_opmode::keyring_aes_opmode_invalid),
      valid_(false) {
  block_mode_key key(mode, block_size);
  auto it = Aes_operation_context::s_blockmodes.find(key);
  if (it != Aes_operation_context::s_blockmodes.end()) {
    opmode_ = it->second;
  }

  valid_ = (opmode_ != Keyring_aes_opmode::keyring_aes_opmode_invalid);
}

/* keep in sync with enum Keyring_aes_opmode in keyring_encryption_service.h */
size_t aes_opmode_key_sizes[] = {
    256 /* aes-256-ecb */,  256 /* aes-256-cbc */,    256 /* aes-256-cfb1 */,
    256 /* aes-256-cfb8 */, 256 /* aes-256-cfb128 */, 256 /* aes-256-ofb */
};

const EVP_CIPHER *aes_evp_type(const Keyring_aes_opmode mode) {
  switch (mode) {
    case Keyring_aes_opmode::keyring_aes_256_ecb:
      return EVP_aes_256_ecb();
    case Keyring_aes_opmode::keyring_aes_256_cbc:
      return EVP_aes_256_cbc();
    case Keyring_aes_opmode::keyring_aes_256_cfb1:
      return EVP_aes_256_cfb1();
    case Keyring_aes_opmode::keyring_aes_256_cfb8:
      return EVP_aes_256_cfb8();
    case Keyring_aes_opmode::keyring_aes_256_cfb128:
      return EVP_aes_256_cfb128();
    case Keyring_aes_opmode::keyring_aes_256_ofb:
      return EVP_aes_256_ofb();
    case Keyring_aes_opmode::keyring_aes_opmode_invalid:
      [[fallthrough]];
    default:
      return nullptr;
  }
}

/**
  Transforms an arbitrary long key into a fixed length AES key

  @param [in]  key               Key to use for real key creation
  @param [in]  key_length        Length of the key
  @param [out] rkey              Generated key
  @param [out] rkey_size         Size of generated key
  @param [in]  opmode            encryption mode

  @returns Key generation status
    @retval true  Success
    @retval false Error
*/

bool aes_create_key(const unsigned char *key, unsigned int key_length,
                    std::unique_ptr<unsigned char[]> &rkey, size_t *rkey_size,
                    Keyring_aes_opmode opmode) {
  if (rkey_size == nullptr) return false;
  *rkey_size = aes_opmode_key_sizes[static_cast<unsigned int>(opmode)] / 8;
  rkey = std::make_unique<unsigned char[]>(*rkey_size);
  if (rkey.get() == nullptr) return false;
  switch (*rkey_size) {
    case 32: /* 256 bit key */ {
      EVP_MD_CTX *md_ctx = EVP_MD_CTX_create();
      EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr);
      EVP_DigestUpdate(
          md_ctx, reinterpret_cast<void *>(const_cast<unsigned char *>(key)),
          (size_t)key_length);
      EVP_DigestFinal_ex(md_ctx, rkey.get(), nullptr);
      EVP_MD_CTX_destroy(md_ctx);
      break;
    }
    default:
      return false;
  }
  return true;
}

size_t get_ciphertext_size(size_t input_size, const Keyring_aes_opmode mode) {
  const EVP_CIPHER *cipher = aes_evp_type(mode);
  size_t block_size;

  block_size = EVP_CIPHER_block_size(cipher);

  return block_size > 1 ? block_size * (input_size / block_size) + block_size
                        : input_size;
}

aes_return_status aes_encrypt(const unsigned char *source,
                              unsigned int source_length, unsigned char *dest,
                              const unsigned char *key, unsigned int key_length,
                              Keyring_aes_opmode mode, const unsigned char *iv,
                              bool padding, size_t *encrypted_length) {
  if (encrypted_length == nullptr) return AES_OUTPUT_SIZE_NULL;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX stack_ctx;
  EVP_CIPHER_CTX *ctx = &stack_ctx;
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) return AES_CTX_ALLOCATION_ERROR;
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

  auto cleanup_guard = create_scope_guard([&] {
    /* need to explicitly clean up the error if we want to ignore it */
    ERR_clear_error();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX_cleanup(ctx);
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    EVP_CIPHER_CTX_free(ctx);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  });

  const EVP_CIPHER *cipher = aes_evp_type(mode);
  if (cipher == nullptr) return AES_INVALID_BLOCK_MODE;

  /* The real key to be used for encryption */
  std::unique_ptr<unsigned char[]> rkey;
  size_t rkey_size;
  if (aes_create_key(key, key_length, rkey, &rkey_size, mode) == false)
    return AES_KEY_TRANSFORMATION_ERROR;

  if (EVP_CIPHER_iv_length(cipher) > 0 && !iv) return AES_IV_EMPTY;

  int u_len, f_len;

  if (!EVP_EncryptInit(ctx, cipher, rkey.get(), iv))
    return AES_ENCRYPTION_ERROR;
  if (!EVP_CIPHER_CTX_set_padding(ctx, padding)) return AES_ENCRYPTION_ERROR;
  if (!EVP_EncryptUpdate(ctx, dest, &u_len, source, source_length))
    return AES_ENCRYPTION_ERROR;
  if (!EVP_EncryptFinal(ctx, dest + u_len, &f_len)) return AES_ENCRYPTION_ERROR;

  /* All is well */
  *encrypted_length = static_cast<size_t>(u_len + f_len);
  return AES_OP_OK;
}

aes_return_status aes_decrypt(const unsigned char *source,
                              unsigned int source_length, unsigned char *dest,
                              const unsigned char *key, unsigned int key_length,
                              enum Keyring_aes_opmode mode,
                              const unsigned char *iv, bool padding,
                              size_t *decrypted_length) {
  if (decrypted_length == nullptr) return AES_OUTPUT_SIZE_NULL;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX stack_ctx;
  EVP_CIPHER_CTX *ctx = &stack_ctx;
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) return AES_CTX_ALLOCATION_ERROR;
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

  auto cleanup_guard = create_scope_guard([&] {
    /* need to explicitly clean up the error if we want to ignore it */
    ERR_clear_error();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX_cleanup(ctx);
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    EVP_CIPHER_CTX_free(ctx);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  });

  const EVP_CIPHER *cipher = aes_evp_type(mode);
  if (cipher == nullptr) return AES_INVALID_BLOCK_MODE;

  /* The real key to be used for encryption */
  std::unique_ptr<unsigned char[]> rkey;
  size_t rkey_size;
  if (aes_create_key(key, key_length, rkey, &rkey_size, mode) == false)
    return AES_KEY_TRANSFORMATION_ERROR;

  if (EVP_CIPHER_iv_length(cipher) > 0 && !iv) return AES_IV_EMPTY;

  int u_len, f_len;

  if (!EVP_DecryptInit(ctx, aes_evp_type(mode), rkey.get(), iv))
    return AES_DECRYPTION_ERROR;
  if (!EVP_CIPHER_CTX_set_padding(ctx, padding)) return AES_DECRYPTION_ERROR;
  if (!EVP_DecryptUpdate(ctx, dest, &u_len, source, source_length))
    return AES_DECRYPTION_ERROR;
  if (!EVP_DecryptFinal_ex(ctx, dest + u_len, &f_len))
    return AES_DECRYPTION_ERROR;

  /* All is well */
  *decrypted_length = static_cast<size_t>(u_len + f_len);
  return AES_OP_OK;
}

}  // namespace aes_encryption
}  // namespace keyring_common
