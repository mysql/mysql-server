/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

// Implements
#include "storage/ndb/plugin/ndb_dd_sync.h"

#include <functional>

#include "m_string.h"                                 // is_prefix
#include "storage/ndb/plugin/ha_ndbcluster_binlog.h"  // ndbcluster_binlog_setup_table
#include "storage/ndb/plugin/ndb_dd.h"                // ndb_dd_fs_name_case
#include "storage/ndb/plugin/ndb_dd_client.h"         // Ndb_dd_client
#include "storage/ndb/plugin/ndb_dd_disk_data.h"       // ndb_dd_disk_data_*
#include "storage/ndb/plugin/ndb_dd_schema.h"          // ndb_dd_schema_*
#include "storage/ndb/plugin/ndb_dd_table.h"           // ndb_dd_table_*
#include "storage/ndb/plugin/ndb_local_connection.h"   // Ndb_local_connection
#include "storage/ndb/plugin/ndb_log.h"                // ndb_log_*
#include "storage/ndb/plugin/ndb_ndbapi_util.h"        // ndb_get_undofile_names
#include "storage/ndb/plugin/ndb_retry.h"              // ndb_trans_retry
#include "storage/ndb/plugin/ndb_schema_dist_table.h"  // Ndb_schema_dist_table
#include "storage/ndb/plugin/ndb_thd.h"                // get_thd_ndb
#include "storage/ndb/plugin/ndb_thd_ndb.h"            // Thd_ndb

bool Ndb_dd_sync::remove_table(const char *schema_name,
                               const char *table_name) const {
  Ndb_dd_client dd_client(m_thd);

  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
    return false;
  }

  if (!dd_client.remove_table(schema_name, table_name)) {
    return false;
  }

  dd_client.commit();
  return true;  // OK
}

bool Ndb_dd_sync::remove_all_metadata() const {
  DBUG_TRACE;
  ndb_log_verbose(50, "Removing all NDB metadata from DD");

  Ndb_dd_client dd_client(m_thd);

  // Remove logfile groups
  // Fetch logfile group names from DD
  std::unordered_set<std::string> lfg_names;
  if (!dd_client.fetch_ndb_logfile_group_names(lfg_names)) {
    ndb_log_error("Failed to fetch logfile group names from DD");
    return false;
  }

  for (const std::string &logfile_group_name : lfg_names) {
    ndb_log_info("Removing logfile group '%s'", logfile_group_name.c_str());
    if (!dd_client.mdl_lock_logfile_group_exclusive(
            logfile_group_name.c_str())) {
      ndb_log_info("MDL lock could not be acquired for logfile group '%s'",
                   logfile_group_name.c_str());
      return false;
    }
    if (!dd_client.drop_logfile_group(logfile_group_name.c_str())) {
      ndb_log_info("Failed to remove logfile group '%s' from DD",
                   logfile_group_name.c_str());
      return false;
    }
  }
  dd_client.commit();

  // Remove tablespaces
  // Retrieve list of tablespaces from DD
  std::unordered_set<std::string> tablespace_names;
  if (!dd_client.fetch_ndb_tablespace_names(tablespace_names)) {
    ndb_log_error("Failed to fetch tablespace names from DD");
    return false;
  }

  for (const std::string &tablespace_name : tablespace_names) {
    ndb_log_info("Removing tablespace '%s'", tablespace_name.c_str());
    if (!dd_client.mdl_lock_tablespace_exclusive(tablespace_name.c_str())) {
      ndb_log_warning("MDL lock could not be acquired on tablespace '%s'",
                      tablespace_name.c_str());
      return true;
    }
    if (!dd_client.drop_tablespace(tablespace_name.c_str())) {
      ndb_log_warning("Failed to remove tablespace '%s' from DD",
                      tablespace_name.c_str());
      return false;
    }
  }
  dd_client.commit();

  // Fetch list of schemas in DD
  std::vector<std::string> schema_names;
  if (!dd_client.fetch_schema_names(&schema_names)) {
    ndb_log_error("Failed to fetch schema names from DD");
    return false;
  }

  ndb_log_verbose(50, "Found %zu schemas in DD", schema_names.size());

  // Iterate over each schema and remove all NDB tables
  for (const std::string &name : schema_names) {
    const char *schema_name = name.c_str();
    // Lock the schema in DD
    if (!dd_client.mdl_lock_schema(schema_name)) {
      ndb_log_error("Failed to acquire MDL lock on schema '%s'", schema_name);
      return false;
    }

    ndb_log_verbose(50, "Fetching list of NDB tables in schema '%s'",
                    schema_name);

    // Fetch list of NDB tables in DD, also acquire MDL lock on table names
    std::unordered_set<std::string> ndb_tables;
    if (!dd_client.get_ndb_table_names_in_schema(schema_name, &ndb_tables)) {
      ndb_log_error("Failed to get list of NDB tables in schema '%s' from DD",
                    schema_name);
      return false;
    }
    ndb_log_verbose(50, "Found %zu NDB tables in schema '%s'",
                    ndb_tables.size(), schema_name);
    for (const std::string &table_name : ndb_tables) {
      // Check if the table has a trigger. Such tables are handled differently
      // and not deleted as that would result in the trigger being deleted as
      // well
      const dd::Table *table_def;
      if (!dd_client.get_table(schema_name, table_name.c_str(), &table_def)) {
        ndb_log_error("Failed to open table '%s.%s' from DD", schema_name,
                      table_name.c_str());
        return false;
      }
      if (ndb_dd_table_has_trigger(table_def)) continue;

      ndb_log_info("Removing table '%s.%s'", schema_name, table_name.c_str());
      if (!remove_table(schema_name, table_name.c_str())) {
        ndb_log_error("Failed to remove table '%s.%s' from DD", schema_name,
                      table_name.c_str());
        return false;
      }
    }
  }
  return true;
}

void Ndb_dd_sync::log_NDB_error(const NdbError &ndb_error) const {
  // Display error code and message returned by NDB
  ndb_log_error("Got error '%d: %s' from NDB", ndb_error.code,
                ndb_error.message);
}

bool Ndb_dd_sync::remove_deleted_tables() const {
  DBUG_TRACE;
  ndb_log_verbose(50, "Looking to remove tables deleted in NDB");

  Ndb_dd_client dd_client(m_thd);
  // Fetch list of schemas in DD
  std::vector<std::string> schema_names;
  if (!dd_client.fetch_schema_names(&schema_names)) {
    ndb_log_error("Failed to fetch schema names from DD");
    return false;
  }

  ndb_log_verbose(50, "Found %zu databases in DD", schema_names.size());

  // Iterate over each schema and remove deleted NDB tables from the DD
  for (const std::string &name : schema_names) {
    const char *schema_name = name.c_str();
    // Lock the schema in DD
    if (!dd_client.mdl_lock_schema(schema_name)) {
      ndb_log_error("Failed to acquire MDL lock on schema '%s'", schema_name);
      return false;
    }

    ndb_log_verbose(50, "Fetching list of NDB tables in schema '%s'",
                    schema_name);

    // Fetch list of NDB tables in DD, also acquire MDL lock on table names
    std::unordered_set<std::string> ndb_tables_in_DD;
    if (!dd_client.get_ndb_table_names_in_schema(schema_name,
                                                 &ndb_tables_in_DD)) {
      ndb_log_error("Failed to get list of NDB tables in schema '%s' from DD",
                    schema_name);
      return false;
    }
    ndb_log_verbose(50, "Found %zu NDB tables in DD", ndb_tables_in_DD.size());

    if (ndb_tables_in_DD.empty()) {
      // No NDB tables in this schema
      continue;
    }

    // Fetch list of tables in NDB. The util tables are skipped since the
    // ndb_schema, ndb_schema_result, and ndb_sql_metadata tables are handled
    // separately during binlog setup. The index stat tables are not installed
    // in the DD.
    std::unordered_set<std::string> tables_in_NDB;
    std::unordered_set<std::string> temp_tables_in_NDB;
    if (!ndb_get_table_names_in_schema(m_thd_ndb->ndb->getDictionary(),
                                       schema_name, &tables_in_NDB,
                                       &temp_tables_in_NDB)) {
      log_NDB_error(m_thd_ndb->ndb->getDictionary()->getNdbError());
      ndb_log_error(
          "Failed to get list of NDB tables in schema '%s' from "
          "NDB",
          schema_name);
      return false;
    }

    ndb_log_verbose(50,
                    "Found %zu NDB tables in schema '%s' in the NDB Dictionary",
                    tables_in_NDB.size(), schema_name);

    remove_copying_alter_temp_tables(schema_name, temp_tables_in_NDB);

    // Iterate over all NDB tables found in DD. If they don't exist in NDB
    // anymore, then remove the table from DD
    for (const std::string &ndb_table_name : ndb_tables_in_DD) {
      if (tables_in_NDB.find(ndb_table_name) == tables_in_NDB.end()) {
        ndb_log_info("Removing table '%s.%s'", schema_name,
                     ndb_table_name.c_str());
        if (!remove_table(schema_name, ndb_table_name.c_str())) {
          ndb_log_error("Failed to remove table '%s.%s' from DD", schema_name,
                        ndb_table_name.c_str());
          return false;
        }
      }
    }
  }

  ndb_log_verbose(50, "Deleted NDB tables removed from DD");
  return true;
}

bool Ndb_dd_sync::install_logfile_group(
    const char *logfile_group_name, NdbDictionary::LogfileGroup ndb_lfg,
    const std::vector<std::string> &undofile_names,
    bool force_overwrite) const {
  Ndb_dd_client dd_client(m_thd);
  if (!dd_client.mdl_lock_logfile_group_exclusive(logfile_group_name)) {
    ndb_log_error("MDL lock could not be acquired for logfile group '%s'",
                  logfile_group_name);
    return false;
  }

  if (!dd_client.install_logfile_group(
          logfile_group_name, undofile_names, ndb_lfg.getObjectId(),
          ndb_lfg.getObjectVersion(), force_overwrite)) {
    ndb_log_error("Logfile group '%s' could not be stored in DD",
                  logfile_group_name);
    return false;
  }

  dd_client.commit();
  return true;
}

bool Ndb_dd_sync::compare_file_list(
    const std::vector<std::string> &file_names_in_NDB,
    const std::vector<std::string> &file_names_in_DD) const {
  if (file_names_in_NDB.size() != file_names_in_DD.size()) {
    return false;
  }

  for (const std::string &file_name : file_names_in_NDB) {
    if (std::find(file_names_in_DD.begin(), file_names_in_DD.end(),
                  file_name) == file_names_in_DD.end()) {
      return false;
    }
  }
  return true;
}

bool Ndb_dd_sync::synchronize_logfile_group(
    const char *logfile_group_name,
    const std::unordered_set<std::string> &lfg_in_DD) const {
  ndb_log_verbose(1, "Synchronizing logfile group '%s'", logfile_group_name);

  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();

  const auto lfg_position = lfg_in_DD.find(logfile_group_name);
  if (lfg_position == lfg_in_DD.end()) {
    // Logfile group exists only in NDB. Install into DD
    ndb_log_info("Logfile group '%s' does not exist in DD, installing..",
                 logfile_group_name);
    NdbDictionary::LogfileGroup ndb_lfg =
        dict->getLogfileGroup(logfile_group_name);
    if (ndb_dict_check_NDB_error(dict)) {
      // Failed to open logfile group from NDB
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get logfile group '%s' from NDB",
                    logfile_group_name);
      return false;
    }
    std::vector<std::string> undofile_names;
    if (!ndb_get_undofile_names(dict, logfile_group_name, &undofile_names)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error(
          "Failed to get undofiles assigned to logfile group '%s' "
          "from NDB",
          logfile_group_name);
      return false;
    }
    if (!install_logfile_group(logfile_group_name, ndb_lfg, undofile_names,
                               false /*force_overwrite*/)) {
      return false;
    }
    return true;
  }

  // Logfile group exists in DD
  Ndb_dd_client dd_client(m_thd);
  if (!dd_client.mdl_lock_logfile_group(logfile_group_name,
                                        true /* intention_exclusive */)) {
    ndb_log_error("MDL lock could not be acquired for logfile group '%s'",
                  logfile_group_name);
    return false;
  }
  const dd::Tablespace *existing = nullptr;
  if (!dd_client.get_logfile_group(logfile_group_name, &existing)) {
    ndb_log_error("Failed to acquire logfile group '%s' from DD",
                  logfile_group_name);
    return false;
  }

  if (existing == nullptr) {
    ndb_log_error("Logfile group '%s' does not exist in DD",
                  logfile_group_name);
    assert(false);
    return false;
  }

  // Check if the DD has the latest definition of the logfile group
  int object_id_in_DD, object_version_in_DD;
  if (!ndb_dd_disk_data_get_object_id_and_version(existing, object_id_in_DD,
                                                  object_version_in_DD)) {
    ndb_log_error(
        "Could not extract id and version from the definition "
        "of logfile group '%s'",
        logfile_group_name);
    assert(false);
    return false;
  }

  NdbDictionary::LogfileGroup ndb_lfg =
      dict->getLogfileGroup(logfile_group_name);
  if (ndb_dict_check_NDB_error(dict)) {
    // Failed to open logfile group from NDB
    log_NDB_error(dict->getNdbError());
    ndb_log_error("Failed to get logfile group '%s' from NDB",
                  logfile_group_name);
    return false;
  }
  const int object_id_in_NDB = ndb_lfg.getObjectId();
  const int object_version_in_NDB = ndb_lfg.getObjectVersion();
  std::vector<std::string> undofile_names_in_NDB;
  if (!ndb_get_undofile_names(dict, logfile_group_name,
                              &undofile_names_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    ndb_log_error(
        "Failed to get undofiles assigned to logfile group '%s' "
        "from NDB",
        logfile_group_name);
    return false;
  }

  std::vector<std::string> undofile_names_in_DD;
  ndb_dd_disk_data_get_file_names(existing, undofile_names_in_DD);
  if (object_id_in_NDB != object_id_in_DD ||
      object_version_in_NDB != object_version_in_DD ||
      /*
        The object version is not updated after an ALTER, so there
        exists a possibility that the object ids and versions match
        but there's a mismatch in the list of undo files assigned to
        the logfile group. Thus, the list of files assigned to the
        logfile group in NDB Dictionary and DD are compared as an
        additional check. This also protects us from the corner case
        that's possible after an initial cluster restart. In such
        cases, it's possible the ids and versions match even though
        they are entirely different objects
      */
      !compare_file_list(undofile_names_in_NDB, undofile_names_in_DD)) {
    ndb_log_info(
        "Logfile group '%s' has outdated version in DD, "
        "reinstalling..",
        logfile_group_name);
    if (!install_logfile_group(logfile_group_name, ndb_lfg,
                               undofile_names_in_NDB,
                               true /* force_overwrite */)) {
      return false;
    }
  }

  // Same definition of logfile group exists in both DD and NDB Dictionary
  return true;
}

bool Ndb_dd_sync::synchronize_logfile_groups() const {
  ndb_log_info("Synchronizing logfile groups");

  // Retrieve list of logfile groups from NDB
  std::unordered_set<std::string> lfg_in_NDB;
  const NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (!ndb_get_logfile_group_names(dict, lfg_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    ndb_log_error("Failed to fetch logfile group names from NDB");
    return false;
  }

  Ndb_dd_client dd_client(m_thd);

  // Retrieve list of logfile groups from DD
  std::unordered_set<std::string> lfg_in_DD;
  if (!dd_client.fetch_ndb_logfile_group_names(lfg_in_DD)) {
    ndb_log_error("Failed to fetch logfile group names from DD");
    return false;
  }

  for (const std::string &logfile_group_name : lfg_in_NDB) {
    if (!synchronize_logfile_group(logfile_group_name.c_str(), lfg_in_DD)) {
      ndb_log_info("Failed to synchronize logfile group '%s'",
                   logfile_group_name.c_str());
    }
    lfg_in_DD.erase(logfile_group_name);
  }

  // Entries left in lfg_in_DD exist in DD alone and not NDB and can be removed
  for (const std::string &logfile_group_name : lfg_in_DD) {
    ndb_log_info("Logfile group '%s' does not exist in NDB, dropping",
                 logfile_group_name.c_str());
    if (!dd_client.mdl_lock_logfile_group_exclusive(
            logfile_group_name.c_str())) {
      ndb_log_info("MDL lock could not be acquired for logfile group '%s'",
                   logfile_group_name.c_str());
      ndb_log_info("Failed to synchronize logfile group '%s'",
                   logfile_group_name.c_str());
      continue;
    }
    if (!dd_client.drop_logfile_group(logfile_group_name.c_str())) {
      ndb_log_info("Failed to synchronize logfile group '%s'",
                   logfile_group_name.c_str());
    }
  }
  dd_client.commit();
  return true;
}

bool Ndb_dd_sync::install_tablespace(
    const char *tablespace_name, NdbDictionary::Tablespace ndb_tablespace,
    const std::vector<std::string> &data_file_names,
    bool force_overwrite) const {
  Ndb_dd_client dd_client(m_thd);
  if (!dd_client.mdl_lock_tablespace_exclusive(tablespace_name)) {
    ndb_log_error("MDL lock could not be acquired for tablespace '%s'",
                  tablespace_name);
    return false;
  }

  if (!dd_client.install_tablespace(
          tablespace_name, data_file_names, ndb_tablespace.getObjectId(),
          ndb_tablespace.getObjectVersion(), force_overwrite)) {
    ndb_log_error("Tablespace '%s' could not be stored in DD", tablespace_name);
    return false;
  }

  dd_client.commit();
  return true;
}

bool Ndb_dd_sync::synchronize_tablespace(
    const char *tablespace_name,
    const std::unordered_set<std::string> &tablespaces_in_DD) const {
  ndb_log_verbose(1, "Synchronizing tablespace '%s'", tablespace_name);

  if (DBUG_EVALUATE_IF("ndb_install_tablespace_fail", true, false)) {
    ndb_log_verbose(20, "Skipping synchronization of tablespace '%s'",
                    tablespace_name);
    return false;
  }

  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  const auto tablespace_position = tablespaces_in_DD.find(tablespace_name);

  if (tablespace_position == tablespaces_in_DD.end()) {
    // Tablespace exists only in NDB. Install in DD
    ndb_log_info("Tablespace '%s' does not exist in DD, installing..",
                 tablespace_name);
    NdbDictionary::Tablespace ndb_tablespace =
        dict->getTablespace(tablespace_name);
    if (ndb_dict_check_NDB_error(dict)) {
      // Failed to open tablespace from NDB
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get tablespace '%s' from NDB", tablespace_name);
      return false;
    }
    std::vector<std::string> datafile_names;
    if (!ndb_get_datafile_names(dict, tablespace_name, &datafile_names)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get datafiles assigned to tablespace '%s'",
                    tablespace_name);
      return false;
    }
    if (!install_tablespace(tablespace_name, ndb_tablespace, datafile_names,
                            false /*force_overwrite*/)) {
      return false;
    }
    return true;
  }

  // Tablespace exists in DD
  Ndb_dd_client dd_client(m_thd);
  if (!dd_client.mdl_lock_tablespace(tablespace_name,
                                     true /* intention_exclusive */)) {
    ndb_log_error("MDL lock could not be acquired on tablespace '%s'",
                  tablespace_name);
    return false;
  }
  const dd::Tablespace *existing = nullptr;
  if (!dd_client.get_tablespace(tablespace_name, &existing)) {
    ndb_log_error("Failed to acquire tablespace '%s' from DD", tablespace_name);
    return false;
  }

  if (existing == nullptr) {
    ndb_log_error("Tablespace '%s' does not exist in DD", tablespace_name);
    assert(false);
    return false;
  }

  // Check if the DD has the latest definition of the tablespace
  int object_id_in_DD, object_version_in_DD;
  if (!ndb_dd_disk_data_get_object_id_and_version(existing, object_id_in_DD,
                                                  object_version_in_DD)) {
    ndb_log_error(
        "Could not extract id and version from the definition "
        "of tablespace '%s'",
        tablespace_name);
    assert(false);
    return false;
  }

  NdbDictionary::Tablespace ndb_tablespace =
      dict->getTablespace(tablespace_name);
  if (ndb_dict_check_NDB_error(dict)) {
    // Failed to open tablespace from NDB
    log_NDB_error(dict->getNdbError());
    ndb_log_error("Failed to get tablespace '%s' from NDB", tablespace_name);
    return false;
  }
  const int object_id_in_NDB = ndb_tablespace.getObjectId();
  const int object_version_in_NDB = ndb_tablespace.getObjectVersion();
  std::vector<std::string> datafile_names_in_NDB;
  if (!ndb_get_datafile_names(dict, tablespace_name, &datafile_names_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    ndb_log_error(
        "Failed to get datafiles assigned to tablespace '%s' from "
        "NDB",
        tablespace_name);
    return false;
  }

  std::vector<std::string> datafile_names_in_DD;
  ndb_dd_disk_data_get_file_names(existing, datafile_names_in_DD);
  if (object_id_in_NDB != object_id_in_DD ||
      object_version_in_NDB != object_version_in_DD ||
      /*
        The object version is not updated after an ALTER, so there
        exists a possibility that the object ids and versions match
        but there's a mismatch in the list of data files assigned to
        the tablespace. Thus, the list of files assigned to the
        tablespace in NDB Dictionary and DD are compared as an
        additional check. This also protects us from the corner case
        that's possible after an initial cluster restart. In such
        cases, it's possible the ids and versions match even though
        they are entirely different objects
      */
      !compare_file_list(datafile_names_in_NDB, datafile_names_in_DD)) {
    ndb_log_info(
        "Tablespace '%s' has outdated version in DD, "
        "reinstalling..",
        tablespace_name);
    if (!install_tablespace(tablespace_name, ndb_tablespace,
                            datafile_names_in_NDB,
                            true /* force_overwrite */)) {
      return false;
    }
  }

  // Same definition of tablespace exists in both DD and NDB Dictionary
  return true;
}

bool Ndb_dd_sync::synchronize_tablespaces() const {
  ndb_log_info("Synchronizing tablespaces");

  // Retrieve list of tablespaces from NDB
  std::unordered_set<std::string> tablespaces_in_NDB;
  const NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (!ndb_get_tablespace_names(dict, tablespaces_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    ndb_log_error("Failed to fetch tablespace names from NDB");
    return false;
  }

  Ndb_dd_client dd_client(m_thd);
  // Retrieve list of tablespaces from DD
  std::unordered_set<std::string> tablespaces_in_DD;
  if (!dd_client.fetch_ndb_tablespace_names(tablespaces_in_DD)) {
    ndb_log_error("Failed to fetch tablespace names from DD");
    return false;
  }

  for (const std::string &tablespace_name : tablespaces_in_NDB) {
    if (!synchronize_tablespace(tablespace_name.c_str(), tablespaces_in_DD)) {
      ndb_log_warning("Failed to synchronize tablespace '%s'",
                      tablespace_name.c_str());
    }
    tablespaces_in_DD.erase(tablespace_name);
  }

  // Entries left in tablespaces_in_DD exist in DD alone and not NDB and can be
  // removed
  for (const std::string &tablespace_name : tablespaces_in_DD) {
    ndb_log_info("Tablespace '%s' does not exist in NDB, dropping",
                 tablespace_name.c_str());
    if (!dd_client.mdl_lock_tablespace_exclusive(tablespace_name.c_str())) {
      ndb_log_warning("MDL lock could not be acquired on tablespace '%s'",
                      tablespace_name.c_str());
      ndb_log_warning("Failed to synchronize tablespace '%s'",
                      tablespace_name.c_str());
      continue;
    }
    if (!dd_client.drop_tablespace(tablespace_name.c_str())) {
      ndb_log_warning("Failed to synchronize tablespace '%s'",
                      tablespace_name.c_str());
    }
  }
  dd_client.commit();
  return true;
}

const NdbError *Ndb_dd_sync::fetch_database_ddls(
    NdbTransaction *ndb_transaction, const NdbDictionary::Table *ndb_schema_tab,
    std::vector<Ndb_schema_tuple> *database_ddls) {
  assert(ndb_transaction != nullptr);
  DBUG_TRACE;

  // Create scan operation and define the read
  NdbScanOperation *op = ndb_transaction->getNdbScanOperation(ndb_schema_tab);
  if (op == nullptr) {
    return &ndb_transaction->getNdbError();
  }

  if (op->readTuples(NdbScanOperation::LM_Read, NdbScanOperation::SF_TupScan,
                     1) != 0) {
    return &op->getNdbError();
  }

  // Define the attributes to be fetched
  NdbRecAttr *ndb_rec_db = op->getValue(Ndb_schema_dist_table::COL_DB);
  NdbRecAttr *ndb_rec_name = op->getValue(Ndb_schema_dist_table::COL_NAME);
  NdbRecAttr *ndb_rec_id = op->getValue(Ndb_schema_dist_table::COL_ID);
  NdbRecAttr *ndb_rec_version =
      op->getValue(Ndb_schema_dist_table::COL_VERSION);
  if (!ndb_rec_db || !ndb_rec_name || !ndb_rec_id || !ndb_rec_version) {
    return &op->getNdbError();
  }

  char query[64000];
  NdbBlob *query_blob_handle =
      op->getBlobHandle(Ndb_schema_dist_table::COL_QUERY);
  if (!query_blob_handle ||
      (query_blob_handle->getValue(query, sizeof(query)) != 0)) {
    return &op->getNdbError();
  }

  // Start scanning
  if (ndb_transaction->execute(NdbTransaction::NoCommit)) {
    return &ndb_transaction->getNdbError();
  }

  // Handle the results and store it in the map
  while ((op->nextResult()) == 0) {
    std::string db_name = Ndb_util_table::unpack_varbinary(ndb_rec_db);
    std::string table_name = Ndb_util_table::unpack_varbinary(ndb_rec_name);
    // Database DDLs are entries with no table_name
    if (table_name.empty()) {
      // NULL terminate the query string using the length of the query blob
      Uint64 query_length = 0;
      if (query_blob_handle->getLength(query_length)) {
        return &query_blob_handle->getNdbError();
      }
      query[query_length] = 0;

      // Inspect the query string further to find out the DDL type
      Ndb_schema_ddl_type type;
      if (native_strncasecmp("CREATE", query, 6) == 0) {
        type = SCHEMA_DDL_CREATE;
      } else if (native_strncasecmp("ALTER", query, 5) == 0) {
        type = SCHEMA_DDL_ALTER;
      } else if (native_strncasecmp("DROP", query, 4) == 0) {
        type = SCHEMA_DDL_DROP;
      } else {
        // Not a database DDL skip this one
        continue;
      }
      // Add the database DDL to the map
      database_ddls->push_back(std::make_tuple(db_name, query, type,
                                               ndb_rec_id->u_32_value(),
                                               ndb_rec_version->u_32_value()));
    }
  }
  // Successfully read the rows. Return to caller
  return nullptr;
}

bool Ndb_dd_sync::synchronize_databases() const {
  /*
    NDB has no representation of the database schema objects, but the
    mysql.ndb_schema table contains the latest schema operations done via a
    MySQL Server, and thus reflects databases created/dropped/altered.
  */
  ndb_log_info("Synchronizing databases");
  DBUG_TRACE;

  /*
    Function should only be called while ndbcluster_global_schema_lock
    is held, to ensure that ndb_schema table is not being updated while
    synchronizing the databases.
  */
  if (!m_thd_ndb->has_required_global_schema_lock(
          "Ndb_binlog_setup::synchronize_databases"))
    return false;

  // Open the ndb_schema table for reading
  Ndb *ndb = m_thd_ndb->ndb;
  Ndb_schema_dist_table ndb_schema_table(m_thd_ndb);
  if (!ndb_schema_table.open()) {
    const NdbError &ndb_error = ndb->getDictionary()->getNdbError();
    ndb_log_error("Failed to open ndb_schema table, Error : %u(%s)",
                  ndb_error.code, ndb_error.message);
    return false;
  }
  const NdbDictionary::Table *ndbtab = ndb_schema_table.get_table();

  // Create the std::function instance of fetch_database_ddls() to be used with
  // ndb_trans_retry()
  std::function<const NdbError *(NdbTransaction *, const NdbDictionary::Table *,
                                 std::vector<Ndb_schema_tuple> *)>
      fetch_db_func = std::bind(&fetch_database_ddls, std::placeholders::_1,
                                std::placeholders::_2, std::placeholders::_3);

  // Read ndb_schema and fetch the database DDLs
  NdbError last_ndb_err;
  std::vector<Ndb_schema_tuple> database_ddls;
  if (!ndb_trans_retry(ndb, m_thd, last_ndb_err, fetch_db_func, ndbtab,
                       &database_ddls)) {
    ndb_log_error(
        "Failed to fetch database DDL from ndb_schema table. Error : %u(%s)",
        last_ndb_err.code, last_ndb_err.message);
    return false;
  }

  // Fetch list of databases used in NDB
  std::unordered_set<std::string> databases_in_NDB;
  if (!ndb_get_database_names_in_dictionary(m_thd_ndb->ndb->getDictionary(),
                                            &databases_in_NDB)) {
    ndb_log_error("Failed to fetch database names from NDB");
    return false;
  }

  // Read all the databases from DD
  Ndb_dd_client dd_client(m_thd);
  std::map<std::string, const dd::Schema *> databases_in_DD;
  if (!dd_client.fetch_all_schemas(databases_in_DD)) {
    ndb_log_error("Failed to fetch schema details from DD");
    return false;
  }

  // Mark this as a participant so that the any DDLs don't get distributed
  Thd_ndb::Options_guard thd_ndb_options(m_thd_ndb);
  thd_ndb_options.set(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT);

  // Inspect the DDLs obtained from ndb_schema and act upon it based on the list
  // of databases available in the DD and NDB
  Ndb_local_connection mysqld(m_thd);
  for (auto &ddl_tuple : database_ddls) {
    // Read all the values from tuple
    std::string db_name, query;
    Ndb_schema_ddl_type schema_ddl_type;
    unsigned int ddl_counter, ddl_node_id;
    std::tie(db_name, query, schema_ddl_type, ddl_counter, ddl_node_id) =
        ddl_tuple;
    assert(ddl_counter != 0 && ddl_node_id != 0);
    ndb_log_verbose(5,
                    "ndb_schema query : '%s', db : '%s', "
                    "counter : %u, node_id : %u",
                    query.c_str(), db_name.c_str(), ddl_counter, ddl_node_id);

    // Check if the database exists in DD and read in its version info
    bool db_exists_in_DD = false;
    bool tables_exist_in_database = false;
    unsigned int schema_counter = 0, schema_node_id = 0;
    /*
      Convert the database name to lower case on platforms that have
      lower_case_table_names set to 2. In such situations, upper case names are
      stored in lower case in NDB Dictionary
    */
    const std::string ndb_db_name = ndb_dd_fs_name_case(db_name.c_str());
    auto it = databases_in_DD.find(ndb_db_name);
    if (it != databases_in_DD.end()) {
      db_exists_in_DD = true;

      // Read se_private_data
      const dd::Schema *schema = it->second;
      ndb_dd_schema_get_counter_and_nodeid(schema, schema_counter,
                                           schema_node_id);
      ndb_log_verbose(5,
                      "Found schema '%s' in DD with "
                      "counter : %u, node_id : %u",
                      db_name.c_str(), ddl_counter, ddl_node_id);

      // Check if there are any local tables
      if (!ndb_dd_has_local_tables_in_schema(m_thd, db_name.c_str(),
                                             tables_exist_in_database)) {
        ndb_log_error("Failed to check if the Schema '%s' has any local tables",
                      db_name.c_str());
        return false;
      }
    }

    // Check if the database has tables in NDB
    tables_exist_in_database |=
        (databases_in_NDB.find(ndb_db_name) != databases_in_NDB.end());

    // Handle the relevant DDL based on the existence of the database in DD and
    // NDB
    switch (schema_ddl_type) {
      case SCHEMA_DDL_CREATE: {
        // Flags to decide if the database needs to be created
        bool create_database = !db_exists_in_DD;
        bool update_version = create_database;

        if (db_exists_in_DD &&
            (ddl_node_id != schema_node_id || ddl_counter != schema_counter)) {
          // Database exists in DD but version differs.
          // Drop and recreate database iff it is empty
          if (!tables_exist_in_database) {
            if (mysqld.drop_database(db_name)) {
              ndb_log_error("Failed to update database '%s'", db_name.c_str());
              return false;
            }
            // Mark that the database needs to be created
            create_database = true;
          } else {
            // Database has tables in it. Just update the version later
            ndb_log_warning(
                "Database '%s' exists already with a different version",
                db_name.c_str());
          }
          /*
            The version information in the ndb_schema is the right version.
            So, always update the version of schema in DD to that in the
            ndb_schema if they differ
          */
          update_version = true;
        }

        if (create_database) {
          // Create it by running the DDL
          if (mysqld.execute_database_ddl(query)) {
            ndb_log_error("Failed to create database '%s'.", db_name.c_str());
            return false;
          }
          ndb_log_info("Created database '%s'", db_name.c_str());
        }

        if (update_version) {
          // Update the schema version
          if (!ndb_dd_update_schema_version(m_thd, db_name.c_str(), ddl_counter,
                                            ddl_node_id)) {
            ndb_log_error("Failed to update version in DD for database : '%s'",
                          db_name.c_str());
            return false;
          }
          ndb_log_info(
              "Updated the version of database '%s' to "
              "counter : %u, node_id : %u",
              db_name.c_str(), ddl_counter, ddl_node_id);
        }

        // Remove the database name from the NDB list
        databases_in_NDB.erase(ndb_db_name);

      } break;
      case SCHEMA_DDL_ALTER: {
        if (!db_exists_in_DD) {
          // Database doesn't exist. Create it
          if (mysqld.create_database(db_name)) {
            ndb_log_error("Failed to create database '%s'", db_name.c_str());
            return false;
          }
          ndb_log_info("Created database '%s'", db_name.c_str());
        }

        // Compare the versions and run the alter if they differ
        if (ddl_node_id != schema_node_id || ddl_counter != schema_counter) {
          if (mysqld.execute_database_ddl(query)) {
            ndb_log_error("Failed to alter database '%s'.", db_name.c_str());
            return false;
          }
          // Update the schema version
          if (!ndb_dd_update_schema_version(m_thd, db_name.c_str(), ddl_counter,
                                            ddl_node_id)) {
            ndb_log_error("Failed to update version in DD for database : '%s'",
                          db_name.c_str());
            return false;
          }
          ndb_log_info("Successfully altered database '%s'", db_name.c_str());
        }

        // Remove the database name from the NDB list
        databases_in_NDB.erase(ndb_db_name);

      } break;
      case SCHEMA_DDL_DROP: {
        if (db_exists_in_DD) {
          // Database exists in DD
          if (!tables_exist_in_database) {
            // Drop it if doesn't have any table in it
            if (mysqld.drop_database(db_name)) {
              ndb_log_error("Failed to drop database '%s'.", db_name.c_str());
              return false;
            }
            ndb_log_info("Dropped database '%s'", db_name.c_str());
          } else {
            // It has table(s) in it. Skip dropping it
            ndb_log_warning("Database '%s' has tables. Skipped dropping it.",
                            db_name.c_str());

            // Update the schema version to drop database DDL's version
            if (!ndb_dd_update_schema_version(m_thd, db_name.c_str(),
                                              ddl_counter, ddl_node_id)) {
              ndb_log_error(
                  "Failed to update version in DD for database : '%s'",
                  db_name.c_str());
              return false;
            }
          }

          // Remove the database name from the NDB list
          databases_in_NDB.erase(ndb_db_name);
        }
      } break;
      default:
        assert(!"Unknown database DDL type");
    }
  }

  /*
    Check that all the remaining databases in NDB are in DD.
    Create them if they don't exist in the DD.
  */
  for (const std::string &db_name : databases_in_NDB) {
    if (databases_in_DD.find(db_name) == databases_in_DD.end()) {
      // Create the database with default properties
      if (mysqld.create_database(db_name)) {
        ndb_log_error("Failed to create database '%s'.", db_name.c_str());
        return false;
      }
      ndb_log_info("Discovered a database : '%s'", db_name.c_str());
    }
  }
  return true;
}

bool Ndb_dd_sync::migrate_table_with_old_extra_metadata(
    const char *schema_name, const char *table_name, void *unpacked_data,
    Uint32 unpacked_len, bool force_overwrite) const {
  ndb_log_info(
      "Table '%s.%s' has obsolete extra metadata. "
      "The table is installed into the data dictionary "
      "by translating the old metadata",
      schema_name, table_name);

  const uchar *frm_data = static_cast<const uchar *>(unpacked_data);

  // Install table in DD
  Ndb_dd_client dd_client(m_thd);

  // First acquire exclusive MDL lock on schema and table
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
    ndb_log_error("Failed to acquire MDL on table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  if (!dd_client.migrate_table(schema_name, table_name, frm_data, unpacked_len,
                               force_overwrite)) {
    // Failed to create DD entry for table
    ndb_log_error("Failed to create entry in DD for table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  // Check if table needs to be setup for binlogging or schema distribution
  const dd::Table *table_def;
  if (!dd_client.get_table(schema_name, table_name, &table_def)) {
    ndb_log_error("Failed to open table '%s.%s' from DD", schema_name,
                  table_name);
    return false;
  }

  if (ndbcluster_binlog_setup_table(m_thd, m_thd_ndb->ndb, schema_name,
                                    table_name, table_def) != 0) {
    ndb_log_error("Failed to setup binlog for table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  dd_client.commit();
  return true;
}

bool Ndb_dd_sync::install_table(const char *schema_name, const char *table_name,
                                const NdbDictionary::Table *ndbtab,
                                bool force_overwrite) const {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("schema_name: %s, table_name: %s", schema_name, table_name));

  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  const std::string tablespace_name = ndb_table_tablespace_name(dict, ndbtab);
  if (!tablespace_name.empty()) {
    /*
      This is a disk data table. Before the table is installed, we check if the
      tablespace exists in DD since it's possible that the tablespace wasn't
      successfully installed during the tablespace synchronization step.
      We try and deal with such scenarios by attempting to install the missing
      tablespace or erroring out should the installation fail once again
    */
    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_tablespace(tablespace_name.c_str(), true)) {
      ndb_log_error("Failed to acquire MDL on tablespace '%s'",
                    tablespace_name.c_str());
      return false;
    }
    bool exists_in_DD;
    if (!dd_client.tablespace_exists(tablespace_name.c_str(), exists_in_DD)) {
      ndb_log_info("Failed to determine if tablespace '%s' was present in DD",
                   tablespace_name.c_str());
      return false;
    }
    if (!exists_in_DD) {
      ndb_log_info("Tablespace '%s' does not exist in DD, installing..",
                   tablespace_name.c_str());
      NdbDictionary::Tablespace ndb_tablespace =
          dict->getTablespace(tablespace_name.c_str());
      if (ndb_dict_check_NDB_error(dict)) {
        // Failed to open tablespace from NDB
        log_NDB_error(dict->getNdbError());
        ndb_log_error("Failed to get tablespace '%s' from NDB",
                      tablespace_name.c_str());
        return false;
      }
      std::vector<std::string> datafile_names;
      if (!ndb_get_datafile_names(dict, tablespace_name, &datafile_names)) {
        ndb_log_error(
            "Failed to get datafiles assigned to tablespace '%s' from NDB",
            tablespace_name.c_str());
        return false;
      }
      if (!install_tablespace(tablespace_name.c_str(), ndb_tablespace,
                              datafile_names, false)) {
        return false;
      }
      ndb_log_info("Tablespace '%s' installed in DD", tablespace_name.c_str());
    }
  }

  dd::sdi_t sdi;
  {
    Uint32 version;
    void *unpacked_data;
    Uint32 unpacked_len;
    const int get_result =
        ndbtab->getExtraMetadata(version, &unpacked_data, &unpacked_len);
    if (get_result != 0) {
      DBUG_PRINT("error",
                 ("Could not get extra metadata, error: %d", get_result));
      return false;
    }

    if (version != 1 && version != 2) {
      // Skip install of table which has unsupported extra metadata versions
      ndb_log_info(
          "Skipping setup of table '%s.%s', it has "
          "unsupported extra metadata version %d.",
          schema_name, table_name, version);
      free(unpacked_data);
      return true;  // Skipped
    }

    if (version == 1) {
      // Migrate table with version 1 metadata to DD and return
      if (!migrate_table_with_old_extra_metadata(schema_name, table_name,
                                                 unpacked_data, unpacked_len,
                                                 force_overwrite)) {
        free(unpacked_data);
        return false;
      }
      free(unpacked_data);
      return true;
    }

    sdi.assign(static_cast<const char *>(unpacked_data), unpacked_len);

    free(unpacked_data);
  }

  // Found table, now install it in DD
  Ndb_dd_client dd_client(m_thd);

  // First acquire exclusive MDL lock on schema and table
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
    ndb_log_error("Couldn't acquire exclusive metadata locks on '%s.%s'",
                  schema_name, table_name);
    return false;
  }

  if (!tablespace_name.empty()) {
    // Acquire IX MDL on tablespace
    if (!dd_client.mdl_lock_tablespace(tablespace_name.c_str(), true)) {
      ndb_log_error("Couldn't acquire metadata lock on tablespace '%s'",
                    tablespace_name.c_str());
      return false;
    }
  }

  if (!dd_client.install_table(
          schema_name, table_name, sdi, ndbtab->getObjectId(),
          ndbtab->getObjectVersion(), ndbtab->getPartitionCount(),
          tablespace_name, force_overwrite)) {
    // Failed to install table
    ndb_log_warning("Failed to install table '%s.%s'", schema_name, table_name);
    return false;
  }

  const dd::Table *table_def;
  if (!dd_client.get_table(schema_name, table_name, &table_def)) {
    ndb_log_error("Couldn't open table '%s.%s' from DD after install",
                  schema_name, table_name);
    return false;
  }

  // Check if binlogging should be setup for this table
  if (ndbcluster_binlog_setup_table(m_thd, m_thd_ndb->ndb, schema_name,
                                    table_name, table_def)) {
    return false;
  }

  dd_client.commit();
  return true;  // OK
}

bool Ndb_dd_sync::synchronize_table(const char *schema_name,
                                    const char *table_name) const {
  ndb_log_verbose(1, "Synchronizing table '%s.%s'", schema_name, table_name);

  Ndb_table_guard ndbtab_g(m_thd_ndb->ndb, schema_name, table_name);
  const NdbDictionary::Table *ndbtab = ndbtab_g.get_table();
  if (!ndbtab) {
    // Failed to open the table from NDB
    log_NDB_error(ndbtab_g.getNdbError());
    ndb_log_error("Failed to setup table '%s.%s'", schema_name, table_name);

    // Failed, table was listed but could not be opened, retry
    return false;
  }

  if (ndbtab->getFrmLength() == 0) {
    ndb_log_verbose(1,
                    "Skipping setup of table '%s.%s', no extra "
                    "metadata",
                    schema_name, table_name);
    return true;  // Ok, table skipped
  }

  {
    Uint32 version;
    void *unpacked_data;
    Uint32 unpacked_length;
    const int get_result =
        ndbtab->getExtraMetadata(version, &unpacked_data, &unpacked_length);

    if (get_result != 0) {
      // Header corrupt or failed to unpack
      ndb_log_error(
          "Failed to setup table '%s.%s', could not "
          "unpack extra metadata, error: %d",
          schema_name, table_name, get_result);
      return false;
    }

    free(unpacked_data);
  }

  Ndb_dd_client dd_client(m_thd);

  // Acquire MDL lock on table
  if (!dd_client.mdl_lock_table(schema_name, table_name)) {
    ndb_log_error("Failed to acquire MDL lock for table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  const dd::Table *existing;
  if (!dd_client.get_table(schema_name, table_name, &existing)) {
    ndb_log_error("Failed to open table '%s.%s' from DD", schema_name,
                  table_name);
    return false;
  }

  if (existing == nullptr) {
    ndb_log_info("Table '%s.%s' does not exist in DD, installing...",
                 schema_name, table_name);

    if (!install_table(schema_name, table_name, ndbtab,
                       false /* need overwrite */)) {
      // Failed to install into DD or setup binlogging
      ndb_log_error("Failed to install table '%s.%s'", schema_name, table_name);
      return false;
    }
    return true;  // OK
  }

  // Skip if table exists in DD, but is in other engine
  const dd::String_type engine = ndb_dd_table_get_engine(existing);
  if (engine != "ndbcluster") {
    ndb_log_info(
        "Skipping table '%s.%s' with same name which is in "
        "engine '%s'",
        schema_name, table_name, engine.c_str());
    return true;  // Skipped
  }

  Ndb_dd_handle dd_handle = ndb_dd_table_get_spi_and_version(existing);
  if (!dd_handle.valid()) {
    ndb_log_error(
        "Failed to extract id and version from table definition "
        "for table '%s.%s'",
        schema_name, table_name);
    assert(false);
    return false;
  }

  {
    // Check that latest version of table definition for this NDB table
    // is installed in DD
    Ndb_dd_handle curr_handle{ndbtab->getObjectId(),
                              ndbtab->getObjectVersion()};
    if (curr_handle != dd_handle) {
      ndb_log_info(
          "Table '%s.%s' have different version in DD, reinstalling...",
          schema_name, table_name);
      if (!install_table(schema_name, table_name, ndbtab,
                         true /* need overwrite */)) {
        // Failed to create table from NDB
        ndb_log_error("Failed to install table '%s.%s' from NDB", schema_name,
                      table_name);
        return false;
      }
    }
  }

  // Check if table needs to be setup for binlogging or schema distribution
  const dd::Table *table_def;
  if (!dd_client.get_table(schema_name, table_name, &table_def)) {
    ndb_log_error("Failed to open table '%s.%s' from DD", schema_name,
                  table_name);
    return false;
  }

  if (ndbcluster_binlog_setup_table(m_thd, m_thd_ndb->ndb, schema_name,
                                    table_name, table_def) != 0) {
    ndb_log_error("Failed to setup binlog for table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  return true;
}

bool Ndb_dd_sync::synchronize_schema(const char *schema_name) const {
  Ndb_dd_client dd_client(m_thd);

  ndb_log_info("Synchronizing schema '%s'", schema_name);

  // Lock the schema in DD
  if (!dd_client.mdl_lock_schema(schema_name)) {
    ndb_log_error("Failed to acquire MDL lock on schema '%s'", schema_name);
    return false;
  }

  std::unordered_set<std::string> ndb_tables_in_NDB;
  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  /*
    Fetch list of tables in NDB. The util tables are skipped since the
    ndb_schema, ndb_schema_result, and ndb_sql_metadata tables are handled
    separately during binlog setup. The index stat tables are not installed in
    the DD. This is due to an issue after an initial system restart. The binlog
    thread of the first MySQL Server connecting to the Cluster post an initial
    restart pokes the index stat thread to create the index stat tables in NDB.
    This happens only after the synchronization phase during binlog setup which
    means that the tables aren't synced to the DD of the first MySQL Server
    connecting to the Cluster. If there are multiple MySQL Servers connecting to
    the Cluster, there's a race condition where the index stat tables could be
    synchronized during subsequent MySQL Server connections depending on when
    the index stat thread creates them in NDB. If the creation occurs in the
    middle of the sync during binlog setup of a Server, it opens the door to
    errors in the sync. The contents of these tables are incomprehensible
    without some kind of parsing and are thus not exposed to the MySQL Server.
    They remain visible and accessible via the ndb_select_all tool.
  */
  if (!ndb_get_table_names_in_schema(dict, schema_name, &ndb_tables_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    ndb_log_error("Failed to get list of NDB tables in schema '%s' from NDB",
                  schema_name);
    return false;
  }

  // Iterate over each table in NDB and synchronize them to DD
  for (const std::string &ndb_table_name : ndb_tables_in_NDB) {
    if (!synchronize_table(schema_name, ndb_table_name.c_str())) {
      ndb_log_info("Failed to synchronize table '%s.%s'", schema_name,
                   ndb_table_name.c_str());
      continue;
    }
  }

  return true;
}

bool Ndb_dd_sync::synchronize() const {
  ndb_log_info("Starting metadata synchronization...");

  // Synchronize logfile groups and tablespaces
  if (!synchronize_logfile_groups()) {
    ndb_log_warning("Failed to synchronize logfile groups");
    return false;
  }

  if (!synchronize_tablespaces()) {
    ndb_log_warning("Failed to synchronize tablespaces");
    return false;
  }

  // Synchronize databases
  if (!synchronize_databases()) {
    ndb_log_warning("Failed to synchronize databases");
    return false;
  }

  Ndb_dd_client dd_client(m_thd);

  // Fetch list of schemas in DD
  std::vector<std::string> schema_names;
  if (!dd_client.fetch_schema_names(&schema_names)) {
    ndb_log_verbose(19,
                    "Failed to synchronize metadata, could not "
                    "fetch schema names");
    return false;
  }

  /*
    Iterate over each schema and synchronize it one by one, the assumption is
    that even large deployments have a manageable number of tables in each
    schema
  */
  for (const std::string &name : schema_names) {
    if (!synchronize_schema(name.c_str())) {
      ndb_log_info("Failed to synchronize metadata, schema: '%s'",
                   name.c_str());
      return false;
    }
  }

  ndb_log_info("Completed metadata synchronization");
  return true;
}

void Ndb_dd_sync::remove_copying_alter_temp_tables(
    const char *schema_name,
    const std::unordered_set<std::string> &temp_tables_in_ndb) const {
  for (const std::string &ndb_table_name : temp_tables_in_ndb) {
    // if the table starts with #sql2, it's the table left behind after
    // renaming original table to temporary one, cannot be deleted to prevent
    // data loss
    if (is_prefix(ndb_table_name.c_str(), "#sql2")) {
      ndb_log_error(
          "Found temporary table %s.%s, which is most likely left behind"
          " by failed copying alter table",
          schema_name, ndb_table_name.c_str());
      continue;
    }

    // the table is temporary and does not start with prefix #sql2,
    // so it must have been left behind before renaming orignal table,
    // if so, it can be deleted to cleanup unfinished copying alter table
    ndb_log_warning(
        "Found temporary table %s.%s, wich is most likely left behind by failed"
        " copying alter table, this table will be removed, the operation"
        " does not affect original data",
        schema_name, ndb_table_name.c_str());
    Ndb_table_guard ndbtab_g(m_thd_ndb->ndb, schema_name,
                             ndb_table_name.c_str());
    auto ndbtab = *ndbtab_g.get_table();
    constexpr int flag = NdbDictionary::Dictionary::DropTableCascadeConstraints;

    if (m_thd_ndb->ndb->getDictionary()->dropTableGlobal(ndbtab, flag)) {
      log_NDB_error(m_thd_ndb->ndb->getDictionary()->getNdbError());
      ndb_log_error("Cannot drop %s.%s", schema_name, ndb_table_name.c_str());
    }
  }
}
