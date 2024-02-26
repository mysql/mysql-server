/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
