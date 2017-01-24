/* Copyright (c) 2014, 2017 Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "binary_log_types.h"
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/dd.h"                            // dd::create_object
#include "dd/dd_schema.h"                     // dd::schema_exists
#include "dd/dd_table.h"                      // dd::get_sql_type_by_field_info
#include "dd/dd_upgrade.h"                    // dd::migrate_event_to_dd
#include "dd/impl/cache/shared_dictionary_cache.h"// Shared_dictionary_cache
#include "dd/impl/cache/storage_adapter.h"    // Storage_adapter
#include "dd/impl/dictionary_impl.h"          // dd::Dictionary_impl
#include "dd/impl/system_registry.h"          // dd::System_tables
#include "dd/impl/tables/character_sets.h"    // dd::tables::Character_sets
#include "dd/impl/tables/collations.h"        // dd::tables::Collations
#include "dd/impl/tables/version.h"           // dd::tables::Version
#include "dd/impl/types/plugin_table_impl.h"  // dd::Plugin_table_impl
#include "dd/impl/types/schema_impl.h"        // dd::Schema_impl
#include "dd/impl/types/table_impl.h"         // dd::Table_impl
#include "dd/impl/types/tablespace_impl.h"    // dd::Table_impl
#include "dd/object_id.h"
#include "dd/properties.h"                    // dd::Properties
#include "dd/types/column.h"                  // dd::Column
#include "dd/types/object_table_definition.h" // dd::Object_table_definition
#include "dd/types/object_table.h"            // dd::Object_table
#include "dd/types/schema.h"
#include "dd/types/table.h"
#include "dd/types/tablespace_file.h"         // dd::Tablespace_file
#include "dd/types/tablespace.h"
#include "dd/types/view.h"                    // dd::View
#include "error_handler.h"                    // No_such_table_error_handler
#include "handler.h"                          // dict_init_mode_t
#include "log.h"                              // sql_print_warning()
#include "m_ctype.h"
#include "mdl.h"
#include "m_string.h"                         // STRING_WITH_LEN
#include "my_dbug.h"
#include "my_global.h"
#include "mysqld_error.h"
#include "mysqld.h"
#include "mysql/plugin.h"
#include "my_sys.h"
#include "sql_class.h"                        // THD
#include "sql_error.h"
#include "sql_list.h"
#include "sql_plugin.h"                       // plugin_foreach
#include "sql_plugin_ref.h"
#include "sql_prepare.h"                      // Ed_connection
#include "sql_profile.h"
#include "sql_show.h"                         // get_schema_table()
#include "system_variables.h"
#include "table.h"
#include "transaction.h"                      // trans_rollback

#include <vector>
#include <memory>

/*
  The variable is used to differentiate between a normal server restart
  and server upgrade.

  For the upgrade, before populating the DD tables, all the plugins needs
  to be initialized. Once the plugins are initialized, the server calls
  DD initialization function again to finish the upgrade process.

  When upgrading on old data directory, creation of tables inside Storage
  Engine should not be done for innodb_table_stats and innodb_index_stats.

  In the statement execution, ALTER VIEW and RENAME TABLE, if we get an
  error, this variables is used to set error status in DA. my_error()
  call does not set DA status when executing from bootstrap thread.

  In case of deleting dictionary tables, we need to delete dictionary
  tables  only from SE but not from Dictionary cache. This flag is used to
  avoid deletion of dd::Table objects from cache.
*/
my_bool dd_upgrade_flag= false;

/*
  The variable is used to differentiate the actions within SE during a
  normal server restart and server upgrade.

  During upgrade, while checking for the existence of version tables,
  the creation of tables inside storage engine should not be done.
  se_private_data should not be retrieved from InnoDB in case dd::Table
  objects are being created to check if we are upgrading or restarting.
*/
my_bool dd_upgrade_skip_se= false;


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


bool store_single_schema_table_meta_data(THD *thd,
                                         const dd::Schema *IS_schema_obj,
                                         ST_SCHEMA_TABLE *schema_table)
{
  // Skip I_S tables that are hidden from users.
  if (schema_table->hidden)
    return false;

  MDL_request db_mdl_request;
  MDL_REQUEST_INIT(&db_mdl_request,
                   MDL_key::SCHEMA, IS_schema_obj->name().c_str(), "",
                   MDL_INTENTION_EXCLUSIVE,
                   MDL_TRANSACTION);
  if (thd->mdl_context.acquire_lock(&db_mdl_request,
                                    thd->variables.lock_wait_timeout))
    return true;

  std::unique_ptr<dd::View> view_obj(IS_schema_obj->create_system_view(thd));

  // column_type_utf8
  const CHARSET_INFO* cs=
    get_charset(system_charset_info->number, MYF(0));

  // Set view properties
  view_obj->set_client_collation_id(IS_schema_obj->default_collation_id());

  view_obj->set_connection_collation_id(IS_schema_obj->default_collation_id());

  view_obj->set_name(schema_table->table_name);

  //
  // Fill columns details
  //
  ST_FIELD_INFO *fields_info= schema_table->fields_info;
  for (; fields_info->field_name; fields_info++)
  {
    dd::Column *col_obj= view_obj->add_column();

    col_obj->set_name(fields_info->field_name);

    /*
      The 5.7 create_schema_table() creates Item_empty_string() item for
      MYSQL_TYPE_STRING. Item_empty_string->field_type() maps to
      MYSQL_TYPE_VARCHAR. So, we map MYSQL_TYPE_STRING to
      MYSQL_TYPE_VARCHAR when storing metadata into DD.
    */
    enum_field_types ft= fields_info->field_type;
    uint32 fl= fields_info->field_length;
    if (fields_info->field_type == MYSQL_TYPE_STRING)
    {
      ft= MYSQL_TYPE_VARCHAR;
      fl= fields_info->field_length * cs->mbmaxlen;
    }

    col_obj->set_type(dd::get_new_field_type(ft));

    col_obj->set_char_length(fields_info->field_length);

    col_obj->set_nullable(fields_info->field_flags & MY_I_S_MAYBE_NULL);

    col_obj->set_unsigned(fields_info->field_flags & MY_I_S_UNSIGNED);

    col_obj->set_zerofill(false);

    // Collation ID
    col_obj->set_collation_id(system_charset_info->number);

    col_obj->set_column_type_utf8(
               dd::get_sql_type_by_field_info(
                 thd, ft, fl, cs));

    col_obj->set_default_value_utf8(dd::String_type(STRING_WITH_LEN("")));
  }

  // Acquire MDL on the view name.
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, MDL_key::TABLE,
                   IS_schema_obj->name().c_str(), view_obj->name().c_str(),
                   MDL_EXCLUSIVE,
                   MDL_TRANSACTION);
  if (thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout))
    return true;

  // Store the metadata into DD
  if (thd->dd_client()->store(view_obj.get()))
    return true;

  return false;
}


/**
  @brief
    Store metadata of schema tables into DD.

  @param THD

  @param plugin_ref
    This function is invoked in two cases,
    1) To store schema tables of MySQL server
       - Pass plugin_ref as nullptr in this case.

    2) To store schema tables of plugins
       - plugin_foreach() function passes proper
         plugin_ref.

  @param p_trx
    Read/Write DD transaction.

  @return
    false on success
    true when fails to store the metadata.
*/
my_bool store_schema_table_meta_data(THD *thd, plugin_ref plugin, void *unused)
{
  // Fetch schema ID of IS schema.
  const dd::Schema *IS_schema_obj= nullptr;
  if (thd->dd_client()->acquire(INFORMATION_SCHEMA_NAME.str,
                                &IS_schema_obj))
  {
    return true;
  }

  if (plugin)
  {
    ST_SCHEMA_TABLE *schema_table= plugin_data<ST_SCHEMA_TABLE*>(plugin);
    return store_single_schema_table_meta_data(thd,
                                               IS_schema_obj,
                                               schema_table);
  }

  ST_SCHEMA_TABLE *schema_tables= get_schema_table(SCH_FIRST);
  for (; schema_tables->table_name; schema_tables++)
  {
    if(store_single_schema_table_meta_data(thd,
                                           IS_schema_obj,
                                           schema_tables))
      return true;
  }

  return false;
}


/**
  This function stores the meta data of IS tables into the DD tables.
  Some IS tables are created as system views on DD tables,
  for which meta data is populated automatically when the
  view is created. This function handles the rest of the IS tables
  that are not system views, i.e., elements in the ST_SCHEMA_TABLES
  schema_tables[] array.

  @param[in] thd      Thread context

  @retval true        Error
  @retval false       Success
*/
bool store_server_I_S_table_meta_data(THD *thd)
{
  bool error= store_schema_table_meta_data(thd, nullptr, nullptr);
  return end_transaction(thd, error);
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
            table->get_name(),
            table->get_table_definition(),
            table->get_table_options(),
            Dictionary_impl::get_target_dd_version());
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


// CREATE stats table with 5.7 definition in case of upgrade
static const dd::String_type stats_table_def(dd::String_type name)
{
  if (strcmp(name.c_str(), "innodb_table_stats") == 0)
    return ("  CREATE TABLE innodb_table_stats (\n"
            "  database_name VARCHAR(64) NOT NULL, \n"
            "  table_name VARCHAR(64) NOT NULL, \n"
            "  last_update TIMESTAMP NOT NULL \n"
            "  DEFAULT CURRENT_TIMESTAMP \n"
            "  ON UPDATE CURRENT_TIMESTAMP, \n"
            "  n_rows BIGINT UNSIGNED NOT NULL, \n"
            "  clustered_index_size BIGINT UNSIGNED NOT NULL, \n"
            "  sum_of_other_index_sizes BIGINT UNSIGNED NOT NULL, \n"
            "  PRIMARY KEY (database_name, table_name) \n)"
            "  ENGINE=INNODB ROW_FORMAT=DYNAMIC "
            "  DEFAULT CHARSET=utf8 COLLATE=utf8_bin "
            "  STATS_PERSISTENT=0");
  else
    return ("  CREATE TABLE innodb_index_stats (\n"
            "  database_name VARCHAR(64) NOT NULL, \n"
            "  table_name VARCHAR(64) NOT NULL, \n"
            "  index_name VARCHAR(64) NOT NULL, \n"
            "  last_update TIMESTAMP NOT NULL NOT NULL \n"
            "  DEFAULT CURRENT_TIMESTAMP \n"
            "  ON UPDATE CURRENT_TIMESTAMP, \n"
            "  stat_name VARCHAR(64) NOT NULL, \n"
            "  stat_value BIGINT UNSIGNED NOT NULL, \n"
            "  sample_size BIGINT UNSIGNED, \n"
            "  stat_description VARCHAR(1024) NOT NULL, \n"
            "  PRIMARY KEY (database_name, table_name, "
                    "index_name, stat_name) \n)"
            " ENGINE=INNODB ROW_FORMAT=DYNAMIC "
            " DEFAULT CHARSET=utf8 COLLATE=utf8_bin "
            " STATS_PERSISTENT=0");
}


/*
  Execute create table statements. This will create the meta data for
  the table. During initial start, the table will also be created
  physically in the DDSE.

  In case of upgrade, definition of mysql.innodb_table_stats and
  mysql.innodb_index_stats table is used as in 5.7 instead of current
  definition. Later, ALTER command is executed to fix these table
  definitions.
*/
bool create_tables(THD *thd, bool is_dd_upgrade,
                   System_tables::Const_iterator *last_table)
{
  // Iterate over DD tables, create tables.
  System_tables::Const_iterator it= System_tables::instance()->begin();

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

  bool error= false;
  for (; it != System_tables::instance()->end(); ++it)
  {
    /*
      Creation of dictionary tables can fail in there is already a entry in
      InnoDB dictionary with the same name as of dictionary table. The iterator
      tracks number of dictionary tables created to delete the same number of
      tables while aborting upgrade.
    */
    if (last_table != nullptr)
      *last_table= it;

    const Object_table_definition *table_def=
            (*it)->entity()->table_definition(thd);

    String_type table_name= (*it)->entity()->name();
    String_type schema_name(MYSQL_SCHEMA_NAME.str);

    const System_tables::Types *table_type= System_tables::instance()->
      find_type(schema_name, table_name);

    /*
      Create innodb stats table with 5.7 definition in case of upgrade.
      Check for innodb stats table by string comparision as other plugins might
      create dictionary tables.
    */
    bool is_innodb_stats_table= (table_type != nullptr) &&
                                (*table_type == System_tables::Types::SUPPORT);
    is_innodb_stats_table &=
      (strcmp(table_name.c_str(), "innodb_table_stats") == 0) ||
      (strcmp(table_name.c_str(), "innodb_index_stats") == 0);

    if (is_dd_upgrade && is_innodb_stats_table)
    {
      if (execute_query(thd, stats_table_def(table_name)))
      {
        error= true;
        break;
      }
    }

    else if (table_def == nullptr ||
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

  // Set iterator to end of system tables
  if (last_table != nullptr)
    *last_table= System_tables::instance()->end();
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
      return end_transaction(thd, true);

    // Acquire the DD table objects.
    for (System_tables::Const_iterator it= System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it)
    {
      const dd::Table *dd_table= nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return end_transaction(thd, true);
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
      return end_transaction(thd, true);

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
        return end_transaction(thd, true);

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
        return end_transaction(thd, true);

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
        return end_transaction(thd, true);

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
        return end_transaction(thd, true);

      std::unique_ptr<Tablespace_impl> tspace_clone(
              dynamic_cast<Tablespace_impl*>(tspace->clone()));

      // We must set the ID to INVALID to enable storing the object.
      tspace_clone->set_id(INVALID_OBJECT_ID);
      if (dd::cache::Storage_adapter::instance()->store(thd,
            static_cast<Tablespace*>(tspace_clone.get())))
        return end_transaction(thd, true);

      // Only the DD tablespace is needed to handle cache misses, so we can
      // just drop the predefined tablespaces from the core registry now that
      // it has been persisted.
      (void) dd::cache::Storage_adapter::instance()->core_drop(thd, tspace);
    }
  }

  // Now, the auto releaser has released the objects, and we can go ahead and
  // reset the shared cache.
  dd::cache::Shared_dictionary_cache::instance()->reset(true);
  bootstrap_stage= bootstrap::BOOTSTRAP_SYNCED;

  return end_transaction(thd, false);
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
      return end_transaction(thd, true);

    // Acquire the DD table objects.
    for (System_tables::Const_iterator it= System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it)
    {
      const dd::Table *dd_table= nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return end_transaction(thd, true);
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
      return end_transaction(thd, true);

    if (dd_tspace)
    {
      Tablespace::name_key_type tspace_key;
      dd_tspace->update_name_key(&tspace_key);
      if (dd::cache::Storage_adapter::instance()->core_sync(thd, tspace_key,
                                                            dd_tspace))
        return end_transaction(thd, true);
    }

    // Get the synced DD schema object id. Needed for the DD table name keys.
    Object_id dd_schema_id= cache::Storage_adapter::instance()->
      core_get_id<Schema>(thd, schema_key);

    // Sync the DD tables.
    for (System_tables::Const_iterator it= System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it)
    {
      Abstract_table::name_key_type table_key;
      const dd::Table *dd_table= nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return end_transaction(thd, true);

      Abstract_table::update_name_key(&table_key, dd_schema_id,
                                      dd_table->name());
      dd::cache::Storage_adapter::instance()->core_drop(thd, dd_table);
      if ((*it)->property() == System_tables::Types::CORE)
        if (dd::cache::Storage_adapter::instance()->core_sync(thd, table_key,
                                                              dd_table))
          return end_transaction(thd, true);
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
        return end_transaction(thd, true);

      dd::cache::Storage_adapter::instance()->core_drop(thd, tspace);
    }
  }

  // Now, the auto releaser has released the objects, and we can go ahead and
  // reset the shared cache.
  dd::cache::Shared_dictionary_cache::instance()->reset(true);
  bootstrap_stage= bootstrap::BOOTSTRAP_SYNCED;

  // Commit and flush tables to force re-opening using the refreshed meta data.
  if (end_transaction(thd, false) || execute_query(thd, "FLUSH TABLES"))
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
      return end_transaction(thd, true);

    std::vector<dd::String_type> stmt= table_def->dml_populate_statements();
    for (std::vector<dd::String_type>::iterator stmt_it= stmt.begin();
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

    if (table->hidden() != (*it)->entity()->hidden())
    {
      table->set_hidden((*it)->entity()->hidden());
      if (thd->dd_client()->update(table))
        return end_transaction(thd, true);
    }
    end_transaction(thd, false);
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
  Verify that the storage adapter contains the core DD objects and
  nothing else.
*/
bool verify_core_objects_present(THD *thd)
{
  // Verify that the DD schema is present, and get its id.
  Schema::name_key_type schema_key;
  Schema::update_name_key(&schema_key, MYSQL_SCHEMA_NAME.str);
  Object_id dd_schema_id= cache::Storage_adapter::instance()->
    core_get_id<Schema>(thd, schema_key);

  DBUG_ASSERT(dd_schema_id != INVALID_OBJECT_ID);
  if (dd_schema_id == INVALID_OBJECT_ID)
  {
    sql_print_error("Unable to start server. The data dictionary schema "
                    "'%s' does not exist.", MYSQL_SCHEMA_NAME.str);
    return end_transaction(thd, true);
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
      core_get_id<Table>(thd, table_key);

    DBUG_ASSERT(dd_table_id != INVALID_OBJECT_ID);
    if (dd_table_id == INVALID_OBJECT_ID)
    {
      sql_print_error("Unable to start server. The data dictionary table "
                      "'%s' does not exist.", (*it)->entity()->name().c_str());
      return end_transaction(thd, true);
    }
  }
  DBUG_ASSERT(cache::Storage_adapter::instance()->
          core_size<Abstract_table>() == n_core_tables);

  // Verify that the system tablespace is present.
#ifndef DBUG_OFF
  Tablespace::name_key_type tspace_key;
  Tablespace::update_name_key(&tspace_key, MYSQL_TABLESPACE_NAME.str);
  Object_id dd_tspace_id= cache::Storage_adapter::instance()->
    core_get_id<Tablespace>(thd, tspace_key);
#endif

  /*
    TODO: No DD tablespace yet. Enable verification later when InnoDB
    worklogs are pushed.

  DBUG_ASSERT(dd_tspace_id != INVALID_OBJECT_ID);
  if (dd_tspace_id == INVALID_OBJECT_ID)
  {
    sql_print_error("Unable to start server. The data dictionary tablespace "
                    "'%s' does not exist.", MYSQL_TABLESPACE_NAME.str);
    return end_transaction(thd, true);
  }
  */

  DBUG_ASSERT(cache::Storage_adapter::instance()->
          core_size<Tablespace>() == (dd_tspace_id != INVALID_OBJECT_ID));

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


// Initialize the data dictionary.
static bool initialize_dictionary(THD *thd, bool is_dd_upgrade,
                                  Dictionary_impl *d,
                                  System_tables::Const_iterator *last_table)
{
  create_predefined_tablespaces(thd);
  if (create_dd_schema(thd) ||
      create_tables(thd, is_dd_upgrade, last_table) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_SERVER,
                        d->get_target_dd_version()) ||
      execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
      flush_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_TABLESPACES,
                        d->get_target_dd_version()) ||
      populate_tables(thd) ||
      add_cyclic_foreign_keys(thd) ||
      store_server_I_S_table_meta_data(thd) ||
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
      initialize_dictionary(thd, false, d, nullptr))
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

  if (create_dd_schema(thd) ||
      create_tables(thd, false, nullptr) ||
      sync_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd)) ||
      repopulate_charsets_and_collations(thd) ||
      verify_core_objects_present(thd))
    return true;

  sql_print_information("Found data dictionary with version %d",
                        d->get_actual_dd_version(thd));
  return false;
}


bool store_plugin_IS_table_metadata(THD *thd)
{
  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.tx_read_only= false;
  thd->tx_read_only= false;

  Disable_autocommit_guard autocommit_guard(thd);

  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  bool  error= plugin_foreach(thd, store_schema_table_meta_data,
                              MYSQL_INFORMATION_SCHEMA_PLUGIN, nullptr);

  return end_transaction(thd, error);
}


/**
  Bootstrap thread executes SQL statements.
  Any error in the execution of SQL statements causes call to my_error().
  At this moment, error handler hook is set to my_message_stderr.
  my_message_stderr() prints the error messages to standard error stream but
  it does not follow the standard error format. Further, the error status is
  not set in Diagnostics Area.

  This class is to create RAII error handler hooks to be used when executing
  statements from bootstrap thread.

  It will print the error in the standard error format.
  Diagnostics Area error status will be set to avoid asserts.
  Error will be handler by caller function.
*/

class Bootstrap_error_handler
{
private:
  void (*m_old_error_handler_hook)(uint, const char *, myf);


  /**
    Set the error in DA. Optionally print error in log.
  */
  static void my_message_bootstrap(uint error, const char *str, myf MyFlags)
  {
    my_message_sql(error, str, MyFlags | (m_log_error ? ME_ERRORLOG : 0));
  }

public:
  Bootstrap_error_handler()
  {
    m_old_error_handler_hook= error_handler_hook;
    error_handler_hook= my_message_bootstrap;
  }

  void set_log_error(bool log_error)
  {
    m_log_error= log_error;
  }

  ~Bootstrap_error_handler()
  {
    error_handler_hook= m_old_error_handler_hook;
  }
  static bool m_log_error;
};
bool Bootstrap_error_handler::m_log_error= true;

// Delete dictionary tables
static void delete_dictionary_and_cleanup(THD *thd,
              const System_tables::Const_iterator &last_table)
{

  // RAII to handle error messages.
  Bootstrap_error_handler bootstrap_error_handler;

  // Set flag to delete DD tables only in SE and not from DD cache.
  dd_upgrade_flag= true;

  // Delete DD tables and SDI files.
  drop_dd_tables_and_sdi_files(thd, last_table);

  dd_upgrade_flag= false;
}


// Initialize DD in case of upgrade.
bool upgrade_do_pre_checks_and_initialize_dd(THD *thd)
{
  // Set both variables false in the beginning
  dd_upgrade_flag= false;
  opt_initialize= false;

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
    Each step in the process below is committed independently,
    either implicitly (for e.g. "CREATE TABLE") or explicitly (for the
    operations in the "populate()" methods). Thus, there is no need to
    commit explicitly here.
  */
  if (DDSE_dict_init(thd, DICT_INIT_CHECK_FILES,
                     d->get_target_dd_version()))
  {
    sql_print_error("Failed to initialize DD Storage Engine");
    return true;
  }

  // RAII to handle error messages.
  Bootstrap_error_handler bootstrap_error_handler;

  Key_length_error_handler key_error_handler;

  create_predefined_tablespaces(thd);
  // This will create dd::Schema object for mysql schema
  // TODO: Enable while fixing upgrade code.
  //if (create_dd_schema(thd))
  //  return true;

  // Mark flag true as DD creation uses it to get version number
  opt_initialize= true;
  // Mark flag true to escape creation of tables inside SE.
  dd_upgrade_skip_se= true;

  /*
    This will create dd::Table objects for DD tables in DD cache.
    Tables will not be created inside SE.

    Ignore ER_TOO_LONG_KEY for dictionary tables. Do not print the error in
    error log as we are creating only the cached objects and not physical
    tables.
    TODO: Workaround due to bug#20629014. Remove when the bug is fixed.
  */
  thd->push_internal_handler(&key_error_handler);
  bootstrap_error_handler.set_log_error(false);
  bool error =false;
  // TODO: Enable while fixing upgrade code.
  //error= create_tables(thd, dd_upgrade_flag, nullptr);
  bootstrap_error_handler.set_log_error(true);
  thd->pop_internal_handler();
  if (error)
    return true;

  // Disable InnoDB warning in case it does not find mysql.version table, and
  // ignore error at the SQL layer.
  ulong saved_verbosity= log_error_verbosity;
  log_error_verbosity= 1;
  No_such_table_error_handler error_handler;
  thd->push_internal_handler(&error_handler);
  bootstrap_error_handler.set_log_error(false);

  bool exists= false;
  // TODO: Enable while fixing upgrade code.
  exists= true;
  uint dd_version= 1;
  //d->get_actual_dd_version(thd, &exists);

  // Reset log error verbosity and pop internal error handler.
  log_error_verbosity= saved_verbosity;
  thd->pop_internal_handler();
  bootstrap_error_handler.set_log_error(true);

  if (exists)
  {
    if (dd_version == d->get_target_dd_version())
    {
      /*
        Delete dd::Schema and dd::Table objects from DD Cache to proceed for
        normal server startup.
      */
      // TODO: Enable while fixing upgrade code.
      // dd::cache::Shared_dictionary_cache::instance()->reset(false);
      opt_initialize= false;
      /*
        Ignore ER_TOO_LONG_KEY for dictionary tables during restart.
        Do not print the error in error log as we are creating only the
        cached objects and not physical tables.
        TODO: Workaround due to bug#20629014. Remove when the bug is fixed.
      */
      thd->push_internal_handler(&key_error_handler);
      bootstrap_error_handler.set_log_error(false);
      error= restart(thd);
      bootstrap_error_handler.set_log_error(true);
      thd->pop_internal_handler();
      return error;
    }
    else
    {
      /*
        This branch has to be extended in the future to handle cases
        when upgrading between different versions of DD.
      */
      sql_print_error("Found partially upgraded DD. Aborting upgrade and "
                      "deleting all DD tables. Start the upgrade process "
                      "again.");
      delete_dictionary_and_cleanup(thd);
      return true;
     }
  }

  /*
   "Create New DD tables in DD and storage engine. Mark dd_upgrade_flag
    to true to escape creation of mysql.innodb_index_stats and
    mysql.innodb_table_stats inside SE.
  */
  dd_upgrade_skip_se= false;
  dd_upgrade_flag= true;

  // Delete dd::Table objects from DD Cache to proceed for upgrade.
  dd::cache::Shared_dictionary_cache::instance()->reset(false);

  if (check_for_dd_tables())
  {
    sql_print_error("Found .frm file with same name as one of the "
                    " Dictionary Tables.");
    return true;
  }

  /*
    Ignore ER_TOO_LONG_KEY for dictionary tables creation.
    TODO: Workaround due to bug#20629014. Remove when the bug is fixed.
  */
  thd->push_internal_handler(&key_error_handler);

  bootstrap_stage= bootstrap::BOOTSTRAP_STARTED;
  System_tables::Const_iterator last_table= System_tables::instance()->begin();
  if (initialize_dictionary(thd, dd_upgrade_flag, d, &last_table))
  {
    thd->pop_internal_handler();
    delete_dictionary_and_cleanup(thd, last_table);
    return true;
  }
  thd->pop_internal_handler();

  error= execute_query(thd, "UPDATE version SET version=0");
  // Commit the statement based population.
  error|= end_transaction(thd, error);

  if (error)
  {
    sql_print_error("Failed to set version number in version table.");
    delete_dictionary_and_cleanup(thd);
    return true;
  }

  sql_print_information("Created Data Dictionary for upgrade");

  opt_initialize= false;

  // Migrate meta data of plugin table to DD.
  // It is used in plugin initialization.
  if (migrate_plugin_table_to_dd(thd))
  {
    delete_dictionary_and_cleanup(thd);
    return true;
  }

  return false;
}


// Server Upgrade from 5.7
bool upgrade_fill_dd_and_finalize(THD *thd)
{
  bool error= false;

  // RAII to handle error messages.
  Bootstrap_error_handler bootstrap_error_handler;

  std::vector<dd::String_type> db_name;
  std::vector<dd::String_type>::iterator it;

  if (find_schema_from_datadir(thd, &db_name))
  {
    delete_dictionary_and_cleanup(thd);
    return true;
  }

  // Upgrade schema and tables, create view without resolving dependency
  for (it= db_name.begin(); it != db_name.end(); it++)
  {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    bool exists= false;
    dd::schema_exists(thd, it->c_str(), &exists);

    if (!exists && migrate_schema_to_dd(thd, it->c_str()))
    {
      delete_dictionary_and_cleanup(thd);
      return true;
    }

    if (find_files_with_metadata(thd, it->c_str(), false))
    {
      // Don't return from here, we want to print all error to error log
      error|= true;
    }
  }

  error|= migrate_events_to_dd(thd);
  error|= migrate_routines_to_dd(thd);

  if (error)
  {
    delete_dictionary_and_cleanup(thd);
    return true;
  }

  // We will not get error in this step unless its a fatal error.
  for (it= db_name.begin(); it != db_name.end(); it++)
  {
    // Upgrade view resolving dependency
    if (find_files_with_metadata(thd, it->c_str(), true))
    {
      // Don't return from here, we want to print all error to error log.
      error= true;
    }
  }

  if (error)
  {
    delete_dictionary_and_cleanup(thd);
    return true;
  }

  // Continue with server startup.
  bootstrap_stage= bootstrap::BOOTSTRAP_CREATED;

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.tx_read_only= false;
  thd->tx_read_only= false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d= dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);

  /*
    ALTER innodb stats table according to new definition.
    We do it after everything else is upgraded as it changes the ibd files.

    Ignore ER_TOO_LONG_KEY for dictionary tables operation.
    TODO: Workaround due to bug#20629014. Remove when the bug is fixed.
  */
  Key_length_error_handler key_error_handler;
  thd->push_internal_handler(&key_error_handler);
  if (execute_query(thd, "ALTER TABLE mysql.innodb_table_stats CHANGE table_name "
                       "table_name VARCHAR(199) COLLATE utf8_bin NOT NULL") ||
      execute_query(thd, "ALTER TABLE mysql.innodb_index_stats CHANGE table_name "
                       "table_name VARCHAR(199) COLLATE utf8_bin NOT NULL"))
  {
    sql_print_error("Error in modifying definition of innodb stats tables");
    thd->pop_internal_handler();
    delete_dictionary_and_cleanup(thd);
    return true;
  }
  thd->pop_internal_handler();

  // Write the server version to indicate completion of upgrade.
  dd::String_type update_version_query= "UPDATE mysql.version SET version=";
  std::string tdv= std::to_string(d->get_target_dd_version());
  update_version_query.append(tdv.begin(), tdv.end());

  error= execute_query(thd, update_version_query);
  // Commit the statement based population.
  error|= end_transaction(thd, error);

  if (error)
  {
    sql_print_error("Failed to set version number in version table.");
    delete_dictionary_and_cleanup(thd);
    return true;
  }

  sql_print_information("Finished populating Data Dictionary tables with data.");

  // Upgrade is successful, create a backup.
  create_metadata_backup(thd);

  /*
    Mark upgrade flag false after create_metadata_backup as RENAME statement
    in the function can fail and we need to set error status in DA after that
    based on this flag.
  */
  dd_upgrade_flag= false;

  DBUG_ASSERT(d->get_target_dd_version() == d->get_actual_dd_version(thd));

  if (sync_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd)) ||
      repopulate_charsets_and_collations(thd) ||
      verify_core_objects_present(thd))
  {
    delete_dictionary_and_cleanup(thd);
    return true;
  }
  else
  {
    sql_print_information("Found data dictionary with version %d",
                          d->get_actual_dd_version(thd));

  }

  return false;
}


// Delete Dictionary tables
bool delete_dictionary_and_cleanup(THD *thd)
{
  // Delete DD tables and SDI files.
  delete_dictionary_and_cleanup(thd, System_tables::instance()->end());
  return false;
}


} // namespace bootstrap
} // namespace dd
