/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__DICTIONARY_OBJECT_COLLECTION
#define DD__DICTIONARY_OBJECT_COLLECTION

#include "my_global.h"

#include "dd/iterator.h"                     // Iterator

#include <vector>                            // Vector

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_key;

///////////////////////////////////////////////////////////////////////////

template <typename Object_type>
class Dictionary_object_collection: public Iterator<const Object_type>
{
private:
  THD *m_thd;
  typedef std::vector<const Object_type*> Array;
  Array m_array;
  typename Array::const_iterator m_iterator;

public:
  Dictionary_object_collection(THD *thd): m_thd(thd)
  { } /* purecov: tested */

  bool fetch(const Object_key *object_key);

  ~Dictionary_object_collection();

  const Object_type *next()
  {
    if (m_iterator == m_array.end())
      return NULL;

    const Object_type *obj= *m_iterator;
    ++m_iterator;

    return obj;
  }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__DICTIONARY_OBJECT_COLLECTION
