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

#ifndef KEYRING_METADATA_QUERY_SERVICE_IMPL_TEMPLATE_INCLUDED
#define KEYRING_METADATA_QUERY_SERVICE_IMPL_TEMPLATE_INCLUDED

#include <cstring>
#include <functional>
#include <memory>

#include <my_dbug.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include <mysqld_error.h>

#include <components/keyrings/common/component_helpers/include/service_requirements.h>

namespace keyring_common {
namespace service_implementation {

/**
  Returns status of the keyring component

  @param [in] callbacks Component specific callbacks

  @returns Status of keyring
    @retval true  Initialized
    @retval false Not initialized
*/
bool keyring_metadata_query_keyring_initialized_template(
    Component_callbacks &callbacks) {
  return callbacks.keyring_initialized();
}

/**
  Initialize metadata iterator

  @param [out] it         Metadata iterator handle
  @param [in]  callbacks  Component callback handle

  @returns Status of iterator initialization
    @retval false Success
    @retval true  Failure. Check error state.
*/
bool keyring_metadata_query_init_template(std::unique_ptr<config_vector> &it,
                                          Component_callbacks &callbacks) {
  try {
    return callbacks.create_config(it);
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "init",
                    "keyring_component_metadata_query");
    return true;
  }
}

/**
  Deinitialize metadata iterator

  @param [in, out] it          Metadata iterator handle

  @returns Status of iterator deinitialization
    @retval false Success
    @retval true  Failure. Check error state.
*/
bool keyring_metadata_query_deinit_template(
    std::unique_ptr<config_vector> &it) {
  try {
    it.reset(nullptr);
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "deinit",
                    "keyring_component_metadata_query");
    return true;
  }
}

/**
  Check validity of iterator

  @param [in] it Metadata iterator handle

  @returns Validity of the the iterator
    @retval true  Iterator valid
    @retval false Iterator invalid
*/
bool keyring_metadata_query_is_valid_template(
    std::unique_ptr<config_vector> &it) {
  try {
    return ((it.get() != nullptr) && (it.get()->size() > 0));
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "is_valid",
                    "keyring_component_metadata_query");
    return false;
  }
}

/**
  Move iterator forward

  @param [in, out] it          Metadata iterator handle

  @returns Status of operation
    @retval false Success
    @retval true  Failure.
*/
bool keyring_metadata_query_next_template(std::unique_ptr<config_vector> &it) {
  try {
    if (it.get()->size() == 0) {
      return true;
    }
    it.get()->erase(it.get()->begin());
    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "next",
                    "keyring_component_metadata_query");
    return true;
  }
}

/**
  Get length information about metadata key and value

  @param [in]  it                  Metadata iterator handle
  @param [out] key_buffer_length   Length of the key buffer
  @param [out] value_buffer_length Length of the value buffer

  @returns Get length information about key and value
    @retval false Success check out parameters
    @retval true  Error
*/
bool keyring_metadata_query_get_length_template(
    std::unique_ptr<config_vector> &it, size_t *key_buffer_length,
    size_t *value_buffer_length) {
  try {
    if (it->size() == 0) {
      return true;
    }

    if (key_buffer_length == nullptr || value_buffer_length == nullptr) {
      assert(false);
      return true;
    }

    auto key_value = (*it)[0];

    // Account for null termination
    *key_buffer_length = key_value.first.length() + 1;
    *value_buffer_length = key_value.second.length() + 1;

    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "get_length",
                    "keyring_component_metadata_query");
    return true;
  }
}

/**
  Get name and value of metadata at current position

  @param [out] key_buffer          Output buffer for key
  @param [in]  key_buffer_length   Length of key buffer
  @param [out] value_buffer        Output buffer for value
  @param [in]  value_buffer_length Length of value buffer
  @param [in]  it                  Metadata iterator handle

  @returns Status of fetch operation
    @retval false Success
    @retval true  Failure. Check error state.
*/
bool keyring_metadata_query_get_template(char *key_buffer,
                                         size_t key_buffer_length,
                                         char *value_buffer,
                                         size_t value_buffer_length,
                                         std::unique_ptr<config_vector> &it) {
  try {
    if (it->size() == 0) {
      return true;
    }

    auto key_value = (*it)[0];

    if (key_value.first.length() >= key_buffer_length) {
      assert(false);
      return true;
    }

    if (key_value.second.length() >= value_buffer_length) {
      assert(false);
      return true;
    }

    memcpy(key_buffer, key_value.first.c_str(), key_value.first.length());
    key_buffer[key_value.first.length()] = '\0';
    memcpy(value_buffer, key_value.second.c_str(), key_value.second.length());
    value_buffer[key_value.second.length()] = '\0';

    return false;
  } catch (...) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_EXCEPTION, "get",
                    "keyring_component_metadata_query");
    return true;
  }
}

}  // namespace service_implementation
}  // namespace keyring_common
#endif  // !KEYRING_METADATA_QUERY_SERVICE_IMPL_TEMPLATE_INCLUDED
