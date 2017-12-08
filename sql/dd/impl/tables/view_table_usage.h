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

#ifndef DD_TABLES__VIEW_TABLE_USAGE_INCLUDED
#define DD_TABLES__VIEW_TABLE_USAGE_INCLUDED

#include "sql/dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "sql/dd/object_id.h"                // dd::Object_id
#include "sql/dd/string_type.h"

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class View_table_usage : public Object_table_impl
{
public:
  static const View_table_usage &instance();

  enum enum_fields
  {
    FIELD_VIEW_ID,
    FIELD_TABLE_CATALOG,
    FIELD_TABLE_SCHEMA,
    FIELD_TABLE_NAME
  };

  enum enum_indexes
  {
    INDEX_PK_VIEW_ID_TABLE_CATALOG_TABLE_SCHEMA_TABLE_NAME,
    INDEX_K_TABLE_CATALOG_TABLE_SCHEMA_TABLE_NAME
  };

  enum enum_foreign_keys
  {
    FK_VIEW_ID
  };

  View_table_usage();

  static Object_key *create_key_by_view_id(Object_id view_id);

  static Object_key *create_primary_key(Object_id view_id,
                                        const String_type &table_catalog,
                                        const String_type &table_schema,
                                        const String_type &table_name);

  static Object_key *create_key_by_name(const String_type &table_catalog,
                                        const String_type &table_schema,
                                        const String_type &table_name);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__VIEW_TABLE_USAGE_INCLUDED
