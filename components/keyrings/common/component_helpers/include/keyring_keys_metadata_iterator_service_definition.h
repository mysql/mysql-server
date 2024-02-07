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

#ifndef KEYRING_KEYS_METADATA_FORWARD_ITERATOR_SERVICE_IMPL_INCLUDED
#define KEYRING_KEYS_METADATA_FORWARD_ITERATOR_SERVICE_IMPL_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/keyring_keys_metadata_iterator.h>

namespace keyring_common {
namespace service_definition {

class Keyring_keys_metadata_iterator_service_impl {
 public:
  /**
    Forward iterator initialization

    @param [out] forward_iterator metadata iterator

    @returns Status of the operation
      @retval false Success
      @retval true  Failure
  */

  static DEFINE_BOOL_METHOD(init, (my_h_keyring_keys_metadata_iterator *
                                   forward_iterator));

  /**
    Iterator deinitialization

    Note: forward_iterator should not be used after call to deinit

    @param [in, out] forward_iterator metadata iterator

    @returns Status of the operation
      @retval false Success
      @retval true  Failure
  */

  static DEFINE_BOOL_METHOD(
      deinit, (my_h_keyring_keys_metadata_iterator forward_iterator));

  /**
    Check validity of the iterator

    @param [in] forward_iterator metadata iterator

    @returns Validty of the iterator
      @retval true  Iterator is valid
      @retval false Iterator is invalid
  */
  static DEFINE_BOOL_METHOD(
      is_valid, (my_h_keyring_keys_metadata_iterator forward_iterator));

  /**
    Move iterator forward.

    @param [in,out] forward_iterator metadata iterator

    @returns Status of the operation
      @retval false Success - indicates that iterator is pointing to next entry
      @retval true  Failure - indicates that iterator has reached the end
  */

  static DEFINE_BOOL_METHOD(
      next, (my_h_keyring_keys_metadata_iterator forward_iterator));

  /**
    Fetch length metadata for current key pointed by iterator

    @param [in]  forward_iterator forward_iterator metadata iterator
    @param [out]  data_id_length   Length of data_id buffer
    @param [out]  auth_id_length   Length of auth_id buffer

    @returns Status of the operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(
      get_length, (my_h_keyring_keys_metadata_iterator forward_iterator,
                   size_t *data_id_length, size_t *auth_id_length));

  /**
    Fetch metadata for current key pointed by iterator

    @param [in]  forward_iterator forward_iterator metadata iterator
    @param [out] data_id          ID information of current data
    @param [in]  data_id_length   Length of data_id buffer
    @param [out] auth_id          Owner of the key
    @param [in]  auth_id_length   Length of auth_id buffer

    @returns Status of the operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(
      get, (my_h_keyring_keys_metadata_iterator forward_iterator, char *data_id,
            size_t data_id_length, char *auth_id, size_t auth_id_length));
};

}  // namespace service_definition
}  // namespace keyring_common

#define KEYRING_KEYS_METADATA_FORWARD_ITERATOR_IMPLEMENTOR(component_name)     \
  BEGIN_SERVICE_IMPLEMENTATION(component_name, keyring_keys_metadata_iterator) \
  keyring_common::service_definition::                                         \
      Keyring_keys_metadata_iterator_service_impl::init,                       \
      keyring_common::service_definition::                                     \
          Keyring_keys_metadata_iterator_service_impl::deinit,                 \
      keyring_common::service_definition::                                     \
          Keyring_keys_metadata_iterator_service_impl::is_valid,               \
      keyring_common::service_definition::                                     \
          Keyring_keys_metadata_iterator_service_impl::next,                   \
      keyring_common::service_definition::                                     \
          Keyring_keys_metadata_iterator_service_impl::get_length,             \
      keyring_common::service_definition::                                     \
          Keyring_keys_metadata_iterator_service_impl::get                     \
          END_SERVICE_IMPLEMENTATION()

#endif  // KEYRING_KEYS_METADATA_FORWARD_ITERATOR_SERVICE_IMPL_INCLUDED
