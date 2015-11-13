/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

#include "dd/object_id.h"                               // dd::Object_id
#include "dd/impl/types/dictionary_object_table_impl.h" // dd::Dictionary_obj...
#include "dd/impl/types/object_table_impl.h"            // dd::Object_table_i...

DD_HEADER_BEGIN

namespace dd {

class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Tables : virtual public Dictionary_object_table_impl,
               virtual public Object_table_impl
{
public:
  static const Tables &instance()
  {
    static Tables s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("tables");
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
    FIELD_COLLATION_ID,
    FIELD_COMMENT,
    FIELD_HIDDEN,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_SE_PRIVATE_ID,
    FIELD_TABLESPACE_ID,
    FIELD_PARTITION_TYPE,
    FIELD_PARTITION_EXPRESSION,
    FIELD_DEFAULT_PARTITIONING,
    FIELD_SUBPARTITION_TYPE,
    FIELD_SUBPARTITION_EXPRESSION,
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
    FIELD_VIEW_CONNECTION_COLLATION_ID
  };

public:
  Tables()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(FIELD_ID,
                           "FIELD_ID",
                           "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(FIELD_SCHEMA_ID,
                           "FIELD_SCHEMA_ID",
                           "schema_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_NAME,
                           "FIELD_NAME",
                           "name VARCHAR(64) NOT NULL COLLATE " +
                           std::string(Object_table_definition_impl::
                                         fs_name_collation()->name));
    m_target_def.add_field(FIELD_TYPE,
                           "FIELD_TYPE",
                           "type ENUM('BASE TABLE', 'VIEW', 'SYSTEM VIEW')"
                           "NOT NULL");
    m_target_def.add_field(FIELD_ENGINE,
                           "FIELD_ENGINE",
                           "engine VARCHAR(64)");
    m_target_def.add_field(FIELD_MYSQL_VERSION_ID,
                           "FIELD_MYSQL_VERSION_ID",
                           "mysql_version_id INT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_COLLATION_ID,
                           "FIELD_COLLATION_ID",
                           "collation_id BIGINT UNSIGNED");
    m_target_def.add_field(FIELD_COMMENT,
                           "FIELD_COMMENT",
                           "comment VARCHAR(2048) NOT NULL");
    m_target_def.add_field(FIELD_HIDDEN,
                           "FIELD_HIDDEN",
                           "hidden BOOL NOT NULL");
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
                           "view_definer VARCHAR(77)");
    m_target_def.add_field(FIELD_VIEW_CLIENT_COLLATION_ID,
                           "FIELD_VIEW_CLIENT_COLLATION_ID",
                           "view_client_collation_id BIGINT UNSIGNED");
    m_target_def.add_field(FIELD_VIEW_CONNECTION_COLLATION_ID,
                           "FIELD_VIEW_CONNECTION_COLLATION_ID",
                           "view_connection_collation_id BIGINT UNSIGNED");

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

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Tables::table_name(); }

  virtual Dictionary_object *create_dictionary_object(
    const Raw_record &r) const;

public:
  static bool update_object_key(Item_name_key *key,
                                Object_id schema_id,
                                const std::string &table_name);

  static bool update_aux_key(Se_private_id_key *key,
                             const std::string &engine,
                             ulonglong se_private_id);

  static Object_key *create_se_private_key(
    const std::string &engine,
    ulonglong se_private_id);

  static Object_key *create_key_by_schema_id(
    Object_id schema_id);

  static bool max_se_private_id(
    Open_dictionary_tables_ctx *otx,
    const std::string &engine,
    ulonglong *max_id);

  static ulonglong read_se_private_id(const Raw_record &r);
};

///////////////////////////////////////////////////////////////////////////

}
}

DD_HEADER_END

#endif // DD_TABLES__TABLES_INCLUDED
