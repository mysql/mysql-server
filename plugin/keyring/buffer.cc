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

#include "buffer.h"
#include "keyring_key.h"

namespace keyring
{
  inline void Buffer::free()
  {
    if (data != NULL)
    {
      delete[] data;
      data= NULL;
    }
    mark_as_empty();
    DBUG_ASSERT(size == 0 && position == 0);
  }

  my_bool Buffer::get_next_key(IKey **key)
  {
    *key= NULL;

    boost::movelib::unique_ptr<Key> key_ptr(new Key());
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

  my_bool Buffer::has_next_key()
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
