/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "plugin/keyring/common/keys_container.h"

#include <assert.h>
#include <stddef.h>
#include <algorithm>

#include <mysqld_error.h>

using std::string;
using std::unique_ptr;

namespace keyring {

extern PSI_memory_key key_memory_KEYRING;

Keys_container::Keys_container(ILogger *logger)
    : keys_hash(new Keys_container::Key_hash(system_charset_info,
                                             key_memory_KEYRING)),
      logger(logger),
      keyring_io(nullptr) {}

Keys_container::~Keys_container() {
  if (keyring_io != nullptr) delete keyring_io;
}

bool Keys_container::init(IKeyring_io *keyring_io,
                          std::string keyring_storage_url) {
  this->keyring_io = keyring_io;
  this->keyring_storage_url = keyring_storage_url;
  keys_hash->clear();
  if (keyring_io->init(&this->keyring_storage_url) ||
      load_keys_from_keyring_storage()) {
    keys_hash->clear();
    return true;
  }
  return false;
}

// Keyring_io passed to this function should be already initialized
void Keys_container::set_keyring_io(IKeyring_io *keyring_io) {
  this->keyring_io = keyring_io;
}

std::string Keys_container::get_keyring_storage_url() {
  return keyring_storage_url;
}

void Keys_container::store_keys_metadata(IKey *key) {
  /* if key metadata not present store it */
  Key_metadata km(key->get_key_id(), key->get_user_id());
  keys_metadata.push_back(km);
}

bool Keys_container::store_key_in_hash(IKey *key) {
  // TODO: This can be written more succinctly with C++17's try_emplace.
  string signature = *key->get_key_signature();
  if (keys_hash->count(signature) != 0)
    return true;
  else {
    keys_hash->emplace(signature, unique_ptr<IKey>(key));
    store_keys_metadata(key);
    return false;
  }
}

bool Keys_container::store_key(IKey *key) {
  if (flush_to_backup() || store_key_in_hash(key)) return true;
  if (flush_to_storage(key, STORE_KEY)) {
    remove_key_from_hash(key);
    return true;
  }
  return false;
}

IKey *Keys_container::get_key_from_hash(IKey *key) {
  return find_or_nullptr(*keys_hash, *key->get_key_signature());
}

void Keys_container::allocate_and_set_data_for_key(
    IKey *key, std::string *source_key_type, uchar *source_key_data,
    size_t source_key_data_size) {
  key->set_key_type(source_key_type);
  uchar *key_data = keyring_malloc<uchar *>(source_key_data_size);
  memcpy(key_data, source_key_data, source_key_data_size);
  key->set_key_data(key_data, source_key_data_size);
}

IKey *Keys_container::fetch_key(IKey *key) {
  assert(key->get_key_data() == nullptr);
  assert(key->get_key_type_as_string()->empty());

  IKey *fetched_key = get_key_from_hash(key);

  if (fetched_key == nullptr) return nullptr;

  if (fetched_key->get_key_type_as_string()->empty()) return nullptr;

  allocate_and_set_data_for_key(key, fetched_key->get_key_type_as_string(),
                                fetched_key->get_key_data(),
                                fetched_key->get_key_data_size());
  return key;
}

bool Keys_container::remove_keys_metadata(IKey *key) {
  Key_metadata src(key->get_key_id(), key->get_user_id());
  auto it =
      std::find_if(keys_metadata.begin(), keys_metadata.end(),
                   [src](Key_metadata const &dest) {
                     return (*src.id == *dest.id && *src.user == *dest.user);
                   });
  if (it != keys_metadata.end()) {
    keys_metadata.erase(it);
    return false;
  }
  return true;
}

bool Keys_container::remove_key_from_hash(IKey *key) {
  auto it = keys_hash->find(*key->get_key_signature());
  if (it == keys_hash->end()) return true;
  it->second.release();  // Prevent erase from removing key from memory
  keys_hash->erase(it);
  remove_keys_metadata(key);
  return false;
}

bool Keys_container::remove_key(IKey *key) {
  IKey *fetched_key_to_delete = get_key_from_hash(key);
  if (fetched_key_to_delete == nullptr || flush_to_backup() ||
      remove_key_from_hash(fetched_key_to_delete))
    return true;
  if (flush_to_storage(fetched_key_to_delete, REMOVE_KEY)) {
    // reinsert the key
    store_key_in_hash(fetched_key_to_delete);
    return true;
  }
  // successfully removed the key from hash and flushed to disk, safely remove
  // the key
  delete fetched_key_to_delete;

  return false;
}

bool Keys_container::load_keys_from_keyring_storage() {
  bool was_error = false;
  ISerialized_object *serialized_keys = nullptr;
  was_error = keyring_io->get_serialized_object(&serialized_keys);
  while (was_error == false && serialized_keys != nullptr) {
    IKey *key_loaded = nullptr;
    while (serialized_keys->has_next_key()) {
      if (serialized_keys->get_next_key(&key_loaded) || key_loaded == nullptr ||
          key_loaded->is_key_valid() == false ||
          store_key_in_hash(key_loaded)) {
        was_error = true;
        delete key_loaded;
        break;
      }
      key_loaded = nullptr;
    }
    delete serialized_keys;
    serialized_keys = nullptr;
    if (was_error == false && keyring_io->has_next_serialized_object())
      was_error = keyring_io->get_serialized_object(&serialized_keys);
  }
  if (was_error)
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_LOAD_KEYRING_CONTENT);
  return was_error;
}

bool Keys_container::flush_to_storage(IKey *key, Key_operation operation) {
  ISerialized_object *serialized_object =
      keyring_io->get_serializer()->serialize(*keys_hash, key, operation);

  if (serialized_object == nullptr ||
      keyring_io->flush_to_storage(serialized_object)) {
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_FLUSH_KEYS_TO_KEYRING);
    delete serialized_object;
    return true;
  }
  delete serialized_object;
  return false;
}

bool Keys_container::flush_to_backup() {
  ISerialized_object *serialized_object =
      keyring_io->get_serializer()->serialize(*keys_hash, nullptr, NONE);

  if (serialized_object == nullptr ||
      keyring_io->flush_to_backup(serialized_object)) {
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_FLUSH_KEYS_TO_KEYRING_BACKUP);
    delete serialized_object;
    return true;
  }
  delete serialized_object;
  return false;
}

}  // namespace keyring
