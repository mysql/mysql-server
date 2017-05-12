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

#ifndef DD_TABLES__TABLE_PARTITIONS_INCLUDED
#define DD_TABLES__TABLE_PARTITIONS_INCLUDED

#include <string>

#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "dd/object_id.h"                    // dd::Object_id
#include "my_inttypes.h"

class THD;

namespace dd {
  class Object_key;
  class Raw_record;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Table_partitions : public Object_table_impl
{
public:
  static const Table_partitions &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("table_partitions");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_TABLE_ID,
    FIELD_LEVEL,
    FIELD_NUMBER,
    FIELD_NAME,
    FIELD_ENGINE,
    FIELD_COMMENT,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_SE_PRIVATE_ID,
    FIELD_TABLESPACE_ID
  };

public:
  Table_partitions();

  virtual const String_type &name() const
  { return Table_partitions::table_name(); }

public:
  static Object_key *create_key_by_table_id(Object_id table_id);

  static ulonglong read_table_id(const Raw_record &r);

  static Object_key *create_se_private_key(
    const String_type &engine,
    Object_id se_private_id);

  static bool get_partition_table_id(
    THD *thd,
    const String_type &engine,
    ulonglong se_private_id,
    Object_id *oid);

};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__TABLE_PARTITIONS_INCLUDED
