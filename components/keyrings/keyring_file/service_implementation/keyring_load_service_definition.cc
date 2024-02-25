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

#include <components/keyrings/common/component_helpers/include/keyring_load_service_definition.h>
#include <components/keyrings/common/component_helpers/include/service_requirements.h>

using keyring_file::g_keyring_file_inited;
using keyring_file::init_or_reinit_keyring;
using keyring_file::set_paths;

namespace keyring_common {

namespace service_definition {

DEFINE_BOOL_METHOD(Keyring_load_service_impl::load,
                   (const char *component_path, const char *instance_path)) {
  try {
    if (set_paths(component_path, instance_path) == true) {
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }

    if (init_or_reinit_keyring() == true) {
      LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }
    g_keyring_file_inited = true;
    LogComponentErr(INFORMATION_LEVEL, ER_NOTE_KEYRING_COMPONENT_INITIALIZED);
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_NOT_INITIALIZED);
    return true;
  }
}

}  // namespace service_definition
}  // namespace keyring_common
