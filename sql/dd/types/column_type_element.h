/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__COLUMN_TYPE_ELEMENT_INCLUDED
#define DD__COLUMN_TYPE_ELEMENT_INCLUDED

#include "my_global.h"

#include "dd/types/weak_object.h"      // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Object_type;

///////////////////////////////////////////////////////////////////////////

class Column_type_element : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~Column_type_element()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Name
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &name() const = 0;
  virtual void set_name(const std::string &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Column
  /////////////////////////////////////////////////////////////////////////

  virtual const Column &column() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index
  /////////////////////////////////////////////////////////////////////////

  virtual uint index() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this type-element from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLUMN_TYPE_ELEMENT_INCLUDED
