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

#ifndef KEYRING_ENCRYPTION_SERVICE_IMPL_TEMPLATE_INCLUDED
#define KEYRING_ENCRYPTION_SERVICE_IMPL_TEMPLATE_INCLUDED

#include <algorithm> /* std::transform */
#include <cctype>
#include <cstring>
#include <functional>
#include <memory>  /* std::unique_ptr */
#include <sstream> /* std::stringstream */

#include <my_dbug.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include <mysqld_error.h>
#include <scope_guard.h>

#include <components/keyrings/common/component_helpers/include/keyring_reader_service_impl_template.h>
#include <components/keyrings/common/component_helpers/include/service_requirements.h>
#include <components/keyrings/common/data/data.h>
#include <components/keyrings/common/encryption/aes.h> /* AES encryption */
#include <components/keyrings/common/operations/operations.h>

namespace keyring_common {

using aes_encryption::aes_decrypt;
using aes_encryption::aes_encrypt;
using aes_encryption::Aes_operation_context;
using aes_encryption::aes_return_status;
using aes_encryption::get_ciphertext_size;
using aes_encryption::Keyring_aes_opmode;
using data::Data;
using operations::Keyring_operations;

namespace service_implementation {
/**
  Retrieve required out buffer length information

  @param [in]  input_length       Length of input text
  @param [in]  mode               Block encryption mode
  @param [in]  block_size         AES block size
  @param [out] out_size           Out buffer length

  @returns Output buffer length or error
    @retval false Success
    @retval true  Error processing given mode and/or block size
*/
bool aes_get_encrypted_size_template(size_t input_length, const char *mode,
                                     size_t block_size, size_t *out_size) {
  try {
    if (mode == nullptr || block_size == 0) {
      LogComponentErr(ERROR_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_INVALID_MODE_BLOCK_SIZE);
      return true;
    }

    const Aes_operation_context context(std::string{}, std::string{}, mode,
                                        block_size);
    if (context.valid() == false) return true;
    *out_size = get_ciphertext_size(input_length, context.opmode());
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "get_size",
                    "keyring_aes");
    return true;
  }
}

/**
  Encrypt given piece of plaintext

  @param [in]  data_id            Name of the key
  @param [in]  auth_id            Owner of the key
  @param [in]  mode               AES mode
  @param [in]  block_size         AES block size information
  @param [in]  iv                 Initialization vector
  @param [in]  padding            padding preference
  @param [in]  data_buffer        Input buffer
  @param [in]  data_buffer_length Input buffer length
  @param [out] out_buffer         Output buffer
  @param [in]  out_buffer_length  Output buffer length
  @param [out] out_length         Length of encrypted data
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns status of the operation
    @retval false Success
    @retval true  Failure

*/
template <typename Backend, typename Data_extension = Data>
bool aes_encrypt_template(
    const char *data_id, const char *auth_id, const char *mode,
    size_t block_size, const unsigned char *iv, bool padding,
    const unsigned char *data_buffer, size_t data_buffer_length,
    unsigned char *out_buffer, size_t out_buffer_length, size_t *out_length,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      return true;
    }

    if (mode == nullptr || block_size == 0) {
      LogComponentErr(ERROR_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_INVALID_MODE_BLOCK_SIZE);
      return true;
    }

    if (data_id == nullptr) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_DATA_IDENTIFIER_EMPTY);
      return true;
    }

    Aes_operation_context context(data_id, auth_id, mode, block_size);

    Keyring_aes_opmode opmode = context.opmode();
    size_t required_out_buffer_size =
        get_ciphertext_size(data_buffer_length, opmode);
    if (out_buffer == nullptr || required_out_buffer_size > out_buffer_length) {
      assert(false);
      return true;
    }

    size_t key_length = 0;
    size_t key_type_length = 0;
    std::unique_ptr<Iterator<Data_extension>> it;
    int retval = init_reader_template<Backend, Data_extension>(
        data_id, auth_id, it, keyring_operations, callbacks);
    auto cleanup_guard = create_scope_guard([&] {
      (void)deinit_reader_template<Backend, Data_extension>(
          it, keyring_operations, callbacks);
    });
    if (retval < 1) {
      // Error would have been raised
      return true;
    }
    if (fetch_length_template<Backend, Data_extension>(
            it, &key_length, &key_type_length, keyring_operations, callbacks) ==
        true) {
      // Error would have been raised
      return true;
    }

    std::unique_ptr<unsigned char[]> key_buffer =
        std::make_unique<unsigned char[]>(key_length);
    if (key_buffer.get() == nullptr) {
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_MEMORY_ALLOCATION_ERROR,
                      "key buffer", "encrypt", "keyring_aes");
    }

    char key_type_buffer[32] = {0};
    size_t dummy_key_buffer_size, dummy_key_type_buffer_size;

    if (fetch_template<Backend, Data_extension>(
            it, key_buffer.get(), key_length, &dummy_key_buffer_size,
            key_type_buffer, 32, &dummy_key_type_buffer_size,
            keyring_operations, callbacks) == true) {
      // Error would have been raised
      return true;
    }

    std::string key_type(key_type_buffer);
    std::transform(key_type.begin(), key_type.end(), key_type.begin(),
                   ::tolower);
    if (key_type != "aes") {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_INVALID_KEY, data_id,
                      (auth_id == nullptr || !*auth_id) ? "NULL" : auth_id);
      return true;
    }

    aes_return_status ret =
        aes_encrypt(data_buffer, (unsigned int)data_buffer_length, out_buffer,
                    key_buffer.get(), (unsigned int)key_length, opmode, iv,
                    padding, out_length);

    if (ret != keyring_common::aes_encryption::AES_OP_OK) {
      std::stringstream ss;
      switch (ret) {
        case keyring_common::aes_encryption::AES_OUTPUT_SIZE_NULL:
          ss << "'Output size buffer is NULL'";
          break;
        case keyring_common::aes_encryption::AES_KEY_TRANSFORMATION_ERROR:
          ss << "'Key transformation error'";
          break;
        case keyring_common::aes_encryption::AES_CTX_ALLOCATION_ERROR:
          ss << "'Failed to allocate memory for encryption context'";
          break;
        case keyring_common::aes_encryption::AES_INVALID_BLOCK_MODE:
          ss << "'Invalid block mode'";
          break;
        case keyring_common::aes_encryption::AES_IV_EMPTY:
          ss << "'IV is empty'";
          break;
        case keyring_common::aes_encryption::AES_ENCRYPTION_ERROR:
          ss << "'Could not complete operation'";
          break;
        default:
          ss << "'Unknown error number: '" << ret;
          break;
      }
      std::string ss_str = ss.str();
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_OPERATION_ERROR,
                      ss_str.c_str(), "encrypt", data_id,
                      (auth_id == nullptr || *auth_id) ? "NULL" : auth_id);
      return true;
    }

    /* All is well */
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "encrypt",
                    "keyring_aes");
    return true;
  }
}

/**
  Decrypt given piece ciphertext

  @param [in]  data_id            Name of the key
  @param [in]  auth_id            Owner of the key
  @param [in]  mode               AES mode
  @param [in]  block_size         AES block size information
  @param [in]  iv                 Initialization vector
  @param [in]  padding            padding preference
  @param [in]  data_buffer        Input buffer
  @param [in]  data_buffer_length Input buffer length
  @param [out] out_buffer         Output buffer
  @param [in]  out_buffer_length  Output buffer length
  @param [out] out_length         Length of decrypted data
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns status of the operation
    @retval false Success
    @retval true  Failure

*/
template <typename Backend, typename Data_extension = Data>
bool aes_decrypt_template(
    const char *data_id, const char *auth_id, const char *mode,
    size_t block_size, const unsigned char *iv, bool padding,
    const unsigned char *data_buffer, size_t data_buffer_length,
    unsigned char *out_buffer, size_t out_buffer_length, size_t *out_length,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      return true;
    }

    if (mode == nullptr || block_size == 0) {
      LogComponentErr(ERROR_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_INVALID_MODE_BLOCK_SIZE);
      return true;
    }

    if (data_id == nullptr) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_DATA_IDENTIFIER_EMPTY);
      return true;
    }

    Aes_operation_context context(data_id, auth_id, mode, block_size);

    Keyring_aes_opmode opmode = context.opmode();

    if (out_buffer == nullptr || data_buffer_length > out_buffer_length) {
      assert(false);
      return true;
    }

    size_t key_length = 0;
    size_t key_type_length = 0;
    std::unique_ptr<Iterator<Data_extension>> it;
    int retval = init_reader_template<Backend, Data_extension>(
        data_id, auth_id, it, keyring_operations, callbacks);
    auto cleanup_guard = create_scope_guard([&] {
      (void)deinit_reader_template<Backend, Data_extension>(
          it, keyring_operations, callbacks);
    });
    if (retval < 1) {
      // Error would have been raised
      return true;
    }
    if (fetch_length_template<Backend, Data_extension>(
            it, &key_length, &key_type_length, keyring_operations, callbacks) ==
        true) {
      // Error would have been raised
      return true;
    }

    std::unique_ptr<unsigned char[]> key_buffer =
        std::make_unique<unsigned char[]>(key_length);
    if (key_buffer.get() == nullptr) {
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_MEMORY_ALLOCATION_ERROR,
                      "key buffer", "decrypt", "keyring_aes");
    }

    char key_type_buffer[32] = {0};
    size_t dummy_key_buffer_size, dummy_key_type_buffer_size;

    if (fetch_template<Backend, Data_extension>(
            it, key_buffer.get(), key_length, &dummy_key_buffer_size,
            key_type_buffer, 32, &dummy_key_type_buffer_size,
            keyring_operations, callbacks) == true) {
      // Error would have been raised
      return true;
    }

    std::string key_type(key_type_buffer);
    std::transform(key_type.begin(), key_type.end(), key_type.begin(),
                   ::tolower);
    if (key_type != "aes") {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_INVALID_KEY, data_id,
                      (auth_id == nullptr || !*auth_id) ? "NULL" : auth_id);
      return true;
    }

    aes_return_status ret =
        aes_decrypt(data_buffer, (unsigned int)data_buffer_length, out_buffer,
                    key_buffer.get(), (unsigned int)key_length, opmode, iv,
                    padding, out_length);

    if (ret != keyring_common::aes_encryption::AES_OP_OK) {
      std::stringstream ss;
      switch (ret) {
        case keyring_common::aes_encryption::AES_OUTPUT_SIZE_NULL:
          ss << "'Output size buffer is NULL'";
          break;
        case keyring_common::aes_encryption::AES_KEY_TRANSFORMATION_ERROR:
          ss << "'Key transformation error'";
          break;
        case keyring_common::aes_encryption::AES_CTX_ALLOCATION_ERROR:
          ss << "'Failed to allocate memory for encryption context'";
          break;
        case keyring_common::aes_encryption::AES_INVALID_BLOCK_MODE:
          ss << "'Invalid block mode'";
          break;
        case keyring_common::aes_encryption::AES_IV_EMPTY:
          ss << "'IV is empty'";
          break;
        case keyring_common::aes_encryption::AES_DECRYPTION_ERROR:
          ss << "'Could not complete operation'";
          break;
        default:
          ss << "'Unknown error number: '" << ret;
          break;
      }
      std::string ss_str = ss.str();
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_AES_OPERATION_ERROR,
                      ss_str.c_str(), "decrypt", data_id,
                      (auth_id == nullptr || *auth_id) ? "NULL" : auth_id);
      return true;
    }

    /* All is well */
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "decrypt",
                    "keyring_aes");
    return true;
  }
}

}  // namespace service_implementation
}  // namespace keyring_common

#endif  // !KEYRING_ENCRYPTION_SERVICE_IMPL_TEMPLATE_INCLUDED
