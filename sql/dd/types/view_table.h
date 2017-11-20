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

#ifndef DD__VIEW_TABLE_INCLUDED
#define DD__VIEW_TABLE_INCLUDED


#include "sql/dd/types/weak_object.h" // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_type;
class View;
class View_table_impl;

namespace tables {
  class View_table_usage;
}

///////////////////////////////////////////////////////////////////////////

class View_table : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();
  typedef View_table_impl Impl;

  typedef tables::View_table_usage cache_partition_table_type;

public:
  virtual ~View_table()
  { };

  /////////////////////////////////////////////////////////////////////////
  // View table catalog name.
  // XXX: do we need it now?
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &table_catalog() const = 0;
  virtual void set_table_catalog(const String_type &table_catalog) = 0;

  /////////////////////////////////////////////////////////////////////////
  // View table schema name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &table_schema() const = 0;
  virtual void set_table_schema(const String_type &table_schema) = 0;

  /////////////////////////////////////////////////////////////////////////
  // View table name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &table_name() const = 0;
  virtual void set_table_name(const String_type &table_name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Parent view.
  /////////////////////////////////////////////////////////////////////////

  virtual const View &view() const = 0;

  virtual View &view() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__VIEW_TABLE_INCLUDED
