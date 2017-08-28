/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__PARAMETER_TYPE_ELEMENT_IMPL_INCLUDED
#define DD__PARAMETER_TYPE_ELEMENT_IMPL_INCLUDED

#include <sys/types.h>
#include <new>

#include "sql/dd/impl/types/weak_object_impl.h" // dd::Weak_object_impl
#include "sql/dd/string_type.h"
#include "sql/dd/types/parameter_type_element.h" // dd::Parameter_type_element

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_key;
class Object_table;
class Open_dictionary_tables_ctx;
class Parameter;
class Parameter_impl;
class Raw_record;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Parameter_type_element_impl : public Weak_object_impl,
                                    public Parameter_type_element
{
public:
  Parameter_type_element_impl()
   :m_index(0)
  { }

  Parameter_type_element_impl(Parameter_impl *parameter)
   :m_index(0),
    m_parameter(parameter)
  { }

  Parameter_type_element_impl(const Parameter_type_element_impl &src,
                              Parameter_impl *parent);

  virtual ~Parameter_type_element_impl()
  { }

public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  virtual const Object_table &object_table() const;

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  void set_ordinal_position(uint ordinal_position)
  { m_index= ordinal_position; }

  virtual uint ordinal_position() const
  { return index(); }

public:
  static Parameter_type_element_impl *restore_item(Parameter_impl *parameter)
  {
    return new (std::nothrow) Parameter_type_element_impl(parameter);
  }

  static Parameter_type_element_impl*
    clone(const Parameter_type_element_impl &other,
          Parameter_impl *parameter)
  {
    return new (std::nothrow) Parameter_type_element_impl(other, parameter);
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
  // Parameter
  /////////////////////////////////////////////////////////////////////////

  virtual const Parameter &parameter() const;

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  virtual uint index() const
  { return m_index; }

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
  Parameter_impl *m_parameter;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PARAMETER_TYPE_ELEMENT_IMPL_INCLUDED
