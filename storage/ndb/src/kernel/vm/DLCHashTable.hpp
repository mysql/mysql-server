/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DLC_HASHTABLE_HPP
#define DLC_HASHTABLE_HPP

#include <ndb_global.h>
#include "DLHashTable.hpp"

#define JAM_FILE_ID 257


// Adds "count" to DLHashTable
template <class T, class U = T>
class DLCHashTable : public DLHashTable<T, U> {
public:
  // Ctor
  DLCHashTable(ArrayPool<T> & thePool) :
    DLHashTable<T, U>(thePool),
    m_count(0)
  {}
  
  // Get count
  Uint32 count() const { return m_count; }

  // Redefine methods which do add or remove

  void add(Ptr<T>& ptr) {
    DLHashTable<T, U>::add(ptr);
    m_count++;
  }
  
  void remove(Ptr<T>& ptr, const T & key) {
    DLHashTable<T, U>::remove(ptr, key);
    m_count--;
  }

  void remove(Uint32 i) {
    DLHashTable<T, U>::remove(i);
    m_count--;
  }

  void remove(Ptr<T>& ptr) {
    DLHashTable<T, U>::remove(ptr);
    m_count--;
  }

  void removeAll() {
    DLHashTable<T, U>::removeAll();
    m_count = 0;
  }
  
  void release(Ptr<T>& ptr, const T & key) {
    DLHashTable<T, U>::release(ptr, key);
    m_count--;
  }

  void release(Uint32 i) {
    DLHashTable<T, U>::release(i);
    m_count--;
  }

  void release(Ptr<T>& ptr) {
    DLHashTable<T, U>::release(ptr);
    m_count--;
  }
  
private:
  Uint32 m_count;
};


#undef JAM_FILE_ID

#endif
