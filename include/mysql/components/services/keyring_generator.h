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

#ifndef KEYRING_GENERATOR_INCLUDED
#define KEYRING_GENERATOR_INCLUDED

#include <mysql/components/service.h>

/**
  @ingroup group_keyring_component_services_inventory

  Key generator service provides a way to generate random data
  and store it in keyring backend.

  Data stored within keyring should be uniquely identified using:
  1. Data ID
      An identifier associated with data - supplied by keyring APIs' callers
  2. Auth ID
      An identifier associated with owner of the data - suppled by keyring
      APIs' callers. If Auth ID is not provided, key is treated as an internal
      key. Such a key shalll not be accessible to database users using
      SQL interface

  This service does not return generated data back to user.
  For that, Keyring reader service should be used.

  @code
  bool generate_key(const char *data_id, const char *auth_id,
                    const char *data_type, size_t data_size) {
    my_service<SERVICE_TYPE(keyring_generator)> keyring_generator(
        "keyring_reader_generator", m_reg_srv);
    if (!keyring_generator.is_valid()) {
      return true;
    }

    if (keyring_generator->generate(data_id, auth_id, data_type, data_size) ==
        true) {
      return true;
    }
    return false;
  }
  @endcode
*/

BEGIN_SERVICE_DEFINITION(keyring_generator)

/**
  Generate random data of length data_size and
  store it in keyring using identifiers as (data_id, auth_id).

  Data_type value is implementation specific. It associates type
  label with data which may be an important indicator for certain
  backends.

  Examples: AES, SECRET

  Note: If components want to support aes_encryption service,
  it must support storing data of type AES.

  If error object is not initialized, the method will initialize it if returns
  false. Caller will be responsible for freeing error state in such cases.
  No error object will be created or modified if return value is true.

  The action should be atomic from caller's point of view.
  As much as possible, deligate data generation to keyring backend.

  @note Implementation can restrict type and/or size of data that can be
        stored in keyring.

  @param [in]  data_id   Data Identifier. Byte string.
  @param [in]  auth_id   Authorization ID. Byte string.
  @param [in]  data_type Type of data. ASCII. Null terminated.
  @param [in]  data_size Size of the data to be generated

  @returns status of the operation
    @retval false Success - Key generated and stored in keyring.
    @retval truen Failure
*/

DECLARE_BOOL_METHOD(generate, (const char *data_id, const char *auth_id,
                               const char *data_type, size_t data_size));

END_SERVICE_DEFINITION(keyring_generator)

#endif  // !KEYRING_GENERATOR_INCLUDED
