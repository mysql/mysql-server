/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include "keys_iterator.h"
#include "plugin/keyring/common/keyring.h"

namespace keyring {

/**
  Standard Constructor.
*/
Keys_iterator::Keys_iterator(ILogger *logger) : logger(logger) {}

/**
  This function initiates the internal pointers to point to
  beginning of the first element of keys metadata.
*/
void Keys_iterator::init() {
  key_metadata_list = keys->get_keys_metadata();
  key_metadata_list_iterator = key_metadata_list.begin();
}

/**
  Call to this function will advance internal pointers point to correct
  key data and return the key information. If there is no more keys to
  fetch then this function would return NULL.

  @param [out] km                  Refers to the fetched key from backend
                                   store. This will be NULL if there is no
                                   key to fetch.

  @return 0 Success
  @return 1 Failure

*/
bool Keys_iterator::get_key(Key_metadata **km) {
  /* if list is empty */
  if (key_metadata_list_iterator == key_metadata_list.end()) {
    *km = nullptr;
    return false;
  } else {
    std::unique_ptr<Key_metadata> key_meta(new Key_metadata());
    key_meta->id = key_metadata_list_iterator->id;
    key_meta->user = key_metadata_list_iterator->user;
    *km = key_meta.release();
  }
  ++key_metadata_list_iterator;
  return false;
}

/**
  Release all internal pointers.
*/
void Keys_iterator::deinit() {
  this->logger = nullptr;
  this->key_metadata_list.clear();
}

Keys_iterator::~Keys_iterator() = default;

}  // namespace keyring
