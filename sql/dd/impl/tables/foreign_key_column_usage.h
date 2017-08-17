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

#ifndef DD_TABLES__FOREIGN_KEY_COLUMN_USAGE_INCLUDED
#define DD_TABLES__FOREIGN_KEY_COLUMN_USAGE_INCLUDED

#include "sql/dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "sql/dd/object_id.h"                // dd::Object_id
#include "sql/dd/string_type.h"

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Foreign_key_column_usage : public Object_table_impl
{
public:
  static const Foreign_key_column_usage &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("foreign_key_column_usage");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_FOREIGN_KEY_ID,
    FIELD_ORDINAL_POSITION,
    FIELD_COLUMN_ID,
    FIELD_REFERENCED_COLUMN_NAME
  };

public:
  Foreign_key_column_usage();

  virtual const String_type &name() const
  { return Foreign_key_column_usage::table_name(); }

public:
  static Object_key *create_key_by_foreign_key_id(Object_id fk_id);

  static Object_key *create_primary_key(
    Object_id fk_id, int ordinal_position);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__FOREIGN_KEY_COLUMN_USAGE_INCLUDED
