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

#ifndef KEYRING_ENCRYPTION_SERVICE_IMPL_INCLUDED
#define KEYRING_ENCRYPTION_SERVICE_IMPL_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/keyring_aes.h>

namespace keyring_common {
namespace service_definition {

class Keyring_aes_service_impl {
 public:
  /**
    Retrieve required out buffer length information

    @param [in]  input_length Length of input text
    @param [in]  mode         AES mode
    @param [in]  block_size   AES block size information
    @param [out] out_size     Size of output buffer

  @returns Output buffer length or error
    @retval false Success
    @retval true   Error processing given mode and/or block size
  */
  static DEFINE_BOOL_METHOD(get_size, (size_t input_length, const char *mode,
                                       size_t block_size, size_t *out_size));

  /**
    Encrypt given piece of plaintext

    @param [in]  data_id            Name of the key
    @param [in]  auth_id            Owner of the key
    @param [in]  mode               AES mode
    @param [in]  block_size         AES block size information
    @param [in]  iv                 Initialization vector
    @param [in]  padding            padding preference (0 implies no padding)
    @param [in]  data_buffer        Input buffer
    @param [in]  data_buffer_length Input buffer length
    @param [out] out_buffer         Output buffer
    @param [in]  out_buffer_length  Output buffer length
    @param [out] out_length         Length of encrypted data

    @returns status of the operation
      @retval false Success
      @retval true  Failure

  */
  static DEFINE_BOOL_METHOD(
      encrypt, (const char *data_id, const char *auth_id, const char *mode,
                size_t block_size, const unsigned char *iv, int padding,
                const unsigned char *data_buffer, size_t data_buffer_length,
                unsigned char *out_buffer, size_t out_buffer_length,
                size_t *out_length));

  /**
    Decrypt given piece ciphertext

    @param [in]  data_id            Name of the key
    @param [in]  auth_id            Owner of the key
    @param [in]  mode               AES mode
    @param [in]  block_size         AES block size information
    @param [in]  iv                 Initialization vector
    @param [in]  padding            padding preference (0 implies no padding)
    @param [in]  data_buffer        Input buffer
    @param [in]  data_buffer_length Input buffer length
    @param [out] out_buffer         Output buffer
    @param [in]  out_buffer_length  Output buffer length
    @param [out] out_length         Length of decrypted data

    @returns status of the operation
      @retval false Success
      @retval true  Failure

  */
  static DEFINE_BOOL_METHOD(
      decrypt, (const char *data_id, const char *auth_id, const char *mode,
                size_t block_size, const unsigned char *iv, int padding,
                const unsigned char *data_buffer, size_t data_buffer_length,
                unsigned char *out_buffer, size_t out_buffer_length,
                size_t *out_length));
};

}  // namespace service_definition
}  // namespace keyring_common

#define KEYRING_AES_IMPLEMENTOR(component_name)                              \
  BEGIN_SERVICE_IMPLEMENTATION(component_name, keyring_aes)                  \
  keyring_common::service_definition::Keyring_aes_service_impl::get_size,    \
      keyring_common::service_definition::Keyring_aes_service_impl::encrypt, \
      keyring_common::service_definition::Keyring_aes_service_impl::decrypt  \
      END_SERVICE_IMPLEMENTATION()

#endif  // KEYRING_ENCRYPTION_SERVICE_IMPL_INCLUDED
