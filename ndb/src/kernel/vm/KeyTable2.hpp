/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef KEY_TABLE2_HPP
#define KEY_TABLE2_HPP

#include <DLHashTable2.hpp>

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

#endif
