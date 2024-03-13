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

#ifndef SERVICE_REQUIREMENTS_INCLUDED
#define SERVICE_REQUIREMENTS_INCLUDED

#include <memory>
#include <string>
#include <vector>

namespace keyring_common::service_implementation {

using config_vector = std::vector<std::pair<std::string, std::string>>;

class Component_callbacks {
 public:
  /**
    Keyring component status

    @returns status of keyring component
      @retval true  Keyring component is initialized
      @retval false Keyring component is not initialized
  */
  bool keyring_initialized();

  /**
    Create configuration vector

    @param [in] metadata Output vector for status metadata

    @returns status of metadata vector creation
      @returns false Success
      @returns true  Failure
  */
  bool create_config(std::unique_ptr<config_vector> &metadata);
};

}  // namespace keyring_common::service_implementation

#endif /* SERVICE_REQUIREMENTS_INCLUDED */
