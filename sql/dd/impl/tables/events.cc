/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/events.h"

#include <new>

#include "dd/impl/raw/object_keys.h"   // dd::Global_name_key
#include "dd/impl/raw/raw_record.h"    // dd::Raw_record
#include "dd/impl/types/event_impl.h"  // dd::Event_impl
#include "dd/impl/types/object_table_definition_impl.h"
#include "m_ctype.h"
#include "m_string.h"
#include "mysql_com.h"

namespace dd {
class Dictionary_object;
}  // namespace dd

namespace dd {
namespace tables {

const Events &Events::instance()
{
  static Events *s_instance= new Events();
  return *s_instance;
}

Events::Events()
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
                         "name VARCHAR(64) NOT NULL COLLATE utf8_general_ci");
  m_target_def.add_field(FIELD_DEFINER,
                         "FIELD_DEFINER",
                         "definer VARCHAR(93) NOT NULL");
  m_target_def.add_field(FIELD_TIME_ZONE,
                         "FIELD_TIME_ZONE",
                         "time_zone VARCHAR(64) NOT NULL");
  m_target_def.add_field(FIELD_DEFINITION,
                         "FIELD_DEFINITION",
                         "definition LONGBLOB NOT NULL");
  m_target_def.add_field(FIELD_DEFINITION_UTF8,
                         "FIELD_DEFINITION_UTF8",
                         "definition_utf8 LONGTEXT NOT NULL");
  m_target_def.add_field(FIELD_EXECUTE_AT,
                         "FIELD_EXECUTE_AT",
                         "execute_at DATETIME");
  m_target_def.add_field(FIELD_INTERVAL_VALUE,
                         "FIELD_INTERVAL_VALUE",
                         "interval_value INT");
  m_target_def.add_field(FIELD_INTERVAL_FIELD,
                         "FIELD_INTERVAL_FIELD",
                         "interval_field ENUM('YEAR','QUARTER','MONTH','DAY','HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR','DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND','DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND','SECOND_MICROSECOND')");
  m_target_def.add_field(FIELD_SQL_MODE,
                         "FIELD_SQL_MODE",
                         "sql_mode SET( \n"
                         "'REAL_AS_FLOAT',\n"
                         "'PIPES_AS_CONCAT',\n"
                         "'ANSI_QUOTES',\n"
                         "'IGNORE_SPACE',\n"
                         "'NOT_USED',\n"
                         "'ONLY_FULL_GROUP_BY',\n"
                         "'NO_UNSIGNED_SUBTRACTION',\n"
                         "'NO_DIR_IN_CREATE',\n"
                         "'POSTGRESQL',\n"
                         "'ORACLE',\n"
                         "'MSSQL',\n"
                         "'DB2',\n"
                         "'MAXDB',\n"
                         "'NO_KEY_OPTIONS',\n"
                         "'NO_TABLE_OPTIONS',\n"
                         "'NO_FIELD_OPTIONS',\n"
                         "'MYSQL323',\n"
                         "'MYSQL40',\n"
                         "'ANSI',\n"
                         "'NO_AUTO_VALUE_ON_ZERO',\n"
                         "'NO_BACKSLASH_ESCAPES',\n"
                         "'STRICT_TRANS_TABLES',\n"
                         "'STRICT_ALL_TABLES',\n"
                         "'NO_ZERO_IN_DATE',\n"
                         "'NO_ZERO_DATE',\n"
                         "'INVALID_DATES',\n"
                         "'ERROR_FOR_DIVISION_BY_ZERO',\n"
                         "'TRADITIONAL',\n"
                         "'NO_AUTO_CREATE_USER',\n"
                         "'HIGH_NOT_PRECEDENCE',\n"
                         "'NO_ENGINE_SUBSTITUTION',\n"
                         "'PAD_CHAR_TO_FULL_LENGTH',\n"
                         "'TIME_TRUNCATE_FRACTIONAL') NOT NULL");
  m_target_def.add_field(FIELD_STARTS,
                         "FIELD_STARTS",
                         "starts DATETIME");
  m_target_def.add_field(FIELD_ENDS,
                         "FIELD_ENDS",
                         "ends DATETIME");
  m_target_def.add_field(FIELD_STATUS,
                         "FIELD_STATUS",
                         "status ENUM('ENABLED', 'DISABLED', 'SLAVESIDE_DISABLED') NOT NULL");
  m_target_def.add_field(FIELD_ON_COMPLETION,
                         "FIELD_ON_COMPLETION",
                         "on_completion ENUM('DROP', 'PRESERVE') NOT NULL");
  m_target_def.add_field(FIELD_CREATED,
                         "FIELD_CREATED",
                         "created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
  m_target_def.add_field(FIELD_LAST_ALTERED,
                         "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP NOT NULL DEFAULT NOW()");
  m_target_def.add_field(FIELD_LAST_EXECUTED,
                         "FIELD_LAST_EXECUTED",
                         "last_executed DATETIME");
  m_target_def.add_field(FIELD_COMMENT,
                         "FIELD_COMMENT",
                         "comment VARCHAR(2048) NOT NULL");
  m_target_def.add_field(FIELD_ORIGINATOR,
                         "FIELD_ORIGINATOR",
                         "originator INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_CLIENT_COLLATION_ID,
                         "FIELD_CLIENT_COLLATION_ID",
                         "client_collation_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_CONNECTION_COLLATION_ID,
                         "FIELD_CONNECTION_COLLATION_ID",
                         "connection_collation_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_SCHEMA_COLLATION_ID,
                         "FIELD_SCHEMA_COLLATION_ID",
                         "schema_collation_id BIGINT UNSIGNED NOT NULL");

  m_target_def.add_index("PRIMARY KEY(id)");
  m_target_def.add_index("UNIQUE KEY(schema_id, name)");

  m_target_def.add_foreign_key("FOREIGN KEY (schema_id) "
                               "REFERENCES schemata(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (client_collation_id) "
                               "REFERENCES collations(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (connection_collation_id) "
                               "REFERENCES collations(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (schema_collation_id) "
                               "REFERENCES collations(id)");
}


///////////////////////////////////////////////////////////////////////////

bool Events::update_object_key(Item_name_key *key,
                               Object_id schema_id,
                               const String_type &event_name)
{
  char buf[NAME_LEN + 1];
  my_stpcpy(buf, event_name.c_str());
  my_casedn_str(&my_charset_utf8_tolower_ci, buf);

  key->update(FIELD_SCHEMA_ID, schema_id,
              FIELD_NAME, buf);
  return false;
}

///////////////////////////////////////////////////////////////////////////

Dictionary_object *Events::create_dictionary_object(const Raw_record &) const
{
  return new (std::nothrow) Event_impl();
}

///////////////////////////////////////////////////////////////////////////

Object_key *Events::create_key_by_schema_id(Object_id schema_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_SCHEMA_ID, schema_id);
}

///////////////////////////////////////////////////////////////////////////

} // namespace tables
} // namespace dd
