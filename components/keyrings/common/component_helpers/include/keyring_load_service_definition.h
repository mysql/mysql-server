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

#ifndef KEYRING_LOAD_SERVICE_DEFINITION_INCLUDED
#define KEYRING_LOAD_SERVICE_DEFINITION_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/keyring_load.h>

namespace keyring_common {
namespace service_definition {

class Keyring_load_service_impl {
 public:
  /**
    Initialize keyring

    @param [in]  component_path Path to component's shared library
    @param [in]  instance_path  Path for instance specific configuration

    @retval Status of the operation
      @retval false Success
      @retval true  Failure
  */

  static DEFINE_BOOL_METHOD(load, (const char *component_path,
                                   const char *instance_path));
};

}  // namespace service_definition
}  // namespace keyring_common

#define KEYRING_LOAD_IMPLEMENTOR(component_name)                      \
  BEGIN_SERVICE_IMPLEMENTATION(component_name, keyring_load)          \
  keyring_common::service_definition::Keyring_load_service_impl::load \
  END_SERVICE_IMPLEMENTATION()

#endif  // !KEYRING_LOAD_SERVICE_DEFINITION_INCLUDED
