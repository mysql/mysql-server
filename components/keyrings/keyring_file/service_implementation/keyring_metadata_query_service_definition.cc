/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <components/keyrings/keyring_file/keyring_file.h>

#include <components/keyrings/common/component_helpers/include/keyring_metadata_query_service_definition.h>
#include <components/keyrings/common/component_helpers/include/keyring_metadata_query_service_impl_template.h>

using keyring_file::g_component_callbacks;
namespace keyring_common {

using service_implementation::config_vector;
using service_implementation::keyring_metadata_query_deinit_template;
using service_implementation::keyring_metadata_query_get_length_template;
using service_implementation::keyring_metadata_query_get_template;
using service_implementation::keyring_metadata_query_init_template;
using service_implementation::keyring_metadata_query_is_valid_template;
using service_implementation::
    keyring_metadata_query_keyring_initialized_template;
using service_implementation::keyring_metadata_query_next_template;

namespace service_definition {

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::is_initialized, ()) {
  return keyring_metadata_query_keyring_initialized_template(
      *g_component_callbacks);
}

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::init,
                   (my_h_keyring_component_metadata_iterator *
                    metadata_iterator)) {
  *metadata_iterator = nullptr;
  std::unique_ptr<config_vector> it;
  bool retval =
      keyring_metadata_query_init_template(it, *g_component_callbacks);
  if (retval == false)
    *metadata_iterator =
        reinterpret_cast<my_h_keyring_component_metadata_iterator>(
            it.release());
  return retval;
}

DEFINE_BOOL_METHOD(
    Keyring_metadata_query_service_impl::deinit,
    (my_h_keyring_component_metadata_iterator metadata_iterator)) {
  std::unique_ptr<config_vector> it;
  it.reset(reinterpret_cast<config_vector *>(metadata_iterator));
  return keyring_metadata_query_deinit_template(it);
}

DEFINE_BOOL_METHOD(
    Keyring_metadata_query_service_impl::is_valid,
    (my_h_keyring_component_metadata_iterator metadata_iterator)) {
  std::unique_ptr<config_vector> it;
  it.reset(reinterpret_cast<config_vector *>(metadata_iterator));
  bool retval = keyring_metadata_query_is_valid_template(it);
  (void)it.release();
  return retval;
}

DEFINE_BOOL_METHOD(
    Keyring_metadata_query_service_impl::next,
    (my_h_keyring_component_metadata_iterator metadata_iterator)) {
  std::unique_ptr<config_vector> it;
  it.reset(reinterpret_cast<config_vector *>(metadata_iterator));
  bool retval = keyring_metadata_query_next_template(it);
  (void)it.release();
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::get_length,
                   (my_h_keyring_component_metadata_iterator metadata_iterator,
                    size_t *key_buffer_length, size_t *value_buffer_length)) {
  std::unique_ptr<config_vector> it;
  it.reset(reinterpret_cast<config_vector *>(metadata_iterator));
  bool retval = keyring_metadata_query_get_length_template(
      it, key_buffer_length, value_buffer_length);
  (void)it.release();
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::get,
                   (my_h_keyring_component_metadata_iterator metadata_iterator,
                    char *key_buffer, size_t key_buffer_len, char *value_buffer,
                    size_t value_buffer_len)) {
  std::unique_ptr<config_vector> it;
  it.reset(reinterpret_cast<config_vector *>(metadata_iterator));
  bool retval = keyring_metadata_query_get_template(
      key_buffer, key_buffer_len, value_buffer, value_buffer_len, it);
  (void)it.release();
  return retval;
}

}  // namespace service_definition
}  // namespace keyring_common
