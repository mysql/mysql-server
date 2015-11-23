/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/bootstrapper.h"

#include "log.h"                    // sql_print_warning()
#include "mysql/mysql_lex_string.h" // LEX_STRING
#include "sql_class.h"              // THD
#include "sql_db.h"                 // check_db_dir_existence
#include "sql_prepare.h"            // Ed_connection
#include "transaction.h"            // trans_rollback

#include "dd/iterator.h"                      // dd::Iterator
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/impl/dictionary_impl.h"          // dd::Dictionary_impl
#include "dd/impl/object_table_registry.h"    // dd::Object_table_registry
#include "dd/types/object_table.h"            // dd::Object_table
#include "dd/types/object_table_definition.h" // dd::Object_table_definition

#include "tables/collations.h"                // tables::Collations
#include "tables/character_sets.h"            // tables::Character_sets

#include <vector>
#include <string>
#include <memory>

namespace dd {

///////////////////////////////////////////////////////////////////////////

static bool execute_query(THD *thd, const std::string &q_buf)
{
  Ed_connection con(thd);
  LEX_STRING str;
  thd->make_lex_string(&str, q_buf.c_str(), q_buf.length(), false);
  return con.execute_direct(str);
}

///////////////////////////////////////////////////////////////////////////

// Create the dictionary schema.
static bool create_schema(THD *thd)
{
  return execute_query(thd, std::string("CREATE SCHEMA ") +
                            std::string(MYSQL_SCHEMA_NAME.str))
    || execute_query(thd, std::string("USE ") +
                          std::string(MYSQL_SCHEMA_NAME.str));
}

// Execute create table statements.
static bool create_tables(THD *thd)
{
  // Iterate over DD tables, create tables.
  std::unique_ptr<Iterator<const Object_table> > it(
      Object_table_registry::instance()->types());

  for (const Object_table *t= it->next(); t != NULL; t= it->next())
    if (execute_query(thd, t->table_definition().build_ddl_create_table()))
      return true;

  return false;
}

// Rollback or commit, depending on error.
static bool end_transaction(THD *thd, bool error)
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

// Store the temporarily saved meta data into the DD tables.
static bool store_meta_data(THD *thd)
{
  std::unique_ptr<Iterator<const Object_table> > it(
      Object_table_registry::instance()->types());

  for (const Object_table *t= it->next(); t != NULL; t= it->next())
    if (thd->dd_client()->store(t->table_definition().meta_data()))
      return end_transaction(thd, true);

  return end_transaction(thd, false);
}

// Insert additional data into the tables
static bool populate_tables(THD *thd)
{
  // Iterate over DD tables, populate tables
  std::unique_ptr<Iterator<const Object_table> > it(
      Object_table_registry::instance()->types());

  bool error= false;
  for (const Object_table *t= it->next(); t != NULL && !error; t= it->next())
  {
    // Retrieve list of SQL statements to execute
    std::vector<std::string> stmt= t->table_definition().
      dml_populate_statements();
    for (std::vector<std::string>::iterator it= stmt.begin();
           it != stmt.end() && !error; ++it)
      error= execute_query(thd, *it);

    // Commit the statement based population.
    error= end_transaction(thd, error);

    // If there is no error, call the low level table population method,
    // and commit it.
    if (!error)
    {
      error= t->populate(thd);
      error= end_transaction(thd, error);
    }
  }

  return error;
}

// Execute alter table statements to add cyclic foreign keys
static bool add_cyclic_foreign_keys(THD *thd)
{
  // Iterate over DD tables, add foreign keys
  std::unique_ptr<Iterator<const Object_table> > it(
      Object_table_registry::instance()->types());

  for (const Object_table *t= it->next(); t != NULL; t= it->next())
    if (execute_query(thd, t->table_definition().
                                build_ddl_add_cyclic_foreign_keys()))
      return true;

  return false;
}

// Set the individual dictionary tables as well as the schema to sticky
// in the cache, to keep the objects from being evicted.
static bool make_objects_sticky(THD *thd)
{
  std::unique_ptr<Iterator<const Object_table> > it(
      Object_table_registry::instance()->types());

  for (const Object_table *t= it->next(); t != NULL; t= it->next())
  {
    const Table *table= NULL;
    if (thd->dd_client()->acquire<Table>(MYSQL_SCHEMA_NAME.str,
            t->name(), &table))
      return end_transaction(thd, true);

    if (!table)
    {
      my_error(ER_BAD_TABLE_ERROR, MYF(0), t->name().c_str());
      return end_transaction(thd, true);
    }
    thd->dd_client()->set_sticky(table, true);
  }

  // Get the system schema and make it sticky too. This is necessary
  // to avoid a cache miss when opening a system table (see below) during
  // an attachable transaction.
  const Schema *sys_schema= NULL;
  if (thd->dd_client()->acquire<Schema>(MYSQL_SCHEMA_NAME.str,
                                        &sys_schema))
    return end_transaction(thd, true);

  if (!sys_schema)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), sys_schema->name().c_str());
    return end_transaction(thd, true);
  }
  thd->dd_client()->set_sticky(sys_schema, true);

  return end_transaction(thd, false);
}

// Set an individual system table to sticky in the cache.
static bool register_system_table(THD *thd, const std::string &table_name)
{
  const Table *t= NULL;
  if (thd->dd_client()->acquire<dd::Table>(MYSQL_SCHEMA_NAME.str,
          table_name, &t))
    return true;

  DBUG_ASSERT(t);
  thd->dd_client()->set_sticky(t, true);
  return false;
}

// Register system tables to avoid cache misses during attachable transactions.
static bool register_system_tables(THD *thd)
{
  bool error=
      register_system_table(thd, "help_category") ||
      register_system_table(thd, "help_keyword") ||
      register_system_table(thd, "help_relation") ||
      register_system_table(thd, "help_topic") ||
      register_system_table(thd, "plugin") ||
      register_system_table(thd, "servers") ||
      register_system_table(thd, "time_zone") ||
      register_system_table(thd, "time_zone_leap_second") ||
      register_system_table(thd, "time_zone_name") ||
      register_system_table(thd, "time_zone_transition") ||
      register_system_table(thd, "time_zone_transition_type");

  return end_transaction(thd, error);
}

// Re-populate character sets and collations upon normal restart.
static bool repopulate_charsets_and_collations(THD *thd)
{
  // If we are in read-only mode, we skip re-populating. Here, 'opt_readonly'
  // is the value of the '--read-only' option.
  if (opt_readonly)
  {
    sql_print_warning("Skip re-populating collations and character "
                      "sets tables in read-only mode.");
    return false;
  }

  // We must also check if the DDSE is started in a way that makes the DD
  // read only. For now, we only support InnoDB as SE for the DD. The call
  // to retrieve the handlerton for the DDSE should be replaced by a more
  // generic mechanism.
  handlerton *ddse= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  if (ddse->is_dict_readonly && ddse->is_dict_readonly())
  {
    sql_print_warning("Skip re-populating collations and character "
                      "sets tables in InnoDB read-only mode.");
    return false;
  }

  // Otherwise, turn off FK checks, delete contents, re-populate and commit.
  bool error= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
              execute_query(thd, std::string("DELETE FROM ") +
                                 tables::Collations::table_name()) ||
              execute_query(thd, std::string("DELETE FROM ") +
                                 tables::Character_sets::table_name()) ||
              tables::Collations::instance().populate(thd) ||
              tables::Character_sets::instance().populate(thd);

  // We must commit the re-population before executing a new query, which
  // expects the transaction to be empty, and finally, turn FK checks back on.
  error|= end_transaction(thd, error);
  error|= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1");

  return error;
}

bool Bootstrapper::start(THD *thd)
{
  // Set tx_read_only to false to allow installing DD tables even
  // if the server is started with --transaction-read-only=true.
  thd->variables.tx_read_only= false;
  thd->tx_read_only= false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d= dd::Dictionary_impl::instance();
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  // Check for DD database directory existence explicitly and
  // quietly to avoid errors printed to stderr.
  // Table creation is committed implicitly, so there is
  // no need to commit explicitly here.
  if (check_db_dir_existence(MYSQL_SCHEMA_NAME.str) ||
      execute_query(thd, std::string("USE ") +
                         std::string(MYSQL_SCHEMA_NAME.str)) ||
      create_tables(thd) ||
      register_system_tables(thd) ||
      repopulate_charsets_and_collations(thd) ||
      d->load_and_cache_server_collation(thd) ||
      make_objects_sticky(thd))
    return true;

  return false;
}

bool Bootstrapper::install(THD *thd)
{
  // Set tx_read_only to false to allow installing DD tables even
  // if the server is started with --transaction-read-only=true.
  thd->variables.tx_read_only= false;
  thd->tx_read_only= false;

  Disable_autocommit_guard autocommit_guard(thd);

  Dictionary_impl *d= dd::Dictionary_impl::instance();
  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  // Each step in the install process below is committed independently,
  // either implicitly (for e.g. "CREATE TABLE") or explicitly (for the
  // operations in the "populate()" methods). Thus, there is no need to
  // commit explicitly here.
  if (create_schema(thd) ||
      create_tables(thd) ||
      execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0") ||
      store_meta_data(thd) ||
      populate_tables(thd) ||
      add_cyclic_foreign_keys(thd) ||
      execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1") ||
      d->load_and_cache_server_collation(thd) ||
      make_objects_sticky(thd))
    return true;

  return false;
}

///////////////////////////////////////////////////////////////////////////

}
