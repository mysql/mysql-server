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

#ifndef DD_TABLES__FOREIGN_KEYS_INCLUDED
#define DD_TABLES__FOREIGN_KEYS_INCLUDED

#include "sql/dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "sql/dd/object_id.h"                // dd::Object_id
#include "sql/dd/string_type.h"

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Foreign_keys : public Object_table_impl
{
public:
  static const Foreign_keys &instance();

  enum enum_fields
  {
    FIELD_ID,
    FIELD_SCHEMA_ID,
    FIELD_TABLE_ID,
    FIELD_NAME,
    FIELD_UNIQUE_CONSTRAINT_NAME,
    FIELD_MATCH_OPTION,
    FIELD_UPDATE_RULE,
    FIELD_DELETE_RULE,
    FIELD_REFERENCED_TABLE_CATALOG,
    FIELD_REFERENCED_TABLE_SCHEMA,
    FIELD_REFERENCED_TABLE,
    FIELD_OPTIONS
  };

  enum enum_indexes
  {
    INDEX_PK_ID= static_cast<uint>(Common_index::PK_ID),
    INDEX_UK_SCHEMA_ID_NAME= static_cast<uint>(Common_index::UK_NAME),
    INDEX_UK_TABLE_ID_NAME,
    INDEX_K_REF_CATALOG_REF_SCHEMA_REF_TABLE
  };

  enum enum_foreign_keys
  {
    FK_SCHEMA_ID
  };

  Foreign_keys();

  static Object_key *create_key_by_table_id(Object_id table_id);

  static Object_key *create_key_by_referenced_name(
    const String_type &referenced_catalog,
    const String_type &referenced_schema,
    const String_type &referenced_table);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__FOREIGN_KEYS_INCLUDED
