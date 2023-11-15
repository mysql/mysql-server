/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/impl/bootstrap/bootstrapper.h"

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_version.h"  // MYSQL_VERSION_ID
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd.h"                       // dd::create_object
#include "sql/dd/impl/bootstrap/bootstrap_ctx.h"        // DD_bootstrap_ctx
#include "sql/dd/impl/cache/shared_dictionary_cache.h"  // Shared_dictionary_cache
#include "sql/dd/impl/cache/storage_adapter.h"          // Storage_adapter
#include "sql/dd/impl/dictionary_impl.h"                // dd::Dictionary_impl
#include "sql/dd/impl/raw/object_keys.h"
#include "sql/dd/impl/sdi.h"                    // dd::sdi::store
#include "sql/dd/impl/tables/character_sets.h"  // dd::tables::Character_sets
#include "sql/dd/impl/tables/collations.h"      // dd::tables::Collations
#include "sql/dd/impl/tables/dd_properties.h"   // dd::tables::DD_properties
#include "sql/dd/impl/tables/tables.h"          // dd::tables::Tables
#include "sql/dd/impl/types/schema_impl.h"      // dd::Schema_impl
#include "sql/dd/impl/types/table_impl.h"       // dd::Table_impl
#include "sql/dd/impl/types/tablespace_impl.h"  // dd::Table_impl
#include "sql/dd/impl/upgrade/dd.h"             // dd::upgrade::upgrade_tables
#include "sql/dd/impl/upgrade/server.h"  // dd::upgrade::do_server_upgrade_checks
#include "sql/dd/impl/utils.h"           // dd::execute_query
#include "sql/dd/info_schema/metadata.h"  // IS_DD_VERSION
#include "sql/dd/object_id.h"
#include "sql/dd/types/abstract_table.h"
#include "sql/dd/types/object_table.h"             // dd::Object_table
#include "sql/dd/types/object_table_definition.h"  // dd::Object_table_definition
#include "sql/dd/types/table.h"
#include "sql/dd/types/tablespace.h"
#include "sql/dd/types/tablespace_file.h"  // dd::Tablespace_file
#include "sql/dd/upgrade/server.h"         // UPGRADE_NONE
#include "sql/handler.h"                   // dict_init_mode_t
#include "sql/mdl.h"
#include "sql/mysqld.h"
#include "sql/sd_notify.h"  // sysd::notify
#include "sql/thd_raii.h"
#include "storage/perfschema/pfs_dd_version.h"  // PFS_DD_VERSION

using namespace dd;

///////////////////////////////////////////////////////////////////////////

namespace {

// Initialize recovery in the DDSE.
bool DDSE_dict_recover(THD *thd, dict_recovery_mode_t dict_recovery_mode,
                       uint version) {
  if (!opt_initialize)
    sysd::notify("STATUS=InnoDB crash recovery in progress\n");
  handlerton *ddse = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->dict_recover == nullptr) return true;

  bool error = ddse->dict_recover(dict_recovery_mode, version);
  if (!opt_initialize)
    sysd::notify("STATUS=InnoDB crash recovery ",
                 error ? "unsuccessful" : "successful", "\n");

  /*
    Commit when tablespaces have been initialized, since in that
    case, tablespace meta data is added.
  */
  if (dict_recovery_mode == DICT_RECOVERY_INITIALIZE_TABLESPACES)
    return dd::end_transaction(thd, error);

  return error;
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
  /*
    We would normally use a range based loop here, but the developerstudio
    compiler on Solaris does not handle this when the base collection has
    pure virtual begin() and end() functions.
  */
  for (Properties::const_iterator it = system_tables_props->begin();
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
          system_tables_props->get(it->first, &tbl_prop_str))
        continue;

      const Object_table *table_def = System_tables::instance()->find_table(
          MYSQL_SCHEMA_NAME.str, it->first);
      assert(table_def);

      std::unique_ptr<dd::Properties> tbl_props(
          Properties::parse_properties(tbl_prop_str));

      String_type def;
      if (tbl_props->get(dd::tables::DD_properties::dd_key(
                             dd::tables::DD_properties::DD_property::DEF),
                         &def)) {
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

// Create a DD table using the target table definition.
bool create_target_table(THD *thd, const Object_table *object_table) {
  assert(object_table != nullptr);

  /*
   The target table definition may not be present if the table
   is abandoned. That's ok, not an error.
  */
  if (object_table->is_abandoned()) return false;

  String_type target_ddl_statement("");
  const Object_table_definition *target_table_def =
      object_table->target_table_definition();

  assert(target_table_def != nullptr);
  target_ddl_statement = target_table_def->get_ddl();
  assert(!target_ddl_statement.empty());

  return dd::execute_query(thd, target_ddl_statement);
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
    assert(bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade());
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
  assert(!actual_ddl_statement.empty());

  return dd::execute_query(thd, actual_ddl_statement);
}
/* purecov: end */

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
    if (table_request == nullptr) return true;
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

      assert((*it)->entity()->name() == (*clone_it)->name());

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
    assert(dd_schema_clone->id() == 1);
    assert(dd_tspace_clone->id() == 1);

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

      assert((*it)->entity()->name() == (*table_it)->name());
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
        assert((*it)->entity()->name() == (*clone_it)->name());
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
      error = dd::execute_query(thd, *stmt_it);

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
    We must check if the DDSE is started in a way that makes the DD
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
  bool error = dd::execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
               tables::Collations::instance().populate(thd) ||
               tables::Character_sets::instance().populate(thd);

  /*
    We must commit the re-population before executing a new query, which
    expects the transaction to be empty, and finally, turn FK checks back on.
  */
  error |= dd::end_transaction(thd, error);
  error |= dd::execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1");
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

  assert(dd_schema_id == MYSQL_SCHEMA_DD_ID);
  if (dd_schema_id == INVALID_OBJECT_ID) {
    LogErr(ERROR_LEVEL, ER_DD_SCHEMA_NOT_FOUND, MYSQL_SCHEMA_NAME.str);
    return dd::end_transaction(thd, true);
  }
  assert(cache::Storage_adapter::instance()->core_size<Schema>() == 1);

  // Verify that the core DD tables are present.
#ifndef NDEBUG
  size_t n_core_tables = 0;
#endif
  for (System_tables::Const_iterator it =
           System_tables::instance()->begin(System_tables::Types::CORE);
       it != System_tables::instance()->end();
       it = System_tables::instance()->next(it, System_tables::Types::CORE)) {
    // Skip extraneous tables for minor downgrade.
    if ((*it)->entity() == nullptr) continue;

#ifndef NDEBUG
    n_core_tables++;
#endif

    Table::Name_key table_key;
    Table::update_name_key(&table_key, dd_schema_id, (*it)->entity()->name());
    Object_id dd_table_id =
        cache::Storage_adapter::instance()->core_get_id<Table>(table_key);

    assert(dd_table_id != INVALID_OBJECT_ID);
    if (dd_table_id == INVALID_OBJECT_ID) {
      LogErr(ERROR_LEVEL, ER_DD_TABLE_NOT_FOUND,
             (*it)->entity()->name().c_str());
      return dd::end_transaction(thd, true);
    }
  }
  assert(cache::Storage_adapter::instance()->core_size<Abstract_table>() ==
         n_core_tables);

  // Verify that the dictionary tablespace is present and that its id == 1.
  Tablespace::Name_key tspace_key;
  Tablespace::update_name_key(&tspace_key, MYSQL_TABLESPACE_NAME.str);
  Object_id dd_tspace_id =
      cache::Storage_adapter::instance()->core_get_id<Tablespace>(tspace_key);

  assert(dd_tspace_id == MYSQL_TABLESPACE_DD_ID);
  if (dd_tspace_id == INVALID_OBJECT_ID) {
    LogErr(ERROR_LEVEL, ER_DD_TABLESPACE_NOT_FOUND, MYSQL_TABLESPACE_NAME.str);
    return dd::end_transaction(thd, true);
  }
  assert(cache::Storage_adapter::instance()->core_size<Tablespace>() == 1);

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
    instances are allocated dynamically in the DDSE. These instances will be
    owned by the System_tables registry by the end of this function.
  */
  List<const Object_table> ddse_tables;
  List<const Plugin_tablespace> ddse_tablespaces;
  sysd::notify("STATUS=InnoDB initialization in progress\n");
  bool innodb_init_failed =
      (ddse->ddse_dict_init == nullptr ||
       ddse->ddse_dict_init(dict_init_mode, version, &ddse_tables,
                            &ddse_tablespaces));
  sysd::notify("STATUS=InnoDB initialization ",
               innodb_init_failed ? "unsuccessful" : "successful", "\n");
  if (innodb_init_failed) return true;

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
    Get the server version number from the DD tablespace header and verify
    that we are allowed to upgrade from that version. The error handling is done
    after adding the ddse tables into the system registry to avoid memory leaks.
  */
  if (!opt_initialize) {
    uint server_version = 0;
    if (ddse->dict_get_server_version == nullptr ||
        ddse->dict_get_server_version(&server_version)) {
      LogErr(ERROR_LEVEL, ER_CANNOT_GET_SERVER_VERSION_FROM_TABLESPACE_HEADER);
      return true;
    }

    if (server_version != MYSQL_VERSION_ID) {
      if (opt_upgrade_mode == UPGRADE_NONE) {
        LogErr(ERROR_LEVEL, ER_SERVER_UPGRADE_OFF);
        return true;
      }
      if (!DD_bootstrap_ctx::instance().supported_server_version(
              server_version)) {
        if (server_version > MYSQL_VERSION_ID &&
            !DD_bootstrap_ctx::instance().is_server_patch_downgrade(
                server_version))
          LogErr(ERROR_LEVEL, ER_INVALID_SERVER_DOWNGRADE_NOT_PATCH,
                 server_version, MYSQL_VERSION_ID);
        else
          LogErr(ERROR_LEVEL, ER_SERVER_UPGRADE_VERSION_NOT_SUPPORTED,
                 server_version);
        return true;
      }
    }
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
bool initialize_dictionary(THD *thd, Dictionary_impl *d) {
  store_predefined_tablespace_metadata(thd);
  if (create_dd_schema(thd) || initialize_dd_properties(thd) ||
      create_tables(thd, nullptr))
    return true;

  if (DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_SERVER,
                        d->get_target_dd_version()) ||
      flush_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_INITIALIZE_TABLESPACES,
                        d->get_target_dd_version()) ||
      populate_tables(thd) ||
      update_properties(thd, nullptr, nullptr,
                        String_type(MYSQL_SCHEMA_NAME.str)) ||
      verify_contents(thd) || update_versions(thd))
    return true;

  DBUG_EXECUTE_IF(
      "schema_read_only",
      if (dd::execute_query(thd, "CREATE SCHEMA schema_read_only") ||
          dd::execute_query(thd, "ALTER SCHEMA schema_read_only READ ONLY=1") ||
          dd::execute_query(thd, "CREATE TABLE schema_read_only.t(i INT)") ||
          dd::execute_query(thd, "DROP SCHEMA schema_read_only"))
          assert(false););

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
  assert(d);
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /*
    Each step in the install process below is committed independently,
    either implicitly (for e.g. "CREATE TABLE") or explicitly (for the
    operations in the "populate()" methods). Thus, there is no need to
    commit explicitly here.
  */
  if (DDSE_dict_init(thd, DICT_INIT_CREATE_FILES, d->get_target_dd_version()) ||
      initialize_dictionary(thd, d))
    return true;

  assert(d->get_target_dd_version() == d->get_actual_dd_version(thd));
  LogErr(INFORMATION_LEVEL, ER_DD_VERSION_INSTALLED,
         d->get_target_dd_version());
  return false;
}

// Initialize dictionary in case of server restart.
bool restart_dictionary(THD *thd) {
  Disable_autocommit_guard autocommit_guard(thd);
  Dictionary_impl *d = dd::Dictionary_impl::instance();
  assert(d);
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  if (bootstrap::DDSE_dict_init(thd, DICT_INIT_CHECK_FILES,
                                d->get_target_dd_version())) {
    LogErr(ERROR_LEVEL, ER_DD_SE_INIT_FAILED);
    return true;
  }

  // RAII to handle error messages.
  dd::upgrade::Bootstrap_error_handler bootstrap_error_handler;

  // RAII to handle error in execution of CREATE TABLE.
  Key_length_error_handler key_error_handler;
  /*
    Ignore ER_TOO_LONG_KEY for dictionary tables during restart.
    Do not print the error in error log as we are creating only the
    cached objects and not physical tables.
    TODO: Workaround due to bug#20629014. Remove when the bug is fixed.
  */
  thd->push_internal_handler(&key_error_handler);
  bootstrap_error_handler.set_log_error(false);

  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::STARTED);

  /*
    Set tx_read_only to false to allow installing DD tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.transaction_read_only = false;
  thd->tx_read_only = false;

  // Set explicit_defaults_for_timestamp variable for dictionary creation
  thd->variables.explicit_defaults_for_timestamp = true;

  store_predefined_tablespace_metadata(thd);

  if (create_dd_schema(thd) || initialize_dd_properties(thd) ||
      create_tables(thd, nullptr) || sync_meta_data(thd) ||
      DDSE_dict_recover(thd, DICT_RECOVERY_RESTART_SERVER,
                        d->get_actual_dd_version(thd)) ||
      upgrade::do_server_upgrade_checks(thd) || upgrade::upgrade_tables(thd) ||
      repopulate_charsets_and_collations(thd) || verify_contents(thd) ||
      update_versions(thd)) {
    bootstrap_error_handler.set_log_error(true);
    thd->pop_internal_handler();
    return true;
  }

  DBUG_EXECUTE_IF(
      "schema_read_only",
      if (dd::execute_query(thd, "CREATE SCHEMA schema_read_only") ||
          dd::execute_query(thd, "ALTER SCHEMA schema_read_only READ ONLY=1") ||
          dd::execute_query(thd, "CREATE TABLE schema_read_only.t(i INT)") ||
          dd::execute_query(thd, "DROP SCHEMA schema_read_only") ||
          dd::execute_query(thd, "CREATE TABLE IF NOT EXISTS S.restart(i INT)"))
          assert(false););

  bootstrap::DD_bootstrap_ctx::instance().set_stage(bootstrap::Stage::FINISHED);
  LogErr(INFORMATION_LEVEL, ER_DD_VERSION_FOUND, d->get_actual_dd_version(thd));

  bootstrap_error_handler.set_log_error(true);
  thd->pop_internal_handler();

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
  assert(d);

  assert(d->get_target_dd_version() == d->get_actual_dd_version(thd));

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
    tablespace->set_options(tablespace_def->get_options());
    tablespace->set_se_private_data(tablespace_def->get_se_private_data());
    tablespace->set_engine(tablespace_def->get_engine());

    // Loop over the tablespace files, create dd::Tablespace_file objects.
    List<const Plugin_tablespace::Plugin_tablespace_file> files =
        tablespace_def->get_files();
    List_iterator<const Plugin_tablespace::Plugin_tablespace_file> file_it(
        files);
    const Plugin_tablespace::Plugin_tablespace_file *file = nullptr;
    while ((file = file_it++)) {
      Tablespace_file *space_file = tablespace->add_file();
      space_file->set_filename(file->get_name());
      space_file->set_se_private_data(file->get_se_private_data());
    }

    // All the predefined tablespace are unencrypted (at least for now).
    tablespace->options().set("encryption", "N");

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

bool create_dd_schema(THD *thd) {
  return dd::execute_query(
             thd, dd::String_type("CREATE SCHEMA ") +
                      dd::String_type(MYSQL_SCHEMA_NAME.str) +
                      dd::String_type(" DEFAULT COLLATE ") +
                      dd::String_type(default_charset_info->m_coll_name)) ||
         dd::execute_query(thd, dd::String_type("USE ") +
                                    dd::String_type(MYSQL_SCHEMA_NAME.str));
}

bool getprop(THD *thd, const char *key, uint *value, bool silent = false,
             loglevel level = ERROR_LEVEL) {
  bool exists = false;
  if (dd::tables::DD_properties::instance().get(thd, key, value, &exists) ||
      !exists) {
    /* purecov: begin inspected */
    if (!silent) LogErr(level, ER_FAILED_GET_DD_PROPERTY, key);
    if (level == ERROR_LEVEL) return true;
    /* purecov: end */
  }
  return false;
}

bool getprop(THD *thd, const char *key, String_type *value, bool silent = false,
             loglevel level = ERROR_LEVEL) {
  bool exists = false;
  if (dd::tables::DD_properties::instance().get(thd, key, value, &exists) ||
      !exists) {
    /* purecov: begin inspected */
    if (!silent) LogErr(level, ER_FAILED_GET_DD_PROPERTY, key);
    if (level == ERROR_LEVEL) return true;
    /* purecov: end */
  }
  return false;
}

bool setprop(THD *thd, const char *key, const uint value, bool silent = false) {
  if (dd::tables::DD_properties::instance().set(thd, key, value)) {
    /* purecov: begin inspected */
    if (!silent) LogErr(ERROR_LEVEL, ER_FAILED_SET_DD_PROPERTY, key);
    return true;
    /* purecov: end */
  }
  return false;
}

bool setprop(THD *thd, const char *key, const String_type &value,
             bool silent = false) {
  if (dd::tables::DD_properties::instance().set(thd, key, value)) {
    /* purecov: begin inspected */
    if (!silent) LogErr(ERROR_LEVEL, ER_FAILED_SET_DD_PROPERTY, key);
    return true;
    /* purecov: end */
  }
  return false;
}

bool initialize_dd_properties(THD *thd) {
  // Create the dd_properties table.
  const Object_table_definition *dd_properties_def =
      dd::tables::DD_properties::instance().target_table_definition();
  if (dd::execute_query(thd, dd_properties_def->get_ddl())) return true;

  /*
    We can now decide which version number we will use for the DD, and
    initialize the DD_bootstrap_ctx with the relevant version number.
  */
  uint actual_dd_version = dd::DD_VERSION;
  uint actual_server_version = MYSQL_VERSION_ID;
  uint upgraded_server_version = MYSQL_VERSION_ID;

  bootstrap::DD_bootstrap_ctx::instance().set_actual_dd_version(
      actual_dd_version);
  bootstrap::DD_bootstrap_ctx::instance().set_upgraded_server_version(
      actual_server_version);

  if (!opt_initialize) {
    /*
      First get the DD version, the actual server version and the last
      completed version upgrade (which may be older in case e.g. the
      system table upgrade step failed).
    */
    bool exists = false;
    if (dd::tables::DD_properties::instance().get(
            thd, "DD_VERSION", &actual_dd_version, &exists) ||
        !exists) {
      LogErr(ERROR_LEVEL, ER_DD_NO_VERSION_FOUND);
      return true;
    }

    if (dd::tables::DD_properties::instance().get(
            thd, "MYSQLD_VERSION", &actual_server_version, &exists) ||
        !exists)
      return true;

    if (dd::tables::DD_properties::instance().get(
            thd, "MYSQLD_VERSION_UPGRADED", &upgraded_server_version,
            &exists) ||
        !exists)
      upgraded_server_version = actual_server_version;

    /*
      Get information from DD properties. Do this after 8.2.0 / 8.0.35.
      Older versions do not have the required information available.
    */
    String_type mysql_version_maturity{"INNOVATION"};
    uint server_downgrade_threshold = 0;
    uint server_upgrade_threshold = 0;
    /* purecov: begin inspected */
    if (actual_server_version >= 80035 && actual_server_version != 80100) {
      (void)getprop(thd, "MYSQL_VERSION_STABILITY", &mysql_version_maturity,
                    false, WARNING_LEVEL);
      (void)getprop(thd, "SERVER_DOWNGRADE_THRESHOLD",
                    &server_downgrade_threshold, false, WARNING_LEVEL);
      (void)getprop(thd, "SERVER_UPGRADE_THRESHOLD", &server_upgrade_threshold,
                    false, WARNING_LEVEL);
    }

    /* Is there a server version change? */
    if (MYSQL_VERSION_ID != actual_server_version) {
      if (MYSQL_VERSION_ID > actual_server_version) {
        // This is an upgrade attempt.
        if ((MYSQL_VERSION_ID / 10000) != (actual_server_version / 10000) &&
            (mysql_version_maturity != "LTS" ||
             actual_server_version / 100 == 800 ||
             MYSQL_VERSION_ID / 10000 != actual_server_version / 10000 + 1)) {
          LogErr(ERROR_LEVEL, ER_INVALID_SERVER_UPGRADE_NOT_LTS,
                 actual_server_version, MYSQL_VERSION_ID,
                 actual_server_version);
          return true;
        } else if (MYSQL_VERSION_ID < server_upgrade_threshold &&
                   MYSQL_VERSION_ID / 100 != actual_server_version / 100) {
          LogErr(ERROR_LEVEL, ER_BEYOND_SERVER_UPGRADE_THRESHOLD,
                 actual_server_version, MYSQL_VERSION_ID,
                 server_upgrade_threshold);
          return true;
        }
      } else {
        // This is a downgrade attempt.
        if (MYSQL_VERSION_ID / 100 != actual_server_version / 100) {
          LogErr(ERROR_LEVEL, ER_INVALID_SERVER_DOWNGRADE_NOT_PATCH,
                 actual_server_version, MYSQL_VERSION_ID);
          return true;
        } else if (MYSQL_VERSION_ID < server_downgrade_threshold) {
          LogErr(ERROR_LEVEL, ER_BEYOND_SERVER_DOWNGRADE_THRESHOLD,
                 actual_server_version, MYSQL_VERSION_ID,
                 server_downgrade_threshold);
          return true;
        }
      }
    }

    if (actual_dd_version != dd::DD_VERSION) {
      bootstrap::DD_bootstrap_ctx::instance().set_actual_dd_version(
          actual_dd_version);

      if (!bootstrap::DD_bootstrap_ctx::instance().supported_dd_version()) {
        /*
          If we are attempting on minor downgrade, make sure this is
          supported.
        */
        if (!bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade()) {
          LogErr(ERROR_LEVEL, ER_DD_UPGRADE_VERSION_NOT_SUPPORTED,
                 actual_dd_version);
          return true;
        }

        uint minor_downgrade_threshold = 0;
        if (dd::tables::DD_properties::instance().get(
                thd, "MINOR_DOWNGRADE_THRESHOLD", &minor_downgrade_threshold,
                &exists) ||
            !exists || minor_downgrade_threshold > dd::DD_VERSION) {
          LogErr(ERROR_LEVEL, ER_DD_MINOR_DOWNGRADE_VERSION_NOT_SUPPORTED,
                 actual_dd_version);
          return true;
        }
      }
    }
    /* purecov: end */

    bootstrap::DD_bootstrap_ctx::instance().set_upgraded_server_version(
        upgraded_server_version);

    /*
      If the previous upgrade was not completed, e.g. because system table
      upgrade failed, then we will not accept a new upgrade attempt to an
      even newer version. First, the previous upgrade must be completed so
      that actual_server_version == upgraded_server_version.
    */
    if (DBUG_EVALUATE_IF("simulate_mysql_upgrade_skip_pending", true,
                         actual_server_version != upgraded_server_version &&
                             actual_server_version != MYSQL_VERSION_ID)) {
      LogErr(ERROR_LEVEL, ER_SERVER_UPGRADE_PENDING, MYSQL_VERSION_ID,
             upgraded_server_version);
      return true;
    }

    /*
      Check if we are doing a server upgrade or a server downgrade. An
      upgrade of the DD will of course imply a server upgrade.
    */
    if (upgraded_server_version != MYSQL_VERSION_ID) {
      /*
        This check is also done in DDSE_dict_init() based on the version
        number from the DD tablespace header. Here, we repeat the check,
        this time based on the server version number stored in the DD
        table 'dd_properties'. The two checks should give the same result,
        so this check should never fail; hence, the debug assert.
      */
      if (!bootstrap::DD_bootstrap_ctx::instance().supported_server_version()) {
        LogErr(ERROR_LEVEL, ER_SERVER_UPGRADE_VERSION_NOT_SUPPORTED,
               actual_server_version);
        assert(false);
        return true;
      }
    }

    DBUG_EXECUTE_IF("error_during_bootstrap", return true;);
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
  else if (bootstrap::DD_bootstrap_ctx::instance().is_minor_downgrade())
    LogErr(INFORMATION_LEVEL, ER_DD_MINOR_DOWNGRADE, actual_dd_version,
           dd::DD_VERSION);
  else {
    /*
      If none of the above, then this must be DD upgrade, server
      upgrade, or patch downgrade.
    */
    if (bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade()) {
      LogErr(SYSTEM_LEVEL, ER_DD_UPGRADE, actual_dd_version, dd::DD_VERSION);
      sysd::notify("STATUS=Data Dictionary upgrade in progress\n");
    }
    if (bootstrap::DD_bootstrap_ctx::instance().is_server_upgrade()) {
      // This condition is hit only if upgrade has been skipped before
      if (opt_upgrade_mode == UPGRADE_NONE) {
        LogErr(ERROR_LEVEL, ER_SERVER_UPGRADE_OFF);
        return true;
      }
      LogErr(INFORMATION_LEVEL, ER_SERVER_UPGRADE_FROM_VERSION,
             upgraded_server_version, MYSQL_VERSION_ID);
    } else if (bootstrap::DD_bootstrap_ctx::instance()
                   .is_server_patch_downgrade()) {
      /* purecov: begin inspected */
      if (opt_upgrade_mode == UPGRADE_NONE) {
        LogErr(ERROR_LEVEL, ER_SERVER_UPGRADE_OFF);
        return true;
      }
      LogErr(INFORMATION_LEVEL, ER_SERVER_DOWNGRADE_FROM_VERSION,
             upgraded_server_version, MYSQL_VERSION_ID);
      /* purecov: end */
    }
    assert(bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade() ||
           bootstrap::DD_bootstrap_ctx::instance().is_server_upgrade() ||
           bootstrap::DD_bootstrap_ctx::instance().is_server_patch_downgrade());
  }

  /*
    Unless this is initialization or restart, we must update the
    System_tables registry with the information from the 'dd_properties'
    regarding the actual DD tables.
  */
  if (!bootstrap::DD_bootstrap_ctx::instance().is_initialize() &&
      bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade() &&
      update_system_tables(thd)) {
    return true;
  }

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::FETCHED_PROPERTIES);

  return false;
}

bool is_non_inert_dd_or_ddse_table(System_tables::Types table_type) {
  return table_type == System_tables::Types::CORE ||
         table_type == System_tables::Types::SECOND ||
         table_type == System_tables::Types::DDSE_PRIVATE ||
         table_type == System_tables::Types::DDSE_PROTECTED;
}

bool create_tables(THD *thd, const std::set<String_type> *create_set) {
  // Turn off FK checks, this is needed since we have cyclic FKs.
  if (dd::execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0")) return true;

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
      (bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade() ||
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
  if (error || dd::execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1")) return true;

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::CREATED_TABLES);

  return false;
}

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

    assert(tmp_schema != nullptr && tmp_tspace != nullptr);
    std::unique_ptr<Schema> persisted_dd_schema(
        const_cast<Schema *>(tmp_schema));
    std::unique_ptr<Tablespace> persisted_dd_tspace(
        const_cast<Tablespace *>(tmp_tspace));

    // If the persisted meta data indicates that the DD tablespace is
    // encrypted, then we record this fact to make sure the DDL statements
    // that are generated during e.g. upgrade will have the correct
    // encryption option.
    String_type encryption("");
    Object_table_definition_impl::set_dd_tablespace_encrypted(
        persisted_dd_tspace->options().exists("encryption") &&
        !persisted_dd_tspace->options().get("encryption", &encryption) &&
        encryption == "Y");

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

      /*
        There is a possibility of the InnoDB system tablespace being extended by
        adding additional datafiles during server restart. Hence, we would need
        to check the DD tables to verify which tablespace datafiles have been
        persisted already and then add the extra datafiles to system tablespace
        and persist the updated metadata.

        The documentation mentions that datafiles can only be added to the sytem
        tablespace and can not be removed.
      */
      Tablespace::Name_key predef_tspace_key;
      tspace->update_name_key(&predef_tspace_key);
      const Tablespace *predef_tspace = nullptr;

      if (dd::cache::Storage_adapter::instance()->get(
              thd, predef_tspace_key, ISO_READ_COMMITTED, true, &predef_tspace))
        return dd::end_transaction(thd, true);

      std::unique_ptr<Tablespace> predef_tspace_persist(
          const_cast<Tablespace *>(predef_tspace));

      size_t existing_datafiles, added_datafiles;
      existing_datafiles = predef_tspace->files().size();
      added_datafiles = tspace->files().size() - existing_datafiles;
      if (added_datafiles) {
        std::unordered_set<std::string> predef_tspace_files;
        for (auto tspace_file_it = predef_tspace->files().begin();
             tspace_file_it != predef_tspace->files().end(); ++tspace_file_it) {
          predef_tspace_files.insert((*tspace_file_it)->filename().c_str());
        }

        List<const Plugin_tablespace::Plugin_tablespace_file> files =
            (*it)->entity()->get_files();
        List_iterator<const Plugin_tablespace::Plugin_tablespace_file> file_it(
            files);
        const Plugin_tablespace::Plugin_tablespace_file *file = nullptr;
        while ((file = file_it++)) {
          if (predef_tspace_files.find(file->get_name()) ==
              predef_tspace_files.end()) {
            Tablespace_file *space_file = predef_tspace_persist->add_file();
            space_file->set_filename(file->get_name());
            space_file->set_se_private_data(file->get_se_private_data());
          }
        }
        dd::cache::Storage_adapter::instance()->store(
            thd, predef_tspace_persist.get());
        DBUG_PRINT("info", ("Persisted metadata for additional datafile(s) "
                            "added to the predefined tablespace %s",
                            predef_tspace_persist->name().c_str()));
      }
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
    assert(persisted_dd_schema->id() == 1);
    assert(persisted_dd_tspace->id() == 1);

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
        assert((*it)->entity()->name() == (*table_it)->name());
        dd::cache::Storage_adapter::instance()->core_drop(thd, *table_it);
      }
    }

    std::vector<std::unique_ptr<Table_impl>>::const_iterator persisted_it =
        persisted_dd_tables.begin();
    for (System_tables::Const_iterator it = System_tables::instance()->begin();
         it != System_tables::instance()->end() &&
         persisted_it != persisted_dd_tables.end();
         ++it, ++persisted_it) {
      /*
        If we are in the process of upgrading, there may not be an entry
        in the persisted_dd_tables for new tables that have been added after
        the version we are upgrading from.
      */
      if ((*persisted_it) == nullptr) continue;

      if ((*it)->property() == System_tables::Types::CORE) {
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
  if (dd::end_transaction(thd, false) || dd::execute_query(thd, "FLUSH TABLES"))
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
    If there are leftover schema names from upgrade, delete them
    and remove the names from the DD properties.
  */
  if (target_schema_exists && !target_schema_name.empty()) {
    std::stringstream ss;
    ss << "DROP SCHEMA IF EXISTS " << target_schema_name;
    if (dd::execute_query(thd, ss.str().c_str())) return true;
  }

  if (actual_schema_exists && !actual_schema_name.empty()) {
    std::stringstream ss;
    ss << "DROP SCHEMA IF EXISTS " << actual_schema_name;
    if (dd::execute_query(thd, ss.str().c_str())) return true;
  }

  /*
   The statements above are auto committed, so there is nothing uncommitted
   at this stage. Go ahead and remove the schema keys.
  */
  if (actual_schema_exists)
    (void)dd::tables::DD_properties::instance().remove(thd,
                                                       "UPGRADE_ACTUAL_SCHEMA");

  if (target_schema_exists)
    (void)dd::tables::DD_properties::instance().remove(thd,
                                                       "UPGRADE_TARGET_SCHEMA");

  if (actual_schema_exists || target_schema_exists)
    return dd::end_transaction(thd, false);

  return false;
}

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
      assert((*it)->entity() != nullptr);
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
      assert(dd_table != nullptr);

      std::unique_ptr<dd::Properties> tbl_props(
          dd::Properties::parse_properties(""));

      using dd::tables::DD_properties;
      tbl_props->set(DD_properties::dd_key(DD_properties::DD_property::ID),
                     dd_table->se_private_id());
      tbl_props->set(DD_properties::dd_key(DD_properties::DD_property::DATA),
                     dd_table->se_private_data().raw_string());
      tbl_props->set(
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

bool update_versions(THD *thd) {
  /*
    During initialize, store the DD version number, the LCTN used, and the
    mysqld server version.
  */
  if (opt_initialize) {
    if (setprop(thd, "DD_VERSION", dd::DD_VERSION) ||
        setprop(thd, "MINOR_DOWNGRADE_THRESHOLD",
                dd::DD_VERSION_MINOR_DOWNGRADE_THRESHOLD) ||
        setprop(thd, "SDI_VERSION", dd::SDI_VERSION) ||
        setprop(thd, "LCTN", lower_case_table_names) ||
        setprop(thd, "MYSQL_VERSION_STABILITY", MYSQL_VERSION_MATURITY) ||
        setprop(thd, "SERVER_DOWNGRADE_THRESHOLD",
                SERVER_DOWNGRADE_THRESHOLD) ||
        setprop(thd, "SERVER_UPGRADE_THRESHOLD", SERVER_UPGRADE_THRESHOLD) ||
        setprop(thd, "MYSQLD_VERSION_LO", MYSQL_VERSION_ID) ||
        setprop(thd, "MYSQLD_VERSION_HI", MYSQL_VERSION_ID) ||
        setprop(thd, "MYSQLD_VERSION", MYSQL_VERSION_ID))
      return dd::end_transaction(thd, true);

    if (setprop(thd, "MYSQLD_VERSION_UPGRADED", MYSQL_VERSION_ID)) return true;
    bootstrap::DD_bootstrap_ctx::instance().set_upgraded_server_version(
        MYSQL_VERSION_ID);
  } else {
    uint mysqld_version_lo = 0;
    uint mysqld_version_hi = 0;
    uint mysqld_version = 0;
    uint upgraded_server_version = 0;

    if (getprop(thd, "MYSQLD_VERSION_LO", &mysqld_version_lo) ||
        getprop(thd, "MYSQLD_VERSION_HI", &mysqld_version_hi) ||
        getprop(thd, "MYSQLD_VERSION", &mysqld_version))
      return dd::end_transaction(thd, true);

    if (getprop(thd, "MYSQLD_VERSION_UPGRADED", &upgraded_server_version,
                true)) {
      if (setprop(thd, "MYSQLD_VERSION_UPGRADED", mysqld_version)) return true;
      upgraded_server_version = mysqld_version;
    }
    bootstrap::DD_bootstrap_ctx::instance().set_upgraded_server_version(
        upgraded_server_version);

    if ((mysqld_version_lo > MYSQL_VERSION_ID &&
         setprop(thd, "MYSQLD_VERSION_LO", MYSQL_VERSION_ID)) ||
        (mysqld_version_hi < MYSQL_VERSION_ID &&
         setprop(thd, "MYSQLD_VERSION_HI", MYSQL_VERSION_ID)) ||
        (mysqld_version != MYSQL_VERSION_ID &&
         (setprop(thd, "MYSQLD_VERSION", MYSQL_VERSION_ID) ||
          setprop(thd, "MYSQL_VERSION_STABILITY", MYSQL_VERSION_MATURITY) ||
          setprop(thd, "SERVER_DOWNGRADE_THRESHOLD",
                  SERVER_DOWNGRADE_THRESHOLD) ||
          setprop(thd, "SERVER_UPGRADE_THRESHOLD", SERVER_UPGRADE_THRESHOLD))))
      return dd::end_transaction(thd, true);

    /*
      Update the SDI version number in case of upgrade.
      Note that on downgrade, we keep the old SDI version.
    */
    uint stored_sdi_version = 0;
    if (getprop(thd, "SDI_VERSION", &stored_sdi_version) ||
        (stored_sdi_version < dd::SDI_VERSION &&
         setprop(thd, "SDI_VERSION", dd::SDI_VERSION)))
      return dd::end_transaction(thd, true);

    /*
      Update the DD version number in case of upgrade.
      Note that on downgrade, we keep the old DD version.
    */
    uint dd_version = 0;
    if (getprop(thd, "DD_VERSION", &dd_version) ||
        (dd_version < dd::DD_VERSION &&
         setprop(thd, "DD_VERSION", dd::DD_VERSION)))
      return dd::end_transaction(thd, true);

    /*
      Update the minor downgrade threshold in case of upgrade.
      Note that on downgrade, we keep the threshold version which is
      already present.
    */
    if (dd_version < dd::DD_VERSION &&
        setprop(thd, "MINOR_DOWNGRADE_THRESHOLD",
                dd::DD_VERSION_MINOR_DOWNGRADE_THRESHOLD))
      return dd::end_transaction(thd, true);
  }

  /*
    Update the server version number in the bootstrap ctx and the
    DD tablespace header if we have been doing a server upgrade.
    Note that the update of the tablespace header is not rolled
    back in case of an abort, so this better be the last step we
    do before committing.
  */
  handlerton *ddse = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (bootstrap::DD_bootstrap_ctx::instance().is_server_upgrade() ||
      bootstrap::DD_bootstrap_ctx::instance().is_server_patch_downgrade()) {
    if (ddse->dict_set_server_version == nullptr ||
        ddse->dict_set_server_version()) {
      LogErr(ERROR_LEVEL, ER_CANNOT_SET_SERVER_VERSION_IN_TABLESPACE_HEADER);
      return dd::end_transaction(thd, true);
    }
  }

  /* Keep a record of upgrades, including update releases. */
  upgrade::update_upgrade_history_file(opt_initialize);

#ifndef NDEBUG
  /*
    Debug code to make sure that after updating version numbers, regardless
    of the type of initialization, restart or upgrade, the server version
    number in the DD tablespace header is indeed the same as this server's
    version number.
  */
  uint version = 0;
  assert(ddse->dict_get_server_version != nullptr);
  assert(!ddse->dict_get_server_version(&version));
  assert(version == MYSQL_VERSION_ID);
#endif

  bootstrap::DD_bootstrap_ctx::instance().set_stage(
      bootstrap::Stage::VERSION_UPDATED);

  /*
    During upgrade, this will commit the swap of the old and new DD tables.
  */
  return dd::end_transaction(thd, false);
}

}  // namespace dd
