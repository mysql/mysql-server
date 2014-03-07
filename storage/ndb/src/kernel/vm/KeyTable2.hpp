/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef KEY_TABLE2_HPP
#define KEY_TABLE2_HPP

#include <DLHashTable2.hpp>

#define JAM_FILE_ID 258


/**
 * KeyTable2 is DLHashTable2 with hardcoded Uint32 key named "key".
 */
template <class T, class U>
class KeyTable2 : public DLHashTable2<T, U> {
public:
  KeyTable2(ArrayPool<U>& pool) :
    DLHashTable2<T, U>(pool) {
  }

  bool find(Ptr<T>& ptr, const T& rec) const {
    return DLHashTable2<T, U>::find(ptr, rec);
  }

  bool find(Ptr<T>& ptr, Uint32 key) const {
    T rec;
    rec.key = key;
    return DLHashTable2<T, U>::find(ptr, rec);
  }
};

template <class T, class U>
class KeyTable2C : public KeyTable2<T, U> {
  Uint32 m_count;
public:
  KeyTable2C(ArrayPool<U>& pool) :
    KeyTable2<T, U>(pool), m_count(0) {
  }

  Uint32 get_count() const { return m_count; }
  
  bool seize(Ptr<T> & ptr) {
    if (KeyTable2<T, U>::seize(ptr))
    {
      m_count ++;
      return true;
    }
    return false;
  }

  void add(Ptr<T> & ptr) {
    KeyTable2<T, U>::add(ptr);
    m_count ++;
  }

  void remove(Ptr<T> & ptr, const T & key) {
    KeyTable2<T, U>::remove(ptr, key);
    if (ptr.i != RNIL)
    {
      assert(m_count);
      m_count --;
    }
  }

  void remove(Uint32 i) {
    KeyTable2<T, U>::remove(i);
    assert(m_count);
    m_count --;
  }

  void remove(Ptr<T> & ptr) {
    KeyTable2<T, U>::remove(ptr);
    assert(m_count);
    m_count --;
  }

  void removeAll() {
    KeyTable2<T, U>::removeAll();
    m_count = 0;
  }
  
  void release(Ptr<T> & ptr, const T & key) {
    KeyTable2<T, U>::release(ptr, key);
    if (ptr.i != RNIL)
    {
      assert(m_count);
      m_count --;
    }
  }

  void release(Uint32 i) {
    KeyTable2<T, U>::release(i);
    assert(m_count);
    m_count --;
  }

  void release(Ptr<T> & ptr) {
    KeyTable2<T, U>::release(ptr);
    assert(m_count);
    m_count --;
  }
};


#undef JAM_FILE_ID

#endif
