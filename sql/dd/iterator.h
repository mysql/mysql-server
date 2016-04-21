/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__ITERATORS_INCLUDED
#define DD__ITERATORS_INCLUDED

namespace dd {

///////////////////////////////////////////////////////////////////////////

/**
  Interface to iterator object of type I.

  The class in implemented by following classes,
    dd::Collection::Collection_iterator
    dd::Object_table_iterator
    dd::System_view_name_iterator
*/
template <typename I>
class Iterator
{
public:
  typedef I Object_type;

  /**
    Retrieve next object in the collection.

    @return I* - Pointer to next object in collection.
    @return NULL - If there is no next object in collection.
  */
  virtual I *next() = 0;

  virtual ~Iterator<I>()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ITERATORS_INCLUDED
