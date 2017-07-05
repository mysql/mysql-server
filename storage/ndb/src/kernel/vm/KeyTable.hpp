/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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
class KeyTable : public DLHashTable<P, T> {
public:
  KeyTable(P & pool) :
    DLHashTable<P, T>(pool) {
  }

  bool find(Ptr<T>& ptr, const T& rec) const {
    return DLHashTable<P, T>::find(ptr, rec);
  }

  bool find(Ptr<T>& ptr, Uint32 key) const {
    T rec;
    rec.key = key;
    return DLHashTable<P, T>::find(ptr, rec);
  }
};

#undef JAM_FILE_ID

#endif
