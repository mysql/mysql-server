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

#include "sql/dd/impl/tables/tables.h"

#include <new>
#include <string>

#include "mysql_com.h"
#include "sql/dd/dd.h"                     // dd::create_object
#include "sql/dd/impl/raw/object_keys.h"   // dd::Item_name_key
#include "sql/dd/impl/raw/raw_record.h"    // dd::Raw_record
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/types/abstract_table.h"
#include "sql/dd/types/table.h"
#include "sql/dd/types/view.h"             // dd::View
#include "sql/mysqld.h"
#include "sql/stateless_allocator.h"

namespace dd {
namespace tables {

const Tables &Tables::instance()
{
  static Tables *s_instance= new Tables();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Tables::Tables()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_SCHEMA_ID,
                         "FIELD_SCHEMA_ID",
                         "schema_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_TYPE,
                         "FIELD_TYPE",
                         "type ENUM('BASE TABLE', 'VIEW', 'SYSTEM VIEW')"
                         "NOT NULL");
  m_target_def.add_field(FIELD_ENGINE,
                         "FIELD_ENGINE",
                         "engine VARCHAR(64) NOT NULL "
                         "COLLATE utf8_general_ci");
  m_target_def.add_field(FIELD_MYSQL_VERSION_ID,
                         "FIELD_MYSQL_VERSION_ID",
                         "mysql_version_id INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_ROW_FORMAT,
                         "FIELD_ROW_FORMAT",
                         "row_format ENUM('Fixed', 'Dynamic', 'Compressed',"
                         "                'Redundant','Compact','Paged')");
  m_target_def.add_field(FIELD_COLLATION_ID,
                         "FIELD_COLLATION_ID",
                         "collation_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_COMMENT,
                         "FIELD_COMMENT",
                         "comment VARCHAR(2048) NOT NULL");
  m_target_def.add_field(FIELD_HIDDEN,
                         "FIELD_HIDDEN",
                         "hidden ENUM('Visible', 'System', 'SE', 'DDL') NOT NULL");
  m_target_def.add_field(FIELD_OPTIONS,
                         "FIELD_OPTIONS",
                         "options MEDIUMBLOB");
  m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                         "FIELD_SE_PRIVATE_DATA",
                         "se_private_data MEDIUMTEXT");
  m_target_def.add_field(FIELD_SE_PRIVATE_ID,
                         "FIELD_SE_PRIVATE_ID",
                         "se_private_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_TABLESPACE_ID,
                         "FIELD_TABLESPACE_ID",
                         "tablespace_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_PARTITION_TYPE,
                         "FIELD_PARTITION_TYPE",
                         "partition_type ENUM(\n"
                         "  'HASH','KEY_51','KEY_55',\n"
                         "  'LINEAR_HASH','LINEAR_KEY_51',\n"
                         "  'LINEAR_KEY_55','RANGE','LIST',\n"
                         "  'RANGE_COLUMNS','LIST_COLUMNS',\n"
                         "  'AUTO'"
                         ")");
  m_target_def.add_field(FIELD_PARTITION_EXPRESSION,
                         "FIELD_PARTITION_EXPRESSION",
                         "partition_expression VARCHAR(2048)");
  m_target_def.add_field(FIELD_PARTITION_EXPRESSION_UTF8,
                         "FIELD_PARTITION_EXPRESSION_UTF8",
                         "partition_expression_utf8 VARCHAR(2048)");
  m_target_def.add_field(FIELD_DEFAULT_PARTITIONING,
                         "FIELD_DEFAULT_PARTITIONING",
                         "default_partitioning ENUM('NO', 'YES', 'NUMBER')");
  m_target_def.add_field(FIELD_SUBPARTITION_TYPE,
                         "FIELD_SUBPARTITION_TYPE",
                         "subpartition_type ENUM(\n"
                         "  'HASH','KEY_51','KEY_55',\n"
                         "  'LINEAR_HASH',\n"
                         "  'LINEAR_KEY_51',\n"
                         "  'LINEAR_KEY_55'\n"
                         ")");
  m_target_def.add_field(FIELD_SUBPARTITION_EXPRESSION,
                         "FIELD_SUBPARTITION_EXPRESSION",
                         "subpartition_expression VARCHAR(2048)");
  m_target_def.add_field(FIELD_SUBPARTITION_EXPRESSION_UTF8,
                         "FIELD_SUBPARTITION_EXPRESSION_UTF8",
                         "subpartition_expression_utf8 VARCHAR(2048)");
  m_target_def.add_field(FIELD_DEFAULT_SUBPARTITIONING,
                         "FIELD_DEFAULT_SUBPARTITIONING",
                         "default_subpartitioning ENUM('NO', 'YES', "
                         "'NUMBER')");
  m_target_def.add_field(FIELD_CREATED,
                         "FIELD_CREATED",
                         "created TIMESTAMP NOT NULL\n"
                         " DEFAULT CURRENT_TIMESTAMP\n"
                         " ON UPDATE CURRENT_TIMESTAMP");
  m_target_def.add_field(FIELD_LAST_ALTERED,
                         "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP NOT NULL DEFAULT NOW()");
  m_target_def.add_field(FIELD_VIEW_DEFINITION,
                         "FIELD_VIEW_DEFINITION",
                         "view_definition LONGBLOB");
  m_target_def.add_field(FIELD_VIEW_DEFINITION_UTF8,
                         "FIELD_VIEW_DEFINITION_UTF8",
                         "view_definition_utf8 LONGTEXT");
  m_target_def.add_field(FIELD_VIEW_CHECK_OPTION,
                         "FIELD_VIEW_CHECK_OPTION",
                         "view_check_option ENUM('NONE', 'LOCAL', "
                                                 "'CASCADED')");
  m_target_def.add_field(FIELD_VIEW_IS_UPDATABLE,
                         "FIELD_VIEW_IS_UPDATABLE",
                         "view_is_updatable ENUM('NO', 'YES')");
  m_target_def.add_field(FIELD_VIEW_ALGORITHM,
                         "FIELD_VIEW_ALGORITHM",
                         "view_algorithm ENUM('UNDEFINED', 'TEMPTABLE', "
                                              "'MERGE')");
  m_target_def.add_field(FIELD_VIEW_SECURITY_TYPE,
                         "FIELD_VIEW_SECURITY_TYPE",
                         "view_security_type ENUM('DEFAULT', 'INVOKER', "
                                                  "'DEFINER')");
  m_target_def.add_field(FIELD_VIEW_DEFINER,
                         "FIELD_VIEW_DEFINER",
                         "view_definer VARCHAR(93)");
  m_target_def.add_field(FIELD_VIEW_CLIENT_COLLATION_ID,
                         "FIELD_VIEW_CLIENT_COLLATION_ID",
                         "view_client_collation_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_VIEW_CONNECTION_COLLATION_ID,
                         "FIELD_VIEW_CONNECTION_COLLATION_ID",
                         "view_connection_collation_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_VIEW_COLUMN_NAMES,
                         "FIELD_VIEW_COLUMN_NAMES",
                         "view_column_names LONGTEXT");

  m_target_def.add_index("PRIMARY KEY (id)");
  m_target_def.add_index("UNIQUE KEY (schema_id, name)");
  m_target_def.add_index("UNIQUE KEY (engine, se_private_id)");
  m_target_def.add_index("KEY(engine)");

  m_target_def.add_foreign_key("FOREIGN KEY (schema_id) "
                               "REFERENCES schemata(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (collation_id) "
                               "REFERENCES collations(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (tablespace_id) "
                               "REFERENCES tablespaces(id)");
}

///////////////////////////////////////////////////////////////////////////

Abstract_table *Tables::create_entity_object(
  const Raw_record &r) const
{
  enum_table_type table_type=
    static_cast<enum_table_type>(r.read_int(FIELD_TYPE));

  if (table_type == enum_table_type::BASE_TABLE)
    return dd::create_object<Table>();
  else
    return dd::create_object<View>();
}

///////////////////////////////////////////////////////////////////////////

bool Tables::update_object_key(Item_name_key *key,
                               Object_id schema_id,
                               const String_type &table_name)
{
  char buf[NAME_LEN + 1];
  key->update(FIELD_SCHEMA_ID, schema_id, FIELD_NAME,
              Object_table_definition_impl::fs_name_case(table_name, buf));
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tables::update_aux_key(Se_private_id_key *key,
                            const String_type &engine,
                            ulonglong se_private_id)
{
  const int SE_PRIVATE_ID_INDEX_ID= 2;
  key->update(SE_PRIVATE_ID_INDEX_ID,
              FIELD_ENGINE,
              engine,
              FIELD_SE_PRIVATE_ID,
              se_private_id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Tables::create_se_private_key(
  const String_type &engine,
  Object_id se_private_id)
{
  const int SE_PRIVATE_ID_INDEX_ID= 2;

  return
    new (std::nothrow) Se_private_id_key(
      SE_PRIVATE_ID_INDEX_ID,
      FIELD_ENGINE,
      engine,
      FIELD_SE_PRIVATE_ID,
      se_private_id);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

Object_key *Tables::create_key_by_schema_id(
  Object_id schema_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_SCHEMA_ID, schema_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Tables::create_key_by_tablespace_id(
  Object_id tablespace_id)
{
  // Use the index that is generated implicitly for the FK.
  const int TABLESPACE_INDEX_ID= 5;
  return new (std::nothrow) Parent_id_range_key(TABLESPACE_INDEX_ID,
                                                FIELD_TABLESPACE_ID,
                                                tablespace_id);
}

///////////////////////////////////////////////////////////////////////////

Object_id Tables::read_se_private_id(const Raw_record &r)
{
  return r.read_uint(Tables::FIELD_SE_PRIVATE_ID, -1);
}

}
}
