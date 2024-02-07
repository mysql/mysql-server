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

#ifndef KEYRING_LOAD_INCLUDED
#define KEYRING_LOAD_INCLUDED

#include <mysql/components/service.h>

/**
  @ingroup group_keyring_component_services_inventory

  Keyring load service provides way to initialize or reiniitalize
  keyring component. This must be implemented by any component
  that aims at providing keyring functionality.
  @code
  bool initialize_keyring() {
    my_service<SERVICE_TYPE(keyring_load)> keyring_load(
        "keyring_load", m_reg_srv);
    if (!keyring_load.is_valid()) {
      log_error("Failed to obtain handle of keyring initialize service");
      return true;
    }

    if (keyring_load->initialize(component_path, instance_path) == true) {
      return true;
    }
    return false;
  }
  @endcode
*/

BEGIN_SERVICE_DEFINITION(keyring_load)

/**
  Initialize or Reinitialize keyring

  A call to (re)initialize service API should result into
  - Reading keyring configuration from its source
  - Connecting to keyring backend
  - Fetch information about stored key and populate
    new in-memory structures - as needed

  Note: This routine should be called in following cases:
    A. After loading keyring component
    B. To refresh keyring component

  @param [in]  component_path Path to component's shared library. Non-null.
  @param [in]  instance_path  Path for instance specific configuration.
                              If null, assumed as current working directory.

  @retval Status of the operation
    @retval false Success
    @retval true  Failure
*/

DECLARE_BOOL_METHOD(load,
                    (const char *component_path, const char *instance_path));

END_SERVICE_DEFINITION(keyring_load)
#endif  // !KEYRING_LOAD_INCLUDED
