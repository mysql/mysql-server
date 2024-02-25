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

#ifndef KEYRING_READER_SERVICE_IMPL_INCLUDED
#define KEYRING_READER_SERVICE_IMPL_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/keyring_reader_with_status.h>

namespace keyring_common {
namespace service_definition {

class Keyring_reader_service_impl {
 public:
  /**
    Initialize reader

    @param [in]  data_id          Data Identifier
    @param [in]  auth_id          Authorization ID
    @param [out] reader_object    Reader object

    @returns status of the operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(init, (const char *data_id, const char *auth_id,
                                   my_h_keyring_reader_object *reader_object));

  /**
    Deinitialize reader

    @param [in] reader_object    Reader object

    @returns status of the operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(deinit, (my_h_keyring_reader_object reader_object));

  /**
    Fetch length of the data

    @param [in]  reader_object      Reader object
    @param [out] data_size          Size of fetched data
    @param [out] data_type_size     Size of data type

    @returns status of the operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(fetch_length,
                            (my_h_keyring_reader_object reader_object,
                             size_t *data_size, size_t *data_type_size));

  /**
    Fetches data from keyring

    @param [in]  reader_object           Reader object
    @param [out] data_buffer             Out buffer for data
    @param [in]  data_buffer_length      Length of out buffer
    @param [out] data_size               Size of fetched data
    @param [out] data_type               Type of data
    @param [in]  data_type_buffer_length Length of data type buffer
    @param [out] data_type_size          Size of data type

    @returns status of the operation
      @retval false Success
      @retval true  Failure
  */

  static DEFINE_BOOL_METHOD(fetch,
                            (my_h_keyring_reader_object reader_object,
                             unsigned char *data_buffer,
                             size_t data_buffer_length, size_t *data_size,
                             char *data_type, size_t data_type_buffer_length,
                             size_t *data_type_size));
};

}  // namespace service_definition
}  // namespace keyring_common

#define KEYRING_READER_IMPLEMENTOR(component_name)                             \
  BEGIN_SERVICE_IMPLEMENTATION(component_name, keyring_reader_with_status)     \
  keyring_common::service_definition::Keyring_reader_service_impl::init,       \
      keyring_common::service_definition::Keyring_reader_service_impl::deinit, \
      keyring_common::service_definition::Keyring_reader_service_impl::        \
          fetch_length,                                                        \
      keyring_common::service_definition::Keyring_reader_service_impl::fetch   \
      END_SERVICE_IMPLEMENTATION()

#endif  // KEYRING_READER_SERVICE_IMPL_INCLUDED
