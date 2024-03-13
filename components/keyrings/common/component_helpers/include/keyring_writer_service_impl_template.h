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

#ifndef KEYRING_WRITER_SERVICE_IMPL_TEMPLATE_INCLUDED
#define KEYRING_WRITER_SERVICE_IMPL_TEMPLATE_INCLUDED

#include <functional> /* std::function */
#include <sstream>

#include <my_dbug.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include <mysqld_error.h>

#include <components/keyrings/common/component_helpers/include/service_requirements.h>
#include <components/keyrings/common/data/data.h>
#include <components/keyrings/common/data/meta.h>
#include <components/keyrings/common/operations/operations.h>

namespace keyring_common::service_implementation {

using keyring_common::data::Data;
using keyring_common::meta::Metadata;
using keyring_common::operations::Keyring_operations;

/**
  Store data in keyring

  @param [in]  data_id            Data Identifier
  @param [in]  auth_id            Authorization ID
  @param [in]  data               Data to be stored
  @param [in]  data_size          Size of data to be stored
  @param [in]  data_type          Type of data
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns status of the operation
    @retval false Success
    @retval true  Failure
*/

template <typename Backend, typename Data_extension = data::Data>
bool store_template(
    const char *data_id, const char *auth_id, const unsigned char *data,
    size_t data_size, const char *data_type,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (!callbacks.keyring_initialized()) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }

    if (data_id == nullptr || !*data_id) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_EMPTY_DATA_ID);
      assert(false);
      return true;
    }

    if (data_size > keyring_operations.maximum_data_length()) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_WRITE_MAXIMUM_DATA_LENGTH,
                      keyring_operations.maximum_data_length());
      return true;
    }

    Metadata metadata(data_id, auth_id);
    Data data_to_be_stored({reinterpret_cast<const char *>(data), data_size},
                           {data_type, data_type ? strlen(data_type) : 0});
    if (keyring_operations.store(metadata, data_to_be_stored)) {
      LogComponentErr(INFORMATION_LEVEL, ER_NOTE_KEYRING_COMPONENT_STORE_FAILED,
                      data_id,
                      (auth_id == nullptr || !*auth_id) ? "NULL" : auth_id);
      return true;
    }
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "store",
                    "keyring_writer");
    return true;
  }
}

/**
  Remove data from keyring

  @param [in]  data_id             Data Identifier
  @param [in]  auth_id             Authorization ID
  @param [in]  keyring_operations  Reference to the object
                                   that handles cache and backend
  @param [in]  callbacks           Handle to component specific callbacks

  @returns status of the operation
    @retval false Success - Key removed successfully or key not present.
    @retval true  Failure
*/
template <typename Backend, typename Data_extension = data::Data>
bool remove_template(
    const char *data_id, const char *auth_id,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (!callbacks.keyring_initialized()) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }

    if (data_id == nullptr || !*data_id) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_EMPTY_DATA_ID);
      assert(false);
      return true;
    }

    Metadata metadata(data_id, auth_id);
    if (keyring_operations.erase(metadata)) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_REMOVE_FAILED, data_id,
                      (auth_id == nullptr || !*auth_id) ? "NULL" : auth_id);
      return true;
    }
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "remove",
                    "keyring_writer");
    return true;
  }
}

}  // namespace keyring_common::service_implementation

#endif  // !KEYRING_WRITER_SERVICE_IMPL_TEMPLATE_INCLUDED
