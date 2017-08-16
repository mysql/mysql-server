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

#ifndef DD__VIEW_ROUTINE_IMPL_INCLUDED
#define DD__VIEW_ROUTINE_IMPL_INCLUDED

#include <sys/types.h>
#include <new>

#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/weak_object_impl.h" // dd::Weak_object_impl
#include "sql/dd/string_type.h"
#include "sql/dd/types/object_type.h"        // dd::Object_type
#include "sql/dd/types/view_routine.h"       // dd::View_routine

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_key;
class Object_table;
class Open_dictionary_tables_ctx;
class View;
class View_impl;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class View_routine_impl : public Weak_object_impl,
                          public View_routine
{
public:
  View_routine_impl();

  View_routine_impl(View_impl *view);

  View_routine_impl(const View_routine_impl &src, View_impl *parent);

  virtual ~View_routine_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return View_routine::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  virtual void debug_print(String_type &outb) const;

  void set_ordinal_position(uint)
  { }

  virtual uint ordinal_position() const
  { return -1; }

public:
  /////////////////////////////////////////////////////////////////////////
  // routine catalog.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &routine_catalog() const
  { return m_routine_catalog; }

  virtual void set_routine_catalog(const String_type &sf_catalog)
  { m_routine_catalog= sf_catalog; }

  /////////////////////////////////////////////////////////////////////////
  // routine schema.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &routine_schema() const
  { return m_routine_schema; }

  virtual void set_routine_schema(const String_type &sf_schema)
  { m_routine_schema= sf_schema; }

  /////////////////////////////////////////////////////////////////////////
  // routine name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &routine_name() const
  { return m_routine_name; }

  virtual void set_routine_name(const String_type &sf_name)
  { m_routine_name= sf_name; }

  /////////////////////////////////////////////////////////////////////////
  // view.
  /////////////////////////////////////////////////////////////////////////

  virtual const View &view() const;

  virtual View &view();

public:
  static View_routine_impl *restore_item(View_impl *view)
  {
    return new (std::nothrow) View_routine_impl(view);
  }

  static View_routine_impl *clone(const View_routine_impl &other,
                                  View_impl *view)
  {
    return new (std::nothrow) View_routine_impl(other, view);
  }

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  String_type m_routine_catalog;
  String_type m_routine_schema;
  String_type m_routine_name;

  // References to other objects
  View_impl *m_view;
};

///////////////////////////////////////////////////////////////////////////

class View_routine_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) View_routine_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__VIEW_ROUTINE_IMPL_INCLUDED
