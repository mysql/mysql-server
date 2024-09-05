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

#include <components/keyrings/keyring_file/keyring_file.h>
#include "option_usage.h"

#include <components/keyrings/common/component_helpers/include/keyring_encryption_service_definition.h>
#include <components/keyrings/common/component_helpers/include/keyring_encryption_service_impl_template.h>

using keyring_file::g_component_callbacks;
using keyring_file::g_keyring_operations;
using keyring_file::backend::Keyring_file_backend;
namespace keyring_common {

using service_implementation::aes_decrypt_template;
using service_implementation::aes_encrypt_template;
using service_implementation::aes_get_encrypted_size_template;

namespace service_definition {

DEFINE_BOOL_METHOD(Keyring_aes_service_impl::get_size,
                   (size_t input_length, const char *mode, size_t block_size,
                    size_t *out_size)) {
  return aes_get_encrypted_size_template(input_length, mode, block_size,
                                         out_size);
}

DEFINE_BOOL_METHOD(Keyring_aes_service_impl::encrypt,
                   (const char *data_id, const char *auth_id, const char *mode,
                    size_t block_size, const unsigned char *iv, int padding,
                    const unsigned char *data_buffer, size_t data_buffer_length,
                    unsigned char *out_buffer, size_t out_buffer_length,
                    size_t *out_length)) {
  keyring_file_component_option_usage_set();
  return aes_encrypt_template<Keyring_file_backend>(
      data_id, auth_id, mode, block_size, iv, padding, data_buffer,
      data_buffer_length, out_buffer, out_buffer_length, out_length,
      *g_keyring_operations, *g_component_callbacks);
}

DEFINE_BOOL_METHOD(Keyring_aes_service_impl::decrypt,
                   (const char *data_id, const char *auth_id, const char *mode,
                    size_t block_size, const unsigned char *iv, int padding,
                    const unsigned char *data_buffer, size_t data_buffer_length,
                    unsigned char *out_buffer, size_t out_buffer_length,
                    size_t *out_length)) {
  keyring_file_component_option_usage_set();
  return aes_decrypt_template<Keyring_file_backend>(
      data_id, auth_id, mode, block_size, iv, padding, data_buffer,
      data_buffer_length, out_buffer, out_buffer_length, out_length,
      *g_keyring_operations, *g_component_callbacks);
}

}  // namespace service_definition
}  // namespace keyring_common
