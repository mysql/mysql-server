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

#ifndef DD__INDEX_ELEMENT_INCLUDED
#define DD__INDEX_ELEMENT_INCLUDED

#include "my_global.h"

#include "dd/types/weak_object.h"      // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Index;
class Object_type;

///////////////////////////////////////////////////////////////////////////

class Index_element : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  enum enum_index_element_order
  {
    ORDER_ASC= 1,
    ORDER_DESC
  };

public:
  virtual ~Index_element()
  { };

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  const Index &index() const
  { return const_cast<Index_element *> (this)->index(); }

  virtual Index &index() = 0;

  /////////////////////////////////////////////////////////////////////////
  // column.
  /////////////////////////////////////////////////////////////////////////

  const Column &column() const
  { return const_cast<Index_element *> (this)->column(); }

  virtual Column &column() = 0;

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // length.
  /////////////////////////////////////////////////////////////////////////

  virtual uint length() const = 0;
  virtual void set_length(uint length) = 0;
  virtual void set_length_null(bool is_null) = 0;

  /////////////////////////////////////////////////////////////////////////
  // order.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_index_element_order order() const = 0;
  virtual void set_order(enum_index_element_order order) = 0;

  /////////////////////////////////////////////////////////////////////////
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const = 0;
  virtual void set_hidden(bool hidden) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Make a clone of this object.
  /////////////////////////////////////////////////////////////////////////

  virtual class Index_element_impl *factory_clone() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this Index-element from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__INDEX_ELEMENT_INCLUDED
