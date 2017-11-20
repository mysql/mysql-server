/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/tables/triggers.h"

#include <memory>
#include <new>

#include "my_dbug.h"
#include "sql/dd/impl/object_key.h"
#include "sql/dd/impl/raw/object_keys.h" // dd::Global_name_key
#include "sql/dd/impl/raw/raw_record.h" // dd::Raw_record
#include "sql/dd/impl/raw/raw_table.h" // dd::Raw_table
#include "sql/dd/impl/transaction_impl.h" // Transaction_ro
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/types/table.h"
#include "sql/handler.h"

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

Triggers::Triggers()
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
                         "name VARCHAR(64) NOT NULL "
                         "COLLATE utf8_general_ci");
  m_target_def.add_field(FIELD_EVENT_TYPE,
                         "FIELD_EVENT_TYPE",
                         "event_type ENUM('INSERT', 'UPDATE', 'DELETE') "
                         "NOT NULL");
  m_target_def.add_field(FIELD_TABLE_ID,
                         "FIELD_TABLE_ID",
                         "table_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_ACTION_TIMING,
                         "FIELD_ACTION_TIMING",
                         "action_timing ENUM('BEFORE', 'AFTER') NOT NULL");
  m_target_def.add_field(FIELD_ACTION_ORDER,
                         "FIELD_ACTION_ORDER",
                         "action_order INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_ACTION_STATEMENT,
                         "FIELD_ACTION_STATEMENT",
                         "action_statement LONGBLOB NOT NULL");
  m_target_def.add_field(FIELD_ACTION_STATEMENT_UTF8,
                         "FIELD_ACTION_STATEMENT_UTF8",
                         "action_statement_utf8 LONGTEXT NOT NULL");
  m_target_def.add_field(FIELD_CREATED,
                         "FIELD_CREATED",
                         "created TIMESTAMP(2) NOT NULL DEFAULT "
                         "CURRENT_TIMESTAMP(2) ON UPDATE "
                         "CURRENT_TIMESTAMP(2)");
  m_target_def.add_field(FIELD_LAST_ALTERED,
                         "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP(2) NOT NULL "
                         "DEFAULT CURRENT_TIMESTAMP(2)");
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
                         "'PAD_CHAR_TO_FULL_LENGTH') NOT NULL");
  m_target_def.add_field(FIELD_DEFINER,
                         "FIELD_DEFINER",
                         "definer VARCHAR(93) NOT NULL");
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
  m_target_def.add_index("UNIQUE KEY (schema_id, name)");
  m_target_def.add_index("UNIQUE KEY (table_id, event_type, "
                         "action_timing, action_order)");

  m_target_def.add_foreign_key("FOREIGN KEY (schema_id) "
                               "REFERENCES schemata(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (table_id) "
                               "REFERENCES tables(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (client_collation_id) "
                               "REFERENCES collations(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (connection_collation_id) "
                               "REFERENCES collations(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (schema_collation_id) "
                               "REFERENCES collations(id)");
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Triggers::create_key_by_schema_id(Object_id schema_id)
{
  const int INDEX_NO= 1;
  return new (std::nothrow) Parent_id_range_key(
                              INDEX_NO, FIELD_SCHEMA_ID, schema_id);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

Object_key *Triggers::create_key_by_table_id(Object_id table_id)
{
  const int INDEX_NO= 2;
  return new (std::nothrow) Parent_id_range_key(
                              INDEX_NO, FIELD_TABLE_ID, table_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Triggers::create_key_by_trigger_name(Object_id schema_id,
                                                 const char *trigger_name)
{
  return new (std::nothrow) Item_name_key(FIELD_SCHEMA_ID, schema_id,
                                          FIELD_NAME, trigger_name);
}

///////////////////////////////////////////////////////////////////////////

Object_id Triggers::read_table_id(const Raw_record &r)
{
  return r.read_uint(FIELD_TABLE_ID, -1);
}

///////////////////////////////////////////////////////////////////////////

bool Triggers::get_trigger_table_id(THD *thd,
                                    Object_id schema_id,
                                    const String_type &trigger_name,
                                    Object_id *oid)
{
  DBUG_ENTER("Triggers::get_trigger_table_id");

  Transaction_ro trx(thd, ISO_READ_COMMITTED);
  trx.otx.register_tables<dd::Table>();
  if (trx.otx.open_tables())
    DBUG_RETURN(true);

  DBUG_ASSERT(oid != nullptr);
  *oid= INVALID_OBJECT_ID;

  const std::unique_ptr<Object_key> key(
    create_key_by_trigger_name(schema_id, trigger_name.c_str()));

  Raw_table *table= trx.otx.get_table(table_name());
  DBUG_ASSERT(table != nullptr);

  // Find record by the object-key.
  std::unique_ptr<Raw_record> record;
  if (table->find_record(*key, record))
    DBUG_RETURN(true);

  if (record.get())
    *oid= read_table_id(*record.get());

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

} // namespace tables
} // namespace dd
