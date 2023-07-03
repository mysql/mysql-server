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

#include "keyring_file.h"

namespace keyring_common {
namespace service_implementation {

bool Component_callbacks::keyring_initialized() {
  return keyring_file::g_keyring_file_inited;
}

bool Component_callbacks::create_config(
    std::unique_ptr<config_vector> &metadata) {
  return keyring_file::config::create_config(metadata);
}

}  // namespace service_implementation
}  // namespace keyring_common

namespace keyring_file {
/** Component callbacks */
keyring_common::service_implementation::Component_callbacks
    *g_component_callbacks = nullptr;
}  // namespace keyring_file
