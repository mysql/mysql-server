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

#include <components/keyrings/keyring_file/keyring_file.h> /* Globals */

#include <components/keyrings/common/component_helpers/include/keyring_generator_service_definition.h>
#include <components/keyrings/common/component_helpers/include/keyring_generator_service_impl_template.h>

using keyring_file::g_component_callbacks;
using keyring_file::g_keyring_operations;
using keyring_file::backend::Keyring_file_backend;

namespace keyring_common {

using service_implementation::generate_template;

namespace service_definition {

DEFINE_BOOL_METHOD(Keyring_generator_service_impl::generate,
                   (const char *data_id, const char *auth_id,
                    const char *data_type, size_t data_size)) {
  return generate_template<Keyring_file_backend>(
      data_id, auth_id, data_type, data_size, *g_keyring_operations,
      *g_component_callbacks);
}

}  // namespace service_definition
}  // namespace keyring_common
