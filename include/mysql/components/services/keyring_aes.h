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

#ifndef KEYRING_AES_INCLUDED
#define KEYRING_AES_INCLUDED

#include <mysql/components/service.h>

/* clang-format off */
/**
  @defgroup group_keyring_component_services_inventory Keyring component services
  @ingroup group_components_services_inventory
*/

/* clang-format on */

/**
  @ingroup group_keyring_component_services_inventory

  Keyring aes encryption service provides APIs to perform AES
  encryption/decryption operation on given data. These methods
  make sure that key never leaves keyring component.

  @code
  my_service<SERVICE_TYPE(keyring_aes)> aes_encryption(
      "keyring_aes", m_reg_srv);
  if (!aes_encryption.is_valid()) {
    return true;
  }

  std::string mode("cbc");
  size_t block_size = 256;
  const unsigned char plaintext[] = "Quick brown fox jumped over the lazy dog.";
  size_t plaintext_length = strlen(static_cast<const char *>(plaintext));
  size_t ciphertext_length = 0;
  if (aes_encryption->get_size(plaintext_length, block_size, mode.c_str,
                               &ciphertext_length) == true) {
    return true;
  }
  std::unique_ptr<unsigned char[]> ciphertext(
      new unsigned char[ciphertext_length]);
  if (ciphertext.get() == nullptr) {
    return true;
  }
  const unsigned char iv[] = "abcefgh12345678";
  size_t out_length = 0;
  if (aes_encryption->encrypt(
          "my_aes_key_1", "testuser@localhost", mode.c_str(), block_size,
          iv, true, plaintext, plaintext_length, ciphertext.get(),
          ciphertext_length, &out_length) == true) {
    return true;
  }

  std::unique_ptr<unsigned char[]> retrieved_plaintext(
      new unsigned char[plaintext_length]);
  if (retrieved_plaintext.get() == nullptr) {
    return true;
  }

  if (aes_encryption->decrypt(
          "my_aes_key_1", "testuser@localhost", mode.c_str(), block_size,
          iv, true, ciphertext.get(), out_length, retrieved_plaintext.get(),
          plaintext_length, &out_length) == true) {
    return true;
  }

  if (plaintext_length != out_length ||
      memcmp(plaintext, retrieved_plaintext.get(), plaintext_length) != 0) {
    return true;
  }

  return false;
  @endcode
*/

BEGIN_SERVICE_DEFINITION(keyring_aes)

/**
  Retrieve required out buffer length information

  Assumption: mode string is in lower case.

  @param [in]  input_length Length of input text
  @param [in]  mode         AES mode. ASCII string.
  @param [in]  block_size   AES block size information
  @param [out] out_size     Size of out buffer

  @returns Output buffer length or error
    @retval false Success
    @retval true  Error processing given mode and/or block size
*/
DECLARE_BOOL_METHOD(get_size, (size_t input_length, const char *mode,
                               size_t block_size, size_t *out_size));

/**
  Encrypt given piece of plaintext

  Block mode for operation (e.g. "ecb", "cbc", cfb1",...)
  Block size (e.g. 256)

  Length of out buffer should be sufficient to hold ciphertext
  data. See get_size() API.

  Encrypted data should be stored in out_buffer with out_length
  set to actual length of data.

  IV must be provided if block mode of operation requires it.

  It is caller's responsibility to supply same IV for encryption/decryption.

  @param [in]  data_id            Name of the key. Byte string.
  @param [in]  auth_id            Owner of the key. Byte string.
  @param [in]  mode               AES mode. ASCII string.
  @param [in]  block_size         AES block size information
  @param [in]  iv                 Initialization vector
  @param [in]  padding            padding preference (0 implies no padding)
  @param [in]  data_buffer        Input buffer. Byte string.
  @param [in]  data_buffer_length Input buffer length
  @param [out] out_buffer         Output buffer. Byte string.
  @param [in]  out_buffer_length  Output buffer length
  @param [out] out_length         Length of encrypted data

  @returns status of the operation
    @retval false Success
    @retval true  Failure

*/
DECLARE_BOOL_METHOD(encrypt,
                    (const char *data_id, const char *auth_id, const char *mode,
                     size_t block_size, const unsigned char *iv, int padding,
                     const unsigned char *data_buffer,
                     size_t data_buffer_length, unsigned char *out_buffer,
                     size_t out_buffer_length, size_t *out_length));

/**
  Decrypt given piece ciphertext

  Block mode for operation (e.g. "ecb", "cbc", cfb1",...)
  Block size (e.g. 256)

  Length of out buffer should be sufficient to hold ciphertext
  data. See get_size() API.


  If block mode requires IV, same should be provided by caller.
  This should same IV that was used for encryption operation.

  @param [in]  data_id            Name of the key. Byte string.
  @param [in]  auth_id            Owner of the key. Byte string.
  @param [in]  mode               AES mode. ASCII string.
  @param [in]  block_size         AES block size information
  @param [in]  iv                 Initialization vector
  @param [in]  padding            padding preference (0 implies no padding)
  @param [in]  data_buffer        Input buffer. Byte string.
  @param [in]  data_buffer_length Input buffer length
  @param [out] out_buffer         Output buffer. Byte string.
  @param [in]  out_buffer_length  Output buffer length
  @param [out] out_length         Length of decrypted data

  @returns status of the operation
    @retval false Success
    @retval true  Failure

*/
DECLARE_BOOL_METHOD(decrypt,
                    (const char *data_id, const char *auth_id, const char *mode,
                     size_t block_size, const unsigned char *iv, int padding,
                     const unsigned char *data_buffer,
                     size_t data_buffer_length, unsigned char *out_buffer,
                     size_t out_buffer_length, size_t *out_length));

END_SERVICE_DEFINITION(keyring_aes)

#endif  // KEYRING_AES_INCLUDED
