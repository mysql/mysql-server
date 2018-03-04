/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/keyring/buffer.h"

#include <memory>

#include "my_dbug.h"
#include "plugin/keyring/common/keyring_key.h"

namespace keyring
{
  void Buffer::free()
  {
    if (data != NULL)
    {
      delete[] data;
      data= NULL;
    }
    mark_as_empty();
    DBUG_ASSERT(size == 0 && position == 0);
  }

  bool Buffer::get_next_key(IKey **key)
  {
    *key= NULL;

    std::unique_ptr<Key> key_ptr(new Key());
    size_t number_of_bytes_read_from_buffer = 0;
    if (data == NULL)
    {
      DBUG_ASSERT(size == 0);
      return TRUE;
    }
    if (key_ptr->load_from_buffer(data + position,
                                  &number_of_bytes_read_from_buffer,
                                  size - position))
      return TRUE;

    position += number_of_bytes_read_from_buffer;
    *key= key_ptr.release();
    return FALSE;
  }

  bool Buffer::has_next_key()
  {
    return position < size;
  }

  void Buffer::reserve(size_t memory_size)
  {
    DBUG_ASSERT(memory_size % sizeof(size_t) == 0); //make sure size is sizeof(size_t) aligned
    free();
    data= reinterpret_cast<uchar*>(new size_t[memory_size / sizeof(size_t)]);//force size_t alignment
    size= memory_size;
    if(data)
      memset(data, 0, size);
    position= 0;
  }
}
