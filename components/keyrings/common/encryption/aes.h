/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef AES_INCLUDED
#define AES_INCLUDED

#include <functional>
#include <map>
#include <string>
#include <utility>

#include <openssl/evp.h>

namespace keyring_common {
namespace aes_encryption {

/** Supported AES cipher/block mode combos */
enum class Keyring_aes_opmode {
  keyring_aes_256_ecb = 0,
  keyring_aes_256_cbc,
  keyring_aes_256_cfb1,
  keyring_aes_256_cfb8,
  keyring_aes_256_cfb128,
  keyring_aes_256_ofb,
  /* Add new values above this */
  keyring_aes_opmode_invalid
};

enum aes_return_status {
  AES_OP_OK,
  AES_OUTPUT_SIZE_NULL,
  AES_KEY_TRANSFORMATION_ERROR,
  AES_CTX_ALLOCATION_ERROR,
  AES_INVALID_BLOCK_MODE,
  AES_IV_EMPTY,
  AES_ENCRYPTION_ERROR,
  AES_DECRYPTION_ERROR
};

using block_mode_key = std::pair<std::string, size_t>;
using Known_block_mode_map = std::map<block_mode_key, Keyring_aes_opmode>;

class Aes_operation_context final {
 public:
  Aes_operation_context(const std::string data_id, const std::string auth_id,
                        const std::string mode, size_t block_size);
  ~Aes_operation_context() = default;
  const std::string data_id() const { return data_id_; }
  const std::string auth_id() const { return auth_id_; }
  Keyring_aes_opmode opmode() const { return opmode_; }
  bool valid() const { return valid_; }
  static const Known_block_mode_map s_blockmodes;

 private:
  std::string data_id_;
  std::string auth_id_;
  Keyring_aes_opmode opmode_;
  bool valid_;
};

const EVP_CIPHER *aes_evp_type(const Keyring_aes_opmode mode);

size_t get_ciphertext_size(size_t input_size, const Keyring_aes_opmode mode);

aes_return_status aes_encrypt(const unsigned char *source,
                              unsigned int source_length, unsigned char *dest,
                              const unsigned char *key, unsigned int key_length,
                              Keyring_aes_opmode mode, const unsigned char *iv,
                              bool padding, size_t *encrypted_length);

aes_return_status aes_decrypt(const unsigned char *source,
                              unsigned int source_length, unsigned char *dest,
                              const unsigned char *key, unsigned int key_length,
                              Keyring_aes_opmode mode, const unsigned char *iv,
                              bool padding, size_t *decrypted_length);

}  // namespace aes_encryption
}  // namespace keyring_common

#endif  // !AES_INCLUDED
