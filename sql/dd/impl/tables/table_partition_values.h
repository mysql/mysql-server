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

#ifndef DD_TABLES__INDEX_PARTITION_VALUES_INCLUDED
#define DD_TABLES__INDEX_PARTITION_VALUES_INCLUDED

#include <string>

#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "dd/object_id.h"                    // dd::Object_id

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Table_partition_values : public Object_table_impl
{
public:
  static const Table_partition_values &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("table_partition_values");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_PARTITION_ID,
    FIELD_LIST_NUM,
    FIELD_COLUMN_NUM,
    FIELD_VALUE_UTF8,
    FIELD_MAX_VALUE
  };

public:
  Table_partition_values();

  virtual const String_type &name() const
  { return Table_partition_values::table_name(); }

public:
  static Object_key *create_key_by_partition_id(Object_id partition_id);

  static Object_key *create_primary_key(Object_id partition_id,
                                        int list_num,
                                        int column_num);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__INDEX_PARTITION_VALUES_INCLUDED
