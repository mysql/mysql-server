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

#ifndef DD_TABLES__TABLES_INCLUDED
#define DD_TABLES__TABLES_INCLUDED

#include <string>

#include "my_inttypes.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_table_impl.h"
#include "sql/dd/object_id.h"                           // dd::Object_id
#include "sql/dd/string_type.h"
#include "sql/dd/types/abstract_table.h"

namespace dd {

class Item_name_key;
class Object_key;
class Open_dictionary_tables_ctx;
class Se_private_id_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Tables : public Entity_object_table_impl
{
public:
  static const Tables &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("tables");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_SCHEMA_ID,
    FIELD_NAME,
    FIELD_TYPE,
    FIELD_ENGINE,
    FIELD_MYSQL_VERSION_ID,
    FIELD_ROW_FORMAT,
    FIELD_COLLATION_ID,
    FIELD_COMMENT,
    FIELD_HIDDEN,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_SE_PRIVATE_ID,
    FIELD_TABLESPACE_ID,
    FIELD_PARTITION_TYPE,
    FIELD_PARTITION_EXPRESSION,
    FIELD_PARTITION_EXPRESSION_UTF8,
    FIELD_DEFAULT_PARTITIONING,
    FIELD_SUBPARTITION_TYPE,
    FIELD_SUBPARTITION_EXPRESSION,
    FIELD_SUBPARTITION_EXPRESSION_UTF8,
    FIELD_DEFAULT_SUBPARTITIONING,
    FIELD_CREATED,
    FIELD_LAST_ALTERED,
    FIELD_VIEW_DEFINITION,
    FIELD_VIEW_DEFINITION_UTF8,
    FIELD_VIEW_CHECK_OPTION,
    FIELD_VIEW_IS_UPDATABLE,
    FIELD_VIEW_ALGORITHM,
    FIELD_VIEW_SECURITY_TYPE,
    FIELD_VIEW_DEFINER,
    FIELD_VIEW_CLIENT_COLLATION_ID,
    FIELD_VIEW_CONNECTION_COLLATION_ID,
    FIELD_VIEW_COLUMN_NAMES
  };

public:
  Tables();

  virtual const String_type &name() const
  { return Tables::table_name(); }

  virtual Abstract_table *create_entity_object(
    const Raw_record &r) const;

public:
  static bool update_object_key(Item_name_key *key,
                                Object_id schema_id,
                                const String_type &table_name);

  static bool update_aux_key(Se_private_id_key *key,
                             const String_type &engine,
                             ulonglong se_private_id);

  static Object_key *create_se_private_key(const String_type &engine,
                                           Object_id se_private_id);

  static Object_key *create_key_by_schema_id(Object_id schema_id);

  static Object_key *create_key_by_tablespace_id(Object_id tablespace_id);

  static ulonglong read_se_private_id(const Raw_record &r);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__TABLES_INCLUDED
