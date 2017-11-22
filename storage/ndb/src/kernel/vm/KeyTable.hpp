/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
/**
 * Using TT instead of T since VisualStudio2013 tries to access private
 * typedef of DLMHashTable instance!
 */
template <typename P, typename TT = typename P::Type>
class KeyTable : public DLHashTable<P, TT> {
public:
  KeyTable(P & pool) :
    DLHashTable<P, TT>(pool) {
  }

  bool find(Ptr<TT>& ptr, const TT& rec) const {
    return DLHashTable<P, TT>::find(ptr, rec);
  }

  bool find(Ptr<TT>& ptr, Uint32 key) const {
    TT rec;
    rec.key = key;
    return DLHashTable<P, TT>::find(ptr, rec);
  }
};

#undef JAM_FILE_ID

#endif
