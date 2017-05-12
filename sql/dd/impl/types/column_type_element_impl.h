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

#ifndef DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED
#define DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED

#include <sys/types.h>
#include <new>
#include <string>

#include "dd/impl/types/weak_object_impl.h"   // dd::Weak_object_impl
#include "dd/sdi_fwd.h"
#include "dd/types/column_type_element.h"     // dd::Column_type_element
#include "dd/types/object_type.h"             // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column_impl;
class Open_dictionary_tables_ctx;
class Raw_record;
class Column;
class Object_key;
class Object_table;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Column_type_element_impl : public Weak_object_impl,
                                 public Column_type_element
{
public:
  Column_type_element_impl()
   :m_index(0)
  { }

  Column_type_element_impl(Column_impl *column)
   :m_index(0),
    m_column(column)
  { }

  Column_type_element_impl(const Column_type_element_impl &src,
                           Column_impl *parent);

  virtual ~Column_type_element_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Column_type_element::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  void set_ordinal_position(uint ordinal_position)
  { m_index= ordinal_position; }

  virtual uint ordinal_position() const
  { return index(); }

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

public:
  static Column_type_element_impl *restore_item(Column_impl *column)
  {
    return new (std::nothrow) Column_type_element_impl(column);
  }

  static Column_type_element_impl *clone(const Column_type_element_impl &other,
                                         Column_impl *column)
  {
    return new (std::nothrow) Column_type_element_impl(other, column);
  }

public:
  /////////////////////////////////////////////////////////////////////////
  // Name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &name() const
  { return m_name; }

  virtual void set_name(const String_type &name)
  { m_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // Column
  /////////////////////////////////////////////////////////////////////////

  virtual const Column &column() const;

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  virtual uint index() const
  { return m_index; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

public:
  virtual void debug_print(String_type &outb) const;

protected:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

protected:
  // Fields
  String_type m_name;
  uint m_index;

  // References to other objects
  Column_impl *m_column;
};

///////////////////////////////////////////////////////////////////////////

class Column_type_element_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Column_type_element_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED
