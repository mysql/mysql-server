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

#include <components/keyrings/keyring_file/keyring_file.h>

#include <components/keyrings/common/component_helpers/include/keyring_reader_service_definition.h>
#include <components/keyrings/common/component_helpers/include/keyring_reader_service_impl_template.h>

using keyring_file::g_component_callbacks;
using keyring_file::g_keyring_operations;
using keyring_file::backend::Keyring_file_backend;

namespace keyring_common {

using service_implementation::deinit_reader_template;
using service_implementation::fetch_length_template;
using service_implementation::fetch_template;
using service_implementation::init_reader_template;

namespace service_definition {

DEFINE_BOOL_METHOD(Keyring_reader_service_impl::init,
                   (const char *data_id, const char *auth_id,
                    my_h_keyring_reader_object *reader_object)) {
  std::unique_ptr<Iterator<Data>> it;
  int retval = init_reader_template<Keyring_file_backend>(
      data_id, auth_id, it, *g_keyring_operations, *g_component_callbacks);
  *reader_object = nullptr;
  if (retval == 1)
    *reader_object = reinterpret_cast<my_h_keyring_reader_object>(it.release());
  return retval < 0;
}

DEFINE_BOOL_METHOD(Keyring_reader_service_impl::deinit,
                   (my_h_keyring_reader_object reader_object)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(reader_object));
  return deinit_reader_template<Keyring_file_backend>(it, *g_keyring_operations,
                                                      *g_component_callbacks);
}

DEFINE_BOOL_METHOD(Keyring_reader_service_impl::fetch_length,
                   (my_h_keyring_reader_object reader_object, size_t *data_size,
                    size_t *data_type_size)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(reader_object));
  bool retval = fetch_length_template<Keyring_file_backend>(
      it, data_size, data_type_size, *g_keyring_operations,
      *g_component_callbacks);
  /* Make sure we don't free the pointer */
  (void)it.release();
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_reader_service_impl::fetch,
                   (my_h_keyring_reader_object reader_object,
                    unsigned char *data_buffer, size_t data_buffer_length,
                    size_t *data_size, char *data_type_buffer,
                    size_t data_type_buffer_length, size_t *data_type_size)) {
  std::unique_ptr<Iterator<Data>> it;
  it.reset(reinterpret_cast<Iterator<Data> *>(reader_object));
  bool retval = fetch_template<Keyring_file_backend>(
      it, data_buffer, data_buffer_length, data_size, data_type_buffer,
      data_type_buffer_length, data_type_size, *g_keyring_operations,
      *g_component_callbacks);
  /* Make sure we don't free the pointer */
  (void)it.release();
  return retval;
}

}  // namespace service_definition
}  // namespace keyring_common
