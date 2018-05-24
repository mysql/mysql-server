/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/impl/bootstrapper.h"

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "lex_string.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql_version.h"  // MYSQL_VERSION_ID
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd.h"                       // dd::create_object
#include "sql/dd/impl/bootstrap_ctx.h"       // DD_bootstrap_ctx
#include "sql/dd/impl/cache/shared_dictionary_cache.h"  // Shared_dictionary_cache
#include "sql/dd/impl/cache/storage_adapter.h"          // Storage_adapter
#include "sql/dd/impl/dictionary_impl.h"                // dd::Dictionary_impl
#include "sql/dd/impl/raw/object_keys.h"
#include "sql/dd/impl/sdi.h"                    // dd::sdi::store
#include "sql/dd/impl/system_registry.h"        // dd::System_tables
#include "sql/dd/impl/tables/character_sets.h"  // dd::tables::Character_sets
#include "sql/dd/impl/tables/collations.h"      // dd::tables::Collations
#include "sql/dd/impl/tables/dd_properties.h"   // dd::tables::DD_properties
#include "sql/dd/impl/tables/foreign_key_column_usage.h"  // dd::tables::Fore...
#include "sql/dd/impl/tables/foreign_keys.h"    // dd::tables::Foreign_keys
#include "sql/dd/impl/tables/tables.h"          // dd::tables::Tables
#include "sql/dd/impl/types/schema_impl.h"      // dd::Schema_impl
#include "sql/dd/impl/types/table_impl.h"       // dd::Table_impl
#include "sql/dd/impl/types/tablespace_impl.h"  // dd::Table_impl
#include "sql/dd/object_id.h"
#include "sql/dd/types/abstract_table.h"
#include "sql/dd/types/object_table.h"             // dd::Object_table
#include "sql/dd/types/object_table_definition.h"  // dd::Object_table_definition
#include "sql/dd/types/schema.h"
#include "sql/dd/types/table.h"
#include "sql/dd/types/tablespace.h"
#include "sql/dd/types/tablespace_file.h"  // dd::Tablespace_file
#include "sql/dd/upgrade/upgrade.h"        // dd::migrate_event_to_dd
#include "sql/error_handler.h"             // No_such_table_error_handler
#include "sql/handler.h"                   // dict_init_mode_t
#include "sql/log.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"
#include "sql/plugin_table.h"
#include "sql/sql_base.h"   // close_thread_tables
#include "sql/sql_class.h"  // THD
#include "sql/sql_list.h"
#include "sql/sql_prepare.h"  // Ed_connection
#include "sql/stateless_allocator.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/transaction.h"  // trans_rollback

// Execute a single SQL query.
bool execute_query(THD *thd, const dd::String_type &q_buf) {
  Ed_connection con(thd);
  LEX_STRING str;
  thd->make_lex_string(&str, q_buf.c_str(), q_buf.length(), false);
  if (con.execute_direct(str)) {
    // Report error to log file during bootstrap.
    if (dd::bootstrap::DD_bootstrap_ctx::instance().get_stage() <
        dd::bootstrap::Stage::FINISHED) {
      LogErr(ERROR_LEVEL, ER_DD_INITIALIZE_SQL_ERROR, q_buf.c_str(),
             con.get_last_errno(), con.get_last_error());
    }
    return true;
  }
  return false;
}

using namespace dd;

///////////////////////////////////////////////////////////////////////////

namespace dd {

// Helper function to do rollback or commit, depending on error.
bool end_transaction(THD *thd, bool error) {
  if (error) {
    // Rollback the statement before we can rollback the real transaction.
    trans_rollback_stmt(thd);
    trans_rollback(thd);
  } else if (trans_commit_stmt(thd) || trans_commit(thd)) {
    error = true;
    trans_rollback(thd);
  }

  // Close tables etc. and release MDL locks, regardless of error.
  close_thread_tables(thd);
  thd->mdl_context.release_transactional_locks();
  return error;
}

}  // end namespace dd

namespace {

// Initialize recovery in the DDSE.
bool DDSE_dict_recover(THD *thd, dict_recovery_mode_t dict_recovery_mode,
                       uint version) {
  handlerton *ddse = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->dict_recover == nullptr) return true;

  bool error = ddse->dict_recover(dict_recovery_mode, version);

  /*
    Commit when tablespaces have been initialized, since in that
    case, tablespace meta data is added.
  */
  if (dict_recovery_mode == DICT_RECOVERY_INITIALIZE_TABLESPACES)
    return dd::end_transaction(thd, error);

  return error;
}

// Create meta data of the predefined tablespaces.
void store_predefined_tablespace_metadata(THD *thd) {
  /*
    Create dd::Tablespace objects and store them (which will add their meta
    data to the storage adapter registry of DD entities). The tablespaces
    are already created physically in the DDSE, so we only need to create
    the corresponding meta data.
  */
  for (System_tablespaces::Const_iterator it =
           System_tablespaces::instance()->begin();
       it != System_tablespaces::instance()->end(); ++it) {
    const Plugin_tablespace *tablespace_def = (*it)->entity();

    // Create the dd::Tablespace object.
    std::unique_ptr<Tablespace> tablespace(dd::create_object<Tablespace>());
    tablespace->set_name(tablespace_def->get_name());
    tablespace->set_options_raw(tablespace_def->get_options());
    tablespace->set_se_private_data_raw(tablespace_def->get_se_private_data());
    tablespace->set_engine(tablespace_def->get_engine());

    // Loop over the tablespace files, create dd::Tablespace_file objects.
    List<const Plugin_tablespace::Plugin_tablespace_file> files =
        tablespace_def->get_files();
    List_iterator<const Plugin_tablespace::Plugin_tablespace_file> file_it(
        files);
    const Plugin_tablespace::Plugin_tablespace_file *file = NULL;
    while ((file = file_it++)) {
      Tablespace_file *space_file = tablespace->add_file();
      space_file->set_filename(file->get_name());
      space_file->set_se_private_data_raw(file->get_se_private_data());
    }

    /*
      Here, we just want to populate the core registry in the storage
      adapter. We do not want to have the object registered in the
      uncommitted registry, this will only add complexity to the
      DD cache usage during bootstrap. Thus, we call the storage adapter
      directly instead of going through the dictionary client.
    */
    dd::cache::Storage_adapter::instance()->store(thd, tablespace.get());
  }
  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::CREATED_TABLESPACES);
}

// Create and use the dictionary schema.
bool create_dd_schema(THD *thd) {
  return execute_query(thd, dd::String_type("CREATE SCHEMA ") +
                                dd::String_type(MYSQL_SCHEMA_NAME.str) +
                                dd::String_type(" DEFAULT COLLATE '") +
                                dd::String_type(default_charset_info->name) +
                                "'") ||
         execute_query(thd, dd::String_type("USE ") +
                                dd::String_type(MYSQL_SCHEMA_NAME.str));
}

/*
  Update the System_tables registry with meta data from 'dd_properties'.
  Iterate over the tables in the DD_properties. If this is minor downgrade,
  add new tables that were added in the newer version to the System_tables
  registry. If this is not minor downgrade, assert that all tables in the
  DD_properties indeed have a corresponding entry in the System_tables
  registry.
*/
bool update_system_tables(THD *thd) {
  std::unique_ptr<dd::Properties> system_tables_props;
  bool exists = false;

  if (dd::tables::DD_properties::instance().get(
          thd, "SYSTEM_TABLES", &system_tables_props, &exists) ||
      !exists) {
    my_error(ER_DD_INIT_FAILED, MYF(0));
    return true;
  }

  for (dd::Properties::Iterator it = system_tables_props->begin();
       it != system_tables_props->end(); ++it) {
    // Check if this is a CORE, INERT, SECOND or DDSE table.
    if (!dd::get_dictionary()->is_dd_table_name(MYSQL_SCHEMA_NAME.str,
                                                it->first)) {
      if (bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade()) {
        /*
          Add tables as type CORE regardless of the actual type, which
          is irrelevant in this case.
        */
        System_tables::instance()->add(MYSQL_SCHEMA_NAME.str, it->first,
                                       System_tables::Types::CORE, nullptr);
      } else {
        my_error(ER_DD_METADATA_NOT_FOUND, MYF(0), it->first.c_str());
        return true;
      }
    } else {
      /*
        The table is a known DD table. Then, we get its definition
        and add it to the Object_table instance. The definition might
        not exist if the table was added after the version that we
        are upgrading from.
      */
      String_type tbl_prop_str;
      if (!system_tables_props->exists(it->first) ||
          system_tables_props->get(it->first, tbl_prop_str))
        continue;

      const Object_table *table_def = System_tables::instance()->find_table(
          MYSQL_SCHEMA_NAME.str, it->first);
      DBUG_ASSERT(table_def);

      std::unique_ptr<dd::Properties> tbl_props(
          Properties::parse_properties(tbl_prop_str));

      String_type def;
      if (tbl_props->get(dd::tables::DD_properties::dd_key(
                             dd::tables::DD_properties::DD_property::DEF),
                         def)) {
        my_error(ER_DD_METADATA_NOT_FOUND, MYF(0), it->first.c_str());
        return true;
      }
      std::unique_ptr<Properties> table_def_properties(
          Properties::parse_properties(def));
      table_def->set_actual_table_definition(*table_def_properties);
    }
  }

  return false;
}

/*
  During --initialize, we create the dd_properties table. During restart,
  create its meta data, and use it to open and read its contents.
*/
bool initialize_dd_properties(THD *thd) {
  // Create the dd_properties table.
  const Object_table_definition *dd_properties_def =
      dd::tables::DD_properties::instance().target_table_definition();
  if (execute_query(thd, dd_properties_def->get_ddl())) return true;

  /*
    We can now decide which version number we will use for the DD, and
    initialize the DD_bootstrap_ctx with the relevant version number.
  */
  uint actual_version = dd::DD_VERSION;

  bootstrap::DD_bootstrap_ctx::instance().set_actual_dd_version(actual_version);

  if (!opt_initialize) {
    bool exists = false;
    // Check 'DD_version' too in order to catch an upgrade from 8.0.3.
    if (dd::tables::DD_properties::instance().get(thd, "DD_VERSION",
                                                  &actual_version, &exists) ||
        !exists) {
      LogErr(ERROR_LEVEL, ER_DD_NO_VERSION_FOUND);
      return true;
    }

    /* purecov: begin inspected */
    if (actual_version != dd::DD_VERSION) {
      bootstrap::DD_bootstrap_ctx::instance().set_actual_dd_version(
          actual_version);
      if (opt_no_dd_upgrade) {
        LogErr(ERROR_LEVEL, ER_DD_UPGRADE_OFF);
        return true;
      }

      if (!bootstrap::DD_bootstrap_ctx::instance().supported_dd_version()) {
        /*
          If we are attempting on minor downgrade, make sure this is
          supported.
        */
        if (!bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade()) {
          LogErr(ERROR_LEVEL, ER_DD_UPGRADE_VERSION_NOT_SUPPORTED,
                 actual_version);
          return true;
        }

        uint minor_downgrade_threshold = 0;
        if (dd::tables::DD_properties::instance().get(
                thd, "MINOR_DOWNGRADE_THRESHOLD", &minor_downgrade_threshold,
                &exists) ||
            !exists || minor_downgrade_threshold > dd::DD_VERSION) {
          LogErr(ERROR_LEVEL, ER_DD_MINOR_DOWNGRADE_VERSION_NOT_SUPPORTED,
                 actual_version);
          return true;
        }
      }
    }
    /* purecov: end */

    /*
      Reject restarting with a changed LCTN setting, since the collation
      for LCTN-dependent columns is decided during server initialization.
    */
    uint actual_lctn = 0;
    exists = false;
    if (dd::tables::DD_properties::instance().get(thd, "LCTN", &actual_lctn,
                                                  &exists) ||
        !exists) {
      LogErr(WARNING_LEVEL, ER_LCTN_NOT_FOUND, lower_case_table_names);
    } else if (actual_lctn != lower_case_table_names) {
      LogErr(ERROR_LEVEL, ER_LCTN_CHANGED, lower_case_table_names, actual_lctn);
      return true;
    }
  }

  if (bootstrap::DD_bootstrap_ctx::instance().is_initialize())
    LogErr(INFORMATION_LEVEL, ER_DD_INITIALIZE, dd::DD_VERSION);
  else if (bootstrap::DD_bootstrap_ctx::instance().is_restart())
    LogErr(INFORMATION_LEVEL, ER_DD_RESTART, dd::DD_VERSION);
  else if (bootstrap::DD_bootstrap_ctx::instance().is_upgrade())
    LogErr(INFORMATION_LEVEL, ER_DD_UPGRADE, actual_version, dd::DD_VERSION);
  else if (bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade())
    LogErr(INFORMATION_LEVEL, ER_DD_MINOR_DOWNGRADE, actual_version,
           dd::DD_VERSION);
  else
    DBUG_ASSERT(false);

  /*
    Unless this is initialization or restart, we must update the
    System_tables registry with the information from the 'dd_properties'
    regarding the actual DD tables.
  */
  if (!bootstrap::DD_bootstrap_ctx::instance().is_initialize() &&
      !bootstrap::DD_bootstrap_ctx::instance().is_restart() &&
      update_system_tables(thd)) {
    return true;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::FETCHED_PROPERTIES);

  return false;
}

// Create a DD table using the target table definition.
bool create_target_table(THD *thd, const Object_table *object_table) {
  DBUG_ASSERT(object_table != nullptr);

  /*
   The target table definition may not be present if the table
   is abandoned. That's ok, not an error.
  */
  if (object_table->is_abandoned()) return false;

  String_type target_ddl_statement("");
  const Object_table_definition *target_table_def =
      object_table->target_table_definition();

  DBUG_ASSERT(target_table_def != nullptr);
  target_ddl_statement = target_table_def->get_ddl();
  DBUG_ASSERT(!target_ddl_statement.empty());

  return execute_query(thd, target_ddl_statement);
}

// Create a DD table using the actual table definition.
/* purecov: begin inspected */
bool create_actual_table(THD *thd, const Object_table *object_table) {
  /*
    For minor downgrade, tables might have been added in the upgraded
    server that we do not have any Object_table instance for. In that
    case, we just skip them.
  */
  if (object_table == nullptr) {
    DBUG_ASSERT(bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade());
    return false;
  }

  String_type actual_ddl_statement("");
  const Object_table_definition *actual_table_def =
      object_table->actual_table_definition();

  /*
    The actual definition may not be present. This will happen during
    upgrade if the new DD version adds a new DD table which was not
    present in the DD we are upgrading from. This is OK, not an error.
  */
  if (actual_table_def == nullptr) return false;

  actual_ddl_statement = actual_table_def->get_ddl();
  DBUG_ASSERT(!actual_ddl_statement.empty());

  return execute_query(thd, actual_ddl_statement);
}
/* purecov: end */

/**
  Predicate to check if a table type is a non-inert DD ot DDSE table.

  @param table_type    Type as defined in the System_tables registry.
  @returns             true if the table is a non-inert DD or DDSE table,
                       false otherwise
*/
bool is_non_inert_dd_or_ddse_table(System_tables::Types table_type) {
  return table_type == System_tables::Types::CORE ||
         table_type == System_tables::Types::SECOND ||
         table_type == System_tables::Types::DDSE_PRIVATE ||
         table_type == System_tables::Types::DDSE_PROTECTED;
}

/**
  Execute SQL statements to create the DD tables.

  The tables created here will be a subset of the target DD tables for this
  DD version. This function is called in the following four cases:

  1. When a server is started the first time, with --initialize. Then, we
     will iterate over all target tables and create them. This will also
     make them be created physically in the DDSE.
  2. When a server is restarted, and the data directory contains a dictionary
     with the same DD version as the target DD version of the starting server.
     In this case, we will iterate over all target tables and create them,
     using the target table SQL DDL definitions. This is done only to create
     the meta data, though; the tables will not be created physically in the
     DDSE since they already exist. But we need to create the meta data to be
     able top open them.
  3. When a server is restarted, and the data directory was last used by a
     more recent MRU within the same GA with a higher target DD version.
     This is considered a 'minor downgrade'. In this case, the restarting
     server will continue to run using the more recent DD version. This is
     possible since only a subset of DD changes are allowed in a DD upgrade
     that can also be downgraded. However, it means that we must create the
     meta data reflecting the *actual* tables, not the target tables. So in
     this case, we iterate over the target tables, but execute the DDL
     statements of the actual tables. We get these statements from the
     'dd_properties' table, where the more recent MRU has stored them.
  4. When a server is restarted, and the data directory was last used by a
     server with a DD version from which the starting server can upgrade. In
     this case, this function is called three times:

     - The first time, we need to create the meta data reflecting the actual
       tables in the persistent DD. This is needed to be able to open the DD
       tables and read the data. This is similar to use case 3. above.
     - The second time, we create the tables that are modified in the new DD
       version. Here, the tables are also created physically in the DDSE.
       In this case, the 'create_set' specifies which subset of the target
       tables should be created. After this stage, we replace the meta data
       in 'dd_properties' by new meta data reflecting the modified tables. We
       also replace the version numbers to make sure a new restart will use
       the upgraded DD.
     - The third time, we do the same as in case 2 above. This is basically
       the same as a shutdown and restart of the server after upgrade was
       completed.

  @param  thd         Thread context.
  @param  create_set  Subset of the target tables which should be created
                      during upgrade.

  @returns false if success, otherwise true.
*/
bool create_tables(THD *thd, const std::set<String_type> *create_set) {
  // Turn off FK checks, this is needed since we have cyclic FKs.
  if (execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0")) return true;

  /*
    Decide whether we should create actual or target tables. For plain
    restart and initialize, we create the target tables. For the second
    table creation stage during upgrade, we also create target tables.
    So we create the actual tables only during the first table creation
    stage for upgrade, and for minor downgrade.
  */
  bool create_target_tables = true;
  if (bootstrap::DD_bootstrap_ctx::instance().get_stage() ==
          bootstrap::Stage::FETCHED_PROPERTIES &&
      (bootstrap::DD_bootstrap_ctx::instance().is_upgrade() ||
       bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade()))
    create_target_tables = false;

  /*
    Iterate over DD tables and create the tables. Note that we do not iterate
    over INERT tables here, there is currently only one INERT table (the
    'dd_properties'), and it is created in 'initialize_dd_properties' in
    order to get hold of e.g. version information.
  */
  bool error = false;
  for (System_tables::Const_iterator it = System_tables::instance()->begin();
       it != System_tables::instance()->end() && !error; ++it) {
    if (is_non_inert_dd_or_ddse_table((*it)->property())) {
      /*
        If a create set is submitted, create only the target tables that
        are in the create set.
      */
      if (create_set == nullptr ||
          create_set->find((*it)->entity()->name()) != create_set->end()) {
        /*
          Use the actual or target definition to create the table depending
          on the context.
        */
        if (create_target_tables)
          error = create_target_table(thd, (*it)->entity());
        else
          error = create_actual_table(thd, (*it)->entity());
      }
    }
  }

  // Turn FK checks back on.
  if (error || execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1")) return true;

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::CREATED_TABLES);

  return false;
}

/*
  Acquire exclusive meta data locks for the DD schema, tablespace and
  table names.
*/
bool acquire_exclusive_mdl(THD *thd) {
  // All MDL requests.
  MDL_request_list mdl_requests;

  // Prepare MDL request for the schema name.
  MDL_request schema_request;
  MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA, MYSQL_SCHEMA_NAME.str, "",
                   MDL_EXCLUSIVE, MDL_TRANSACTION);
  mdl_requests.push_front(&schema_request);

  // Prepare MDL request for the tablespace name.
  MDL_request tablespace_request;
  MDL_REQUEST_INIT(&tablespace_request, MDL_key::TABLESPACE, "",
                   MYSQL_TABLESPACE_NAME.str, MDL_EXCLUSIVE, MDL_TRANSACTION);
  mdl_requests.push_front(&tablespace_request);

  // Prepare MDL requests for all tables names.
  for (System_tables::Const_iterator it = System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it) {
    // Skip extraneous tables during minor downgrade.
    if ((*it)->entity() == nullptr) continue;

    MDL_request *table_request = new (thd->mem_root) MDL_request;
    if (table_request == NULL) return true;
    MDL_REQUEST_INIT(table_request, MDL_key::TABLE, MYSQL_SCHEMA_NAME.str,
                     (*it)->entity()->name().c_str(), MDL_EXCLUSIVE,
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
bool flush_meta_data(THD *thd) {
  // Acquire exclusive meta data locks for the relevant DD objects.
  if (acquire_exclusive_mdl(thd)) return true;

  {
    /*
      Use a scoped auto releaser to make sure the cached objects are released
      before the shared cache is reset.
    */
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    /*
      First, we acquire the DD schema and tablespace and keep them in
      local variables. We also clone them, the clones will be used for
      updating the ids. We also acquire all the DD table objects to make
      sure the shared cache is populated, and we keep the original objects
      as well as clones in a vector. The auto releaser will make sure the
      objects are not evicted. This must be ensured since we need to make
      sure the ids stay consistent across all objects in the shared cache.
    */
    const Schema *dd_schema = nullptr;
    const Tablespace *dd_tspace = nullptr;
    std::vector<const Table_impl *> dd_tables;  // Owned by the shared cache.
    std::vector<std::unique_ptr<Table_impl>> dd_table_clones;

    if (thd->dd_client()->acquire(dd::String_type(MYSQL_SCHEMA_NAME.str),
                                  &dd_schema) ||
        thd->dd_client()->acquire(dd::String_type(MYSQL_TABLESPACE_NAME.str),
                                  &dd_tspace))
      return dd::end_transaction(thd, true);

    std::unique_ptr<Schema_impl> dd_schema_clone(
        dynamic_cast<Schema_impl *>(dd_schema->clone()));

    std::unique_ptr<Tablespace_impl> dd_tspace_clone(
        dynamic_cast<Tablespace_impl *>(dd_tspace->clone()));

    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it) {
      /*
        We add nullptr to the dd_tables vector for abandoned
        tables and system tables to have the same number of objects
        in the System_tables list, the dd_tables vector and the
        dd_table_clones vector.
      */
      const dd::Table *dd_table = nullptr;
      if ((*it)->property() != System_tables::Types::SYSTEM &&
          thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return dd::end_transaction(thd, true);

      dd_tables.push_back(dynamic_cast<const Table_impl *>(dd_table));

      /*
        If this is an abandoned table, we can't clone it. Thus, we
        push back a nullptr to make sure we have the same number of
        elements in the dd_table_clones as in the System_tables.
      */
      std::unique_ptr<Table_impl> dd_table_clone;
      if (dd_table != nullptr) {
        dd_table_clones.push_back(std::unique_ptr<Table_impl>(
            dynamic_cast<Table_impl *>(dd_table->clone())));
      } else {
        dd_table_clones.push_back(nullptr);
      }
    }

    /*
      We have now populated the shared cache with the core objects, and kept
      clones of all DD objects. The scoped auto releaser makes sure we will
      not evict the objects from the shared cache until the auto releaser
      exits scope. Thus, within the scope of the auto releaser, we can modify
      the contents of the core registry in the storage adapter without risking
      that this will interfere with the contents of the shared cache, because
      the DD transactions will acquire the core objects from the shared cache.
    */

    /*
      First, we modify and store the DD schema without changing the cached
      copy. We cannot use acquire_for_modification() here, because that
      would make the DD sub-transactions (e.g. when calling store()) see a
      partially modified set of core objects, where e.g. the mysql
      schema object has got its new, real id (from the auto-inc column
      in the dd.schemata table), whereas the core DD table objects still
      refer to the id that was allocated when creating the scaffolding.

      So we first store all the objects persistently, and make sure that
      the on-disk data will have correct and consistent ids. When all objects
      are stored, we update the contents of the core registry in the
      storage adapter to reflect the persisted data. Finally, the shared
      cache is reset so that on next acquisition, the DD objects will be
      fetched from the core registry in the storage adapter.
    */

    /*
      We must set the ID to INVALID to make the object get a fresh ID from
      the auto inc ID column.
    */
    dd_schema_clone->set_id(INVALID_OBJECT_ID);
    dd_tspace_clone->set_id(INVALID_OBJECT_ID);
    if (dd::cache::Storage_adapter::instance()->store(
            thd, static_cast<Schema *>(dd_schema_clone.get())) ||
        dd::cache::Storage_adapter::instance()->store(
            thd, static_cast<Tablespace *>(dd_tspace_clone.get())))
      return dd::end_transaction(thd, true);

    /*
      Now, the DD schema and DD tablespace are stored persistently. We will
      not update the core registry until after we have stored all DD tables.
      At that point, we can update all the core registry objects in one go
      and avoid using a partially update core registry for e.g. object
      acquisition.
    */
    std::vector<std::unique_ptr<Table_impl>>::iterator clone_it =
        dd_table_clones.begin();
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end() &&
         clone_it != dd_table_clones.end();
         ++it, ++clone_it) {
      // Skip abandoned tables and system tables.
      if ((*clone_it) == nullptr ||
          (*it)->property() == System_tables::Types::SYSTEM)
        continue;

      DBUG_ASSERT((*it)->entity()->name() == (*clone_it)->name());

      // We must set the ID to INVALID to let the object get an auto inc ID.
      (*clone_it)->set_id(INVALID_OBJECT_ID);

      /*
        Change the schema and tablespace id to match the ids of the
        persisted objects. Note that this means the persisted DD table
        objects will have consistent IDs, but the IDs in the objects in
        the core registry will not be updated yet.
      */
      (*clone_it)->set_schema_id(dd_schema_clone->id());
      (*clone_it)->set_tablespace_id(dd_tspace_clone->id());
      if (dd::cache::Storage_adapter::instance()->store(
              thd, static_cast<Table *>((*clone_it).get())))
        return dd::end_transaction(thd, true);
    }

    /*
      Update and store the predefined tablespace objects. The DD tablespace
      has already been stored above, so we iterate only over the tablespaces
      of type PREDEFINED_DDSE.
    */
    for (System_tablespaces::Const_iterator it =
             System_tablespaces::instance()->begin(
                 System_tablespaces::Types::PREDEFINED_DDSE);
         it != System_tablespaces::instance()->end();
         it = System_tablespaces::instance()->next(
             it, System_tablespaces::Types::PREDEFINED_DDSE)) {
      const dd::Tablespace *tspace = nullptr;
      if (thd->dd_client()->acquire((*it)->key().second, &tspace))
        return dd::end_transaction(thd, true);

      std::unique_ptr<Tablespace_impl> tspace_clone(
          dynamic_cast<Tablespace_impl *>(tspace->clone()));

      // We must set the ID to INVALID to enable storing the object.
      tspace_clone->set_id(INVALID_OBJECT_ID);
      if (dd::cache::Storage_adapter::instance()->store(
              thd, static_cast<Tablespace *>(tspace_clone.get())))
        return dd::end_transaction(thd, true);

      /*
        Only the DD tablespace is needed to handle cache misses, so we can
        just drop the predefined tablespaces from the core registry now that
        it has been persisted.
      */
      dd::cache::Storage_adapter::instance()->core_drop(thd, tspace);
    }

    /*
      Now, the DD schema and tablespace as well as the DD tables have been
      persisted. The last thing we do before resetting the shared cache is
      to update the contents of the core registry to match the persisted
      objects. First, we update the core registry with the persisted DD
      schema and tablespace.
     */
    dd::cache::Storage_adapter::instance()->core_drop(thd, dd_schema);
    dd::cache::Storage_adapter::instance()->core_store(
        thd, static_cast<Schema *>(dd_schema_clone.get()));

    dd::cache::Storage_adapter::instance()->core_drop(thd, dd_tspace);
    dd::cache::Storage_adapter::instance()->core_store(
        thd, static_cast<Tablespace *>(dd_tspace_clone.get()));

    // Make sure the IDs after storing are as expected.
    DBUG_ASSERT(dd_schema_clone->id() == 1);
    DBUG_ASSERT(dd_tspace_clone->id() == 1);

    /*
      Finally, we update the core registry of the DD tables. This must be
      done in two loops to avoid issues related to overlapping ID sequences.
    */
    std::vector<const Table_impl *>::const_iterator table_it =
        dd_tables.begin();
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end() && table_it != dd_tables.end();
         ++it, ++table_it) {
      // Skip abandoned tables and system tables.
      if ((*table_it) == nullptr ||
          (*it)->property() == System_tables::Types::SYSTEM)
        continue;

      DBUG_ASSERT((*it)->entity()->name() == (*table_it)->name());
      dd::cache::Storage_adapter::instance()->core_drop(
          thd, static_cast<const Table *>(*table_it));
    }

    clone_it = dd_table_clones.begin();
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end() &&
         clone_it != dd_table_clones.end();
         ++it, ++clone_it) {
      // Skip abandoned tables and system tables.
      if ((*clone_it) == nullptr ||
          (*it)->property() == System_tables::Types::SYSTEM)
        continue;

      if ((*it)->property() == System_tables::Types::CORE) {
        DBUG_ASSERT((*it)->entity()->name() == (*clone_it)->name());
        dd::cache::Storage_adapter::instance()->core_store(
            thd, static_cast<Table *>((*clone_it).get()));
      }
    }
  }

  /*
    Now, the auto releaser has released the objects, and we can go ahead and
    reset the shared cache.
  */
  dd::cache::Shared_dictionary_cache::instance()->reset(true);

  if (dd::end_transaction(thd, false)) {
    return true;
  }

  /*
    Use a scoped auto releaser to make sure the objects cached for SDI
    writing, FK parent information reload, and DD property storage are
    released.
  */
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  // Acquire the DD tablespace and write SDI
  const Tablespace *dd_tspace = nullptr;
  if (thd->dd_client()->acquire(dd::String_type(MYSQL_TABLESPACE_NAME.str),
                                &dd_tspace) ||
      dd::sdi::store(thd, dd_tspace)) {
    return dd::end_transaction(thd, true);
  }

  // Acquire the DD schema and write SDI
  const Schema *dd_schema = nullptr;
  if (thd->dd_client()->acquire(dd::String_type(MYSQL_SCHEMA_NAME.str),
                                &dd_schema) ||
      dd::sdi::store(thd, dd_schema)) {
    return dd::end_transaction(thd, true);
  }

  /*
    Acquire the DD table objects and write SDI for them. Also sync from
    the DD tables in order to get the FK parent information reloaded.
  */
  for (System_tables::Const_iterator it = System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it) {
    // Skip system tables.
    if ((*it)->property() == System_tables::Types::SYSTEM) continue;

    const dd::Table *dd_table = nullptr;
    if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                  (*it)->entity()->name(), &dd_table)) {
      return dd::end_transaction(thd, true);
    }

    // Skip abandoned tables.
    if (dd_table == nullptr) continue;

    /*
      Make sure the registry of the core DD objects is updated with an
      object read from the DD tables, with updated FK parent information.
      Store the object to make sure SDI is written.
    */
    Abstract_table::Name_key table_key;
    Abstract_table::update_name_key(&table_key, dd_schema->id(),
                                    dd_table->name());
    const dd::Abstract_table *persisted_dd_table = nullptr;
    if (dd::cache::Storage_adapter::instance()->get(
            thd, table_key, ISO_READ_COMMITTED, true, &persisted_dd_table) ||
        persisted_dd_table == nullptr ||
        dd::sdi::store(thd, dynamic_cast<const Table *>(persisted_dd_table))) {
      if (persisted_dd_table != nullptr) delete persisted_dd_table;
      return dd::end_transaction(thd, true);
    }

    if ((*it)->property() == System_tables::Types::CORE) {
      dd::cache::Storage_adapter::instance()->core_drop(thd, dd_table);
      dd::cache::Storage_adapter::instance()->core_store(
          thd, dynamic_cast<Table *>(
                   const_cast<Abstract_table *>(persisted_dd_table)));
    }

    if (persisted_dd_table != nullptr) delete persisted_dd_table;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::SYNCED);

  return dd::end_transaction(thd, false);
}

/*
  Acquire the DD schema, tablespace and table objects. Read the persisted
  objects from the DD tables, and replace the contents of the core
  registry in the storage adapter.
*/
bool sync_meta_data(THD *thd) {
  // Acquire exclusive meta data locks for the relevant DD objects.
  if (acquire_exclusive_mdl(thd)) return true;

  {
    /*
      Use a scoped auto releaser to make sure the cached objects are released
      before the shared cache is reset.
    */
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    /*
      First, we acquire the DD schema and tablespace and keep them in
      local variables. The DD table objects are acquired and put into
      a vector. We also get hold of the corresponding persisted objects.

      In this way, we make sure the shared cache is populated. The auto
      releaser will make sure the objects are not evicted. This must be
      ensured since we need to make sure the ids stay consistent across
      all objects in the shared cache.
    */

    const Schema *dd_schema = nullptr;
    const Tablespace *dd_tspace = nullptr;
    if (thd->dd_client()->acquire(dd::String_type(MYSQL_SCHEMA_NAME.str),
                                  &dd_schema) ||
        thd->dd_client()->acquire(dd::String_type(MYSQL_TABLESPACE_NAME.str),
                                  &dd_tspace))
      return dd::end_transaction(thd, true);

    std::vector<Table *> dd_tables;  // Owned by the shared cache.
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it) {
      // Skip extraneous tables during minor downgrade.
      if ((*it)->entity() == nullptr) continue;

      const dd::Table *dd_table = nullptr;
      if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str,
                                    (*it)->entity()->name(), &dd_table))
        return dd::end_transaction(thd, true);
      dd_tables.push_back(const_cast<Table *>(dd_table));
    }

    // Get the persisted DD schema and tablespace.
    Schema::Name_key schema_key;
    dd_schema->update_name_key(&schema_key);
    const Schema *tmp_schema = nullptr;

    Tablespace::Name_key tspace_key;
    dd_tspace->update_name_key(&tspace_key);
    const Tablespace *tmp_tspace = nullptr;

    if (dd::cache::Storage_adapter::instance()->get(
            thd, schema_key, ISO_READ_COMMITTED, true, &tmp_schema) ||
        dd::cache::Storage_adapter::instance()->get(
            thd, tspace_key, ISO_READ_COMMITTED, true, &tmp_tspace))
      return dd::end_transaction(thd, true);

    DBUG_ASSERT(tmp_schema != nullptr && tmp_tspace != nullptr);
    std::unique_ptr<Schema> persisted_dd_schema(
        const_cast<Schema *>(tmp_schema));
    std::unique_ptr<Tablespace> persisted_dd_tspace(
        const_cast<Tablespace *>(tmp_tspace));

    // Get the persisted DD table objects into a vector.
    std::vector<std::unique_ptr<Table_impl>> persisted_dd_tables;
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end(); ++it) {
      // Skip extraneous tables during minor downgrade.
      if ((*it)->entity() == nullptr) continue;

      const dd::Abstract_table *dd_table = nullptr;
      dd::Abstract_table::Name_key table_key;
      Abstract_table::update_name_key(&table_key, persisted_dd_schema->id(),
                                      (*it)->entity()->name());

      if (dd::cache::Storage_adapter::instance()->get(
              thd, table_key, ISO_READ_COMMITTED, true, &dd_table))
        return dd::end_transaction(thd, true);

      std::unique_ptr<Table_impl> persisted_dd_table(
          dynamic_cast<Table_impl *>(const_cast<Abstract_table *>(dd_table)));
      persisted_dd_tables.push_back(std::move(persisted_dd_table));
    }

    // Drop the tablespaces with type PREDEFINED_DDSE from the storage adapter.
    for (System_tablespaces::Const_iterator it =
             System_tablespaces::instance()->begin(
                 System_tablespaces::Types::PREDEFINED_DDSE);
         it != System_tablespaces::instance()->end();
         it = System_tablespaces::instance()->next(
             it, System_tablespaces::Types::PREDEFINED_DDSE)) {
      const Tablespace *tspace = nullptr;
      if (thd->dd_client()->acquire((*it)->entity()->get_name(), &tspace))
        return dd::end_transaction(thd, true);

      dd::cache::Storage_adapter::instance()->core_drop(thd, tspace);
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
      We have also read the DD schema and tablespace as well as the DD
      tables from persistent storage. The last thing we do before resetting
      the shared cache is to update the contents of the core registry to
      match the persisted objects. First, we update the core registry with
      the persisted DD schema and tablespace.
    */
    dd::cache::Storage_adapter::instance()->core_drop(thd, dd_schema);
    dd::cache::Storage_adapter::instance()->core_store(
        thd, persisted_dd_schema.get());

    dd::cache::Storage_adapter::instance()->core_drop(thd, dd_tspace);
    dd::cache::Storage_adapter::instance()->core_store(
        thd, persisted_dd_tspace.get());

    // Make sure the IDs after storing are as expected.
    DBUG_ASSERT(persisted_dd_schema->id() == 1);
    DBUG_ASSERT(persisted_dd_tspace->id() == 1);

    /*
      Finally, we update the core registry of the DD tables. This must be
      done in two loops to avoid issues related to overlapping ID sequences.
    */
    std::vector<Table *>::const_iterator table_it = dd_tables.begin();
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end() && table_it != dd_tables.end();
         ++it, ++table_it) {
      /*
        If we are in the process of upgrading, there may not be an entry
        in the dd_tables for new tables that have been added after the
        version we are upgrading from.
      */
      if ((*table_it) != nullptr) {
        DBUG_ASSERT((*it)->entity()->name() == (*table_it)->name());
        dd::cache::Storage_adapter::instance()->core_drop(thd, *table_it);
      }
    }

    std::vector<std::unique_ptr<Table_impl>>::const_iterator persisted_it =
        persisted_dd_tables.begin();
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end() &&
         persisted_it != persisted_dd_tables.end();
         ++it, ++persisted_it) {
      if ((*it)->property() == System_tables::Types::CORE) {
        DBUG_ASSERT((*it)->entity()->name() == (*persisted_it)->name());
        dd::cache::Storage_adapter::instance()->core_store(
            thd, static_cast<Table *>((*persisted_it).get()));
      }
    }
  }

  /*
    Now, the auto releaser has released the objects, and we can go ahead and
    reset the shared cache.
  */
  dd::cache::Shared_dictionary_cache::instance()->reset(true);
  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::SYNCED);

  // Commit and flush tables to force re-opening using the refreshed meta data.
  if (dd::end_transaction(thd, false) || execute_query(thd, "FLUSH TABLES"))
    return true;

  // Get hold of the temporary actual and target schema names.
  String_type target_schema_name;
  bool target_schema_exists = false;
  if (dd::tables::DD_properties::instance().get(thd, "UPGRADE_TARGET_SCHEMA",
                                                &target_schema_name,
                                                &target_schema_exists))
    return true;

  String_type actual_schema_name;
  bool actual_schema_exists = false;
  if (dd::tables::DD_properties::instance().get(thd, "UPGRADE_ACTUAL_SCHEMA",
                                                &actual_schema_name,
                                                &actual_schema_exists))
    return true;

  // Reset the DDSE local dictionary cache.
  handlerton *ddse = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->dict_cache_reset == nullptr) return true;

  for (System_tables::Const_iterator it = System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it) {
    // Skip extraneous tables during minor downgrade.
    if ((*it)->entity() == nullptr) continue;

    if ((*it)->property() == System_tables::Types::CORE ||
        (*it)->property() == System_tables::Types::SECOND) {
      ddse->dict_cache_reset(MYSQL_SCHEMA_NAME.str,
                             (*it)->entity()->name().c_str());
      if (target_schema_exists && !target_schema_name.empty())
        ddse->dict_cache_reset(target_schema_name.c_str(),
                               (*it)->entity()->name().c_str());
      if (actual_schema_exists && !actual_schema_name.empty())
        ddse->dict_cache_reset(actual_schema_name.c_str(),
                               (*it)->entity()->name().c_str());
    }
  }

  /*
    At this point, we're to a large extent open for business.
    If there are leftover schema names from upgrade, delete them.
  */
  if (target_schema_exists && !target_schema_name.empty()) {
    std::stringstream ss;
    ss << "DROP SCHEMA IF EXISTS " << target_schema_name;
    if (execute_query(thd, ss.str().c_str())) return true;
  }

  if (actual_schema_exists && !actual_schema_name.empty()) {
    std::stringstream ss;
    ss << "DROP SCHEMA IF EXISTS " << actual_schema_name;
    if (execute_query(thd, ss.str().c_str())) return true;
  }

  /*
   The statements above are auto committed, so there is nothing uncommitted
   at this stage.
  */

  return false;
}

/*
  Create the temporary schemas needed during upgrade, and fetch their ids.
*/
/* purecov: begin inspected */
bool create_temporary_schemas(THD *thd, Object_id *mysql_schema_id,
                              Object_id *target_table_schema_id,
                              String_type *target_table_schema_name,
                              Object_id *actual_table_schema_id) {
  /*
    Find an unused target schema name. Prepare a base name, and append
    a counter, increment until a non-existing name is found
  */
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *schema = nullptr;
  std::stringstream ss;

  DBUG_ASSERT(target_table_schema_name != nullptr);
  *target_table_schema_name = String_type("");
  ss << "dd_upgrade_targets_" << MYSQL_VERSION_ID;
  String_type tmp_schema_name_base{ss.str().c_str()};
  int count = 0;
  do {
    if (thd->dd_client()->acquire(ss.str().c_str(), &schema)) return true;
    if (schema == nullptr) {
      *target_table_schema_name = ss.str().c_str();
      break;
    }
    ss.str("");
    ss.clear();
    ss << tmp_schema_name_base << "_" << count++;
  } while (count < 1000);

  if (target_table_schema_name->empty()) {
    LogErr(ERROR_LEVEL, ER_DD_UPGRADE_SCHEMA_UNAVAILABLE, ss.str().c_str());
    return true;
  }

  /*
    Find an unused schema name where we can temporarily move the actual
    tables to be removed or modified.
  */
  String_type actual_table_schema_name{""};
  ss.str("");
  ss.clear();
  ss << "dd_upgrade_garbage_" << MYSQL_VERSION_ID;
  tmp_schema_name_base = String_type(ss.str().c_str());
  count = 0;
  do {
    if (thd->dd_client()->acquire(ss.str().c_str(), &schema)) return true;
    if (schema == nullptr) {
      actual_table_schema_name = ss.str().c_str();
      break;
    }
    ss.str("");
    ss.clear();
    ss << tmp_schema_name_base << "_" << count++;
  } while (count < 1000);

  if (actual_table_schema_name.empty()) {
    LogErr(ERROR_LEVEL, ER_DD_UPGRADE_SCHEMA_UNAVAILABLE, ss.str().c_str());
    return true;
  }

  /*
    Store the schema names in DD_properties and commit. The schemas will
    now be removed on next restart.
  */
  if (dd::tables::DD_properties::instance().set(thd, "UPGRADE_TARGET_SCHEMA",
                                                *target_table_schema_name) ||
      dd::tables::DD_properties::instance().set(thd, "UPGRADE_ACTUAL_SCHEMA",
                                                actual_table_schema_name)) {
    return dd::end_transaction(thd, true);
  }

  if (dd::end_transaction(thd, false)) return true;

  if (execute_query(
          thd, dd::String_type("CREATE SCHEMA ") + actual_table_schema_name +
                   dd::String_type(" DEFAULT COLLATE '") +
                   dd::String_type(default_charset_info->name) + "'") ||
      execute_query(
          thd, dd::String_type("CREATE SCHEMA ") + *target_table_schema_name +
                   dd::String_type(" DEFAULT COLLATE '") +
                   dd::String_type(default_charset_info->name) + "'") ||
      execute_query(thd, dd::String_type("USE ") + *target_table_schema_name)) {
    return true;
  }

  /*
    Get hold of the schema ids of the temporary target schema, the
    temporary actual schema, and the mysql schema. These are needed
    later in various situations in the upgrade execution.
  */
  if (thd->dd_client()->acquire(MYSQL_SCHEMA_NAME.str, &schema) ||
      schema == nullptr)
    return true;

  DBUG_ASSERT(mysql_schema_id != nullptr);
  *mysql_schema_id = schema->id();
  DBUG_ASSERT(*mysql_schema_id == 1);

  if (thd->dd_client()->acquire(*target_table_schema_name, &schema) ||
      schema == nullptr)
    return true;

  DBUG_ASSERT(target_table_schema_id != nullptr);
  *target_table_schema_id = schema->id();

  if (thd->dd_client()->acquire(actual_table_schema_name, &schema) ||
      schema == nullptr)
    return true;

  DBUG_ASSERT(actual_table_schema_id != nullptr);
  *actual_table_schema_id = schema->id();

  return false;
}
/* purecov: end */

/*
  Establish the sets of names of tables to be created and/or removed.
*/
/* purecov: begin inspected */
void establish_table_name_sets(std::set<String_type> *create_set,
                               std::set<String_type> *remove_set) {
  /*
    Establish the table change sets:
    - The 'remove' set contains the tables that will eventually be removed,
      i.e., they are present in the actual version, and either abandoned
      or replaced by another table definition in the target version.
    - The 'create' set contains the tables that must be created, i.e., they
      are either new tables in the target version, or they replace an
      existing table in the actual version.
  */
  DBUG_ASSERT(create_set != nullptr && create_set->empty());
  DBUG_ASSERT(remove_set != nullptr && remove_set->empty());
  for (System_tables::Const_iterator it = System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it) {
    if (is_non_inert_dd_or_ddse_table((*it)->property())) {
      /*
        In this context, all tables should have an Object_table. Minor
        downgrade is the only situation where an Object_table may not exist,
        but minor upgrade will never enter this code path.
      */
      DBUG_ASSERT((*it)->entity() != nullptr);

      String_type target_ddl_statement("");
      const Object_table_definition *target_table_def =
          (*it)->entity()->target_table_definition();
      /*
         The target table definition may not be present if the table
         is abandoned.
      */
      if (target_table_def) {
        target_ddl_statement = target_table_def->get_ddl();
      }

      String_type actual_ddl_statement("");
      const Object_table_definition *actual_table_def =
          (*it)->entity()->actual_table_definition();
      /*
        The actual definition may not be present if this is a new table
        which has been added.
      */
      if (actual_table_def) {
        actual_ddl_statement = actual_table_def->get_ddl();
      }

      /*
        Remove and/or create as needed. If the definitions are non-null
        and equal, no change has been done, and hence upgrade of the table
        is irrelevant.
      */
      if (target_table_def == nullptr && actual_table_def != nullptr)
        remove_set->insert((*it)->entity()->name());
      else if (target_table_def != nullptr && actual_table_def == nullptr)
        create_set->insert((*it)->entity()->name());
      else if (target_ddl_statement != actual_ddl_statement) {
        /*
          Abandoned tables that are not present will have target and actual
          statements == "", and will therefore not be added to the create
          nor remove set.
        */
        remove_set->insert((*it)->entity()->name());
        create_set->insert((*it)->entity()->name());
      }
    }
  }
}
/* purecov: end */

/**
  Copy meta data from the actual tables to the target tables.

  The default is to copy all data. This is sufficient if we e.g. add a
  new index in the new DD version. If there are changes to the table
  columns, e.g. if we add or remove a column, we must add code to handle
  each case specifically. Suppose e.g. we add a new column to allow defining
  a default tablespace for each schema, and store the tablespace id
  in that column. Then, we could migrate the meta data for 'schemata' and
  set a default value for all existing schemas:

  ...
  migrated_set.insert("schemata");
  if (execute_query(thd, "INSERT INTO schemata "
         "SELECT id, catalog_id, name, default_collation_id, 1, "
         "       created, last_altered, options FROM mysql.schemata"))
  ...

  The code block above would go into the 'Version dependent migration'
  part of the function below.

  @param   thd         Thread context.
  @param   create_set  Set of new or modified tables to be created.
  @param   remove_set  Set of abandoned or modified tables to be removed.

  @returns false if success. otherwise true.
*/
/* purecov: begin inspected */
bool migrate_meta_data(THD *thd, const std::set<String_type> &create_set,
                       const std::set<String_type> &remove_set) {
  /*
    Turn off foreign key checks while migrating the meta data.
  */
  if (execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0"))
    return dd::end_transaction(thd, true);

  /*
    Explicitly migrate meta data for each table which has been modified.
    Register the table name in the migrated_set to skip it in the default
    handling below.
  */
  std::set<String_type> migrated_set{};

  /* Version dependent migration of meta data can be added here. */

  /*
    8.0.11 allowed entries with 0 timestamps to be created. These must
    be updated, otherwise, upgrade will fail since 0 timstamps are not
    allowed with the default SQL mode.
  */
  if (bootstrap::DD_bootstrap_ctx::instance().is_upgrade_from_before(
          bootstrap::DD_VERSION_80012)) {
    if (execute_query(thd,
                      "UPDATE mysql.tables SET last_altered = "
                      "CURRENT_TIMESTAMP WHERE last_altered = 0"))
      return dd::end_transaction(thd, true);
    if (execute_query(thd,
                      "UPDATE mysql.tables SET created = CURRENT_TIMESTAMP "
                      "WHERE created = 0"))
      return dd::end_transaction(thd, true);
  }

  /*
    Default handling: Copy all meta data for the tables that have been
    modified (i.e., all tables which are both in the remove- and create set),
    unless they were migrated explicitly above.
  */
  for (std::set<String_type>::const_iterator it = create_set.begin();
       it != create_set.end(); ++it) {
    if (migrated_set.find(*it) == migrated_set.end() &&
        remove_set.find(*it) != remove_set.end()) {
      std::stringstream ss;
      ss << "INSERT INTO " << (*it) << " SELECT * FROM "
         << MYSQL_SCHEMA_NAME.str << "." << (*it);
      if (execute_query(thd, ss.str().c_str()))
        return dd::end_transaction(thd, true);
    }
  }

  /*
    Turn foreign key checks back on and commit explicitly.
  */
  if (execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1"))
    return dd::end_transaction(thd, true);

  return dd::end_transaction(thd, false);
}
/* purecov: end */

/*
  Update properties in the DD_properties table. Note that upon failure, we
  will rollback, whereas upon success, commit will be delayed.
*/
bool update_properties(THD *thd, const std::set<String_type> *create_set,
                       const std::set<String_type> *remove_set,
                       const String_type &target_table_schema_name) {
  /*
    Populate the dd properties with the SQL DDL and SE private data.
    Store meta data of non-inert tables only.
  */
  std::unique_ptr<dd::Properties> system_tables_props(
      dd::Properties::parse_properties(""));

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  for (System_tables::Const_iterator it = System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it) {
    if (is_non_inert_dd_or_ddse_table((*it)->property())) {
      /*
        This will not be called for minor downgrade, so all tables
        will have a corresponding Object_table.
      */
      DBUG_ASSERT((*it)->entity() != nullptr);
      const Object_table_definition *table_def =
          (*it)->entity()->target_table_definition();

      // May be null for abandoned tables, which should be skipped.
      if (table_def == nullptr) {
        continue;
      }

      /*
        Tables that are in the remove_set, but not in the create_set,
        should not be reflected in the DD properties.
      */
      if (remove_set != nullptr && create_set != nullptr &&
          remove_set->find((*it)->entity()->name()) != remove_set->end() &&
          create_set->find((*it)->entity()->name()) == create_set->end()) {
        continue;
      }

      /*
        If a create set is submitted, use this to decide whether we should
        get the meta data from the table in the 'mysql' schema or the temporary
        target schema.
      */
      String_type table_schema_name{MYSQL_SCHEMA_NAME.str};
      if (create_set != nullptr &&
          create_set->find((*it)->entity()->name()) != create_set->end()) {
        table_schema_name = target_table_schema_name;
      }

      /*
        Acquire the table object to get hold of the se private data etc.
        Note that we must acquire it from the appropriate schema.
      */
      const dd::Table *dd_table = nullptr;
      if (thd->dd_client()->acquire(table_schema_name, (*it)->entity()->name(),
                                    &dd_table))
        return dd::end_transaction(thd, true);

      // All non-abandoned tables should have a table object present.
      DBUG_ASSERT(dd_table != nullptr);

      std::unique_ptr<dd::Properties> tbl_props(
          dd::Properties::parse_properties(""));

      using dd::tables::DD_properties;
      tbl_props->set_uint64(
          DD_properties::dd_key(DD_properties::DD_property::ID),
          dd_table->se_private_id());
      tbl_props->set(DD_properties::dd_key(DD_properties::DD_property::DATA),
                     dd_table->se_private_data().raw_string());
      tbl_props->set_uint64(
          DD_properties::dd_key(DD_properties::DD_property::SPACE_ID),
          dd_table->tablespace_id());

      // Store the structured representation of the table definition.
      std::unique_ptr<Properties> definition(Properties::parse_properties(""));
      table_def->store_into_properties(definition.get());
      tbl_props->set(DD_properties::dd_key(DD_properties::DD_property::DEF),
                     definition->raw_string());

      // Store the se private data for each index.
      dd::Table::Index_collection::const_iterator idx(
          dd_table->indexes().begin());
      for (int count = 0; idx != dd_table->indexes().end(); ++idx, ++count) {
        std::stringstream ss;
        ss << DD_properties::dd_key(DD_properties::DD_property::IDX) << count;
        tbl_props->set(ss.str().c_str(),
                       (*idx)->se_private_data().raw_string());
      }

      // Store the se private data for each column.
      dd::Table::Column_collection::const_iterator col(
          dd_table->columns().begin());
      for (int count = 0; col != dd_table->columns().end(); ++col, ++count) {
        std::stringstream ss;
        ss << DD_properties::dd_key(DD_properties::DD_property::COL) << count;
        tbl_props->set(ss.str().c_str(),
                       (*col)->se_private_data().raw_string());
      }

      // All tables should be reflected in the System tables list.
      system_tables_props->set(dd_table->name(), tbl_props->raw_string());
    }
  }
  if (dd::tables::DD_properties::instance().set(thd, "SYSTEM_TABLES",
                                                *system_tables_props.get()))
    return dd::end_transaction(thd, true);

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::STORED_DD_META_DATA);

  // Delay commit.
  return false;
}

/*
  Adjust the object ids to "move" tables between schemas by using DML.

  At this point, we have a set of old DD tables, in the 'remove_set', that
  should be removed. These are a subset of the actual DD tables. And we
  have a set of new DD tables, in the 'create_set', that should replace the
  old ones. The tables in the 'create_set' are a subset of the target DD
  tables.

  What we want to do is to move the tables in the 'remove_set' out of the
  'mysql' schema and into a different schema with id 'actual_table_schema_id',
  and then move the tables in the 'create_set' (which are in a schema with
  id 'target_table_schema_id' and name 'target_table_schema_name') out of
  that schema and into the 'mysql' schema.

  We could do this by 'RENAME TABLE' statements, but that would not be atomic
  since the statements will be auto committing. So instead, we manipulate the
  DD tables directly, and update the schema ids related to the relevant tables.
  This is possible since the tables are stored in a general tablespace, and
  moving them to a different schema will not affect the DDSE.

  The updates we need to do on the DD tables are the following:

  - For the tables in the 'remove_set' and the 'create_set', we must change
    the schema id of the entry in the 'tables' table according to where we
    want to move the tables.
  - For the tables in the 'remove_set', we delete all foreign keys where the
    table to be removed is a child.
  - For the tables in the 'create_set', we change the schema id and name of
    all foreign keys, where the table to be created is a child, from the
    'target_table_schema_name' to that of the 'mysql' schema.

  See also additional comments in the code below.

  @param  create_set               Set of tables to be created.
  @param  remove_set               Set of tables to be removed.
  @param  mysql_schema_id          Id of the 'mysql' schema.
  @param  target_table_schema_id   Id of the schema where the tables in the
                                   create_set are located.
  @param  target_table_schema_name Name of the schema where the tables in the
                                   create_set are located.
  @param  actual_table_schema_id   Id of the schema where the tables in the
                                   remove_set will be moved.

  @returns false if success, true otherwise.
*/
/* purecov: begin inspected */
bool update_object_ids(THD *thd, const std::set<String_type> &create_set,
                       const std::set<String_type> &remove_set,
                       Object_id mysql_schema_id,
                       Object_id target_table_schema_id,
                       const String_type &target_table_schema_name,
                       Object_id actual_table_schema_id) {
  if (execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0"))
    return dd::end_transaction(thd, true);

  /*
    If mysql.tables has been modified, do the change on the copy, otherwise
    do the change on mysql.tables
  */
  String_type tables_table = tables::Tables::instance().name();
  if (create_set.find(tables_table) != create_set.end()) {
    tables_table = target_table_schema_name + String_type(".") + tables_table;
  } else {
    tables_table =
        String_type(MYSQL_SCHEMA_NAME.str) + String_type(".") + tables_table;
  }

  /*
    For each actual table to be removed (i.e., modified or abandoned),
    change tables.schema_id to the actual table schema id.
  */
  for (std::set<String_type>::const_iterator it = remove_set.begin();
       it != remove_set.end(); ++it) {
    std::stringstream ss;
    ss << "UPDATE " << tables_table
       << " SET schema_id= " << actual_table_schema_id
       << " WHERE schema_id= " << mysql_schema_id << " AND name LIKE '" << (*it)
       << "'";

    if (execute_query(thd, ss.str().c_str()))
      return dd::end_transaction(thd, true);
  }

  /*
    For each target table to be created (i.e., modified or added),
    change tables.schema_id to the mysql schema id, and set the hidden
    property according to the corresponding Object_table.
  */
  for (std::set<String_type>::const_iterator it = create_set.begin();
       it != create_set.end(); ++it) {
    // Get the corresponding Object_table instance.
    String_type hidden{""};
    if (System_tables::instance()
            ->find_table(MYSQL_SCHEMA_NAME.str, (*it))
            ->is_hidden())
      hidden = String_type(", hidden= 'System'");

    std::stringstream ss;
    ss << "UPDATE " << tables_table << " SET schema_id= " << mysql_schema_id
       << hidden << " WHERE schema_id= " << target_table_schema_id
       << " AND name LIKE '" << (*it) << "'";

    if (execute_query(thd, ss.str().c_str()))
      return dd::end_transaction(thd, true);
  }

  /*
    If mysql.foreign_keys has been modified, do the change on the copy,
    otherwise do the change on mysql.foreign_keys. And likewise, if
    mysql.foreign_key_column_usage has been modified, do the change on
    the copy, otherwise do the change on mysql.foreign_key_column_usage.
  */
  String_type foreign_keys_table = tables::Foreign_keys::instance().name();
  ;
  if (create_set.find(foreign_keys_table) != create_set.end()) {
    foreign_keys_table =
        target_table_schema_name + String_type(".") + foreign_keys_table;
  } else {
    foreign_keys_table = String_type(MYSQL_SCHEMA_NAME.str) + String_type(".") +
                         foreign_keys_table;
  }

  String_type foreign_key_column_usage_table =
      tables::Foreign_key_column_usage::instance().name();
  ;
  if (create_set.find(foreign_key_column_usage_table) != create_set.end()) {
    foreign_key_column_usage_table = target_table_schema_name +
                                     String_type(".") +
                                     foreign_key_column_usage_table;
  } else {
    foreign_key_column_usage_table = String_type(MYSQL_SCHEMA_NAME.str) +
                                     String_type(".") +
                                     foreign_key_column_usage_table;
  }

  /*
    For each actual (i.e., modified or abandoned) table to be removed,
    remove the entries from the foreign_keys and foreign_key_column_usage
    table. There is no point in trying to maintain the foreign keys since
    the tables will be removed eventually anyway.
  */
  for (std::set<String_type>::const_iterator it = remove_set.begin();
       it != remove_set.end(); ++it) {
    std::stringstream ss;
    ss << "DELETE FROM " << foreign_key_column_usage_table
       << " WHERE foreign_key_id IN ("
       << "  SELECT id FROM " << foreign_keys_table
       << "   WHERE table_id= (SELECT id FROM " << tables_table
       << "     WHERE name LIKE '" << (*it) << "' AND "
       << "     schema_id= " << actual_table_schema_id << "))";
    if (execute_query(thd, ss.str().c_str()))

      return dd::end_transaction(thd, true);

    ss.str("");
    ss.clear();
    ss << "DELETE FROM " << foreign_keys_table
       << "   WHERE table_id= (SELECT id FROM " << tables_table
       << "     WHERE name LIKE '" << (*it) << "' AND "
       << "     schema_id= " << actual_table_schema_id << ")";
    if (execute_query(thd, ss.str().c_str()))
      return dd::end_transaction(thd, true);
  }

  /*
    For each target (i.e., modified or added)  table to be moved, change
    foreign_keys.schema_id and foreign_keys.referenced_schema_name to the
    mysql schema id and name. For the created tables, the target schema id
    and name are reflected in the foreign_keys tables, so we don't need a
    subquery based on table names.
  */
  std::stringstream ss;
  ss << "UPDATE " << foreign_keys_table << " SET schema_id= " << mysql_schema_id
     << ", "
     << "     referenced_table_schema= '" << MYSQL_SCHEMA_NAME.str << "'"
     << " WHERE schema_id= " << target_table_schema_id
     << "       AND referenced_table_schema= '" << target_table_schema_name
     << "'";

  if (execute_query(thd, ss.str().c_str()))
    return dd::end_transaction(thd, true);

  if (execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1"))
    return dd::end_transaction(thd, true);

  // Delay commit in the case of success, since we need to do this atomically.
  return false;
}
/* purecov: end */

/*
  Update the version numbers in the 'dd_properties' table.
*/
bool update_versions(THD *thd) {
  /*
    During initialize, store the DD version number, the LCTN used, and the
    mysqld server version.
  */
  if (opt_initialize) {
    if (dd::tables::DD_properties::instance().set(thd, "DD_VERSION",
                                                  dd::DD_VERSION) ||
        dd::tables::DD_properties::instance().set(
            thd, "MINOR_DOWNGRADE_THRESHOLD",
            dd::DD_VERSION_MINOR_DOWNGRADE_THRESHOLD) ||
        dd::tables::DD_properties::instance().set(thd, "SDI_VERSION",
                                                  dd::sdi_version) ||
        dd::tables::DD_properties::instance().set(thd, "LCTN",
                                                  lower_case_table_names) ||
        dd::tables::DD_properties::instance().set(thd, "MYSQLD_VERSION_LO",
                                                  MYSQL_VERSION_ID) ||
        dd::tables::DD_properties::instance().set(thd, "MYSQLD_VERSION_HI",
                                                  MYSQL_VERSION_ID) ||
        dd::tables::DD_properties::instance().set(thd, "MYSQLD_VERSION",
                                                  MYSQL_VERSION_ID))
      return dd::end_transaction(thd, true);
  } else {
    uint mysqld_version_lo = 0;
    uint mysqld_version_hi = 0;
    uint mysqld_version = 0;
    bool exists_lo = false;
    bool exists_hi = false;
    bool exists = false;
    if ((dd::tables::DD_properties::instance().get(
             thd, "MYSQLD_VERSION_LO", &mysqld_version_lo, &exists_lo) ||
         !exists_lo) ||
        (dd::tables::DD_properties::instance().get(
             thd, "MYSQLD_VERSION_HI", &mysqld_version_hi, &exists_hi) ||
         !exists_hi) ||
        (dd::tables::DD_properties::instance().get(thd, "MYSQLD_VERSION",
                                                   &mysqld_version, &exists) ||
         !exists))
      return dd::end_transaction(thd, true);

    if ((mysqld_version_lo > MYSQL_VERSION_ID &&
         dd::tables::DD_properties::instance().set(thd, "MYSQLD_VERSION_LO",
                                                   MYSQL_VERSION_ID)) ||
        (mysqld_version_hi < MYSQL_VERSION_ID &&
         dd::tables::DD_properties::instance().set(thd, "MYSQLD_VERSION_HI",
                                                   MYSQL_VERSION_ID)) ||
        (mysqld_version != MYSQL_VERSION_ID &&
         dd::tables::DD_properties::instance().set(thd, "MYSQLD_VERSION",
                                                   MYSQL_VERSION_ID)))
      return dd::end_transaction(thd, true);

    /*
      Update the SDI version number in case of upgrade.
      Note that on downgrade, we keep the old SDI version.
    */
    uint stored_sdi_version = 0;
    bool exists_sdi = false;
    if ((dd::tables::DD_properties::instance().get(
             thd, "SDI_VERSION", &stored_sdi_version, &exists_sdi) ||
         !exists_sdi) ||
        (stored_sdi_version < dd::sdi_version &&
         dd::tables::DD_properties::instance().set(thd, "SDI_VERSION",
                                                   dd::sdi_version)))
      return dd::end_transaction(thd, true);

    /*
      Update the DD version number in case of upgrade.
      Note that on downgrade, we keep the old DD version.
    */
    uint dd_version = 0;
    bool exists_dd = false;
    if ((dd::tables::DD_properties::instance().get(thd, "DD_VERSION",
                                                   &dd_version, &exists_dd) ||
         !exists_dd) ||
        (dd_version < dd::DD_VERSION &&
         dd::tables::DD_properties::instance().set(thd, "DD_VERSION",
                                                   dd::DD_VERSION)))
      return dd::end_transaction(thd, true);

    /*
      Update the minor downgrade threshold in case of upgrade.
      Note that on downgrade, we keep the threshold version which is
      already present.
    */
    if (dd_version < dd::DD_VERSION &&
        dd::tables::DD_properties::instance().set(
            thd, "MINOR_DOWNGRADE_THRESHOLD",
            dd::DD_VERSION_MINOR_DOWNGRADE_THRESHOLD))
      return dd::end_transaction(thd, true);
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::VERSION_UPDATED);

  /*
    During upgrade, this will commit the swap of the old and new DD tables.
  */
  return dd::end_transaction(thd, false);
}

// Create the target tables for upgrade and migrate the meta data.
/* purecov: begin inspected */
bool upgrade_tables(THD *thd) {
  if (!bootstrap::DD_bootstrap_ctx::instance().is_upgrade()) return false;

  /*
    Create the temporary schemas used for target and actual tables,
    and get hold of their ids.
  */
  Object_id mysql_schema_id = INVALID_OBJECT_ID;
  Object_id target_table_schema_id = INVALID_OBJECT_ID;
  Object_id actual_table_schema_id = INVALID_OBJECT_ID;
  String_type target_table_schema_name;
  if (create_temporary_schemas(thd, &mysql_schema_id, &target_table_schema_id,
                               &target_table_schema_name,
                               &actual_table_schema_id))
    return true;

  /*
    Establish the sets of table names to be removed and/or created.
  */
  std::set<String_type> remove_set = {};
  std::set<String_type> create_set = {};
  establish_table_name_sets(&create_set, &remove_set);

  /*
    Loop over all DD tables, and create the target tables. We may do version
    specific handling, but the default is to create the target table if it is
    different from the actual table (or if there is no corresponding actual
    table). The table creation is done by executing DDL statements that are
    auto committed.
  */
  if (create_tables(thd, &create_set)) return true;

  /*
    Loop over all DD tables and migrate the meta data. We may do version
    specific handling, but the default is to just copy all meta data from
    the actual to the target table, assuming the number and type of columns
    are the same (e.g. if an index is added). The data migration is committed.
  */
  if (migrate_meta_data(thd, create_set, remove_set)) return true;

  /*
    We are now ready to do the atomic switch of the actual and target DD
    tables. Thus, the next three steps must be done without intermediate
    commits. Note that in case of failure, rollback is done immediately.
    In case of success, no commit is done until at the very end of
    update_versions(). The switch is done as follows:

    - First, update the DD properties. Note that we must acquire the
      modified DD tables from the temporary target schema. This is done
      before the object ids are modified, because that step also may mess
      up object acquisition (if we change the schema id of a newly created
      table to that of the 'mysql' schema, and then try acquire(), we will
      get the table from the core registry in the storage adapter, and that
      is not what we want).

    - Then, update the object ids and schema names to simulate altering the
      schema of the modified tables. The changes are done on the 'tables',
      'foreign_keys' and 'foreign_key_column_usage' tables. If these tables
      are modified, the changes must be done on the corresponding new table
      in the target schema. If not, the change must be done on the actual
      table in the 'mysql' schema.

    - Finally, update the version numbers and commit. In update_versions(),
      the atomic switch will either be committed.
  */
  if (update_properties(thd, &create_set, &remove_set,
                        target_table_schema_name) ||
      update_object_ids(thd, create_set, remove_set, mysql_schema_id,
                        target_table_schema_id, target_table_schema_name,
                        actual_table_schema_id) ||
      update_versions(thd))
    return true;

  /*
    Flush tables, reset the shared dictionary cache and the storage adapter.
    Start over DD bootstrap from the beginning.
  */
  if (execute_query(thd, "FLUSH TABLES")) return true;

  dd::cache::Shared_dictionary_cache::instance()->reset(false);

  // Reset the DDSE local dictionary cache.
  handlerton *ddse = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->dict_cache_reset == nullptr) return true;

  for (System_tables::Const_iterator it =
           System_tables::instance()->begin(System_tables::Types::CORE);
       it != System_tables::instance()->end();
       it = System_tables::instance()->next(it, System_tables::Types::CORE)) {
    ddse->dict_cache_reset(MYSQL_SCHEMA_NAME.str,
                           (*it)->entity()->name().c_str());
    ddse->dict_cache_reset(target_table_schema_name.c_str(),
                           (*it)->entity()->name().c_str());
  }

  /*
    We need to start over DD initialization. This is done by executing the
    first stages of the procedure followed at restart. Note that this
    will see and use the newly upgraded DD that was created above. Cleanup
    of the temporary schemas is done at the end of 'sync_meta_data()'.
  */
  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::STARTED);

  store_predefined_tablespace_metadata(thd);
  if (create_dd_schema(thd) || initialize_dd_properties(thd) ||
      create_tables(thd, nullptr) || sync_meta_data(thd)) {
    return true;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::UPGRADED_TABLES);

  return false;
}
/* purecov: end */

// Insert additional data into the DD tables.
bool populate_tables(THD *thd) {
  // Iterate over DD tables, populate tables.
  for (System_tables::Const_iterator it = System_tables::instance()->begin();
       it != System_tables::instance()->end(); ++it) {
    // Skip system tables.
    if ((*it)->property() == System_tables::Types::SYSTEM) continue;

    // Retrieve list of SQL statements to execute.
    const Object_table_definition *table_def =
        (*it)->entity()->target_table_definition();

    // Skip abandoned tables.
    if (table_def == nullptr) continue;

    bool error = false;
    std::vector<dd::String_type> stmt = table_def->get_dml();
    for (std::vector<dd::String_type>::iterator stmt_it = stmt.begin();
         stmt_it != stmt.end() && !error; ++stmt_it)
      error = execute_query(thd, *stmt_it);

    // Commit the statement based population.
    if (dd::end_transaction(thd, error)) return true;

    // If no error, call the low level table population method, and commit it.
    error = (*it)->entity()->populate(thd);
    if (dd::end_transaction(thd, error)) return true;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::POPULATED);

  return false;
}

// Re-populate character sets and collations upon normal restart.
bool repopulate_charsets_and_collations(THD *thd) {
  /*
    If we are in read-only mode, we skip re-populating. Here, 'opt_readonly'
    is the value of the '--read-only' option.
  */
  if (opt_readonly) {
    LogErr(WARNING_LEVEL, ER_DD_NO_WRITES_NO_REPOPULATION, "", "");
    return false;
  }

  /*
    We must also check if the DDSE is started in a way that makes the DD
    read only. For now, we only support InnoDB as SE for the DD. The call
    to retrieve the handlerton for the DDSE should be replaced by a more
    generic mechanism.
  */
  handlerton *ddse = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->is_dict_readonly && ddse->is_dict_readonly()) {
    LogErr(WARNING_LEVEL, ER_DD_NO_WRITES_NO_REPOPULATION, "InnoDB", " ");
    return false;
  }

  /*
    Otherwise, turn off FK checks, re-populate and commit.
    The FK checks must be turned off since the collations and
    character sets reference each other.
  */
  bool error = execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
               tables::Collations::instance().populate(thd) ||
               tables::Character_sets::instance().populate(thd);

  /*
    We must commit the re-population before executing a new query, which
    expects the transaction to be empty, and finally, turn FK checks back on.
  */
  error |= dd::end_transaction(thd, error);
  error |= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1");
  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::POPULATED);

  return error;
}

/*
  Verify that the storage adapter contains the core DD objects and
  nothing else.
*/
bool verify_contents(THD *thd) {
  // Verify that the DD schema is present, and that its id == 1.
  Schema::Name_key schema_key;
  Schema::update_name_key(&schema_key, MYSQL_SCHEMA_NAME.str);
  Object_id dd_schema_id =
      cache::Storage_adapter::instance()->core_get_id<Schema>(schema_key);

  DBUG_ASSERT(dd_schema_id == MYSQL_SCHEMA_DD_ID);
  if (dd_schema_id == INVALID_OBJECT_ID) {
    LogErr(ERROR_LEVEL, ER_DD_SCHEMA_NOT_FOUND, MYSQL_SCHEMA_NAME.str);
    return dd::end_transaction(thd, true);
  }
  DBUG_ASSERT(cache::Storage_adapter::instance()->core_size<Schema>() == 1);

  // Verify that the core DD tables are present.
#ifndef DBUG_OFF
  size_t n_core_tables = 0;
#endif
  for (System_tables::Const_iterator it =
           System_tables::instance()->begin(System_tables::Types::CORE);
       it != System_tables::instance()->end();
       it = System_tables::instance()->next(it, System_tables::Types::CORE)) {
    // Skip extraneous tables for minor downgrade.
    if ((*it)->entity() == nullptr) continue;

#ifndef DBUG_OFF
    n_core_tables++;
#endif

    Table::Name_key table_key;
    Table::update_name_key(&table_key, dd_schema_id, (*it)->entity()->name());
    Object_id dd_table_id =
        cache::Storage_adapter::instance()->core_get_id<Table>(table_key);

    DBUG_ASSERT(dd_table_id != INVALID_OBJECT_ID);
    if (dd_table_id == INVALID_OBJECT_ID) {
      LogErr(ERROR_LEVEL, ER_DD_TABLE_NOT_FOUND,
             (*it)->entity()->name().c_str());
      return dd::end_transaction(thd, true);
    }
  }
  DBUG_ASSERT(cache::Storage_adapter::instance()->core_size<Abstract_table>() ==
              n_core_tables);

  // Verify that the dictionary tablespace is present and that its id == 1.
  Tablespace::Name_key tspace_key;
  Tablespace::update_name_key(&tspace_key, MYSQL_TABLESPACE_NAME.str);
  Object_id dd_tspace_id =
      cache::Storage_adapter::instance()->core_get_id<Tablespace>(tspace_key);

  DBUG_ASSERT(dd_tspace_id == MYSQL_TABLESPACE_DD_ID);
  if (dd_tspace_id == INVALID_OBJECT_ID) {
    LogErr(ERROR_LEVEL, ER_DD_TABLESPACE_NOT_FOUND, MYSQL_TABLESPACE_NAME.str);
    return dd::end_transaction(thd, true);
  }
  DBUG_ASSERT(cache::Storage_adapter::instance()->core_size<Tablespace>() == 1);

  return dd::end_transaction(thd, false);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////

namespace dd {
namespace bootstrap {
/*
  Do the necessary DD-related initialization in the DDSE, and get the
  predefined tables and tablespaces.
*/
bool DDSE_dict_init(THD *thd, dict_init_mode_t dict_init_mode, uint version) {
  handlerton *ddse = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);

  /*
    The lists with element wrappers are mem root allocated. The wrapped
    instances are allocated dynamically in the DDSE, and will be owned
    by the System_tables registry.
  */
  List<const Object_table> ddse_tables;
  List<const Plugin_tablespace> ddse_tablespaces;
  if (ddse->ddse_dict_init == nullptr ||
      ddse->ddse_dict_init(dict_init_mode, version, &ddse_tables,
                           &ddse_tablespaces))
    return true;

  /*
    Iterate over the table definitions and add them to the System_tables
    registry. The Object_table instances will later be used to execute
    CREATE TABLE statements to actually create the tables.

    If Object_table::is_hidden(), then we add the tables as type DDSE_PRIVATE
    (not available neither for DDL nor DML), otherwise, we add them as type
    DDSE_PROTECTED (available for DML, not for DDL).
  */
  List_iterator<const Object_table> table_it(ddse_tables);
  const Object_table *ddse_table = nullptr;
  while ((ddse_table = table_it++)) {
    System_tables::Types table_type = System_tables::Types::DDSE_PROTECTED;
    if (ddse_table->is_hidden()) {
      table_type = System_tables::Types::DDSE_PRIVATE;
    }
    System_tables::instance()->add(MYSQL_SCHEMA_NAME.str, ddse_table->name(),
                                   table_type, ddse_table);
  }

  /*
    At this point, the Systen_tables registry contains the INERT DD tables,
    and the DDSE tables. Before we continue, we must add the remaining
    DD tables.
  */
  System_tables::instance()->add_remaining_dd_tables();

  /*
    Iterate over the tablespace definitions, add the names and the
    tablespace meta data to the System_tablespaces registry. The
    meta data will be used later to create dd::Tablespace objects.
    The Plugin_tablespace instances are owned by the DDSE.
  */
  List_iterator<const Plugin_tablespace> tablespace_it(ddse_tablespaces);
  const Plugin_tablespace *tablespace = nullptr;
  while ((tablespace = tablespace_it++)) {
    // Add the name and the object instance to the registry with the
    // appropriate property.
    if (my_strcasecmp(system_charset_info, MYSQL_TABLESPACE_NAME.str,
                      tablespace->get_name()) == 0)
      System_tablespaces::instance()->add(
          tablespace->get_name(), System_tablespaces::Types::DD, tablespace);
    else
      System_tablespaces::instance()->add(
          tablespace->get_name(), System_tablespaces::Types::PREDEFINED_DDSE,
          tablespace);
  }

  return false;
}

// Initialize the data dictionary.
bool initialize_dictionary(THD *thd, bool is_dd_upgrade_57,
                           Dictionary_impl *d) {
  if (is_dd_upgrade_57)
    bootstrap::DD_bootstrap_ctx::instance().set_stage(
        bootstrap::Stage::STARTED);

  store_predefined_tablespace_metadata(thd);
  if (create_dd_schema(thd) || initialize_dd_properties(thd) ||
      create_tables(thd, nullptr))
    return true;

  if (is_dd_upgrade_57) {
    // Add status to mark creation of dictionary in InnoDB.
    // Till this step, no new undo log is created by InnoDB.
    if (upgrade_57::Upgrade_status().update(
            upgrade_57::Upgrade_status::enum_stage::DICT_TABLES_CREATED))
      return true;
  }

  DBUG_EXECUTE_IF("dd_upgrade_stage_2", if (is_dd_upgrade_57) {
    /*
      Server will crash will upgrading 5.7 data directory.
      This will leave server is an inconsistent state.
      File tracking upgrade will have Stage 2 written in it.
      Next restart of server on same data directory should
      revert all changes done by upgrade and data directory
      should be reusable by 5.7 server.
    */
    DBUG_SUICIDE();
  });

  if (DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_SERVER,
                        d->get_target_dd_version()) ||
      flush_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_TABLESPACES,
                        d->get_target_dd_version()) ||
      populate_tables(thd) ||
      update_properties(thd, nullptr, nullptr,
                        String_type(MYSQL_SCHEMA_NAME.str)) ||
      verify_contents(thd) || update_versions(thd)) {
    return true;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::FINISHED);

  return false;
}

// First time server start and initialization of the data dictionary.
bool initialize(THD *thd) {
  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::STARTED);

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.transaction_read_only = false;
  thd->tx_read_only = false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d = dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /*
    Each step in the install process below is committed independently,
    either implicitly (for e.g. "CREATE TABLE") or explicitly (for the
    operations in the "populate()" methods). Thus, there is no need to
    commit explicitly here.
  */
  if (DDSE_dict_init(thd, DICT_INIT_CREATE_FILES, d->get_target_dd_version()) ||
      initialize_dictionary(thd, false, d))
    return true;

  DBUG_ASSERT(d->get_target_dd_version() == d->get_actual_dd_version(thd));
  LogErr(INFORMATION_LEVEL, ER_DD_VERSION_INSTALLED,
         d->get_target_dd_version());
  return false;
}

// Normal server restart.
bool restart(THD *thd) {
  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::STARTED);

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.transaction_read_only = false;
  thd->tx_read_only = false;

  // Set explicit_defaults_for_timestamp variable for dictionary creation
  thd->variables.explicit_defaults_for_timestamp = true;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d = dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  store_predefined_tablespace_metadata(thd);

  if (create_dd_schema(thd) || initialize_dd_properties(thd) ||
      create_tables(thd, nullptr) || sync_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd)) ||
      upgrade_tables(thd) || repopulate_charsets_and_collations(thd) ||
      verify_contents(thd) || update_versions(thd)) {
    return true;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::FINISHED);
  LogErr(INFORMATION_LEVEL, ER_DD_VERSION_FOUND, d->get_actual_dd_version(thd));

  return false;
}

// Initialize dictionary in case of server restart.
void recover_innodb_upon_upgrade(THD *thd) {
  Dictionary_impl *d = dd::Dictionary_impl::instance();
  store_predefined_tablespace_metadata(thd);
  // RAII to handle error in execution of CREATE TABLE.
  Key_length_error_handler key_error_handler;
  /*
    Ignore ER_TOO_LONG_KEY for dictionary tables during restart.
    Do not print the error in error log as we are creating only the
    cached objects and not physical tables.
TODO: Workaround due to bug#20629014. Remove when the bug is fixed.
   */
  thd->push_internal_handler(&key_error_handler);
  if (create_dd_schema(thd) || initialize_dd_properties(thd) ||
      create_tables(thd, nullptr) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd))) {
    // Error is not be handled in this case as we are on cleanup code path.
    LogErr(WARNING_LEVEL, ER_DD_INIT_UPGRADE_FAILED);
  }
  thd->pop_internal_handler();
  return;
}

bool setup_dd_objects_and_collations(THD *thd) {
  // Continue with server startup.
  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::CREATED_TABLES);

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.transaction_read_only = false;
  thd->tx_read_only = false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d = dd::Dictionary_impl::instance();
  DBUG_ASSERT(d);

  DBUG_ASSERT(d->get_target_dd_version() == d->get_actual_dd_version(thd));

  /*
    In this context, we initialize the target tables directly since this
    is a restart based on a pre-transactional-DD server, so ordinary
    upgrade does not need to be considered.
  */
  if (sync_meta_data(thd) || repopulate_charsets_and_collations(thd) ||
      verify_contents(thd) || update_versions(thd)) {
    return true;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::FINISHED);
  LogErr(INFORMATION_LEVEL, ER_DD_VERSION_FOUND, d->get_actual_dd_version(thd));

  return false;
}

}  // namespace bootstrap
}  // namespace dd
