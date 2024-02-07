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

#include <components/keyrings/common/component_helpers/include/keyring_writer_service_definition.h>
#include <components/keyrings/common/component_helpers/include/keyring_writer_service_impl_template.h>

using keyring_file::g_component_callbacks;
using keyring_file::g_keyring_operations;
using keyring_file::backend::Keyring_file_backend;

namespace keyring_common {

using service_implementation::remove_template;
using service_implementation::store_template;

namespace service_definition {
DEFINE_BOOL_METHOD(Keyring_writer_service_impl::store,
                   (const char *data_id, const char *auth_id,
                    const unsigned char *data, size_t data_size,
                    const char *data_type)) {
  return store_template<Keyring_file_backend>(data_id, auth_id, data, data_size,
                                              data_type, *g_keyring_operations,
                                              *g_component_callbacks);
}

DEFINE_BOOL_METHOD(Keyring_writer_service_impl::remove,
                   (const char *data_id, const char *auth_id)) {
  return remove_template<Keyring_file_backend>(
      data_id, auth_id, *g_keyring_operations, *g_component_callbacks);
}

}  // namespace service_definition
}  // namespace keyring_common
