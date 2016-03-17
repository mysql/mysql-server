/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "bootstrapper.h"

#include "handler.h"                          // dict_init_mode_t
#include "log.h"                              // sql_print_warning()
#include "mysql/mysql_lex_string.h"           // LEX_STRING
#include "sql_class.h"                        // THD
#include "sql_prepare.h"                      // Ed_connection
#include "transaction.h"                      // trans_rollback

#include "dd/dd.h"                            // dd::create_object
#include "dd/iterator.h"                      // dd::Iterator
#include "dd/properties.h"                    // dd::Properties
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/impl/collection_impl.h"          // dd::Collection_impl
#include "dd/impl/dictionary_impl.h"          // dd::Dictionary_impl
#include "dd/impl/system_registry.h"          // dd::System_tables
#include "dd/impl/cache/storage_adapter.h"    // dd::cache::Storage_adapter
#include "dd/impl/tables/character_sets.h"    // dd::tables::Character_sets
#include "dd/impl/tables/collations.h"        // dd::tables::Collations
#include "dd/impl/tables/version.h"           // dd::tables::Version
#include "dd/impl/types/plugin_table_impl.h"  // dd::Plugin_table_impl
#include "dd/impl/types/schema_impl.h"        // dd::Schema_impl
#include "dd/impl/types/table_impl.h"         // dd::Table_impl
#include "dd/impl/types/tablespace_impl.h"    // dd::Table_impl
#include "dd/types/object_table.h"            // dd::Object_table
#include "dd/types/object_table_definition.h" // dd::Object_table_definition
#include "dd/types/tablespace_file.h"         // dd::Tablespace_file

#include <vector>
#include <string>
#include <memory>

using namespace dd;

///////////////////////////////////////////////////////////////////////////

namespace {
  bootstrap::enum_bootstrap_stage bootstrap_stage=
          dd::bootstrap::BOOTSTRAP_NOT_STARTED;


// Helper function to do rollback or commit, depending on error.
bool end_transaction(THD *thd, bool error)
{
  if (error)
  {
    // Rollback the statement before we can rollback the real transaction.
    trans_rollback_stmt(thd);
    trans_rollback(thd);
  }
  else if (trans_commit_stmt(thd) || trans_commit(thd))
  {
    error= true;
    trans_rollback(thd);
  }

  // Close tables etc. and release MDL locks, regardless of error.
  thd->mdl_context.release_transactional_locks();
  return error;
}


// Execute a single SQL query.
bool execute_query(THD *thd, const std::string &q_buf)
{
  Ed_connection con(thd);
  LEX_STRING str;
  thd->make_lex_string(&str, q_buf.c_str(), q_buf.length(), false);
  return con.execute_direct(str);
}


/*
  Do the necessary DD-related initialization in the DDSE, and get the
  predefined tables and tablespaces.
*/
bool DDSE_dict_init(THD *thd,
                    dict_init_mode_t dict_init_mode,
                    uint version)
{
  handlerton *ddse= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  List<const Plugin_table> plugin_tables;
  List<const Plugin_tablespace> plugin_tablespaces;
  if (ddse->dict_init == nullptr ||
      ddse->dict_init(dict_init_mode, version,
                      &plugin_tables, &plugin_tablespaces))
    return true;

  /*
    Iterate over the table definitions, create Plugin_table instances,
    and add them to the System_tables registry. The Plugin_table instances
    will later be used to execute CREATE TABLE statements to actually
    create the tables.
  */
  List_iterator<const Plugin_table> table_it(plugin_tables);
  const Plugin_table *table= nullptr;
  while ((table= table_it++))
  {
    Plugin_table_impl *plugin_table= new (std::nothrow) Plugin_table_impl(
            table->get_name(),
            table->get_table_definition(),
            table->get_table_options(),
            Dictionary_impl::get_target_dd_version());
    System_tables::instance()->add(MYSQL_SCHEMA_NAME.str, table->get_name(),
                                   System_tables::Types::DDSE, plugin_table);
  }

  /*
    Iterate over the tablespace definitions, add the names to the
    System_tablespaces registry. Create dd::Tablespace objects and
    add them to the shared dd cache. The tablespaces are already created
    physically in the DDSE, so we only need to create the corresponding
    meta data.
  */
  List_iterator<const Plugin_tablespace> tablespace_it(plugin_tablespaces);
  const Plugin_tablespace *tablespace= nullptr;
  while ((tablespace= tablespace_it++))
  {
    // Add the name to the registry with the appropriate property.
    if (my_strcasecmp(system_charset_info, MYSQL_TABLESPACE_NAME.str,
                      tablespace->get_name()) == 0)
      System_tablespaces::instance()->add(tablespace->get_name(),
                                          System_tablespaces::DD);
    else
      System_tablespaces::instance()->add(tablespace->get_name(),
                                          System_tablespaces::PREDEFINED_DDSE);

    // Create the dd::Tablespace object.
    Tablespace *space= dd::create_object<Tablespace>();
    space->set_name(tablespace->get_name());
    space->set_options_raw(tablespace->get_options());
    space->set_se_private_data_raw(tablespace->get_se_private_data());
    space->set_engine(tablespace->get_engine());

    // Loop over the tablespace files, create dd::Tablespace_file objects.
    List <const Plugin_tablespace::Plugin_tablespace_file> files=
      tablespace->get_files();
    List_iterator<const Plugin_tablespace::Plugin_tablespace_file>
      file_it(files);
    const Plugin_tablespace::Plugin_tablespace_file *file= NULL;
    while ((file= file_it++))
    {
      Tablespace_file *space_file= space->add_file();
      space_file->set_filename(file->get_name());
      space_file->set_se_private_data_raw(file->get_se_private_data());
    }

    // Add to the shared cache with a temporary id, and make it sticky.
    thd->dd_client()->add_and_reset_id(space);
    thd->dd_client()->set_sticky(space, true);
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_PREPARED;

  return false;
}


// Initialize recovery in the DDSE.
bool DDSE_dict_recover(THD *thd,
                       dict_recovery_mode_t dict_recovery_mode,
                       uint version)
{
  handlerton *ddse= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->dict_recover == nullptr)
    return true;

  bool error= ddse->dict_recover(dict_recovery_mode, version);

  /*
    Commit when tablespaces have been initialized, since in that
    case, tablespace meta data is added.
  */
  if (dict_recovery_mode == DICT_RECOVERY_INITIALIZE_TABLESPACES)
    return end_transaction(thd, error);

  return error;
}


// Create and use the dictionary schema.
bool create_schema(THD *thd)
{
  return execute_query(thd, std::string("CREATE SCHEMA ") +
                            std::string(MYSQL_SCHEMA_NAME.str) +
                            std::string(" DEFAULT COLLATE ") +
                            std::string(default_charset_info->name))
    || execute_query(thd, std::string("USE ") +
                          std::string(MYSQL_SCHEMA_NAME.str));
}


/*
  Execute create table statements. This will create the meta data for
  the table. During initial start, the table will also be created
  physically in the DDSE.
*/
bool create_tables(THD *thd)
{
  // Iterate over DD tables, create tables.
  System_tables::Iterator it= System_tables::instance()->begin();

  // Make sure the version table is the first element.
#ifndef DBUG_OFF
  const Object_table_definition *version_def=
          dd::tables::Version::instance().table_definition(thd);
  DBUG_ASSERT(*it != nullptr);
  const Object_table_definition *first_def=
          (*it)->entity()->table_definition(thd);
  DBUG_ASSERT(version_def != nullptr && first_def != nullptr &&
          first_def == version_def);
#endif

  for (; it != System_tables::instance()->end(); ++it)
  {
    const Object_table_definition *table_def=
            (*it)->entity()->table_definition(thd);
    if (table_def == nullptr ||
        execute_query(thd, table_def->build_ddl_create_table()))
      return true;
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_CREATED;

  return false;
}


/*
  Acquire exclusive meta data locks for the DD schema, tablespace and
  table names.
*/
bool acquire_exclusive_mdl(THD *thd)
{
  // All MDL requests.
  MDL_request_list mdl_requests;

  // Prepare MDL request for the schema name.
  MDL_request schema_request;
  MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA,
                   MYSQL_SCHEMA_NAME.str,
                   "",
                   MDL_EXCLUSIVE,
                   MDL_TRANSACTION);
  mdl_requests.push_front(&schema_request);

  // Prepare MDL request for the tablespace name.
  MDL_request tablespace_request;
  MDL_REQUEST_INIT(&tablespace_request, MDL_key::TABLESPACE,
                   "",
                   MYSQL_TABLESPACE_NAME.str,
                   MDL_EXCLUSIVE,
                   MDL_TRANSACTION);
  mdl_requests.push_front(&tablespace_request);

  // Prepare MDL requests for all tables names.
  for (System_tables::Iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    MDL_request *table_request= new (thd->mem_root) MDL_request;
    if (table_request == NULL)
      return true;

    MDL_REQUEST_INIT(table_request, MDL_key::TABLE,
                     MYSQL_SCHEMA_NAME.str,
                     (*it)->entity()->name().c_str(),
                     MDL_EXCLUSIVE,
                     MDL_TRANSACTION);
    mdl_requests.push_front(table_request);
  }

  // Finally, acquire all the MDL locks.
  return (thd->mdl_context.acquire_locks(&mdl_requests,
                                         thd->variables.lock_wait_timeout));
}


/*
  Store the temporarily saved meta data into the DD tables. This must be
  done in several steps to keep the DD table meta data consistent with the
  ID of the system schema and tablespace. Consistency is necessary
  throughout this process because the DD tables are opened when writing
  or reading the meta data.
*/
bool store_meta_data(THD *thd)
{
  // Acquire exclusive meta data locks for the relevant DD objects.
  if (acquire_exclusive_mdl(thd))
    return true;

  // Acquire the system schema and tablespace.
  const Schema *sys_schema= nullptr;
  const Tablespace *sys_tspace= nullptr;
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  if (thd->dd_client()->acquire(std::string(MYSQL_SCHEMA_NAME.str),
                                &sys_schema) ||
      thd->dd_client()->acquire(std::string(MYSQL_TABLESPACE_NAME.str),
                                &sys_tspace))
    return end_transaction(thd, true);

  // Expected IDs after creating the scaffolding.
  DBUG_ASSERT(sys_schema != nullptr && sys_schema->id() == 1);
  DBUG_ASSERT(sys_tspace == nullptr || sys_tspace->id() == 1);

  /*
    Update the system schema object. This will store it and update the in
    memory copy.
  */
  std::unique_ptr<Schema_impl> sys_chema_clone(
          dynamic_cast<Schema_impl*>(sys_schema->clone()));
  sys_chema_clone->set_id(INVALID_OBJECT_ID);
  if (thd->dd_client()->update(&sys_schema,
                               static_cast<Schema*>(sys_chema_clone.get())))
     return end_transaction(thd, true);

  // Make sure the ID after storing is as expected.
  DBUG_ASSERT(sys_schema != nullptr && sys_schema->id() == 1);

  /*
    Update the system tablespace object. This will store it and update the in
    memory copy.
  */
  if (sys_tspace != nullptr)
  {
    std::unique_ptr<Tablespace_impl> sys_tspace_clone(
            dynamic_cast<Tablespace_impl*>(sys_tspace->clone()));
    sys_tspace_clone->set_id(INVALID_OBJECT_ID);
    if (thd->dd_client()->update(&sys_tspace,
                       static_cast<Tablespace*>(sys_tspace_clone.get())))
       return end_transaction(thd, true);

    // Make sure the ID after storing is as expected.
    DBUG_ASSERT(sys_tspace->id() == 1);
  }

  /*
    Then, we can store the table objects. This is done by setting the ID to
    INVALID and doing an update. This will both store the object persistently
    and update the cached object.
  */
  for (System_tables::Iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    const dd::Table *sys_table= nullptr;
    if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                  (*it)->entity()->name(), &sys_table))
      return end_transaction(thd, true);

    DBUG_ASSERT(sys_table->schema_id() == sys_schema->id());
    DBUG_ASSERT(sys_tspace == nullptr ||
                sys_table->tablespace_id() == sys_tspace->id());

    std::unique_ptr<Table_impl> sys_table_clone(
            dynamic_cast<Table_impl*>(sys_table->clone()));
#ifndef DBUG_OFF
    Object_id old_id= sys_table->id();
#endif
    // We must set the ID to INVALID to enable storing the object.
    sys_table_clone->set_id(INVALID_OBJECT_ID);
    if (thd->dd_client()->update(&sys_table,
                                 static_cast<Table*>(sys_table_clone.get())))
      return end_transaction(thd, true);

    DBUG_ASSERT(old_id == sys_table->id());
  }

  /*
    Acquire and store the predefined system tablespaces. The DD tablespace
    has already been stored above, so we iterate only over the tablespaces
    with property PREDEFINED_DDSE.
  */
  for (System_tablespaces::Iterator it= System_tablespaces::instance()->begin(
         System_tablespaces::PREDEFINED_DDSE);
       it != System_tablespaces::instance()->end();
       it= System_tablespaces::instance()->next(it,
             System_tablespaces::PREDEFINED_DDSE))
  {
    const dd::Tablespace *tspace= nullptr;
    if (thd->dd_client()->acquire((*it)->key().second, &tspace))
      return end_transaction(thd, true);

    std::unique_ptr<Tablespace_impl> tspace_clone(
            dynamic_cast<Tablespace_impl*>(tspace->clone()));
    tspace_clone->set_id(INVALID_OBJECT_ID);
    if (thd->dd_client()->update(&tspace,
                             static_cast<Tablespace*>(tspace_clone.get())))
       return end_transaction(thd, true);
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_SYNCED;

  return end_transaction(thd, false);
}


/*
  Sync up the preliminary meta data with the DD tables. This must be
  done in several steps to keep the DD table meta data consistent.
  Consistency is necessary throughout this process because the DD tables
  are opened when reading the meta data.
*/
bool read_meta_data(THD *thd)
{
  // Acquire exclusive meta data locks for the relevant DD objects.
  if (acquire_exclusive_mdl(thd))
    return true;

  // Acquire the system schema and tablespace.
  const Schema *sys_schema= nullptr;
  const Tablespace *sys_tspace= nullptr;
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  if (thd->dd_client()->acquire(std::string(MYSQL_SCHEMA_NAME.str),
                                &sys_schema) ||
      thd->dd_client()->acquire(std::string(MYSQL_TABLESPACE_NAME.str),
                                &sys_tspace))
    return end_transaction(thd, true);

  // Expected IDs after creating the scaffolding.
  DBUG_ASSERT(sys_schema != nullptr && sys_schema->id() == 1);
  DBUG_ASSERT(sys_tspace == nullptr || sys_tspace->id() == 1);

  /*
    Acquire all the DD objects into a vector. We must update the schema id
    for all objects in one step without acquisition interleaved.
  */
  std::vector<const Table*> sys_tables;
  for (System_tables::Iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    const dd::Table *table= nullptr;
    if (thd->dd_client()->acquire(std::string(MYSQL_SCHEMA_NAME.str),
                                  (*it)->entity()->name(), &table))
      return end_transaction(thd, true);
    sys_tables.push_back(table);
  }

  // Read and update the system schema object from disk.
  const Schema *stored_sys_schema= nullptr;
  if (thd->dd_client()->acquire_uncached(std::string(MYSQL_SCHEMA_NAME.str),
                                         &stored_sys_schema))
    return end_transaction(thd, true);

  DBUG_ASSERT(stored_sys_schema != nullptr);
  std::unique_ptr<Schema> stored_sys_schema_clone(stored_sys_schema->clone());
  delete stored_sys_schema;
  if (thd->dd_client()->update(&sys_schema,
                               stored_sys_schema_clone.get(), false))
    return end_transaction(thd, true);

  /*
    We must update the schema id of the DD tables to be consistent with
    the real schema id after the schema object was refreshed. We must also
    rehash the objects since the schema id is part of the name key.
  */
  for (std::vector<const Table*>::iterator it= sys_tables.begin();
       it != sys_tables.end(); ++it)
  {
    std::unique_ptr<Table> sys_table_clone((*it)->clone());
    sys_table_clone->set_schema_id(sys_schema->id());
    if (thd->dd_client()->update(&(*it), sys_table_clone.get(), false))
      return end_transaction(thd, true);
  }

  // Read and update the system tablespace object from disk.
  if (sys_tspace)
  {
    const Tablespace *stored_sys_tspace= nullptr;
    if (thd->dd_client()->acquire_uncached(std::string(MYSQL_TABLESPACE_NAME.str),
                                           &stored_sys_tspace))
      return end_transaction(thd, true);

    DBUG_ASSERT(stored_sys_tspace != nullptr);
    std::unique_ptr<Tablespace> stored_sys_tspace_clone(
            stored_sys_tspace->clone());
    delete stored_sys_tspace;
    if (thd->dd_client()->update(&sys_tspace,
                                 stored_sys_tspace_clone.get(), false))
      return end_transaction(thd, true);

    /*
      We must update the tablespace id of the DD tables to be consistent with
      the real tablespace id after the tablespace object was refreshed.
    */
    for (std::vector<const Table*>::iterator it= sys_tables.begin();
         it != sys_tables.end(); ++it)
    {
      std::unique_ptr<Table> sys_table_clone((*it)->clone());
      sys_table_clone->set_tablespace_id(sys_tspace->id());
      if (thd->dd_client()->update(&(*it), sys_table_clone.get(), false))
        return end_transaction(thd, true);
    }
  }

  /*
    At this point, the schema and tablespace objects in the shared cache
    have been synchronized with the DD tables. Additionally, the schema
    and tablespace ids in the DD table objects have been updated so the
    cached DD table objects can be used to open the DD tables. The remaining
    step is to refresh the DD table objects, so the cached objects become
    synchronized with the persistent meta data.
  */
  for (std::vector<const Table*>::iterator it= sys_tables.begin();
       it != sys_tables.end(); ++it)
  {
    const Table *stored_sys_table= nullptr;
    if (thd->dd_client()->acquire_uncached(MYSQL_SCHEMA_NAME.str,
                                           (*it)->name(),
                                           &stored_sys_table))
      return end_transaction(thd, true);

    DBUG_ASSERT(stored_sys_table != nullptr);
    std::unique_ptr<Table> stored_sys_table_clone(stored_sys_table->clone());
    delete stored_sys_table;
    if (thd->dd_client()->update(&(*it), stored_sys_table_clone.get(), false))
      return end_transaction(thd, true);
  }

  /*
    Finally, acquire and refresh the predefined system tablespaces. The DD
    tablespace has already been refreshed above, so we iterate only over the
    tablespaces with property PREDEFINED_DDSE.
  */
  for (System_tablespaces::Iterator it= System_tablespaces::instance()->begin(
         System_tablespaces::PREDEFINED_DDSE);
       it != System_tablespaces::instance()->end();
       it= System_tablespaces::instance()->next(it,
             System_tablespaces::PREDEFINED_DDSE))
  {
    const Tablespace *tspace= nullptr;
    const Tablespace *stored_tspace= nullptr;

    // Acquire from the cache (the scaffolding) and from the DD tables.
    if (thd->dd_client()->acquire((*it)->key().second, &tspace) ||
        thd->dd_client()->acquire_uncached((*it)->key().second,
                                           &stored_tspace))
      return end_transaction(thd, true);

    std::unique_ptr<Tablespace> stored_tspace_clone(
            stored_tspace->clone());
    delete stored_tspace;
    if (thd->dd_client()->update(&tspace, stored_tspace_clone.get(), false))
      return end_transaction(thd, true);
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_SYNCED;

  // Commit and flush tables to force re-opening using the refreshed meta data.
  return end_transaction(thd, false) || execute_query(thd, "FLUSH TABLES");
}


// Insert additional data into the tables.
bool populate_tables(THD *thd)
{
  // Iterate over DD tables, populate tables.
  bool error= false;
  for (System_tables::Iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    // Retrieve list of SQL statements to execute.
    const Object_table_definition *table_def=
            (*it)->entity()->table_definition(thd);
    if (table_def == nullptr)
      return end_transaction(thd, true);

    std::vector<std::string> stmt= table_def->dml_populate_statements();
    for (std::vector<std::string>::iterator stmt_it= stmt.begin();
           stmt_it != stmt.end() && !error; ++stmt_it)
      error= execute_query(thd, *stmt_it);

    // Commit the statement based population.
    error= end_transaction(thd, error);

    // If no error, call the low level table population method, and commit it.
    if (!error)
    {
      error= (*it)->entity()->populate(thd);
      error= end_transaction(thd, error);
    }
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_POPULATED;

  return error;
}


// Execute alter table statements to add cyclic foreign keys.
bool add_cyclic_foreign_keys(THD *thd)
{
  // Iterate over DD tables, add foreign keys.
  for (System_tables::Iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    const Object_table_definition *table_def=
            (*it)->entity()->table_definition(thd);
    if (table_def == nullptr ||
        execute_query(thd, table_def->build_ddl_add_cyclic_foreign_keys()))
      return true;
  }

  return false;
}


// Re-populate character sets and collations upon normal restart.
bool repopulate_charsets_and_collations(THD *thd)
{
  /*
    If we are in read-only mode, we skip re-populating. Here, 'opt_readonly'
    is the value of the '--read-only' option.
  */
  if (opt_readonly)
  {
    sql_print_warning("Skip re-populating collations and character "
                      "sets tables in read-only mode.");
    return false;
  }

  /*
    We must also check if the DDSE is started in a way that makes the DD
    read only. For now, we only support InnoDB as SE for the DD. The call
    to retrieve the handlerton for the DDSE should be replaced by a more
    generic mechanism.
  */
  handlerton *ddse= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->is_dict_readonly && ddse->is_dict_readonly())
  {
    sql_print_warning("Skip re-populating collations and character "
                      "sets tables in InnoDB read-only mode.");
    return false;
  }

  // Otherwise, turn off FK checks, re-populate and commit.
  // The FK checks must be turned off since the collations and
  // character sets reference each other.
  bool error= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
              tables::Collations::instance().populate(thd) ||
              tables::Character_sets::instance().populate(thd);

  /*
    We must commit the re-population before executing a new query, which
    expects the transaction to be empty, and finally, turn FK checks back on.
  */
  error|= end_transaction(thd, error);
  error|= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1");
  bootstrap_stage= bootstrap::BOOTSTRAP_POPULATED;

  return error;
}


/*
  Verify that the individual dictionary tables as well as the schema
  are sticky in the cache, to keep the objects from being evicted.
*/
bool verify_objects_sticky(THD *thd)
{
  // Get the DD tables and verify that they are sticky.
  for (System_tables::Iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    const Table *table= nullptr;
    if (thd->dd_client()->acquire<Table>(MYSQL_SCHEMA_NAME.str,
            (*it)->entity()->name(), &table))
      return end_transaction(thd, true);

    if (!table)
    {
      my_error(ER_BAD_TABLE_ERROR, MYF(0), (*it)->entity()->name().c_str());
      return end_transaction(thd, true);
    }
    DBUG_ASSERT(thd->dd_client()->is_sticky(table));
    if (!thd->dd_client()->is_sticky(table))
      return end_transaction(thd, true);
  }

  // Get the system schema and verify that it is sticky.
  const Schema *sys_schema= nullptr;
  if (thd->dd_client()->acquire<Schema>(MYSQL_SCHEMA_NAME.str,
                                        &sys_schema))
    return end_transaction(thd, true);

  if (!sys_schema)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), sys_schema->name().c_str());
    return end_transaction(thd, true);
  }
  DBUG_ASSERT(thd->dd_client()->is_sticky(sys_schema));
  if (!thd->dd_client()->is_sticky(sys_schema))
    return end_transaction(thd, true);

  // Get the predefined tablespaces and verify that they are sticky.
  for (System_tablespaces::Iterator it= System_tablespaces::instance()->begin();
       it != System_tablespaces::instance()->end(); ++it)
  {
    const dd::Tablespace *tablespace= nullptr;
    if (thd->dd_client()->acquire((*it)->key().second, &tablespace))
      return end_transaction(thd, true);

    DBUG_ASSERT(tablespace);
    DBUG_ASSERT(thd->dd_client()->is_sticky(tablespace));
    if (!thd->dd_client()->is_sticky(tablespace))
      return end_transaction(thd, true);
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_FINISHED;

  return end_transaction(thd, false);
}
} // namespace anonymoous

///////////////////////////////////////////////////////////////////////////

namespace dd {
namespace bootstrap {


// Return the current stage of bootstrapping.
enum_bootstrap_stage stage()
{ return bootstrap_stage; }


// First time server start and initialization of the data dictionary.
bool initialize(THD *thd)
{
  bootstrap_stage= bootstrap::BOOTSTRAP_STARTED;

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.tx_read_only= false;
  thd->tx_read_only= false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d= dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /*
    Each step in the install process below is committed independently,
    either implicitly (for e.g. "CREATE TABLE") or explicitly (for the
    operations in the "populate()" methods). Thus, there is no need to
    commit explicitly here.
  */
  if (DDSE_dict_init(thd, DICT_INIT_CREATE_FILES,
                     d->get_target_dd_version()) ||
      create_schema(thd) ||
      create_tables(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_SERVER,
                        d->get_target_dd_version()) ||
      execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
      store_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_TABLESPACES,
                        d->get_target_dd_version()) ||
      populate_tables(thd) ||
      add_cyclic_foreign_keys(thd) ||
      execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1") ||
      verify_objects_sticky(thd))
    return true;

  DBUG_ASSERT(d->get_target_dd_version() == d->get_actual_dd_version(thd));
  sql_print_information("Installed data dictionary with version %d",
                        d->get_target_dd_version());
  return false;
}


// Normal server restart.
bool restart(THD *thd)
{
  bootstrap_stage= bootstrap::BOOTSTRAP_STARTED;

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.tx_read_only= false;
  thd->tx_read_only= false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d= dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /*
    Start out by calling dict init to preserve old
    behavior.
  */
  if (DDSE_dict_init(thd, DICT_INIT_CHECK_FILES, 0) ||
      create_schema(thd) ||
      create_tables(thd) ||
      read_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd)) ||
      repopulate_charsets_and_collations(thd) ||
      verify_objects_sticky(thd))
    return true;

  sql_print_information("Found data dictionary with version %d",
                        d->get_actual_dd_version(thd));
  return false;
}

} // namespace bootstrap
} // namespace dd
