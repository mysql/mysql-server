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

#ifndef DD__OBJECT_TABLE_REGISTRY_INCLUDED
#define DD__OBJECT_TABLE_REGISTRY_INCLUDED

#include "my_global.h"

#include <vector>

namespace dd {

class Object_table;
template <typename I> class Iterator;

///////////////////////////////////////////////////////////////////////////

typedef std::vector<const Object_table *> Object_table_array;

///////////////////////////////////////////////////////////////////////////

class Object_table_registry
{
public:
  static Object_table_registry *instance()
  {
    static Object_table_registry s_instance;
    return &s_instance;
  }

  static bool init();

public:
  void add_type(const Object_table &t)
  { return m_types.push_back(&t); }

  Iterator<const Object_table> *types();

private:
  Object_table_registry()
  { }

  ~Object_table_registry()
  { }

private:
  Object_table_array m_types;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__OBJECT_TABLE_REGISTRY_INCLUDED
