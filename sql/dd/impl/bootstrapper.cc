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

#include "sql/dd/impl/bootstrapper.h"

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "lex_string.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/dd/cache/dictionary_client.h"   // dd::cache::Dictionary_client
#include "sql/dd/dd.h"                        // dd::create_object
#include "sql/dd/impl/cache/shared_dictionary_cache.h"// Shared_dictionary_cache
#include "sql/dd/impl/cache/storage_adapter.h" // Storage_adapter
#include "sql/dd/impl/dictionary_impl.h"      // dd::Dictionary_impl
#include "sql/dd/impl/raw/object_keys.h"
#include "sql/dd/impl/sdi.h"                  // dd::sdi::store
#include "sql/dd/impl/system_registry.h"      // dd::System_tables
#include "sql/dd/impl/tables/character_sets.h" // dd::tables::Character_sets
#include "sql/dd/impl/tables/collations.h"    // dd::tables::Collations
#include "sql/dd/impl/tables/dd_properties.h" // dd::tables::DD_properties
#include "sql/dd/impl/types/plugin_table_impl.h" // dd::Plugin_table_impl
#include "sql/dd/impl/types/schema_impl.h"    // dd::Schema_impl
#include "sql/dd/impl/types/table_impl.h"     // dd::Table_impl
#include "sql/dd/impl/types/tablespace_impl.h" // dd::Table_impl
#include "sql/dd/object_id.h"
#include "sql/dd/types/abstract_table.h"
#include "sql/dd/types/object_table.h"        // dd::Object_table
#include "sql/dd/types/object_table_definition.h" // dd::Object_table_definition
#include "sql/dd/types/schema.h"
#include "sql/dd/types/table.h"
#include "sql/dd/types/tablespace.h"
#include "sql/dd/types/tablespace_file.h"     // dd::Tablespace_file
#include "sql/dd/upgrade/upgrade.h"           // dd::migrate_event_to_dd
#include "sql/error_handler.h"                // No_such_table_error_handler
#include "sql/handler.h"                      // dict_init_mode_t
#include "sql/log.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"
#include "sql/sql_base.h"                     // close_thread_tables
#include "sql/sql_class.h"                    // THD
#include "sql/sql_list.h"
#include "sql/sql_prepare.h"                  // Ed_connection
#include "sql/stateless_allocator.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/transaction.h"                  // trans_rollback

// Execute a single SQL query.
bool execute_query(THD *thd, const dd::String_type &q_buf)
{
  Ed_connection con(thd);
  LEX_STRING str;
  thd->make_lex_string(&str, q_buf.c_str(), q_buf.length(), false);
  return con.execute_direct(str);
}

using namespace dd;

///////////////////////////////////////////////////////////////////////////

namespace dd {

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
  close_thread_tables(thd);
  thd->mdl_context.release_transactional_locks();
  return error;
}

} // end namespace dd

namespace {
  bootstrap::enum_bootstrap_stage bootstrap_stage=
          dd::bootstrap::BOOTSTRAP_NOT_STARTED;

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
    return dd::end_transaction(thd, error);

  return error;
}


// Create meta data of the predefined tablespaces.
void create_predefined_tablespaces(THD *thd)
{
  /*
    Create dd::Tablespace objects and store them (which will add their meta
    data to the storage adapter registry of DD entities). The tablespaces
    are already created physically in the DDSE, so we only need to create
    the corresponding meta data.
  */
  for (System_tablespaces::Const_iterator it=
         System_tablespaces::instance()->begin();
       it != System_tablespaces::instance()->end(); ++it)
  {
    const Plugin_tablespace *tablespace_def= (*it)->entity();

    // Create the dd::Tablespace object.
    std::unique_ptr<Tablespace> tablespace(dd::create_object<Tablespace>());
    tablespace->set_name(tablespace_def->get_name());
    tablespace->set_options_raw(tablespace_def->get_options());
    tablespace->set_se_private_data_raw(tablespace_def->get_se_private_data());
    tablespace->set_engine(tablespace_def->get_engine());

    // Loop over the tablespace files, create dd::Tablespace_file objects.
    List <const Plugin_tablespace::Plugin_tablespace_file> files=
      tablespace_def->get_files();
    List_iterator<const Plugin_tablespace::Plugin_tablespace_file>
      file_it(files);
    const Plugin_tablespace::Plugin_tablespace_file *file= NULL;
    while ((file= file_it++))
    {
      Tablespace_file *space_file= tablespace->add_file();
      space_file->set_filename(file->get_name());
      space_file->set_se_private_data_raw(file->get_se_private_data());
    }

    // Here, we just want to populate the core registry in the storage
    // adapter. We do not want to have the object registered in the
    // uncommitted registry, this will only add complexity to the
    // DD cache usage during bootstrap. Thus, we call the storage adapter
    // directly instead of going through the dictionary client.
    dd::cache::Storage_adapter::instance()->store(thd, tablespace.get());
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_PREPARED;
}


// Create and use the dictionary schema.
bool create_dd_schema(THD *thd)
{
  return execute_query(thd, dd::String_type("CREATE SCHEMA ") +
                            dd::String_type(MYSQL_SCHEMA_NAME.str) +
                            dd::String_type(" DEFAULT COLLATE '") +
                            dd::String_type(default_charset_info->name)+"'")
  || execute_query(thd, dd::String_type("USE ") +
                          dd::String_type(MYSQL_SCHEMA_NAME.str));
}


/*
  Execute create table statements. This will create the meta data for
  the table. During initial start, the table will also be created
  physically in the DDSE.
*/
bool create_tables(THD *thd)
{
  // Iterate over DD tables, create tables.
  System_tables::Const_iterator it= System_tables::instance()->begin();

  // Make sure the dd_properties table is the first element.
#ifndef DBUG_OFF
  const Object_table_definition *dd_properties_def=
          dd::tables::DD_properties::instance().table_definition(thd);
  DBUG_ASSERT(*it != nullptr);
  const Object_table_definition *first_def=
          (*it)->entity()->table_definition(thd);
  DBUG_ASSERT(dd_properties_def != nullptr && first_def != nullptr &&
          first_def == dd_properties_def);
#endif

  bool error= false;
  for (; it != System_tables::instance()->end(); ++it)
  {
    const Object_table_definition *table_def=
            (*it)->entity()->table_definition(thd);

    if (table_def == nullptr ||
             execute_query(thd, table_def->build_ddl_create_table()))
    {
      error= true;
      break;
    }
  }

  // The modified objects are now reflected in the core registry in the
  // storage adapter. We remove the objects from the uncommitted registry
  // because their presence there will just complicate handling during
  // bootstrap.
  thd->dd_client()->rollback_modified_objects();

  if (error)
    return true;

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
  for (System_tables::Const_iterator it= System_tables::instance()->begin();
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
  Acquire the DD schema, tablespace and table objects. Clone the objects,
  reset ID, store persistently, and update the storage adapter.
*/
bool flush_meta_data(THD *thd)
{
  // Acquire exclusive meta data locks for the relevant DD objects.
  if (acquire_exclusive_mdl(thd))
    return true;

  {
    /*
      Use a scoped auto releaser to make sure the cached objects are released
      before the shared cache is reset.
    */
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    // Acquire the DD schema and tablespace.
    const Schema *dd_schema= nullptr;
    const Tablespace *dd_tspace= nullptr;
    if (thd->dd_client()->acquire(dd::String_type(MYSQL_SCHEMA_NAME.str),
                                  &dd_schema) ||
        thd->dd_client()->acquire(dd::String_type(MYSQL_TABLESPACE_NAME.str),
                                  &dd_tspace))
      return dd::end_transaction(thd, true);

    // Acquire the DD table objects.
    for (System_tables::Const_iterator it= System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it)
    {
      const dd::Table *dd_table= nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return dd::end_transaction(thd, true);
    }

    /*
      We have now populated the shared cache with the core objects. The
      scoped auto releaser makes sure we will not evict the objects from
      the shared cache until the auto releaser exits scope. Thus, within
      the scope of the auto releaser, we can modify the contents of the
      core registry in the storage adapter without risking that this will
      interfere with the contents of the shared cache, because the DD
      transactions will acquire the core objects from the shared cache.
    */

    /*
      Modify and store the DD schema without changing the cached copy.
      Cannot use acquire_for_modification() here, because that would
      make the DD sub-transactions (e.g. when calling store()) see a
      partially modified set of core objects, where e.g. the mysql
      schema object has got its new, real id (from the auto-inc column
      in the dd.schemata table), whereas the core DD table objects still
      refer to the id that was allocated when creating the scaffolding.
      This comment also applies to the modification of tablespaces
      and tables.
    */
    std::unique_ptr<Schema_impl> dd_schema_clone(
      dynamic_cast<Schema_impl*>(dd_schema->clone()));

    // We must set the ID to INVALID to enable storing the object.
    dd_schema_clone->set_id(INVALID_OBJECT_ID);
    if (dd::cache::Storage_adapter::instance()->store(thd,
          static_cast<Schema*>(dd_schema_clone.get())))
      return dd::end_transaction(thd, true);

    // Make sure the registry of the core DD objects is updated.
    dd::cache::Storage_adapter::instance()->core_drop(thd, dd_schema);
    dd::cache::Storage_adapter::instance()->core_store(thd,
      static_cast<Schema*>(dd_schema_clone.get()));

    // Make sure the ID after storing is as expected.
    DBUG_ASSERT(dd_schema_clone->id() == 1);

    // Update and store the DD tablespace without changing the cached copy.
    std::unique_ptr<Tablespace_impl> dd_tspace_clone(nullptr);
    if (dd_tspace != nullptr)
    {
      dd_tspace_clone.reset(
        dynamic_cast<Tablespace_impl*>(dd_tspace->clone()));

      // We must set the ID to INVALID to enable storing the object.
      dd_tspace_clone->set_id(INVALID_OBJECT_ID);
      if (dd::cache::Storage_adapter::instance()->store(thd,
            static_cast<Tablespace*>(dd_tspace_clone.get())))
        return dd::end_transaction(thd, true);

      // Make sure the registry of the core DD objects is updated.
      dd::cache::Storage_adapter::instance()->core_drop(thd, dd_tspace);
      dd::cache::Storage_adapter::instance()->core_store(thd,
            static_cast<Tablespace*>(dd_tspace_clone.get()));

      // Make sure the ID after storing is as expected.
      DBUG_ASSERT(dd_tspace_clone->id() == 1);
    }

    // Update and store the DD table objects without changing the cached copy.
    for (System_tables::Const_iterator it= System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it)
    {
      const dd::Table *dd_table= nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return dd::end_transaction(thd, true);

      std::unique_ptr<Table_impl> dd_table_clone(
              dynamic_cast<Table_impl*>(dd_table->clone()));

      // We must set the ID to INVALID to enable storing the object.
      dd_table_clone->set_id(INVALID_OBJECT_ID);

      // Change the schema and tablespace id to match the ids of the
      // persisted objects.
      dd_table_clone->set_schema_id(dd_schema_clone->id());
      if (dd_tspace != nullptr)
        dd_table_clone->set_tablespace_id(dd_tspace_clone->id());
      if (dd::cache::Storage_adapter::instance()->store(thd,
            static_cast<Table*>(dd_table_clone.get())))
        return dd::end_transaction(thd, true);

      // No need to keep non-core dd tables in the core registry.
      (void) dd::cache::Storage_adapter::instance()->core_drop(thd, dd_table);
      // Make sure the registry of the core DD objects is updated.
      if ((*it)->property() == System_tables::Types::CORE)
          dd::cache::Storage_adapter::instance()->core_store(thd,
            static_cast<Table*>(dd_table_clone.get()));
    }

    // Update and store the predefined tablespace objects. The DD tablespace
    // has already been stored above, so we iterate only over the tablespaces
    // of type PREDEFINED_DDSE.
    for (System_tablespaces::Const_iterator it=
           System_tablespaces::instance()->begin(
             System_tablespaces::Types::PREDEFINED_DDSE);
         it != System_tablespaces::instance()->end();
         it= System_tablespaces::instance()->next(it,
               System_tablespaces::Types::PREDEFINED_DDSE))
    {
      const dd::Tablespace *tspace= nullptr;
      if (thd->dd_client()->acquire((*it)->key().second, &tspace))
        return dd::end_transaction(thd, true);

      std::unique_ptr<Tablespace_impl> tspace_clone(
              dynamic_cast<Tablespace_impl*>(tspace->clone()));

      // We must set the ID to INVALID to enable storing the object.
      tspace_clone->set_id(INVALID_OBJECT_ID);
      if (dd::cache::Storage_adapter::instance()->store(thd,
            static_cast<Tablespace*>(tspace_clone.get())))
        return dd::end_transaction(thd, true);

      // Only the DD tablespace is needed to handle cache misses, so we can
      // just drop the predefined tablespaces from the core registry now that
      // it has been persisted.
      (void) dd::cache::Storage_adapter::instance()->core_drop(thd, tspace);
    }
  }

  // Now, the auto releaser has released the objects, and we can go ahead and
  // reset the shared cache.
  dd::cache::Shared_dictionary_cache::instance()->reset(true);

  if (dd::end_transaction(thd, false))
  {
    return true;
  }

  /*
    Use a scoped auto releaser to make sure the objects cached for SDI
    writing and FK parent information reload are released.
  */
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  // Acquire the DD tablespace and write SDI
  const Tablespace *dd_tspace= nullptr;
  if (thd->dd_client()->acquire(dd::String_type(MYSQL_TABLESPACE_NAME.str),
                                &dd_tspace) ||
      dd::sdi::store(thd, dd_tspace))
  {
    return dd::end_transaction(thd, true);
  }

  // Acquire the DD schema and write SDI
  const Schema *dd_schema= nullptr;
  if (thd->dd_client()->acquire(dd::String_type(MYSQL_SCHEMA_NAME.str),
                                &dd_schema) ||
      dd::sdi::store(thd, dd_schema))
  {
    return dd::end_transaction(thd, true);
  }

  // Acquire the DD table objects and write SDI for them. Also sync from
  // the DD tables in order to get the FK parent informnation reloaded.
  for (System_tables::Const_iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    const dd::Table *dd_table= nullptr;
    if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                  (*it)->entity()->name(), &dd_table))
    {
      return dd::end_transaction(thd, true);
    }

    // Make sure the registry of the core DD objects is updated with an
    // object read from the DD tables, with updated FK parent information.
    // Store the object to make sure SDI is written.
    Abstract_table::name_key_type table_key;
    Abstract_table::update_name_key(&table_key, dd_schema->id(),
                                    dd_table->name());
    if (((*it)->property() == System_tables::Types::CORE &&
         dd::cache::Storage_adapter::instance()->core_sync(thd, table_key,
                                                           dd_table)) ||
        dd::sdi::store(thd, dd_table))
    {
      return dd::end_transaction(thd, true);
    }
  }

  bootstrap_stage= bootstrap::BOOTSTRAP_SYNCED;

  return dd::end_transaction(thd, false);
}


/*
  Acquire the DD schema, tablespace and table objects. Read the "real"
  objects from the DD tables, and replace the contents of the core
  registry in the storage adapter.
*/
bool sync_meta_data(THD *thd)
{
  // Acquire exclusive meta data locks for the relevant DD objects.
  if (acquire_exclusive_mdl(thd))
    return true;
  {
    // Acquire the DD schema and tablespace.
    const Schema *dd_schema= nullptr;
    const Tablespace *dd_tspace= nullptr;
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    if (thd->dd_client()->acquire(dd::String_type(MYSQL_SCHEMA_NAME.str),
                                  &dd_schema) ||
        thd->dd_client()->acquire(dd::String_type(MYSQL_TABLESPACE_NAME.str),
                                  &dd_tspace))
      return dd::end_transaction(thd, true);

    // Acquire the DD table objects.
    for (System_tables::Const_iterator it= System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it)
    {
      const dd::Table *dd_table= nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return dd::end_transaction(thd, true);
    }

    /*
      We have now populated the shared cache with the core objects. The
      scoped auto releaser makes sure we will not evict the objects from
      the shared cache until the auto releaser exits scope. Thus, within
      the scope of the auto releaser, we can modify the contents of the
      core registry in the storage adapter without risking that this will
      interfere with the contents of the shared cache, because the DD
      transactions will acquire the core objects from the shared cache.
    */

    // Sync the DD schema and tablespace.
    Schema::name_key_type schema_key;
    dd_schema->update_name_key(&schema_key);
    if (dd::cache::Storage_adapter::instance()->core_sync(thd, schema_key,
                                                          dd_schema))
      return dd::end_transaction(thd, true);

    if (dd_tspace)
    {
      Tablespace::name_key_type tspace_key;
      dd_tspace->update_name_key(&tspace_key);
      if (dd::cache::Storage_adapter::instance()->core_sync(thd, tspace_key,
                                                            dd_tspace))
        return dd::end_transaction(thd, true);
    }

    // Get the synced DD schema object id. Needed for the DD table name keys.
    Object_id dd_schema_id= cache::Storage_adapter::instance()->
      core_get_id<Schema>(schema_key);

    // Sync the DD tables.
    for (System_tables::Const_iterator it= System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it)
    {
      Abstract_table::name_key_type table_key;
      const dd::Table *dd_table= nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return dd::end_transaction(thd, true);

      Abstract_table::update_name_key(&table_key, dd_schema_id,
                                      dd_table->name());
      dd::cache::Storage_adapter::instance()->core_drop(thd, dd_table);
      if ((*it)->property() == System_tables::Types::CORE)
        if (dd::cache::Storage_adapter::instance()->core_sync(thd, table_key,
                                                              dd_table))
          return dd::end_transaction(thd, true);
    }

    /*
      The DD tablespace has already been refreshed above, so we iterate only
      over the tablespaces with type PREDEFINED_DDSE. These can be dropped
      from the storage adapter.
    */
    for (System_tablespaces::Const_iterator it=
           System_tablespaces::instance()->begin(
             System_tablespaces::Types::PREDEFINED_DDSE);
         it != System_tablespaces::instance()->end();
         it= System_tablespaces::instance()->next(it,
               System_tablespaces::Types::PREDEFINED_DDSE))
    {
      const Tablespace *tspace= nullptr;
      if (thd->dd_client()->acquire((*it)->entity()->get_name(), &tspace))
        return dd::end_transaction(thd, true);

      dd::cache::Storage_adapter::instance()->core_drop(thd, tspace);
    }
  }

  // Now, the auto releaser has released the objects, and we can go ahead and
  // reset the shared cache.
  dd::cache::Shared_dictionary_cache::instance()->reset(true);
  bootstrap_stage= bootstrap::BOOTSTRAP_SYNCED;

  // Commit and flush tables to force re-opening using the refreshed meta data.
  if (dd::end_transaction(thd, false) || execute_query(thd, "FLUSH TABLES"))
    return true;

  // Reset the DDSE local dictionary cache.
  handlerton *ddse= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->dict_cache_reset == nullptr)
    return true;

  for (System_tables::Const_iterator it=
         System_tables::instance()->begin(System_tables::Types::CORE);
       it != System_tables::instance()->end(); it=
         System_tables::instance()->next(it, System_tables::Types::CORE))
    ddse->dict_cache_reset(MYSQL_SCHEMA_NAME.str, (*it)->entity()->name().c_str());

  return false;
}


// Insert additional data into the tables.
bool populate_tables(THD *thd)
{
  // Iterate over DD tables, populate tables.
  bool error= false;
  for (System_tables::Const_iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    // Retrieve list of SQL statements to execute.
    const Object_table_definition *table_def=
            (*it)->entity()->table_definition(thd);
    if (table_def == nullptr)
      return dd::end_transaction(thd, true);

    std::vector<dd::String_type> stmt= table_def->dml_populate_statements();
    for (std::vector<dd::String_type>::iterator stmt_it= stmt.begin();
           stmt_it != stmt.end() && !error; ++stmt_it)
      error= execute_query(thd, *stmt_it);

    // Commit the statement based population.
    error= dd::end_transaction(thd, error);

    // If no error, call the low level table population method, and commit it.
    if (!error)
    {
      error= (*it)->entity()->populate(thd);
      error= dd::end_transaction(thd, error);
    }
  }
  bootstrap_stage= bootstrap::BOOTSTRAP_POPULATED;

  return error;
}


// Execute alter table statements to add cyclic foreign keys.
bool add_cyclic_foreign_keys(THD *thd)
{
  // Iterate over DD tables, add foreign keys.
  for (System_tables::Const_iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it)
  {
    const Object_table_definition *table_def=
            (*it)->entity()->table_definition(thd);
    if (table_def == nullptr ||
        execute_query(thd, table_def->build_ddl_add_cyclic_foreign_keys()))
      return true;

    // Acquire the table object, maintain table hiding.
    dd::Table *table= nullptr;
    if (thd->dd_client()->acquire_for_modification(
          dd::String_type(MYSQL_SCHEMA_NAME.str),
          (*it)->entity()->name(), &table))
      return true;

    if ((table->hidden() == dd::Abstract_table::HT_HIDDEN_SYSTEM) !=
        (*it)->entity()->hidden())
    {
      table->set_hidden((*it)->entity()->hidden() ?
                        dd::Abstract_table::HT_HIDDEN_SYSTEM:
                        dd::Abstract_table::HT_VISIBLE);
      if (thd->dd_client()->update(table))
        return dd::end_transaction(thd, true);
    }
    dd::end_transaction(thd, false);
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
    LogErr(WARNING_LEVEL, ER_DD_NO_WRITES_NO_REPOPULATION, "", "");
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
    LogErr(WARNING_LEVEL, ER_DD_NO_WRITES_NO_REPOPULATION, "InnoDB", " ");
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
  error|= dd::end_transaction(thd, error);
  error|= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1");
  bootstrap_stage= bootstrap::BOOTSTRAP_POPULATED;

  return error;
}


/*
  Verify that the storage adapter contains the core DD objects and
  nothing else.
*/
bool verify_core_objects_present(THD *thd)
{
  // Verify that the DD schema is present, and get its id.
  Schema::name_key_type schema_key;
  Schema::update_name_key(&schema_key, MYSQL_SCHEMA_NAME.str);
  Object_id dd_schema_id= cache::Storage_adapter::instance()->
    core_get_id<Schema>(schema_key);

  DBUG_ASSERT(dd_schema_id != INVALID_OBJECT_ID);
  if (dd_schema_id == INVALID_OBJECT_ID)
  {
    LogErr(ERROR_LEVEL, ER_DD_SCHEMA_NOT_FOUND, MYSQL_SCHEMA_NAME.str);
    return dd::end_transaction(thd, true);
  }
  DBUG_ASSERT(cache::Storage_adapter::instance()->
          core_size<Schema>() == 1);

  // Verify that the core DD tables are present.
#ifndef DBUG_OFF
  size_t n_core_tables= 0;
#endif
  for (System_tables::Const_iterator it= System_tables::instance()->begin(
         System_tables::Types::CORE);
       it != System_tables::instance()->end();
       it= System_tables::instance()->next(it,
             System_tables::Types::CORE))
  {
#ifndef DBUG_OFF
    n_core_tables++;
#endif
    Table::name_key_type table_key;
    Table::update_name_key(&table_key, dd_schema_id,
                           (*it)->entity()->name());
    Object_id dd_table_id= cache::Storage_adapter::instance()->
      core_get_id<Table>(table_key);

    DBUG_ASSERT(dd_table_id != INVALID_OBJECT_ID);
    if (dd_table_id == INVALID_OBJECT_ID)
    {
      LogErr(ERROR_LEVEL, ER_DD_TABLE_NOT_FOUND,
             (*it)->entity()->name().c_str());
      return dd::end_transaction(thd, true);
    }
  }
  DBUG_ASSERT(cache::Storage_adapter::instance()->
          core_size<Abstract_table>() == n_core_tables);

  // Verify that the system tablespace is present.
#ifndef DBUG_OFF
  Tablespace::name_key_type tspace_key;
  Tablespace::update_name_key(&tspace_key, MYSQL_TABLESPACE_NAME.str);
  Object_id dd_tspace_id= cache::Storage_adapter::instance()->
    core_get_id<Tablespace>(tspace_key);
#endif

  /*
    TODO: No DD tablespace yet. Enable verification later when InnoDB
    worklogs are pushed.

  DBUG_ASSERT(dd_tspace_id != INVALID_OBJECT_ID);
  if (dd_tspace_id == INVALID_OBJECT_ID)
  {
    LogErr(ERROR_LEVEL, ER_DD_TABLESPACE_NOT_FOUND, MYSQL_TABLESPACE_NAME.str);
    return dd::end_transaction(thd, true);
  }
  */

  DBUG_ASSERT(cache::Storage_adapter::instance()->
          core_size<Tablespace>() == (dd_tspace_id != INVALID_OBJECT_ID));

  bootstrap_stage= bootstrap::BOOTSTRAP_FINISHED;

  return dd::end_transaction(thd, false);
}
} // namespace anonymoous

///////////////////////////////////////////////////////////////////////////

namespace dd {
namespace bootstrap {


// Return the current stage of bootstrapping.
enum_bootstrap_stage stage()
{ return bootstrap_stage; }


/*
  Do the necessary DD-related initialization in the DDSE, and get the
  predefined tables and tablespaces.
*/
bool DDSE_dict_init(THD *thd,
                    dict_init_mode_t dict_init_mode,
                    uint version)
{
  handlerton *ddse= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);

  // The lists with element wrappers are mem root allocated. The wrapped
  // instances are owned by the DDSE.
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
            table->get_schema_name(),
            table->get_name(),
            table->get_table_definition(),
            table->get_table_options(),
            Dictionary_impl::get_target_dd_version(),
            table->get_tablespace_name());
    System_tables::instance()->add(MYSQL_SCHEMA_NAME.str, table->get_name(),
                                   System_tables::Types::SUPPORT, plugin_table);
  }

  /*
    Iterate over the tablespace definitions, add the names and the
    tablespace meta data to the System_tablespaces registry. The
    meta data will be used later to create dd::Tablespace objects.
    The Plugin_tablespace instances are owned by the DDSE.
  */
  List_iterator<const Plugin_tablespace> tablespace_it(plugin_tablespaces);
  const Plugin_tablespace *tablespace= nullptr;
  while ((tablespace= tablespace_it++))
  {
    // Add the name and the object instance to the registry with the
    // appropriate property.
    if (my_strcasecmp(system_charset_info, MYSQL_TABLESPACE_NAME.str,
                      tablespace->get_name()) == 0)
      System_tablespaces::instance()->add(tablespace->get_name(),
                                          System_tablespaces::Types::DD,
                                          tablespace);
    else
      System_tablespaces::instance()->add(tablespace->get_name(),
                            System_tablespaces::Types::PREDEFINED_DDSE,
                            tablespace);
  }

  return false;
}


// Initialize the data dictionary.
bool initialize_dictionary(THD *thd, bool is_dd_upgrade,
                           Dictionary_impl *d)
{
  if (is_dd_upgrade)
    bootstrap_stage= bootstrap::BOOTSTRAP_STARTED;

  create_predefined_tablespaces(thd);
  if (create_dd_schema(thd) ||
      create_tables(thd))
    return true;

  if (is_dd_upgrade)
  {
    // Add status to mark creation of dictionary in InnoDB.
    // Till this step, no new undo log is created by InnoDB.
    if (upgrade::Upgrade_status().update(
          upgrade::Upgrade_status::enum_stage::DICT_TABLES_CREATED))
      return true;
  }

  DBUG_EXECUTE_IF("dd_upgrade_stage_2",
                  if (is_dd_upgrade)
                  {
                    /*
                      Server will crash will upgrading 5.7 data directory.
                      This will leave server is an inconsistent state.
                      File tracking upgrade will have Stage 2 written in it.
                      Next restart of server on same data directory should
                      revert all changes done by upgrade and data directory
                      should be reusable by 5.7 server.
                    */
                    DBUG_SUICIDE();
                  }
                 );


  if (DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_SERVER,
                        d->get_target_dd_version()) ||
      execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
      flush_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_TABLESPACES,
                        d->get_target_dd_version()) ||
      populate_tables(thd) ||
      add_cyclic_foreign_keys(thd) ||
      execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1") ||
      verify_core_objects_present(thd))
    return true;
  return false;
}


// First time server start and initialization of the data dictionary.
bool initialize(THD *thd)
{
  bootstrap_stage= bootstrap::BOOTSTRAP_STARTED;

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.transaction_read_only= false;
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
      initialize_dictionary(thd, false, d))
    return true;

  DBUG_ASSERT(d->get_target_dd_version() == d->get_actual_dd_version(thd));
  LogErr(INFORMATION_LEVEL, ER_DD_VERSION_INSTALLED,
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
  thd->variables.transaction_read_only= false;
  thd->tx_read_only= false;

  // Set explicit_defaults_for_timestamp variable for dictionary creation
  thd->variables.explicit_defaults_for_timestamp= true;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d= dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  create_predefined_tablespaces(thd);

  if (create_dd_schema(thd) ||
      create_tables(thd) ||
      sync_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd)) ||
      repopulate_charsets_and_collations(thd) ||
      verify_core_objects_present(thd))
    return true;

  LogErr(INFORMATION_LEVEL, ER_DD_VERSION_FOUND,
         d->get_actual_dd_version(thd));
  return false;
}


// Initialize dictionary in case of server restart.
void recover_innodb_upon_upgrade(THD *thd)
{
  Dictionary_impl *d= dd::Dictionary_impl::instance();
  create_predefined_tablespaces(thd);
  // RAII to handle error in execution of CREATE TABLE.
  Key_length_error_handler key_error_handler;
  /*
    Ignore ER_TOO_LONG_KEY for dictionary tables during restart.
    Do not print the error in error log as we are creating only the
    cached objects and not physical tables.
TODO: Workaround due to bug#20629014. Remove when the bug is fixed.
   */
  thd->push_internal_handler(&key_error_handler);
  if (create_dd_schema(thd) ||
      create_tables(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd)))
  {
    // Error is not be handled in this case as we are on cleanup code path.
    LogErr(WARNING_LEVEL, ER_DD_INIT_UPGRADE_FAILED);
  }
  thd->pop_internal_handler();
  return;
}


bool setup_dd_objects_and_collations(THD *thd)
{
  // Continue with server startup.
  bootstrap_stage= bootstrap::BOOTSTRAP_CREATED;

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.transaction_read_only= false;
  thd->tx_read_only= false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d= dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);

  DBUG_ASSERT(d->get_target_dd_version() == d->get_actual_dd_version(thd));

  if (sync_meta_data(thd) ||
      repopulate_charsets_and_collations(thd) ||
      verify_core_objects_present(thd))
  {
    return true;
  }
  else
  {
    LogErr(INFORMATION_LEVEL, ER_DD_VERSION_FOUND,
           d->get_actual_dd_version(thd));
  }

  return false;
}


} // namespace bootstrap
} // namespace dd
