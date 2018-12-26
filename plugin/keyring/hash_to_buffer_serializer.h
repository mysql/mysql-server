/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_HASH_TO_BUFFER_SERIALIZER_H
#define MYSQL_HASH_TO_BUFFER_SERIALIZER_H

#include "i_serializer.h"
#include "i_keyring_key.h"
#include "buffer.h"

namespace keyring
{
  class Hash_to_buffer_serializer : public ISerializer
  {
  public:
    ISerialized_object* serialize(HASH *keys_hash, IKey *key,
                                  const Key_operation operation);

    void set_memory_needed_for_buffer(size_t memory_needed_for_buffer)
    {
      this->memory_needed_for_buffer= memory_needed_for_buffer;
    }
  protected:
    size_t memory_needed_for_buffer;

    my_bool store_keys_in_buffer(HASH *keys_hash, Buffer *buffer);
    my_bool store_key_in_buffer(const IKey* key, Buffer *buffer);
  };
}

#endif //MYSQL_HASH_TO_BUFFER_SERIALIZER_H
