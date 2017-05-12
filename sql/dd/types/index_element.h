/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/sdi_fwd.h"                // dd::Sdi_wcontext
#include "dd/types/weak_object.h"      // dd::Weak_object
#include "my_inttypes.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Index;
class Index_element_impl;
class Object_type;

///////////////////////////////////////////////////////////////////////////

class Index_element : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();
  typedef Index_element_impl Impl;

public:
  enum enum_index_element_order
  {
    ORDER_UNDEF= 1,
    ORDER_ASC,
    ORDER_DESC
  };

public:
  virtual ~Index_element()
  { };

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  virtual const Index &index() const = 0;

  virtual Index &index() = 0;

  /////////////////////////////////////////////////////////////////////////
  // column.
  /////////////////////////////////////////////////////////////////////////

  virtual const Column &column() const = 0;

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


  /**
    Converts *this into json.

    Converts all member variables that are to be included in the sdi
    into json by transforming them appropriately and passing them to
    the rapidjson writer provided.

    @param wctx opaque context for data needed by serialization
    @param w rapidjson writer which will perform conversion to json

  */

  virtual void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const = 0;


  /**
    Re-establishes the state of *this by reading sdi information from
    the rapidjson DOM subobject provided.

    Cross-references encountered within this object are tracked in
    sdictx, so that they can be updated when the entire object graph
    has been established.

    @param rctx stores book-keeping information for the
    deserialization process
    @param val subobject of rapidjson DOM containing json
    representation of this object
  */

  virtual bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) = 0;

  /**
    Check if index element (a.k.a. key part) is a prefix of the column used.
  */
  virtual bool is_prefix() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__INDEX_ELEMENT_INCLUDED
