/*
   Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements the functions declared in ndb_dd.h
#include "storage/ndb/plugin/ndb_dd.h"

// Using
#include "sql/dd/dd.h"
#include "sql/dd/impl/types/object_table_definition_impl.h"  // fs_name_case()
#include "sql/dd/properties.h"
#include "sql/dd/types/index.h"
#include "sql/dd/types/partition.h"
#include "sql/dd/types/partition_index.h"
#include "sql/dd/types/table.h"
#include "sql/sql_class.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/transaction.h"
#include "storage/ndb/plugin/ndb_dd_client.h"
#include "storage/ndb/plugin/ndb_dd_sdi.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_name_util.h"

bool ndb_sdi_serialize(THD *thd, const dd::Table *table_def,
                       const char *schema_name_str, dd::sdi_t &sdi) {
  const dd::String_type schema_name(schema_name_str);
  // Require the table to be visible, hidden by SE(like mysql.ndb_schema)
  // or else have temporary name
  DBUG_ASSERT(table_def->hidden() == dd::Abstract_table::HT_VISIBLE ||
              table_def->hidden() == dd::Abstract_table::HT_HIDDEN_SE ||
              ndb_name_is_temp(table_def->name().c_str()));

  // Make a copy of the table definition to allow it to
  // be modified before serialization
  std::unique_ptr<dd::Table> table_def_clone(table_def->clone());

  // Check that dd::Table::clone() properly clones the table definition
  // by comparing the serialized table def before and after clone()
  DBUG_ASSERT(ndb_dd_sdi_serialize(thd, *table_def, schema_name) ==
              ndb_dd_sdi_serialize(thd, *table_def_clone, schema_name));

  // Don't include the se_private_id in the serialized table def.
  table_def_clone->set_se_private_id(dd::INVALID_OBJECT_ID);

  // Don't include any se_private_data properties in the
  // serialized table def.
  table_def_clone->se_private_data().clear();

  sdi = ndb_dd_sdi_serialize(thd, *table_def_clone, schema_name);
  if (sdi.empty()) {
    return false;  // Failed to serialize
  }
  return true;  // OK
}

/*
  Workaround for BUG#25657041

  During inplace alter table, the table has a temporary
  tablename and is also marked as hidden. Since the temporary
  name and hidden status is part of the serialized table
  definition, there's a mismatch down the line when this is
  stored as extra metadata in the NDB dictionary.

  The workaround for now involves setting the table as a user
  visible table and restoring the original table name
*/

void ndb_dd_fix_inplace_alter_table_def(dd::Table *table_def,
                                        const char *proper_table_name) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("table_name: %s", table_def->name().c_str()));
  DBUG_PRINT("enter", ("proper_table_name: %s", proper_table_name));

  // Check that the proper_table_name is not a temporary name
  DBUG_ASSERT(!ndb_name_is_temp(proper_table_name));

  table_def->set_name(proper_table_name);
  table_def->set_hidden(dd::Abstract_table::HT_VISIBLE);
}

/**
  Update the version of the Schema object in DD. All the DDLs
  creating/altering a database will be associated with a unique counter
  value and the node id from which they originated in the ndb_schema table.
  These two values, the counter and node id, together form the version of
  the schema and are set in the se_private_data field of the Schema.

  @param thd                  Thread object
  @param schema_name          The name of the Schema to be updated.
  @param counter              The unique counter associated with the DDL that
                              created/altered the database.
  @param node_id              The node id in which the DDL originated.
  @param skip_commit          If set true, function will skip the commit,
                              disable auto rollback.
                              If set false, function will commit the changes.
                              (default)
  @return true        On success.
  @return false       On failure
*/
bool ndb_dd_update_schema_version(THD *thd, const char *schema_name,
                                  unsigned int counter, unsigned int node_id,
                                  bool skip_commit) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("Schema : %s, counter : %u, node_id : %u", schema_name,
                       counter, node_id));

  Ndb_dd_client dd_client(thd);

  if (!dd_client.mdl_lock_schema(schema_name, true)) {
    DBUG_PRINT("error", ("Failed to acquire exclusive locks on Schema : '%s'",
                         schema_name));
    return false;
  }

  if (!dd_client.update_schema_version(schema_name, counter, node_id)) {
    return false;
  }

  if (!skip_commit) {
    dd_client.commit();
  } else {
    dd_client.disable_auto_rollback();
  }

  return true;
}

bool ndb_dd_has_local_tables_in_schema(THD *thd, const char *schema_name,
                                       bool &tables_exist_in_database) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("Checking if schema '%s' has local tables", schema_name));

  Ndb_dd_client dd_client(thd);

  /* Lock the schema in DD */
  if (!dd_client.mdl_lock_schema(schema_name)) {
    DBUG_PRINT("error", ("Failed to MDL lock schema : '%s'", schema_name));
    return false;
  }

  /* Check if there are any local tables */
  if (!dd_client.have_local_tables_in_schema(schema_name,
                                             &tables_exist_in_database)) {
    DBUG_PRINT("error", ("Failed to check if the Schema '%s' has any tables",
                         schema_name));
    return false;
  }

  return true;
}

const std::string ndb_dd_fs_name_case(const dd::String_type &name) {
  char name_buf[NAME_LEN + 1];
  const std::string lc_name =
      dd::Object_table_definition_impl::fs_name_case(name, name_buf);
  return lc_name;
}
