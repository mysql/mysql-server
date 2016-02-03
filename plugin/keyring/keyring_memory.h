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

#ifndef MYSQL_KEYRING_MEMORY_H
#define MYSQL_KEYRING_MEMORY_H

#include <my_global.h>
#include <mysql/plugin_keyring.h>
#include <limits>
#include <memory>

namespace keyring {

  extern PSI_memory_key key_memory_KEYRING;

  template <class T>
  T keyring_malloc(size_t size)
  {
    void *allocated_memory= my_malloc(key_memory_KEYRING, size, MYF(MY_WME));
    return allocated_memory ? reinterpret_cast<T>(allocated_memory) : NULL;
  }

  class Keyring_alloc
  {
    public:
      static void *operator new(size_t size) throw ()
      {
        return keyring_malloc<void*>(size);
      }
      static void *operator new[](size_t size) throw ()
      {
        return keyring_malloc<void*>(size);
      }
      static void operator delete(void* ptr, std::size_t sz)
      {
          my_free(ptr);
      }
      static void operator delete[](void* ptr, std::size_t sz)
      {
          my_free(ptr);
      }
  };
} //namespace keyring
 
#endif //MYSQL_KEYRING_MEMORY_H
