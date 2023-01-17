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

#ifndef MYSQL_HASH_TO_BUFFER_SERIALIZER_H
#define MYSQL_HASH_TO_BUFFER_SERIALIZER_H

#include <memory>
#include <string>

#include "map_helpers.h"
#include "my_inttypes.h"
#include "plugin/keyring/buffer.h"
#include "plugin/keyring/common/i_keyring_key.h"
#include "plugin/keyring/common/i_serializer.h"

namespace keyring {
class Hash_to_buffer_serializer : public ISerializer {
 public:
  ISerialized_object *serialize(
      const collation_unordered_map<std::string, std::unique_ptr<IKey>>
          &keys_hash,
      IKey *key, const Key_operation operation) override;

  void set_memory_needed_for_buffer(size_t memory_needed_for_buffer) {
    this->memory_needed_for_buffer = memory_needed_for_buffer;
  }

 protected:
  size_t memory_needed_for_buffer;

  bool store_keys_in_buffer(
      const collation_unordered_map<std::string, std::unique_ptr<IKey>>
          &keys_hash,
      Buffer *buffer);
  bool store_key_in_buffer(const IKey *key, Buffer *buffer);
};
}  // namespace keyring

#endif  // MYSQL_HASH_TO_BUFFER_SERIALIZER_H
