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

#ifndef DD_TABLES__VIEW_ROUTINE_USAGE_INCLUDED
#define DD_TABLES__VIEW_ROUTINE_USAGE_INCLUDED

#include "sql/dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "sql/dd/object_id.h"                // dd::Object_id
#include "sql/dd/string_type.h"

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class View_routine_usage : virtual public Object_table_impl
{
public:
  static const View_routine_usage &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("view_routine_usage");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_VIEW_ID,
    FIELD_ROUTINE_CATALOG,
    FIELD_ROUTINE_SCHEMA,
    FIELD_ROUTINE_NAME
  };

public:
  View_routine_usage();

  virtual const String_type &name() const
  { return View_routine_usage::table_name(); }

public:
  static Object_key *create_key_by_view_id(Object_id view_id);

  static Object_key *create_primary_key(Object_id view_id,
                                        const String_type &routine_catalog,
                                        const String_type &routine_schema,
                                        const String_type &routine_name);

  static Object_key *create_key_by_name(const String_type &routine_catalog,
                                        const String_type &routine_schema,
                                        const String_type &routine_name);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__VIEW_ROUTINE_USAGE_INCLUDED
