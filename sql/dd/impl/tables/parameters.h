/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__PARAMETERS_INCLUDED
#define DD_TABLES__PARAMETERS_INCLUDED

#include "sql/dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "sql/dd/object_id.h"                // dd::Object_id
#include "sql/dd/string_type.h"

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Parameters : public Object_table_impl
{
public:
  static const Parameters &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("parameters");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_ROUTINE_ID,
    FIELD_ORDINAL_POSITION,
    FIELD_MODE,
    FIELD_NAME,
    FIELD_DATA_TYPE,
    FIELD_DATA_TYPE_UTF8,
    FIELD_IS_ZEROFILL,
    FIELD_IS_UNSIGNED,
    FIELD_CHAR_LENGTH,
    FIELD_NUMERIC_PRECISION,
    FIELD_NUMERIC_SCALE,
    FIELD_DATETIME_PRECISION,
    FIELD_COLLATION_ID,
    FIELD_OPTIONS
  };

public:
  Parameters();

  virtual const String_type &name() const
  { return Parameters::table_name(); }

public:
  static Object_key *create_key_by_routine_id(Object_id routine_id);

  static Object_key *create_primary_key(Object_id routine_id,
                                        int ordinal_position);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__PARAMETERS_INCLUDED
