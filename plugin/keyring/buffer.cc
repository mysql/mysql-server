<<<<<<< HEAD
/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

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

<<<<<<< HEAD
#include <assert.h>
=======
<<<<<<< HEAD
>>>>>>> pr/231
#include <memory>

#include "plugin/keyring/common/keyring_key.h"

namespace keyring {
void Buffer::free() {
  if (data != nullptr) {
    delete[] data;
<<<<<<< HEAD
    data = nullptr;
=======
    data = NULL;
=======
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
    assert(size == 0 && position == 0);
  }

  my_bool Buffer::get_next_key(IKey **key)
  {
    *key= NULL;

    boost::movelib::unique_ptr<Key> key_ptr(new Key());
    size_t number_of_bytes_read_from_buffer = 0;
    if (data == NULL)
    {
      assert(size == 0);
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
    assert(memory_size % sizeof(size_t) == 0); //make sure size is sizeof(size_t) aligned
    free();
    data= reinterpret_cast<uchar*>(new size_t[memory_size / sizeof(size_t)]);//force size_t alignment
    size= memory_size;
    if(data)
      memset(data, 0, size);
    position= 0;
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  mark_as_empty();
  assert(size == 0 && position == 0);
}

bool Buffer::get_next_key(IKey **key) {
  *key = nullptr;

  std::unique_ptr<Key> key_ptr(new Key());
  size_t number_of_bytes_read_from_buffer = 0;
  if (data == nullptr) {
    assert(size == 0);
    return true;
  }
  if (key_ptr->load_from_buffer(
          data + position, &number_of_bytes_read_from_buffer, size - position))
    return true;

  position += number_of_bytes_read_from_buffer;
  *key = key_ptr.release();
  return false;
}

bool Buffer::has_next_key() { return position < size; }

void Buffer::reserve(size_t memory_size) {
  assert(memory_size % sizeof(size_t) ==
         0);  // make sure size is sizeof(size_t) aligned
  free();
  data = reinterpret_cast<uchar *>(
      new size_t[memory_size / sizeof(size_t)]);  // force size_t alignment
  size = memory_size;
  if (data) memset(data, 0, size);
  position = 0;
}

}  // namespace keyring
