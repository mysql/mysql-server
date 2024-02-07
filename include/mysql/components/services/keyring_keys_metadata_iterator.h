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

#ifndef KEYRING_FORWARD_ITERATOR_INCLUDED
#define KEYRING_FORWARD_ITERATOR_INCLUDED

#include <mysql/components/service.h>

DEFINE_SERVICE_HANDLE(my_h_keyring_keys_metadata_iterator);

/**
  @ingroup group_keyring_component_services_inventory

  Keyring keys metadata iterator service provides APIs to create and use
  iterator to access metadata associated with all keys stored in keyring.

  @code
  bool print_keys_metadata() {
    char data_id[KEYRING_ITEM_BUFFER_SIZE] = "\0";
    char auth_id[KEYRING_ITEM_BUFFER_SIZE] = "\0";
    my_h_keyring_keys_metadata_iterator forward_iterator = nullptr;
    my_service<SERVICE_TYPE(keyring_keys_metadata_iterator)>
        keys_metadata_iterator("keyring_keys_metadata_iterator", m_reg_srv);
    if (!keys_metadata_iterator.is_valid()) {
      return true;
    }

    if (keys_metadata_iterator->init(&forward_iterator) == true) {
      return true;
    }

    bool ok = false;
    bool move_next = false;
    for (; keys_metadata_iterator->is_valid(forward_iterator) && move_next;
         move_next = !keys_metadata_iterator->next(forward_iterator)) {
      if (keys_metadata_iterator->get(
              forward_iterator, data_id, KEYRING_ITEM_BUFFER_SIZE, auth_id,
              KEYRING_ITEM_BUFFER_SIZE) == true) {
        ok = true;
        break;
      }
      std::cout << "Key name: " << data_id << ". User name: " << auth_id << "."
                << std::endl;
      memset(data_id, 0, KEYRING_ITEM_BUFFER_SIZE);
      memset(auth_id, 0, KEYRING_ITEM_BUFFER_SIZE);
    }

    if (keys_metadata_iterator->deinit(forward_iterator)) {
      return true;
    }
    return ok;
  }
  @endcode
*/

BEGIN_SERVICE_DEFINITION(keyring_keys_metadata_iterator)

/**
  Forward iterator initialization.

  This function allocates required memory for forward_iterator and initializes
  it. Caller should use deinit() to perform clean-up.

  An iterator may become invalid if content of keyring is changed.

  @param [out] forward_iterator metadata iterator

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/

DECLARE_BOOL_METHOD(init,
                    (my_h_keyring_keys_metadata_iterator * forward_iterator));

/**
  Iterator deinitialization

  @note forward_iterator should not be used after call to deinit

  @param [in, out] forward_iterator metadata iterator

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/

DECLARE_BOOL_METHOD(deinit,
                    (my_h_keyring_keys_metadata_iterator forward_iterator));

/**
  Check validity of the iterator

  @param [in] forward_iterator metadata iterator

  @returns Validty of the iterator
    @retval true  Success
    @retval false Failure
*/
DECLARE_BOOL_METHOD(is_valid,
                    (my_h_keyring_keys_metadata_iterator forward_iterator));

/**
  Move iterator forward.

  @param [in,out] forward_iterator metadata iterator

  @returns Status of the operation
    @retval false Success - indicates that iterator is pointing to next entry
    @retval true  Failure - Failure in moving iterator forward or next was
                            called after iterator reached the end.
*/

DECLARE_BOOL_METHOD(next,
                    (my_h_keyring_keys_metadata_iterator forward_iterator));

/**
  Fetch length metadata for current key pointed by iterator

  @param [in]  forward_iterator forward_iterator metadata iterator
  @param [out]  data_id_length   Length of data_id buffer
  @param [out]  auth_id_length   Length of auth_id buffer

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/
DECLARE_BOOL_METHOD(get_length,
                    (my_h_keyring_keys_metadata_iterator forward_iterator,
                     size_t *data_id_length, size_t *auth_id_length));
/**
  Fetch metadata for current key pointed by iterator

  Out buffers should be big enough to accommodate data + null terminating
  character

  @param [in]  forward_iterator forward_iterator metadata iterator
  @param [out] data_id          ID information of current data. Byte string.
  @param [in]  data_id_length   Length of data_id buffer
  @param [out] auth_id          Owner of the key. Byte string.
  @param [in]  auth_id_length   Length of auth_id buffer

  @returns Status of the operation
    @retval false Success
    @retval true  Failure
*/
DECLARE_BOOL_METHOD(get, (my_h_keyring_keys_metadata_iterator forward_iterator,
                          char *data_id, size_t data_id_length, char *auth_id,
                          size_t auth_id_length));

END_SERVICE_DEFINITION(keyring_keys_metadata_iterator)

#endif  // !KEYRING_FORWARD_ITERATOR_INCLUDED
