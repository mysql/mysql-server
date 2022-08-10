/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "plugin/keyring/hash_to_buffer_serializer.h"

#include <stddef.h>
#include <sys/types.h>

namespace keyring {
bool Hash_to_buffer_serializer::store_key_in_buffer(const IKey *key,
                                                    Buffer *buffer) {
  if (buffer->size < buffer->position + key->get_key_pod_size()) return true;
  key->store_in_buffer(buffer->data, &(buffer->position));
  return false;
}

bool Hash_to_buffer_serializer::store_keys_in_buffer(
    const collation_unordered_map<std::string, std::unique_ptr<IKey>>
        &keys_hash,
    Buffer *buffer) {
  for (const auto &key_and_value : keys_hash) {
    if (store_key_in_buffer(key_and_value.second.get(), buffer)) return true;
  }
  return false;
}

ISerialized_object *Hash_to_buffer_serializer::serialize(
    const collation_unordered_map<std::string, std::unique_ptr<IKey>>
        &keys_hash,
    IKey *key, const Key_operation operation) {
  size_t memory_needed_for_buffer_after_operation = memory_needed_for_buffer;

  switch (operation) {
    case STORE_KEY:
      memory_needed_for_buffer_after_operation += key->get_key_pod_size();
      break;
    case REMOVE_KEY:
      memory_needed_for_buffer_after_operation -= key->get_key_pod_size();
      break;
    case NONE:
    case ROTATE:
      break;
  }

  Buffer *buffer = new Buffer(memory_needed_for_buffer_after_operation);
  buffer->set_key_operation(operation);
  if (store_keys_in_buffer(keys_hash, buffer)) {
    delete buffer;
    return nullptr;
  }
  return buffer;
}
}  // namespace keyring
