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

#ifndef MYSQL_BUFFER_H
#define MYSQL_BUFFER_H

#include "keyring_memory.h"

struct Buffer
{
  Buffer() : data(NULL)
  {
    mark_as_empty();
  }
  ~Buffer()
  {
    if(data != NULL)
      delete[] data;
  }
  inline void free()
  {
    if (data != NULL)
    {
      delete[] data;
      data= NULL;
    }
    mark_as_empty();
  }
  void reserve(size_t memory_size)
  {
    DBUG_ASSERT(memory_size % sizeof(size_t) == 0); //make sure size is sizeof(size_t) aligned
    data= reinterpret_cast<uchar*>(new size_t[memory_size / sizeof(size_t)]);//force size_t alignment
    size= memory_size;
    if(data)
      memset(data, 0, size);
    position= 0;
  }
  uchar *data;
  size_t size;
  size_t position;
private:
  Buffer(const Buffer&);
  Buffer& operator=(const Buffer&);

  inline void mark_as_empty()
  {
    size= position= 0;
  }
};

#endif //MYSQL_BUFFER_H
