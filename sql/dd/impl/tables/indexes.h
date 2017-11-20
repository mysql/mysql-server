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

#ifndef DD_TABLES__INDEXES_INCLUDED
#define DD_TABLES__INDEXES_INCLUDED

#include "sql/dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "sql/dd/object_id.h"                // dd::Object_id
#include "sql/dd/string_type.h"

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Indexes : public Object_table_impl
{
public:
  static const Indexes &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("indexes");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_TABLE_ID,
    FIELD_NAME,
    FIELD_TYPE,
    FIELD_ALGORITHM,
    FIELD_IS_ALGORITHM_EXPLICIT,
    FIELD_IS_VISIBLE,
    FIELD_IS_GENERATED,
    FIELD_HIDDEN,
    FIELD_ORDINAL_POSITION,
    FIELD_COMMENT,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_TABLESPACE_ID,
    FIELD_ENGINE
  };

public:
  Indexes();

  virtual const String_type &name() const
  { return Indexes::table_name(); }

public:
  static Object_key *create_key_by_table_id(Object_id table_id);

};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__INDEXES_INCLUDED
