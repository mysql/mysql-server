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

#ifndef KEY_TABLE_HPP
#define KEY_TABLE_HPP

#include <DLHashTable.hpp>

#define JAM_FILE_ID 221


/**
 * KeyTable2 is DLHashTable2 with hardcoded Uint32 key named "key".
 */
template <typename P, typename T>
class KeyTableImpl : public DLHashTableImpl<P, T> {
public:
  KeyTableImpl(P & pool) :
    DLHashTableImpl<P, T>(pool) {
  }

  bool find(Ptr<T>& ptr, const T& rec) const {
    return DLHashTableImpl<P, T>::find(ptr, rec);
  }

  bool find(Ptr<T>& ptr, Uint32 key) const {
    T rec;
    rec.key = key;
    return DLHashTableImpl<P, T>::find(ptr, rec);
  }
};

// Specializations

template <typename T>
class KeyTable : public KeyTableImpl<ArrayPool<T>, T>
{
public:
  KeyTable(ArrayPool<T> & p) : KeyTableImpl<ArrayPool<T>, T>(p) {}
};


#undef JAM_FILE_ID

#endif
