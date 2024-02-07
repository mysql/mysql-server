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

#ifndef KEYRING_WRITER_INCLUDED
#define KEYRING_WRITER_INCLUDED

#include <mysql/components/service.h>

/**
  @ingroup group_keyring_component_services_inventory

  Keyring writer service provides APIs to add/remove
  sensitive data to/from keyring backend.

  Data stored within keyring should be uniquely identified using:
  1. Data ID
      An identifier associated with data - supplied by keyring APIs' callers
  2. Auth ID
      An identifier associated with owner of the data - suppled by keyring
      APIs' callers. If Auth ID is not provided, key is treated as an internal
      key. Such a key shalll not be accessible to database users using
      SQL interface

  @code
  bool write_key(const char *data_id, const char *auth_id,
                 const unsigned char *data_buffer, size_t data_length,
                 const char *data_type) {
    my_service<SERVICE_TYPE(keyring_writer)> keyring_writer("keyring_writer",
                                                            m_reg_srv);
    if (!keyring_writer.is_valid()) {
      return true;
    }

    return keyring_writer->store(data_id, auth_id, data_buffer, data_length,
                              data_type);
  }

  bool remove_key(const char *data_id, const char *auth_id) {
    my_service<SERVICE_TYPE(keyring_writer)> keyring_writer("keyring_writer",
                                                            m_reg_srv);
    if (!keyring_writer.is_valid()) {
      return true;
    }

    return keyring_writer->remove(data_id, auth_id);
  }
  @endcode
*/

BEGIN_SERVICE_DEFINITION(keyring_writer)

/**
  Store data identified with (data_id, auth_id) in keyring backend

  Data_type value is implementation specific. It associates type
  label with data which may be an important indicator for certain
  backends.

  Examples: AES, SECRET

  Note: If components want to support aes_encryption service,
  it must support storing data of type AES.

  A success status implies that data is stored persistently on
  keyring backend and shall always be available for access unless
  removed explicitly.

  @note Implementation can restrict type and/or size of data that can be
        stored in keyring.

  @param [in]  data_id        Data Identifier. Byte string.
  @param [in]  auth_id        Authorization ID. Byte string.
  @param [in]  data           Data to be stored. Byte string.
  @param [in]  data_size      Size of data to be stored
  @param [in]  data_type      Type of data. ASCII. Null terminated.

  @returns status of the operation
    @retval false Success - Data is stored successfully in backend
    @retval true  Failure
*/

DECLARE_BOOL_METHOD(store, (const char *data_id, const char *auth_id,
                            const unsigned char *data, size_t data_size,
                            const char *data_type));

/**
  Remove data identified by (data_id, auth_id) from keyring backend
  if present.

  Data_type value is implementation specific. It associates type
  label with data which may be an important indicator for certain
  backends.

  Examples: AES, SECRET

  Once removed, data should not be accessible through keyring implementation.
  Based on keyring backend, implementor may decide to either destroy the data
  completely or change the state of the data to make in unavailable.

  @param [in] data_id Data Identifier. Byte string.
  @param [in] auth_id Authorization ID. Byte string.

  @returns status of the operation
    @retval false Success - Key removed successfully or key not present.
    @retval true  Failure
*/

DECLARE_BOOL_METHOD(remove, (const char *data_id, const char *auth_id));

END_SERVICE_DEFINITION(keyring_writer)

#endif  // !KEYRING_WRITER_INCLUDED
