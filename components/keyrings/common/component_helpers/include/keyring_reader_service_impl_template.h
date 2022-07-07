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

#ifndef KEYRING_READER_SERVICE_IMPL_TEMPLATE_INCLUDED
#define KEYRING_READER_SERVICE_IMPL_TEMPLATE_INCLUDED

#include <functional> /* std::function */
#include <sstream>

#include <my_dbug.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include <mysqld_error.h>

#include <components/keyrings/common/component_helpers/include/service_requirements.h>
#include <components/keyrings/common/data/data.h>
#include <components/keyrings/common/data/meta.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/operations/operations.h>

using keyring_common::data::Data;
using keyring_common::iterator::Iterator;
using keyring_common::meta::Metadata;
using keyring_common::operations::Keyring_operations;

namespace keyring_common {
namespace service_implementation {

/**
  Initialize reader

  @param [in]  data_id             Data Identifier
  @param [in]  auth_id             Authorization ID
  @param [out] it                  Iterator
  @param [in]  keyring_operations  Reference to the object
                                   that handles cache and backend
  @param [in]  callbacks           Handle to component specific callbacks

  @returns status of the operation
    @retval -1 Keyring error. reader_object will not be created.
    @retval  0 Key not found OR error fetching keys.
               reader_object will not be created.
    @retval  1 Key found, check out parameters
*/
template <typename Backend, typename Data_extension = data::Data>
int init_reader_template(
    const char *data_id, const char *auth_id,
    std::unique_ptr<Iterator<Data_extension>> &it,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      return -1;
    }

    if (data_id == nullptr || !*data_id) {
      assert(false);
      return 0;
    }

    Metadata metadata(data_id, auth_id);
    if (keyring_operations.init_read_iterator(it, metadata) == true) {
      return 0;
    }

    if (keyring_operations.is_valid(it) == false) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_READ_DATA_NOT_FOUND, data_id,
                      (auth_id == nullptr || !*auth_id) ? "NULL" : auth_id);
      keyring_operations.deinit_forward_iterator(it);
      return 0;
    }

    return 1;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "init",
                    "keyring_reader_with_status");
    return -1;
  }
}

/**
  Deinitialize reader

  @param [in, out] it                  Iterator
  @param [in]      keyring_operations  Reference to the object
                                       that handles cache and backend
  @param [in]      callbacks           Handle to component specific callbacks

  @returns status of the operation
    @retval false Success
    @retval true  Failure
*/

template <typename Backend, typename Data_extension = data::Data>
bool deinit_reader_template(
    std::unique_ptr<Iterator<Data_extension>> &it,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      return true;
    }
    keyring_operations.deinit_forward_iterator(it);
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "deinit",
                    "keyring_reader_with_status");
    return true;
  }
}

/**
  Fetch length of the data

  @param [in]  it                 Iterator
  @param [out] data_size          Size of fetched data
  @param [out] data_type_size     Size of data type
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks
  @returns status of the operation
    @retval false Success
    @retval true  Failure
*/
template <typename Backend, typename Data_extension = data::Data>
bool fetch_length_template(
    std::unique_ptr<Iterator<Data_extension>> &it, size_t *data_size,
    size_t *data_type_size,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      return true;
    }

    if (data_size == nullptr || data_type_size == nullptr) {
      assert(false);
      return true;
    }

    Data_extension data;
    Metadata metadata;
    if (keyring_operations.get_iterator_data(it, metadata, data) == true) {
      return true;
    }

    *data_size = data.data().length();
    *data_type_size = data.type().length();
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "fetch_length",
                    "keyring_reader_with_status");
    return true;
  }
}

/**
  Fetches data from keyring

  @param [in]  it                      Iterator
  @param [out] data_buffer             Out buffer for data
  @param [in]  data_buffer_length      Length of out buffer
  @param [out] data_size               Size of fetched data
  @param [out] data_type_buffer        Type of data
  @param [in]  data_type_buffer_length Length of data type buffer
  @param [out] data_type_size          Size of data type
  @param [in]  keyring_operations      Reference to the object
                                       that handles cache and backend
  @param [in]  callbacks               Handle to component specific callbacks

  @returns status of the operation
    @retval false Success
    @retval true  Failure
*/
template <typename Backend, typename Data_extension = data::Data>
bool fetch_template(
    std::unique_ptr<Iterator<Data_extension>> &it, unsigned char *data_buffer,
    size_t data_buffer_length, size_t *data_size, char *data_type_buffer,
    size_t data_type_buffer_length, size_t *data_type_size,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      return true;
    }

    Data_extension data;
    Metadata metadata;
    if (keyring_operations.get_iterator_data(it, metadata, data) == true) {
      return true;
    }

    if (data_buffer_length < data.data().length() || data_buffer == nullptr) {
      assert(false);
      return true;
    }

    if (data_type_buffer_length < data.type().length() ||
        data_type_buffer == nullptr) {
      assert(false);
      return true;
    }

    memset(data_buffer, 0, data_buffer_length);
    memset(data_type_buffer, 0, data_type_buffer_length);

    memcpy(data_buffer, data.data().c_str(), data.data().length());
    *data_size = data.data().length();

    memcpy(data_type_buffer, data.type().c_str(), data.type().length());
    *data_type_size = data.type().length();

    return false;
  } catch (...) {
    memset(data_buffer, 0, data_buffer_length);
    memset(data_type_buffer, 0, data_type_buffer_length);
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "fetch",
                    "keyring_reader_with_status");
    return true;
  }
}

}  // namespace service_implementation
}  // namespace keyring_common

#endif  // KEYRING_READER_SERVICE_IMPL_TEMPLATE_INCLUDED
