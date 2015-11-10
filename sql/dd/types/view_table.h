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

#ifndef DD__VIEW_TABLE_INCLUDED
#define DD__VIEW_TABLE_INCLUDED

#include "my_global.h"

#include "dd/types/weak_object.h"    // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class View;
class Object_type;

///////////////////////////////////////////////////////////////////////////

class View_table : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~View_table()
  { };

  /////////////////////////////////////////////////////////////////////////
  // View table catalog name.
  // XXX: do we need it now?
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &table_catalog() const = 0;
  virtual void set_table_catalog(const std::string &table_catalog) = 0;

  /////////////////////////////////////////////////////////////////////////
  // View table schema name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &table_schema() const = 0;
  virtual void set_table_schema(const std::string &table_schema) = 0;

  /////////////////////////////////////////////////////////////////////////
  // View table name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &table_name() const = 0;
  virtual void set_table_name(const std::string &table_name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Parent view.
  /////////////////////////////////////////////////////////////////////////

  const View &view() const
  { return const_cast<View_table *> (this)->view(); }

  virtual View &view() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this view-table from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__VIEW_TABLE_INCLUDED
