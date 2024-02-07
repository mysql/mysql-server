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

#ifndef KEYRING_METADATA_QUERY_SERVICE_IMPL_INCLUDED
#define KEYRING_METADATA_QUERY_SERVICE_IMPL_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/keyring_metadata_query.h>

namespace keyring_common {
namespace service_definition {

class Keyring_metadata_query_service_impl {
 public:
  /** Returns status of the keyring component */
  static DEFINE_BOOL_METHOD(is_initialized, ());
  /**
    Initialize metadata iterator

    @param [out] metadata_iterator Metadata iterator handle

    @returns Status of iterator initialization
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(init, (my_h_keyring_component_metadata_iterator *
                                   metadata_iterator));

  /**
    Deinitialize metadata iterator

    @param [in, out] metadata_iterator Metadata iterator handle

    @returns Status of iterator deinitialization
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(
      deinit, (my_h_keyring_component_metadata_iterator metadata_iterator));

  /**
    Check validity of iterator

    @param [in] metadata_iterator Metadata iterator handle

    @returns Validity of the the iterator
      @retval true  Iterator valid
      @retval false Iterator invalid
  */
  static DEFINE_BOOL_METHOD(
      is_valid, (my_h_keyring_component_metadata_iterator metadata_iterator));

  /**
    Move iterator forward

    @param [in, out] metadata_iterator Metadata iterator handle

    @returns Status of operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(
      next, (my_h_keyring_component_metadata_iterator metadata_iterator));

  /**
    Get length information about metadata key and value

    @param [in]  metadata_iterator   Metadata iterator handle
    @param [out] key_buffer_length   Length of the key buffer
    @param [out] value_buffer_length Length of the value buffer

    @returns Get length information about key and value
      @retval false Success check out parameters
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(
      get_length, (my_h_keyring_component_metadata_iterator metadata_iterator,
                   size_t *key_buffer_length, size_t *value_buffer_length));

  /**
    Get name and value of metadata at current position

    @param [in]  metadata_iterator   Metadata iterator handle
    @param [out] key_buffer          Output buffer for key
    @param [in]  key_buffer_length   Length of key buffer
    @param [out] value_buffer        Output buffer for value
    @param [in]  value_buffer_length Length of value buffer

    @returns Status of fetch operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(
      get, (my_h_keyring_component_metadata_iterator metadata_iterator,
            char *key_buffer, size_t key_buffer_length, char *value_buffer,
            size_t value_buffer_length));
};

}  // namespace service_definition
}  // namespace keyring_common

#define KEYRING_COMPONENT_STATUS_IMPLEMENTOR(component_name)                \
  BEGIN_SERVICE_IMPLEMENTATION(component_name, keyring_component_status)    \
  keyring_common::service_definition::Keyring_metadata_query_service_impl:: \
      is_initialized                                                        \
      END_SERVICE_IMPLEMENTATION()

#define KEYRING_COMPONENT_METADATA_QUERY_IMPLEMENTOR(component_name)        \
  BEGIN_SERVICE_IMPLEMENTATION(component_name,                              \
                               keyring_component_metadata_query)            \
  keyring_common::service_definition::Keyring_metadata_query_service_impl:: \
      init,                                                                 \
      keyring_common::service_definition::                                  \
          Keyring_metadata_query_service_impl::deinit,                      \
      keyring_common::service_definition::                                  \
          Keyring_metadata_query_service_impl::is_valid,                    \
      keyring_common::service_definition::                                  \
          Keyring_metadata_query_service_impl::next,                        \
      keyring_common::service_definition::                                  \
          Keyring_metadata_query_service_impl::get_length,                  \
      keyring_common::service_definition::                                  \
          Keyring_metadata_query_service_impl::get                          \
          END_SERVICE_IMPLEMENTATION()

#endif  // KEYRING_METADATA_QUERY_SERVICE_IMPL_INCLUDED
