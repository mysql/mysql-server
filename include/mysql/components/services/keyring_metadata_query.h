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

#ifndef KEYRING_METADATA_QUERY_SERVICE_INCLUDED
#define KEYRING_METADATA_QUERY_SERVICE_INCLUDED

#include <mysql/components/service.h>

DEFINE_SERVICE_HANDLE(my_h_keyring_component_metadata_iterator);

/**
  @ingroup group_keyring_component_services_inventory

  Keyring component status provides a way to check
  whether keyring is active or not.

  @code
  my_service<SERVICE_TYPE(keyring_component_status)> component_status(
      "keyring_component_status", m_reg_srv);
  if (!component_status.is_valid()) {
    return false;
  }
  return component_status->keyring_initialized();
  @endcode
*/

BEGIN_SERVICE_DEFINITION(keyring_component_status)

/**
  Returns status of the keyring component

  returns true if keyring initialized, false otherwise.
*/
DECLARE_BOOL_METHOD(is_initialized, ());

END_SERVICE_DEFINITION(keyring_component_status)

/**
  @ingroup group_keyring_component_services_inventory

  Keyring component metadata query service provides APIs
  to obtain component specific metadata in string format.
  Metadata would be in (key, value) pair.

  Implementor can decide what metadata should be exposed
  through these APIs.

  One of the primary consumer of this metadata is
  Performance schema table keyring_component_status.

  @code
  bool print_component_status() {
    bool next_status = false;
    my_h_keyring_component_metadata_iterator iterator = nullptr;
    my_service<SERVICE_TYPE(keyring_component_metadata_query)>
        metadata_query_service("keyring_component_metadata_query",
                               m_reg_srv);
    if (!metadata_query_service.valid()) {
      return false;
    }

    if (metadata_query_service->init(&iterator) == true) {
      return false;
    }

    bool ok = true;
    for (; metadata_query_service->is_valid(iterator) && next_status;
         next_status = metadata_query_service->next(iterator)) {
      size_t key_buffer_length = 0;
      size_t value_buffer_length = 0;
      if (metadata_query_service->get_length(iterator, &key_buffer_length,
                                             &value_buffer_length) == true) {
        ok = false;
        break;
      }

      std::unique_ptr<char[]> key_buffer(new char[key_buffer_length]);
      std::unique_ptr<char[]> value_buffer(new char[value_buffer_length]);

      if (key_buffer.get() == nullptr || value_buffer.get() == nullptr) break;

      memset(key_buffer.get(), 0, key_buffer_length);
      memset(value_buffer.get(), 0, value_buffer_length);

      if (metadata_query_service->get(
              iterator, key_buffer.get(), key_buffer_length, value_buffer.get(),
              value_buffer_length) == true) {
        ok = false;
        break;
      }

      std::cout << "Metadata key: " << key_buffer.get()
                << ". Metadata value: " << value_buffer.get.get() << std::endl;
    }

    if (metadata_query_service->deinit(iterator) {
      return false;
    }

    return ok;
  }
  @endcode
*/

BEGIN_SERVICE_DEFINITION(keyring_component_metadata_query)

/**
  Initialize metadata iterator. deinit should be called for clean-up.

  @param [out] metadata_iterator Metadata iterator handle

  @returns Status of iterator initialization
    @retval false Success
    @retval true  Failure
*/
DECLARE_BOOL_METHOD(init, (my_h_keyring_component_metadata_iterator *
                           metadata_iterator));

/**
  Deinitialize metadata iterator

  @param [in, out] metadata_iterator Metadata iterator handle

  @returns Status of iterator deinitialization
    @retval false Success
    @retval true  Failure
*/
DECLARE_BOOL_METHOD(
    deinit, (my_h_keyring_component_metadata_iterator metadata_iterator));

/**
  Check validity of iterator

  @param [in] metadata_iterator Metadata iterator handle

  @returns Validity of the the iterator
    @retval true  Iterator valid
    @retval false Iterator invalid
*/
DECLARE_BOOL_METHOD(
    is_valid, (my_h_keyring_component_metadata_iterator metadata_iterator));

/**
  Move iterator forward

  @param [in, out] metadata_iterator Metadata iterator handle

  @returns Status of operation
    @retval false Success
    @retval true  Failure. Either iterator already reached end position
                           or some other error was encountered.
*/
DECLARE_BOOL_METHOD(
    next, (my_h_keyring_component_metadata_iterator metadata_iterator));

/**
  Get length information about metadata key and value

  @param [in]  metadata_iterator   Metadata iterator handle
  @param [out] key_buffer_length   Length of the key buffer
  @param [out] value_buffer_length Length of the value buffer

  @returns Get length information about key and value
    @retval false Success
    @retval true  Failure
*/
DECLARE_BOOL_METHOD(get_length,
                    (my_h_keyring_component_metadata_iterator metadata_iterator,
                     size_t *key_buffer_length, size_t *value_buffer_length));

/**
  Get name and value of metadata at current position

  @param [in]  metadata_iterator   Metadata iterator handle
  @param [out] key_buffer          Output buffer for key. Byte string.
  @param [in]  key_buffer_length   Length of key buffer
  @param [out] value_buffer        Output buffer for value. Byte string.
  @param [in]  value_buffer_length Length of value buffer

  @returns Status of fetch operation
    @retval false Success
    @retval true  Failure
*/
DECLARE_BOOL_METHOD(get,
                    (my_h_keyring_component_metadata_iterator metadata_iterator,
                     char *key_buffer, size_t key_buffer_len,
                     char *value_buffer, size_t value_buffer_len));

END_SERVICE_DEFINITION(keyring_component_metadata_query)

#endif  // !KEYRING_METADATA_INCLUDED
