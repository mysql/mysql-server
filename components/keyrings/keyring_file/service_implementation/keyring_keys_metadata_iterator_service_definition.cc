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

#include <memory>

#include <components/keyrings/keyring_file/keyring_file.h> /* Globals */

#include <components/keyrings/common/component_helpers/include/keyring_keys_metadata_iterator_service_definition.h>
#include <components/keyrings/common/component_helpers/include/keyring_keys_metadata_iterator_service_impl_template.h>

using keyring_file::g_component_callbacks;
using keyring_file::g_keyring_operations;
using keyring_file::backend::Keyring_file_backend;

namespace keyring_common {

using service_implementation::deinit_keys_metadata_iterator_template;
using service_implementation::init_keys_metadata_iterator_template;
using service_implementation::keys_metadata_get_length_template;
using service_implementation::keys_metadata_get_template;
using service_implementation::keys_metadata_iterator_is_valid;
using service_implementation::keys_metadata_iterator_next;

namespace service_definition {
DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::init,
                   (my_h_keyring_keys_metadata_iterator * forward_iterator)) {
  std::unique_ptr<Iterator<Data>> it;
  bool retval = init_keys_metadata_iterator_template<Keyring_file_backend>(
      it, *g_keyring_operations, *g_component_callbacks);
  if (retval == false)
    *forward_iterator =
        reinterpret_cast<my_h_keyring_keys_metadata_iterator>(it.release());
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::deinit,
                   (my_h_keyring_keys_metadata_iterator forward_iterator)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(forward_iterator));
  return deinit_keys_metadata_iterator_template<Keyring_file_backend>(
      it, *g_keyring_operations, *g_component_callbacks);
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::is_valid,
                   (my_h_keyring_keys_metadata_iterator forward_iterator)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(forward_iterator));
  bool retval = keys_metadata_iterator_is_valid<Keyring_file_backend>(
      it, *g_keyring_operations, *g_component_callbacks);
  /* Make sure we don't free the pointer */
  (void)it.release();
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::next,
                   (my_h_keyring_keys_metadata_iterator forward_iterator)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(forward_iterator));
  bool retval = keys_metadata_iterator_next<Keyring_file_backend>(
      it, *g_keyring_operations, *g_component_callbacks);
  /* Make sure we don't free the pointer */
  (void)it.release();
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::get_length,
                   (my_h_keyring_keys_metadata_iterator forward_iterator,
                    size_t *data_id_length, size_t *auth_id_length)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(forward_iterator));
  bool retval = keys_metadata_get_length_template<Keyring_file_backend>(
      it, data_id_length, auth_id_length, *g_keyring_operations,
      *g_component_callbacks);
  /* Make sure we don't free the pointer */
  (void)it.release();
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::get,
                   (my_h_keyring_keys_metadata_iterator forward_iterator,
                    char *data_id, size_t data_id_length, char *auth_id,
                    size_t auth_id_length)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(forward_iterator));
  bool retval = keys_metadata_get_template<Keyring_file_backend>(
      it, data_id, data_id_length, auth_id, auth_id_length,
      *g_keyring_operations, *g_component_callbacks);
  /* Make sure we don't free the pointer */
  (void)it.release();
  return retval;
}

}  // namespace service_definition
}  // namespace keyring_common
