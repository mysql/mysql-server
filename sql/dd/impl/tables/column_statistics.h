/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__COLUMN_STATISTICS_INCLUDED
#define DD_TABLES__COLUMN_STATISTICS_INCLUDED

#include "sql/dd/impl/types/entity_object_table_impl.h"
#include "sql/dd/object_id.h"                           // Object_id
#include "sql/dd/string_type.h"
#include "sql/dd/types/column_statistics.h"

namespace dd {

class Dictionary_object;
class Item_name_key;
class Object_key;
class Raw_record;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Column_statistics final : public Entity_object_table_impl
{
public:
  static const Column_statistics &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("column_statistics");
    return s_table_name;
  }

  enum enum_fields
  {
    FIELD_ID,
    FIELD_CATALOG_ID,
    FIELD_NAME,
    FIELD_SCHEMA_NAME,
    FIELD_TABLE_NAME,
    FIELD_COLUMN_NAME,
    FIELD_HISTOGRAM
  };

  Column_statistics();

  const String_type &name() const override
  { return Column_statistics::table_name(); }

  dd::Column_statistics *
  create_entity_object(const Raw_record &) const override;

  static bool update_object_key(Item_name_key *key, Object_id catalog_id,
                                const String_type &name);

  static Object_key *create_key_by_catalog_id(Object_id catalog_id);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__COLUMN_STATISTICS_INCLUDED
