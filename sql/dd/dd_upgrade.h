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

#ifndef DD_UPGRADE_INCLUDED
#define DD_UPGRADE_INCLUDED

#include "dd/impl/system_registry.h"          // dd::System_tables
#include "sql_class.h"

namespace dd {

/**
  Function to scan mysql schema to check if any tables exist
  with the same name as DD tables to be created.

  This function checks existence of .frm files in mysql schema.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool check_for_dd_tables();

/**
  Migrate plugin table to Data Dictionary.

  All plugin initialization should happens before
  user tables upgrade. It is needed to initialize
  all the Storage Engines.

  @param[in]  thd        Thread handle.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_plugin_table_to_dd(THD *thd);

/**
  Identify all .frm files to upgrade in a database.

  @param[in]  thd                        Thread handle.
  @param[in]  dbname                     Pointer for database name.
  @param[in]  is_fix_view_cols_and_deps  Fix view col data, table and
                                         routines dependency.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool find_files_with_metadata(THD *thd, const char *dbname,
                              bool is_fix_view_cols_and_deps);

/**
  Find all the directories inside data directory. Every directory will be
  treated as a schema. These directories are in filename-encoded form.

  @param[in]  thd        Thread handle.
  @param[out] db_name    An std::vector containing all database name.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool find_schema_from_datadir(THD *thd, std::vector<String_type> *db_name);

/**
  Create entry in mysql.schemata for all the folders found in data directory.
  If db.opt file is not present in any folder, that folder will be treated as
  a database and a warning is issued.

  @param[in]  thd        Thread handle.
  @param[in]  dbname     Schema name.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_schema_to_dd(THD *thd, const char *dbname);

/**
  Migrate all events from mysql.event to mysql.events table.

  @param[in]  thd        Thread handle.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_events_to_dd(THD *thd);

/**
  Migrate all SP/SF from mysql.proc to mysql.routines dd table.

  @param[in]  thd        Thread handle.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_routines_to_dd(THD *thd);

/**
  In case of successful upgrade, move all temporary files
  to a separate folder for backup.

  This function creates a folder named 'backup_metadata_57'
  in data directory. All .frm, .TRG, .TRG, .par, .opt, .isl
  files from all databases are moved inside 'mysql_metadata_57'
  retaining the original hierarchy.

  @param[in]  thd        Thread handle.
*/
void create_metadata_backup(THD *thd);

/**
  Function to implement clean up after upgrade process
  errors out.

  This function deletes all DD tables and .SDI
  files created during upgrade will be deleted.

  mysql.innodb_table_stats and mysql.innodb_index_stats tables
  are not deleted in case upgrade fails.

  @param[in] thd         Thread handle.
  @param[in] last_table  iterator bound to delete dictionary tables.
*/
void drop_dd_tables_and_sdi_files(THD *thd,
       const System_tables::Const_iterator &last_table);

} // namespace dd
#endif // DD_UPGRADE_INCLUDED
