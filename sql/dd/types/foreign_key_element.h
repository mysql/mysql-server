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

#ifndef DD__FOREIGN_KEY_ELEMENT_INCLUDED
#define DD__FOREIGN_KEY_ELEMENT_INCLUDED

#include "my_global.h"

#include "dd/types/weak_object.h"      // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Foreign_key;
class Object_type;

///////////////////////////////////////////////////////////////////////////

class Foreign_key_element : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~Foreign_key_element()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Foreign key.
  /////////////////////////////////////////////////////////////////////////

  const Foreign_key &foreign_key() const
  { return const_cast<Foreign_key_element *> (this)->foreign_key(); }

  virtual Foreign_key &foreign_key() = 0;

  /////////////////////////////////////////////////////////////////////////
  // column.
  /////////////////////////////////////////////////////////////////////////

  const Column &column() const
  { return const_cast<Foreign_key_element *> (this)->column(); }

  virtual Column &column() = 0;

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // referenced column name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &referenced_column_name() const = 0;
  virtual void referenced_column_name(const std::string &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this FK-element from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__FOREIGN_KEY_ELEMENT_INCLUDED
