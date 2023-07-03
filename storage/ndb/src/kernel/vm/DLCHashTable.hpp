/*
   Copyright (c) 2005, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DLC_HASHTABLE_HPP
#define DLC_HASHTABLE_HPP

#include <ndb_global.h>
#include "DLHashTable.hpp"

#define JAM_FILE_ID 257


// Adds "getCount" to DLHashTable
template <class P, class U = typename P::Type>
class DLCHashTable : public DLHashTable<P, U> {
  typedef typename P::Type T;
public:
  // Ctor
  DLCHashTable(P & thePool) :
    DLHashTable<P, U>(thePool),
    m_count(0)
  {}
  
  // Get count
  Uint32 getCount() const { return m_count; }

  // Redefine methods which do add or remove

  void add(Ptr<T>& ptr) {
    DLHashTable<P, U>::add(ptr);
    m_count++;
  }
  
  void remove(Ptr<T>& ptr, const T & key) {
    DLHashTable<P, U>::remove(ptr, key);
    m_count--;
  }

  void remove(Uint32 i) {
    DLHashTable<P, U>::remove(i);
    m_count--;
  }

  void remove(Ptr<T>& ptr) {
    DLHashTable<P, U>::remove(ptr);
    m_count--;
  }

  void removeAll() {
    DLHashTable<P, U>::removeAll();
    m_count = 0;
  }
  
  void release(Ptr<T>& ptr, const T & key) {
    DLHashTable<P, U>::release(ptr, key);
    m_count--;
  }

  void release(Uint32 i) {
    DLHashTable<P, U>::release(i);
    m_count--;
  }

  void release(Ptr<T>& ptr) {
    DLHashTable<P, U>::release(ptr);
    m_count--;
  }
  
private:
  Uint32 m_count;
};


#undef JAM_FILE_ID

#endif
