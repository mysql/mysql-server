/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef KEY_TABLE2_REF_HPP
#define KEY_TABLE2_REF_HPP

#include "KeyTable2.hpp"

#define JAM_FILE_ID 317


/**
 * KeyTable2 is DLHashTable2 with hardcoded Uint32 key named "key".
 */
template <class T, class U, class V>
class KeyTable2Ref
{
  KeyTable2<U, V>& m_ref;
public:
  KeyTable2Ref(KeyTable2<U, V>& ref) :m_ref(ref) {}
  
  bool find(Ptr<T>& ptr, Uint32 key) const {
    U rec;
    rec.key = key;
    Ptr<U> tmp;
    bool ret = m_ref.find(tmp, rec);
    ptr.i = tmp.i;
    ptr.p = static_cast<T*>(tmp.p);
    return ret;
  }
  
  bool seize(Ptr<T> & ptr) {
    Ptr<U> tmp;
    bool ret = m_ref.seize(tmp);
    ptr.i = tmp.i;
    ptr.p = static_cast<T*>(tmp.p);
    return ret;
  }
  
  void add(Ptr<T> & ptr) {
    Ptr<U> tmp;
    tmp.i = ptr.i;
    tmp.p = static_cast<U*>(ptr.p);
    m_ref.add(tmp);
  }
  
  void release(Ptr<T> & ptr) {
    Ptr<U> tmp;
    tmp.i = ptr.i;
    tmp.p = static_cast<U*>(ptr.p);
    m_ref.release(tmp);
  }
};


#undef JAM_FILE_ID

#endif
