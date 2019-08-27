/*
   Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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
#include "storage/ndb/plugin/ndb_metadata_sync.h"

#include "sql/sql_class.h"                            // THD
#include "sql/sql_table.h"                            // build_table_filename
#include "storage/ndb/include/ndbapi/Ndb.hpp"         // Ndb
#include "storage/ndb/plugin/ha_ndbcluster_binlog.h"  // ndbcluster_binlog_setup_table
#include "storage/ndb/plugin/ndb_dd_client.h"         // Ndb_dd_client
#include "storage/ndb/plugin/ndb_dd_disk_data.h"  // ndb_dd_disk_data_get_object_id_and_version
#include "storage/ndb/plugin/ndb_dd_table.h"  // ndb_dd_table_get_object_id_and_version
#include "storage/ndb/plugin/ndb_log.h"          // ndb_log_*
#include "storage/ndb/plugin/ndb_metadata.h"     // Ndb_metadata
#include "storage/ndb/plugin/ndb_ndbapi_util.h"  // ndb_logfile_group_exists
#include "storage/ndb/plugin/ndb_schema_dist.h"  // Ndb_schema_dist
#include "storage/ndb/plugin/ndb_table_guard.h"  // Ndb_table_guard
#include "storage/ndb/plugin/ndb_tdc.h"          // ndb_tdc_close_cached_table
#include "storage/ndb/plugin/ndb_thd.h"          // get_thd_ndb
#include "storage/ndb/plugin/ndb_thd_ndb.h"      // Thd_ndb

const char *Ndb_metadata_sync::object_type_str(
    object_detected_type type) const {
  switch (type) {
    case LOGFILE_GROUP_OBJECT:
      return "LOGFILE GROUP";
    case TABLESPACE_OBJECT:
      return "TABLESPACE";
    case TABLE_OBJECT:
      return "TABLE";
    default:
      DBUG_ASSERT(false);
      return "";
  }
}

bool Ndb_metadata_sync::object_sync_pending(
    const Detected_object &object) const {
  for (const auto &detected_object : m_objects) {
    if (detected_object.m_type == object.m_type &&
        detected_object.m_db_name == object.m_db_name &&
        detected_object.m_name == object.m_name) {
      if (object.m_type == object_detected_type::TABLE_OBJECT) {
        ndb_log_info(
            "Object '%s.%s' of type %s is already in the queue of "
            "objects waiting to be synchronized",
            object.m_db_name.c_str(), object.m_name.c_str(),
            object_type_str(object.m_type));
      } else {
        ndb_log_info(
            "Object '%s' of type %s is already in the queue of objects"
            " waiting to be synchronized",
            object.m_name.c_str(), object_type_str(object.m_type));
      }
      return true;
    }
  }
  return false;
}

bool Ndb_metadata_sync::object_blacklisted(
    const Detected_object &object) const {
  std::lock_guard<std::mutex> guard(m_blacklist_mutex);
  for (const auto &blacklisted_object : m_blacklist) {
    if (blacklisted_object.m_type == object.m_type &&
        blacklisted_object.m_db_name == object.m_db_name &&
        blacklisted_object.m_name == object.m_name) {
      if (object.m_type == object_detected_type::TABLE_OBJECT) {
        ndb_log_info(
            "Object '%s.%s' of type %s is currently blacklisted and needs to "
            "be synced manually",
            object.m_db_name.c_str(), object.m_name.c_str(),
            object_type_str(object.m_type));
      } else {
        ndb_log_info(
            "Object '%s' of type %s is currently blacklisted and needs to be "
            "synced manually",
            object.m_name.c_str(), object_type_str(object.m_type));
      }
      return true;
    }
  }
  return false;
}

bool Ndb_metadata_sync::add_logfile_group(const std::string &lfg_name) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj("", lfg_name,
                            object_detected_type::LOGFILE_GROUP_OBJECT);
  if (object_sync_pending(obj) || object_blacklisted(obj)) {
    return false;
  }
  m_objects.emplace_back(obj);
  ndb_log_info(
      "Logfile group '%s' added to queue of objects waiting to be "
      "synchronized",
      lfg_name.c_str());
  return true;
}

bool Ndb_metadata_sync::add_tablespace(const std::string &tablespace_name) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj("", tablespace_name,
                            object_detected_type::TABLESPACE_OBJECT);
  if (object_sync_pending(obj) || object_blacklisted(obj)) {
    return false;
  }
  m_objects.emplace_back(obj);
  ndb_log_info(
      "Tablespace '%s' added to queue of objects waiting to be "
      "synchronized",
      tablespace_name.c_str());
  return true;
}

bool Ndb_metadata_sync::add_table(const std::string &schema_name,
                                  const std::string &table_name) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj(schema_name, table_name,
                            object_detected_type::TABLE_OBJECT);
  if (object_sync_pending(obj) || object_blacklisted(obj)) {
    return false;
  }
  m_objects.emplace_back(obj);
  ndb_log_info(
      "Table '%s.%s' added to queue of objects waiting to be "
      "synchronized",
      schema_name.c_str(), table_name.c_str());
  return true;
}

bool Ndb_metadata_sync::object_queue_empty() const {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  return m_objects.empty();
}

void Ndb_metadata_sync::get_next_object(std::string &db_name, std::string &name,
                                        object_detected_type &type) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj = m_objects.front();
  db_name = obj.m_db_name;
  name = obj.m_name;
  type = obj.m_type;
  m_objects.pop_front();
}

static long long g_blacklist_size =
    0;  // protected implicitly by m_blacklist_mutex
static void increment_blacklist_size() { g_blacklist_size++; }
static void decrement_blacklist_size() { g_blacklist_size--; }
static SHOW_VAR ndb_status_vars_blacklist_size[] = {
    {"metadata_blacklist_size", reinterpret_cast<char *>(&g_blacklist_size),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_metadata_blacklist_size(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = reinterpret_cast<char *>(&ndb_status_vars_blacklist_size);
  return 0;
}

void Ndb_metadata_sync::add_object_to_blacklist(const std::string &db_name,
                                                const std::string &name,
                                                object_detected_type type) {
  std::lock_guard<std::mutex> guard(m_blacklist_mutex);
  const Detected_object obj(db_name, name, type);
  m_blacklist.emplace_back(obj);
  ndb_log_info("Object '%s' of type %s added to blacklist", name.c_str(),
               object_type_str(type));
  increment_blacklist_size();
}

bool Ndb_metadata_sync::get_blacklist_object_for_validation(
    std::string &db_name, std::string &name, object_detected_type &type) {
  std::lock_guard<std::mutex> guard(m_blacklist_mutex);
  for (Detected_object &obj : m_blacklist) {
    switch (obj.m_validation_state) {
      case object_validation_state::PENDING: {
        // Found object pending validation. Retrieve details and mark the object
        // as being validated
        db_name = obj.m_db_name;
        name = obj.m_name;
        type = obj.m_type;
        obj.m_validation_state = object_validation_state::IN_PROGRESS;
        return true;
      } break;
      case object_validation_state::DONE: {
      } break;
      case object_validation_state::IN_PROGRESS: {
        // Not possible since there can't be two objects being validated at once
        DBUG_ASSERT(false);
        return false;
      } break;
      default:
        // Unknown state, not possible
        DBUG_ASSERT(false);
        return false;
    }
  }
  // Reached the end of the blacklist having found no objects pending validation
  return false;
}

bool Ndb_metadata_sync::check_blacklist_object_mismatch(
    THD *thd, const std::string &db_name, const std::string &name,
    object_detected_type type) const {
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb *ndb = thd_ndb->ndb;
  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  Ndb_dd_client dd_client(thd);
  switch (type) {
    case object_detected_type::LOGFILE_GROUP_OBJECT: {
      bool exists_in_NDB;
      if (!ndb_logfile_group_exists(dict, name, exists_in_NDB)) {
        ndb_log_info(
            "Failed to determine if logfile group '%s' exists "
            "in NDB, object remains blacklisted",
            name.c_str());
        return true;
      }

      if (!dd_client.mdl_lock_logfile_group(name.c_str(), true)) {
        ndb_log_info(
            "Failed to acquire MDL on logfile group '%s', object "
            "remains blacklisted",
            name.c_str());
        return true;
      }
      bool exists_in_DD;
      if (!dd_client.logfile_group_exists(name.c_str(), exists_in_DD)) {
        ndb_log_info(
            "Failed to determine if logfile group '%s' exists in DD, "
            "object remains blacklisted",
            name.c_str());
        return true;
      }

      if (exists_in_NDB == exists_in_DD) {
        ndb_log_info(
            "Mismatch doesn't exist any more, logfile group '%s' "
            "will be removed from blacklist",
            name.c_str());
        return false;
      } else {
        ndb_log_info(
            "Mismatch still exists, logfile group '%s' remains "
            "blacklisted",
            name.c_str());
        return true;
      }
    } break;
    case object_detected_type::TABLESPACE_OBJECT: {
      bool exists_in_NDB;
      if (!ndb_tablespace_exists(dict, name, exists_in_NDB)) {
        ndb_log_info(
            "Failed to determine if tablespace '%s' exists in NDB, "
            "object remains blacklisted",
            name.c_str());
        return true;
      }

      if (!dd_client.mdl_lock_tablespace(name.c_str(), true)) {
        ndb_log_info(
            "Failed to acquire MDL on tablespace '%s', object "
            "remains blacklisted",
            name.c_str());
        return true;
      }
      bool exists_in_DD;
      if (!dd_client.tablespace_exists(name.c_str(), exists_in_DD)) {
        ndb_log_info(
            "Failed to determine if tablespace '%s' exists in DD, "
            "object remains blacklisted",
            name.c_str());
        return true;
      }

      if (exists_in_NDB == exists_in_DD) {
        ndb_log_info(
            "Mismatch doesn't exist any more, tablespace '%s' "
            "will be removed from blacklist",
            name.c_str());
        return false;
      } else {
        ndb_log_info(
            "Mismatch still exists, tablespace '%s' remains "
            "blacklisted",
            name.c_str());
        return true;
      }
    } break;
    case object_detected_type::TABLE_OBJECT: {
      bool exists_in_NDB;
      if (!ndb_table_exists(dict, db_name, name, exists_in_NDB)) {
        ndb_log_info(
            "Failed to determine if table '%s.%s' exists in NDB, "
            "object remains blacklisted",
            db_name.c_str(), name.c_str());
        return true;
      }

      if (!dd_client.mdl_lock_table(db_name.c_str(), name.c_str())) {
        ndb_log_info(
            "Failed to acquire MDL on table '%s.%s', object "
            "remains blacklisted",
            db_name.c_str(), name.c_str());
        return true;
      }
      bool exists_in_DD;
      if (!dd_client.table_exists(db_name.c_str(), name.c_str(),
                                  exists_in_DD)) {
        ndb_log_info(
            "Failed to determine if table '%s.%s' exists in DD, "
            "object remains blacklisted",
            db_name.c_str(), name.c_str());
        return true;
      }

      if (exists_in_NDB == exists_in_DD) {
        ndb_log_info(
            "Mismatch doesn't exist any more, table '%s.%s' will be removed "
            "from blacklist",
            db_name.c_str(), name.c_str());
        return false;
      } else {
        ndb_log_info(
            "Mismatch still exists, table '%s.%s' remains "
            "blacklisted",
            db_name.c_str(), name.c_str());
        return true;
      }
    } break;
    default:
      ndb_log_error("Unknown object type found in blacklist");
      DBUG_ASSERT(false);
  }
  return true;
}

void Ndb_metadata_sync::validate_blacklist_object(bool check_mismatch_result) {
  std::lock_guard<std::mutex> guard(m_blacklist_mutex);
  for (auto it = m_blacklist.begin(); it != m_blacklist.end(); it++) {
    Detected_object &obj = *it;
    if (obj.m_validation_state == object_validation_state::IN_PROGRESS) {
      if (!check_mismatch_result) {
        // Mismatch no longer exists, remove object from blacklist
        m_blacklist.erase(it);
        decrement_blacklist_size();
      } else {
        // Mark object as already validated for this cycle
        obj.m_validation_state = object_validation_state::DONE;
      }
      return;
    }
  }
  DBUG_ASSERT(false);
}

void Ndb_metadata_sync::reset_blacklist_state() {
  std::lock_guard<std::mutex> guard(m_blacklist_mutex);
  for (Detected_object &obj : m_blacklist) {
    obj.m_validation_state = object_validation_state::PENDING;
  }
}

void Ndb_metadata_sync::validate_blacklist(THD *thd) {
  while (true) {
    std::string db_name, name;
    object_detected_type type;
    if (!get_blacklist_object_for_validation(db_name, name, type)) {
      // No more objects pending validation
      break;
    }
    const bool check_mismatch_result =
        check_blacklist_object_mismatch(thd, db_name, name, type);
    validate_blacklist_object(check_mismatch_result);
  }
  // Reset the states of all blacklisted objects
  reset_blacklist_state();
}

bool Ndb_metadata_sync::sync_logfile_group(THD *thd,
                                           const std::string &lfg_name,
                                           bool &temp_error) const {
  Ndb_dd_client dd_client(thd);
  if (!dd_client.mdl_lock_logfile_group_exclusive(lfg_name.c_str(), true, 10)) {
    ndb_log_info("Failed to acquire MDL on logfile group '%s'",
                 lfg_name.c_str());
    temp_error = true;
    // Since it's a temporary error, the THD conditions should be cleared but
    // not logged
    clear_thd_conditions(thd);
    return false;
  }

  ndb_log_info("Synchronizing logfile group '%s'", lfg_name.c_str());

  // Errors detected in the remainder of the function are not temporary
  temp_error = false;

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  NdbDictionary::Dictionary *dict = thd_ndb->ndb->getDictionary();
  bool exists_in_NDB;
  if (!ndb_logfile_group_exists(dict, lfg_name, exists_in_NDB)) {
    ndb_log_warning("Failed to determine if logfile group '%s' exists in NDB",
                    lfg_name.c_str());
    return false;
  }

  bool exists_in_DD;
  if (!dd_client.logfile_group_exists(lfg_name.c_str(), exists_in_DD)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
    ndb_log_warning("Failed to determine if logfile group '%s' exists in DD",
                    lfg_name.c_str());
    return false;
  }

  if (exists_in_NDB == exists_in_DD) {
    // Mismatch doesn't exist any more, return success
    return true;
  }

  if (exists_in_DD) {
    // Logfile group exists in DD but not in NDB. Correct this by removing the
    // logfile group from DD
    if (!dd_client.drop_logfile_group(lfg_name.c_str())) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to drop logfile group '%s' in DD",
                    lfg_name.c_str());
      return false;
    }
    dd_client.commit();
    ndb_log_info("Logfile group '%s' dropped from DD", lfg_name.c_str());
    return true;
  }

  // Logfile group exists in NDB but not in DD. Correct this by installing the
  // logfile group in the DD
  std::vector<std::string> undofile_names;
  if (!ndb_get_undofile_names(dict, lfg_name, &undofile_names)) {
    ndb_log_error("Failed to get undofiles assigned to logfile group '%s'",
                  lfg_name.c_str());
    return false;
  }

  int ndb_id, ndb_version;
  if (!ndb_get_logfile_group_id_and_version(dict, lfg_name, ndb_id,
                                            ndb_version)) {
    ndb_log_error("Failed to get id and version of logfile group '%s'",
                  lfg_name.c_str());
    return false;
  }
  if (!dd_client.install_logfile_group(lfg_name.c_str(), undofile_names, ndb_id,
                                       ndb_version,
                                       false /* force_overwrite */)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to install logfile group '%s' in DD",
                  lfg_name.c_str());
    return false;
  }
  dd_client.commit();
  ndb_log_info("Logfile group '%s' installed in DD", lfg_name.c_str());
  return true;
}

bool Ndb_metadata_sync::sync_tablespace(THD *thd, const std::string &ts_name,
                                        bool &temp_error) const {
  Ndb_dd_client dd_client(thd);
  if (!dd_client.mdl_lock_tablespace_exclusive(ts_name.c_str(), true, 10)) {
    ndb_log_info("Failed to acquire MDL on tablespace '%s'", ts_name.c_str());
    temp_error = true;
    // Since it's a temporary error, the THD conditions should be cleared but
    // not logged
    clear_thd_conditions(thd);
    return false;
  }

  ndb_log_info("Synchronizing tablespace '%s'", ts_name.c_str());

  // Errors detected in the remainder of the function are not temporary
  temp_error = false;

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  NdbDictionary::Dictionary *dict = thd_ndb->ndb->getDictionary();
  bool exists_in_NDB;
  if (!ndb_tablespace_exists(dict, ts_name, exists_in_NDB)) {
    ndb_log_warning("Failed to determine if tablespace '%s' exists in NDB",
                    ts_name.c_str());
    return false;
  }

  bool exists_in_DD;
  if (!dd_client.tablespace_exists(ts_name.c_str(), exists_in_DD)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
    ndb_log_warning("Failed to determine if tablespace '%s' exists in DD",
                    ts_name.c_str());
    return false;
  }

  if (exists_in_NDB == exists_in_DD) {
    // Mismatch doesn't exist any more, return success
    return true;
  }

  if (exists_in_DD) {
    // Tablespace exists in DD but not in NDB. Correct this by removing the
    // tablespace from DD
    if (!dd_client.drop_tablespace(ts_name.c_str())) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to drop tablespace '%s' in DD", ts_name.c_str());
      return false;
    }
    dd_client.commit();
    ndb_log_info("Tablespace '%s' dropped from DD", ts_name.c_str());
    return true;
  }

  // Tablespace exists in NDB but not in DD. Correct this by installing the
  // tablespace in the DD
  std::vector<std::string> datafile_names;
  if (!ndb_get_datafile_names(dict, ts_name, &datafile_names)) {
    ndb_log_error("Failed to get datafiles assigned to tablespace '%s'",
                  ts_name.c_str());
    return false;
  }

  int ndb_id, ndb_version;
  if (!ndb_get_tablespace_id_and_version(dict, ts_name, ndb_id, ndb_version)) {
    ndb_log_error("Failed to get id and version of tablespace '%s'",
                  ts_name.c_str());
    return false;
  }
  if (!dd_client.install_tablespace(ts_name.c_str(), datafile_names, ndb_id,
                                    ndb_version, false /* force_overwrite */)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to install tablespace '%s' in DD", ts_name.c_str());
    return false;
  }
  dd_client.commit();
  ndb_log_info("Tablespace '%s' installed in DD", ts_name.c_str());
  return true;
}

class Mutex_guard {
 public:
  Mutex_guard(mysql_mutex_t &mutex) : m_mutex(mutex) {
    mysql_mutex_lock(&m_mutex);
  }
  Mutex_guard(const Mutex_guard &) = delete;
  ~Mutex_guard() { mysql_mutex_unlock(&m_mutex); }

 private:
  mysql_mutex_t &m_mutex;
};

extern mysql_mutex_t ndbcluster_mutex;
void Ndb_metadata_sync::drop_ndb_share(const char *db_name,
                                       const char *table_name) const {
  char key[FN_REFLEN + 1];
  build_table_filename(key, sizeof(key) - 1, db_name, table_name, "", 0);
  NDB_SHARE *share =
      NDB_SHARE::acquire_reference_by_key(key,
                                          "table_sync");  // temporary ref
  if (share) {
    Mutex_guard ndbcluster_mutex_guard(ndbcluster_mutex);
    NDB_SHARE::mark_share_dropped(&share);
    DBUG_ASSERT(share);
    NDB_SHARE::release_reference_have_lock(share,
                                           "table_sync");  // temporary ref
  }
}

bool Ndb_metadata_sync::sync_table(THD *thd, const std::string &db_name,
                                   const std::string &table_name,
                                   bool &temp_error) {
  if (DBUG_EVALUATE_IF("ndb_metadata_sync_fail", true, false)) {
    temp_error = false;
    return false;
  }
  Ndb_dd_client dd_client(thd);
  if (!dd_client.mdl_locks_acquire_exclusive(db_name.c_str(),
                                             table_name.c_str(), true, 10)) {
    ndb_log_info("Failed to acquire MDL on table '%s.%s'", db_name.c_str(),
                 table_name.c_str());
    temp_error = true;
    // Since it's a temporary error, the THD conditions should be cleared but
    // not logged
    clear_thd_conditions(thd);
    return false;
  }

  ndb_log_info("Synchronizing table '%s.%s'", db_name.c_str(),
               table_name.c_str());

  // Most of the errors detected after this are not temporary
  temp_error = false;

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb *ndb = thd_ndb->ndb;
  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  bool exists_in_NDB;
  if (!ndb_table_exists(dict, db_name, table_name, exists_in_NDB)) {
    ndb_log_warning("Failed to determine if table '%s.%s' exists in NDB",
                    db_name.c_str(), table_name.c_str());
    return false;
  }

  bool exists_in_DD;
  if (!dd_client.table_exists(db_name.c_str(), table_name.c_str(),
                              exists_in_DD)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
    ndb_log_warning("Failed to determine if table '%s.%s' exists in DD",
                    db_name.c_str(), table_name.c_str());
    return false;
  }

  if (exists_in_NDB == exists_in_DD) {
    // Mismatch doesn't exist any more, return success
    return true;
  }

  if (exists_in_DD) {
    // Table exists in DD but not in NDB
    // Check if it's a local table
    bool local_table;
    if (!dd_client.is_local_table(db_name.c_str(), table_name.c_str(),
                                  local_table)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to determine if table '%s.%s' was a local table",
                    db_name.c_str(), table_name.c_str());
      return false;
    }
    if (local_table) {
      // Local table, the mismatch is expected
      return true;
    }

    // Remove the table from DD
    Ndb_referenced_tables_invalidator invalidator(thd, dd_client);
    if (!dd_client.remove_table(db_name.c_str(), table_name.c_str(),
                                &invalidator)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to drop table '%s.%s' in DD", db_name.c_str(),
                    table_name.c_str());
      return false;
    }

    if (!invalidator.invalidate()) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error(
          "Failed to invalidate tables referencing table '%s.%s' in "
          "DD",
          db_name.c_str(), table_name.c_str());
      return false;
    }

    // Drop share if it exists
    drop_ndb_share(db_name.c_str(), table_name.c_str());
    ndb_tdc_close_cached_table(thd, db_name.c_str(), table_name.c_str());

    // Invalidate the table in NdbApi
    if (ndb->setDatabaseName(db_name.c_str())) {
      ndb_log_error("Failed to set database name of NDB object");
      return false;
    }
    dd_client.commit();
    ndb_log_info("Table '%s.%s' dropped from DD", db_name.c_str(),
                 table_name.c_str());
    Ndb_table_guard ndbtab_guard(dict, table_name.c_str());
    ndbtab_guard.invalidate();
    return true;
  }

  // Table exists in NDB but not in DD. Correct this by installing the table in
  // the DD
  if (ndb->setDatabaseName(db_name.c_str())) {
    ndb_log_error("Failed to set database name of NDB object");
    return false;
  }

  Ndb_table_guard ndbtab_guard(dict, table_name.c_str());
  const NdbDictionary::Table *ndbtab = ndbtab_guard.get_table();
  if (ndbtab == nullptr) {
    // Mismatch doesn't exist any more, return success
    return true;
  }
  Uint32 extra_metadata_version, unpacked_len;
  void *unpacked_data;
  const int get_result = ndbtab->getExtraMetadata(
      extra_metadata_version, &unpacked_data, &unpacked_len);
  if (get_result != 0) {
    ndb_log_info("Failed to get extra metadata of table '%s.%s'",
                 db_name.c_str(), table_name.c_str());
    return false;
  }

  if (extra_metadata_version == 1) {
    // Table with "old" metadata found
    if (!dd_client.migrate_table(
            db_name.c_str(), table_name.c_str(),
            static_cast<const unsigned char *>(unpacked_data), unpacked_len,
            false, true)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error(
          "Failed to migrate table '%s.%s' with extra metadata "
          "version 1",
          db_name.c_str(), table_name.c_str());
      free(unpacked_data);
      return false;
    }
    free(unpacked_data);
    const dd::Table *dd_table;
    if (!dd_client.get_table(db_name.c_str(), table_name.c_str(), &dd_table)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error(
          "Failed to get table '%s.%s' from DD after it was installed",
          db_name.c_str(), table_name.c_str());
      return false;
    }
    if (ndbcluster_binlog_setup_table(thd, ndb, db_name.c_str(),
                                      table_name.c_str(), dd_table) != 0) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to setup binlogging for table '%s.%s'",
                    db_name.c_str(), table_name.c_str());
      return false;
    }
    ndb_log_info("Table '%s.%s' installed in DD", db_name.c_str(),
                 table_name.c_str());
    return true;
  }
  dd::sdi_t sdi;
  sdi.assign(static_cast<const char *>(unpacked_data), unpacked_len);
  free(unpacked_data);

  const std::string tablespace_name = ndb_table_tablespace_name(dict, ndbtab);
  if (!tablespace_name.empty()) {
    // Acquire IX MDL on tablespace
    if (!dd_client.mdl_lock_tablespace(tablespace_name.c_str(), true)) {
      ndb_log_info("Failed to acquire MDL on tablespace '%s'",
                   tablespace_name.c_str());
      temp_error = true;
      // Since it's a temporary error, the THD conditions should be cleared but
      // not logged
      clear_thd_conditions(thd);
      return false;
    }

    bool tablespace_exists;
    if (!dd_client.tablespace_exists(tablespace_name.c_str(),
                                     tablespace_exists)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
      ndb_log_warning("Failed to determine if tablespace '%s' exists in DD",
                      tablespace_name.c_str());
      return false;
    }
    if (!tablespace_exists) {
      const Detected_object obj("", tablespace_name, TABLESPACE_OBJECT);
      if (object_blacklisted(obj)) {
        // The tablespace was detected but its sync failed. Such errors
        // shouldn't be treated as temporary errors and the table is added to
        // the blacklist
        ndb_log_error("Tablespace '%s' is currently blacklisted",
                      tablespace_name.c_str());
        ndb_log_error("Failed to install disk data table '%s.%s'",
                      db_name.c_str(), table_name.c_str());
        return false;
      } else {
        // There's a possibility (especially when ndb_restore is used) that a
        // disk data table is being synchronized before the tablespace has been
        // synchronized which is a temporary error since the next detection
        // cycle will detect and attempt to sync the tablespace before the table
        ndb_log_info(
            "Disk data table '%s.%s' not synced since tablespace '%s' "
            "hasn't been synced yet",
            db_name.c_str(), table_name.c_str(), tablespace_name.c_str());
        temp_error = true;
        // Since it's a temporary error, the THD conditions should be cleared
        // but not logged
        clear_thd_conditions(thd);
        return false;
      }
    }
  }
  Ndb_referenced_tables_invalidator invalidator(thd, dd_client);
  if (!dd_client.install_table(
          db_name.c_str(), table_name.c_str(), sdi, ndbtab->getObjectId(),
          ndbtab->getObjectVersion(), ndbtab->getPartitionCount(),
          tablespace_name, false, &invalidator)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to install table '%s.%s' in DD", db_name.c_str(),
                  table_name.c_str());
    return false;
  }

  if (!invalidator.invalidate()) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to invalidate tables referencing table '%s.%s' in DD",
                  db_name.c_str(), table_name.c_str());
    return false;
  }
  const dd::Table *dd_table;
  if (!dd_client.get_table(db_name.c_str(), table_name.c_str(), &dd_table)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to get table '%s.%s' from DD after it was installed",
                  db_name.c_str(), table_name.c_str());
    return false;
  }
  if (!Ndb_metadata::compare(thd, ndbtab, dd_table, true, dict)) {
    ndb_log_error("Definition of table '%s.%s' in NDB Dictionary has changed",
                  db_name.c_str(), table_name.c_str());
    return false;
  }
  if (ndbcluster_binlog_setup_table(thd, ndb, db_name.c_str(),
                                    table_name.c_str(), dd_table) != 0) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to setup binlogging for table '%s.%s'",
                  db_name.c_str(), table_name.c_str());
    return false;
  }
  dd_client.commit();
  ndb_log_info("Table '%s.%s' installed in DD", db_name.c_str(),
               table_name.c_str());
  return true;
}
