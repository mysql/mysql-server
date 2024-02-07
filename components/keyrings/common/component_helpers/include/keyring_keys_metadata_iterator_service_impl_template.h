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

#ifndef KEYRING_KEYS_METADATA_SERVICE_IMPL_TEMPLATE_INCLUDED
#define KEYRING_KEYS_METADATA_SERVICE_IMPL_TEMPLATE_INCLUDED

#include <cstring>
#include <functional> /* std::function */
#include <memory>

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
  Forward iterator initialization

  @param [out] it                 metadata iterator
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/
template <typename Backend, typename Data_extension = Data>
bool init_keys_metadata_iterator_template(
    std::unique_ptr<Iterator<Data_extension>> &it,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }

    if (keyring_operations.init_forward_iterator(it, false) == true) {
      LogComponentErr(
          INFORMATION_LEVEL,
          ER_NOTE_KEYRING_COMPONENT_KEYS_METADATA_ITERATOR_INIT_FAILED);
      return true;
    }

    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "init",
                    "keyring_keys_metadata_iterator");
    return true;
  }
}

/**
  Iterator deinitialization

  @param [out] it                 metadata iterator
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/
template <typename Backend, typename Data_extension = Data>
bool deinit_keys_metadata_iterator_template(
    std::unique_ptr<Iterator<Data_extension>> &it,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }
    keyring_operations.deinit_forward_iterator(it);
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "deinit",
                    "keyring_keys_metadata_iterator");
    return true;
  }
}

/**
  Check validity of the iterator

  @param [in] it                  metadata iterator
  @param [in] keyring_operations  Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns Validty of the iterator
    @retval true  Iterator is valid
    @retval false Iterator is invalid
*/
template <typename Backend, typename Data_extension = Data>
bool keys_metadata_iterator_is_valid(
    std::unique_ptr<Iterator<Data_extension>> &it,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return false;
    }
    return keyring_operations.is_valid(it);
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "is_valid",
                    "keyring_keys_metadata_iterator");
    return false;
  }
}

/**
  Move iterator forward.

  @param [out] it                 metadata iterator
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Component specific callbacks

  @returns Status of the operation
    @retval false Success - indicates that iterator is pointing to next entry
    @retval true  Failure - indicates that iterator has reached the end
*/
template <typename Backend, typename Data_extension = Data>
bool keys_metadata_iterator_next(
    std::unique_ptr<Iterator<Data_extension>> &it,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }
    if (keyring_operations.next(it) == true) {
      return true;
    }
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "next",
                    "keyring_keys_metadata_iterator");
    return true;
  }
}

/**
  Fetch length of metadata for current key pointed by iterator

  @param [out]  it                 metadata iterator
  @param [out]  data_id_length     Length of data_id buffer
  @param [out]  auth_id_length     Length of auth_id buffer
  @param [in]   keyring_operations Reference to the object
                                   that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/
template <typename Backend, typename Data_extension = Data>
bool keys_metadata_get_length_template(
    std::unique_ptr<Iterator<Data_extension>> &it, size_t *data_id_length,
    size_t *auth_id_length,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }

    Data_extension data;
    Metadata metadata;
    if (keyring_operations.get_iterator_data(it, metadata, data) == true) {
      LogComponentErr(
          INFORMATION_LEVEL,
          ER_NOTE_KEYRING_COMPONENT_KEYS_METADATA_ITERATOR_FETCH_FAILED);
      return true;
    }

    *data_id_length = metadata.key_id().length();
    *auth_id_length = metadata.owner_id().length();
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "get_length",
                    "keyring_keys_metadata_iterator");
    return true;
  }
}
/**
  Fetch metadata for current key pointed by iterator

  @param [out] it                 metadata iterator
  @param [out] data_id            ID information of current data
  @param [in]  data_id_length     Length of data_id buffer
  @param [out] auth_id            Owner of the key
  @param [in]  auth_id_length     Length of auth_id buffer
  @param [in]  keyring_operations Reference to the object
                                  that handles cache and backend
  @param [in]  callbacks          Handle to component specific callbacks

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/
template <typename Backend, typename Data_extension = Data>
bool keys_metadata_get_template(
    std::unique_ptr<Iterator<Data_extension>> &it, char *data_id,
    size_t data_id_length, char *auth_id, size_t auth_id_length,
    Keyring_operations<Backend, Data_extension> &keyring_operations,
    Component_callbacks &callbacks) {
  try {
    if (callbacks.keyring_initialized() == false) {
      LogComponentErr(INFORMATION_LEVEL,
                      ER_NOTE_KEYRING_COMPONENT_NOT_INITIALIZED);
      return true;
    }

    Data_extension data;
    Metadata metadata;
    if (keyring_operations.get_iterator_metadata(it, metadata, data) == true) {
      LogComponentErr(
          INFORMATION_LEVEL,
          ER_NOTE_KEYRING_COMPONENT_KEYS_METADATA_ITERATOR_FETCH_FAILED);
      return true;
    }

    if (metadata.key_id().length() >= data_id_length) {
      assert(false);
      return true;
    }

    if (metadata.owner_id().length() >= auth_id_length) {
      assert(false);
      return true;
    }

    memcpy(data_id, metadata.key_id().c_str(), metadata.key_id().length());
    data_id[metadata.key_id().length()] = '\0';
    memcpy(auth_id, metadata.owner_id().c_str(), metadata.owner_id().length());
    auth_id[metadata.owner_id().length()] = '\0';
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "get",
                    "keyring_keys_metadata_iterator");
    return true;
  }
}

}  // namespace service_implementation
}  // namespace keyring_common

#endif  // !KEYRING_KEYS_METADATA_SERVICE_IMPL_TEMPLATE_INCLUDED
