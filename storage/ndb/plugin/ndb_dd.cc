/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include "storage/ndb/plugin/ndb_dd_fk.h"
#include "storage/ndb/plugin/ndb_dd_sdi.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_fk_util.h"
#include "storage/ndb/plugin/ndb_name_util.h"
#include "storage/ndb/plugin/ndb_schema_dist_table.h"

bool ndb_sdi_serialize(THD *thd, const dd::Table *table_def,
                       const char *schema_name, dd::sdi_t &sdi) {
  // Require the table to be visible, hidden by SE(like mysql.ndb_schema)
  // or else have temporary name
  assert(table_def->hidden() == dd::Abstract_table::HT_VISIBLE ||
         table_def->hidden() == dd::Abstract_table::HT_HIDDEN_SE ||
         ndb_name_is_temp(table_def->name().c_str()));

  // Make a copy of the table definition to allow it to
  // be modified before serialization
  std::unique_ptr<dd::Table> table_def_clone(table_def->clone());

  // Check that dd::Table::clone() properly clones the table definition
  // by comparing the serialized table def before and after clone()
  assert(ndb_dd_sdi_serialize(thd, *table_def, schema_name) ==
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
  assert(!ndb_name_is_temp(proper_table_name));

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
  /* Convert the schema name to lower case on platforms that have
     lower_case_table_names set to 2 */
  const std::string dd_schema_name = ndb_dd_fs_name_case(schema_name);

  if (!dd_client.mdl_lock_schema_exclusive(dd_schema_name.c_str())) {
    DBUG_PRINT("error", ("Failed to acquire exclusive lock on schema '%s'",
                         schema_name));
    return false;
  }

  if (!dd_client.update_schema_version(dd_schema_name.c_str(), counter,
                                       node_id)) {
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
  /* Convert the schema name to lower case on platforms that have
     lower_case_table_names set to 2 */
  const std::string dd_schema_name = ndb_dd_fs_name_case(schema_name);

  /* Lock the schema in DD */
  if (!dd_client.mdl_lock_schema(dd_schema_name.c_str())) {
    DBUG_PRINT("error", ("Failed to acquire MDL on schema '%s'", schema_name));
    return false;
  }

  /* Check if there are any local tables */
  if (!dd_client.have_local_tables_in_schema(dd_schema_name.c_str(),
                                             &tables_exist_in_database)) {
    DBUG_PRINT("error", ("Failed to check if the schema '%s' has local tables",
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

bool ndb_dd_get_schema_uuid(THD *thd, dd::String_type *dd_schema_uuid) {
  DBUG_TRACE;
  Ndb_dd_client dd_client(thd);

  const char *schema_name = Ndb_schema_dist_table::DB_NAME.c_str();
  const char *table_name = Ndb_schema_dist_table::TABLE_NAME.c_str();

  // Lock the table for reading schema uuid
  if (!dd_client.mdl_lock_table(schema_name, table_name)) {
    DBUG_PRINT("error",
               ("Failed to lock `%s.%s` in DD.", schema_name, table_name));
    return false;
  }

  // Retrieve the schema uuid stored in the ndb_schema table in DD
  if (!dd_client.get_schema_uuid(dd_schema_uuid)) {
    DBUG_PRINT("error", ("Failed to read schema UUID from DD"));
    return false;
  }

  return true;
}

bool ndb_dd_update_schema_uuid(THD *thd, const std::string &ndb_schema_uuid) {
  DBUG_TRACE;
  Ndb_dd_client dd_client(thd);

  const char *schema_name = Ndb_schema_dist_table::DB_NAME.c_str();
  const char *table_name = Ndb_schema_dist_table::TABLE_NAME.c_str();

  // Acquire exclusive locks on the table
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
    DBUG_PRINT("error", ("Failed to acquire exclusive lock `%s.%s` in DD.",
                         schema_name, table_name));
    return false;
  }

  // Update the schema UUID in DD
  if (!dd_client.update_schema_uuid(ndb_schema_uuid.c_str())) {
    DBUG_PRINT("error", ("Failed to update schema uuid in DD."));
    return false;
  }

  // Commit the change into DD and return
  dd_client.commit();
  return true;
}

/**
  Extract all the foreign key constraint definitions on the given table from
  NDB and install them in the DD table.

  @param[out] dd_table_def    The DD table object on which the foreign keys
                              are to be defined.
  @param ndb                  The Ndb object.
  @param schema_name          The schema name of the NDB table
  @param ndb_table            The NDB table object from which the foreign key
                              definitions are to be extracted.

  @return true        On success.
  @return false       On failure
*/
bool ndb_dd_upgrade_foreign_keys(dd::Table *dd_table_def, Ndb *ndb,
                                 const char *schema_name,
                                 const NdbDictionary::Table *ndb_table) {
  DBUG_TRACE;

  // Retrieve the foreign key list
  Ndb_fk_list fk_list;
  if (!retrieve_foreign_key_list_from_ndb(ndb->getDictionary(), ndb_table,
                                          &fk_list)) {
    return false;
  }

  // Loop all foreign keys and add them to the dd table object
  for (const NdbDictionary::ForeignKey &ndb_fk : fk_list) {
    char child_schema_name[FN_REFLEN + 1];
    const char *child_table_name =
        fk_split_name(child_schema_name, ndb_fk.getChildTable());
    if (strcmp(child_schema_name, schema_name) != 0 ||
        strcmp(child_table_name, ndb_table->getName())) {
      // The FK is just referencing the table. Skip it.
      // It will be handled by the table on which it exists.
      continue;
    }

    // Add the foreign key to the DD table
    dd::Foreign_key *dd_fk_def = dd_table_def->add_foreign_key();

    // Open the parent table from NDB
    char parent_schema_name[FN_REFLEN + 1];
    const char *parent_table_name =
        fk_split_name(parent_schema_name, ndb_fk.getParentTable());
    if (strcmp(child_schema_name, parent_schema_name) == 0 &&
        strcmp(child_table_name, parent_table_name) == 0) {
      // Self referencing foreign key.
      // Use the child table as parent and update the foreign key information.
      if (!ndb_dd_fk_set_values_from_ndb(dd_fk_def, dd_table_def, ndb_fk,
                                         ndb_table, ndb_table,
                                         parent_schema_name)) {
        return false;
      }
    } else {
      Ndb_table_guard ndb_parent_table_guard(ndb, parent_schema_name,
                                             parent_table_name);
      const NdbDictionary::Table *ndb_parent_table =
          ndb_parent_table_guard.get_table();
      if (ndb_parent_table == nullptr) {
        DBUG_PRINT("error",
                   ("Unable to load table '%s.%s' from ndb. Error : %s",
                    parent_schema_name, parent_table_name,
                    ndb->getDictionary()->getNdbError().message));
        return false;
      }

      // Update the foreign key information
      if (!ndb_dd_fk_set_values_from_ndb(dd_fk_def, dd_table_def, ndb_fk,
                                         ndb_table, ndb_parent_table,
                                         parent_schema_name)) {
        return false;
      }
    }
  }
  return true;
}
