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

#ifndef KEYRING_READER_WITH_STATUS_INCLUDED
#define KEYRING_READER_WITH_STATUS_INCLUDED

#include <mysql/components/service.h>

DEFINE_SERVICE_HANDLE(my_h_keyring_reader_object);

/**
  @ingroup group_keyring_component_services_inventory

  Keyring reader with status service provides APIs to
  fetch sensitive data from keyring backend

  It is designed to be compatible with corresponding plugin
  method which returns state of the keyring as well.

  Data stored within keyring should be uniquely identified using:
  1. Data ID
      An identifier associated with data - supplied by keyring APIs' callers
  2. Auth ID
      An identifier associated with owner of the data - suppled by keyring
      APIs' callers. If Auth ID is not provided, key is treated as an internal
      key. Such a key shalll not be accessible to database users using
      SQL interface

  fetch and fetch_length APIs return a value indicating
  one of the 3 possible states.
  1. An error in keyring component
  2. Key is missing or there is a problem performing the operation
  3. Key is found and returned

  @code
  bool read_key(const char *data_id, const char *auth_id, char **out_key_buffer,
                size_t *out_key_length, char **out_key_type) {
    *out_key_buffer = nullptr;
    *out_key_type = nullptr;
    *out_key_length = 0;
    my_service<SERVICE_TYPE(keyring_reader_with_status)> keyring_reader(
        "keyring_reader_with_status", m_reg_srv);
    if (!keyring_reader.is_valid()) {
      return true;
    }

    my_h_keyring_reader_object reader_object = nullptr;
    int key_exists = 0;
    bool retval = keyring_reader->init(data_id, auth_id, &reader_object,
  &key_exists); if (retval) return true;

    if (key_exists == 0)
      return true;

    auto cleanup_object = create_scope_guard([&]{
      if (reader_object != nullptr) keyring_reader->deinit(reader_object);
      reader_object = nullptr;
    });
    size_t key_length, key_type_length;
    if (keyring_reader->fetch_length(data_id, auth_id, &key_length,
                                     &key_type_length) == true) {
      return true;
    }

    std::unique_ptr<char[]> key_buffer(new char[key_length]);
    std::unique_ptr<char[]> key_type_buffer(new char[key_type_length + 1]);
    if (key_buffer.get() == nullptr || key_type_buffer.get() == nullptr) {
      return true;
    }
    memset(key_buffer.get(), 0, key_length);
    memset(key_type_buffer.get(), 0, key_type_length + 1);

    size t fetched_key_length = 0, fetched_key_type_length = 0;
    if( keyring_reader->fetch(data_id, auth_id, key_buffer.get(),
                              key_length, &fetched_key_length,
                              key_type_buffer, key_type_length,
                              &fetched_key_type_length) == true)
      return true;

    *out_key_buffer = new char[](fetched_key_length);
    *out_key_type = new char[](fetched_key_type_length + 1);
    if (*out_key_buffer == nullptr || *out_key_type == nullptr) {
      return true;
    }
    memset(*out_key_buffer, 0, fetched_key_length);
    memset(*out_key_type, 0, fetched_key_type_length + 1);
    memcpy(*out_key_buffer, key_buffer.get(), fetched_key_length);
    memcpy(*out_key_type, key_type_buffer.get(), fetched_key_type_length);
    *out_key_length = fetched_key_length;

    return false;
  }
  @endcode

  Implementor can choose to:
  A. Read data from backend on each request
  B. Cache data in memory and server read requests from the cache

  In case of B, care should be taken to keep cached data
  in sync with backend.

  To go one step further, implementation may let user choose
  behavior (cached or otherwise) for read operation through
  configuration options.

*/

BEGIN_SERVICE_DEFINITION(keyring_reader_with_status)

/**
  Initialize reader

  @param [in]  data_id          Data Identifier. Byte string.
  @param [in]  auth_id          Authorization ID. Byte string.
  @param [out] reader_object    Reader object

  If return value is false, here is how value of reader_object
  is interpreted:
  reader_object == nullptr implies key does not exist
  reader_object != nullptr implies key exists

  @returns status of the operation
     @retval false Success - Does not mean that key is found.
     @retval true  Failure
*/
DECLARE_BOOL_METHOD(init, (const char *data_id, const char *auth_id,
                           my_h_keyring_reader_object *reader_object));

/**
  Deinitialize reader

   @param [in] reader_object    Reader object

   @returns status of the operation
     @retval false Success
     @retval true  Failure
*/
DECLARE_BOOL_METHOD(deinit, (my_h_keyring_reader_object reader_object));

/**
  Fetch length of the data if it exists
  data_size and data_type_size must not be nullptr.

  Data_type value is implementation specific. It associates type
  label with data which may be an important indicator for certain
  backends.

  Minimum expectation: AES, SECRET

  @param [in]  reader_object      Reader object
  @param [out] data_size          Size of fetched data in bytes
  @param [out] data_type_size     Size of data type

  @returns status of the operation
    @retval false success
    @retval true  failure
*/
DECLARE_BOOL_METHOD(fetch_length, (my_h_keyring_reader_object reader_object,
                                   size_t *data_size, size_t *data_type_size));

/**
  Fetches data if it exists.
  All pointer parameters must be non-null.

  Data_type value is implementation specific. It associates type
  label with data which may be an important indicator for certain
  backends.

  Minimum expectation: AES, SECRET

  data_buffer size must be enough to hold data
  data_type size must be enough to hold datatype and
  a null-terminating character

  @sa fetch_length - To fetch length information about sensitive data

  @param [in]  reader_object           Reader object
  @param [out] data_buffer             Out buffer for data. Byte string.
  @param [in]  data_buffer_length      Length of out buffer
  @param [out] data_size               Size of fetched data
  @param [out] data_type               Type of data. ASCII. Null terminated.
  @param [in]  data_type_buffer_length Length of data type buffer
  @param [out] data_type_size          Size of fetched datatype

  @returns status of the operation
    @retval false success
    @retval true  failure
*/

DECLARE_BOOL_METHOD(fetch,
                    (my_h_keyring_reader_object reader_object,
                     unsigned char *data_buffer, size_t data_buffer_length,
                     size_t *data_size, char *data_type,
                     size_t data_type_buffer_length, size_t *data_type_size));

END_SERVICE_DEFINITION(keyring_reader_with_status)

#endif  // !KEYRING_READER_WITH_STATUS_INCLUDED
