/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include <sstream>

#include "sql/sql_class.h"                            // THD
#include "storage/ndb/include/ndbapi/Ndb.hpp"         // Ndb
#include "storage/ndb/plugin/ha_ndbcluster_binlog.h"  // ndbcluster_binlog_setup_table
#include "storage/ndb/plugin/ndb_dd.h"                // ndb_dd_fs_name_case
#include "storage/ndb/plugin/ndb_dd_client.h"         // Ndb_dd_client
#include "storage/ndb/plugin/ndb_dd_disk_data.h"  // ndb_dd_disk_data_get_object_id_and_version
#include "storage/ndb/plugin/ndb_dd_table.h"  // ndb_dd_table_get_spi_and_version
#include "storage/ndb/plugin/ndb_local_connection.h"  // Ndb_local_connection
#include "storage/ndb/plugin/ndb_log.h"               // ndb_log_*
#include "storage/ndb/plugin/ndb_metadata.h"          // Ndb_metadata
#include "storage/ndb/plugin/ndb_ndbapi_util.h"  // ndb_logfile_group_exists
#include "storage/ndb/plugin/ndb_schema_dist.h"  // Ndb_schema_dist
#include "storage/ndb/plugin/ndb_sync_excluded_objects_table.h"  // Ndb_sync_excluded_objects_table
#include "storage/ndb/plugin/ndb_sync_pending_objects_table.h"  // Ndb_sync_pending_objects_table
#include "storage/ndb/plugin/ndb_table_guard.h"  // Ndb_table_guard
#include "storage/ndb/plugin/ndb_tdc.h"          // ndb_tdc_close_cached_table
#include "storage/ndb/plugin/ndb_thd.h"          // get_thd_ndb
#include "storage/ndb/plugin/ndb_thd_ndb.h"      // Thd_ndb

std::string Ndb_metadata_sync::object_type_and_name_str(
    const Detected_object &object) const {
  std::stringstream type_name_str;
  switch (object.m_type) {
    case LOGFILE_GROUP_OBJECT: {
      type_name_str << "Logfile group "
                    << "'" << object.m_name << "'";
      break;
    }
    case TABLESPACE_OBJECT: {
      type_name_str << "Tablespace "
                    << "'" << object.m_name << "'";
      break;
    }
    case SCHEMA_OBJECT: {
      type_name_str << "Schema "
                    << "'" << object.m_schema_name << "'";
      break;
    }
    case TABLE_OBJECT: {
      type_name_str << "Table "
                    << "'" << object.m_schema_name << "." << object.m_name
                    << "'";
      break;
    }
    default: {
      assert(false);
      type_name_str << "";
    }
  }
  return type_name_str.str();
}

bool Ndb_metadata_sync::object_sync_pending(
    const Detected_object &object) const {
  for (const auto &detected_object : m_objects) {
    if (detected_object.m_type == object.m_type &&
        detected_object.m_schema_name == object.m_schema_name &&
        detected_object.m_name == object.m_name) {
      ndb_log_verbose(
          10,
          "%s is already in the queue of objects waiting to be synchronized",
          object_type_and_name_str(detected_object).c_str());
      return true;
    }
  }
  return false;
}

bool Ndb_metadata_sync::object_excluded(const Detected_object &object) const {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  for (const auto &excluded_object : m_excluded_objects) {
    if (excluded_object.m_type == object.m_type &&
        excluded_object.m_schema_name == object.m_schema_name &&
        excluded_object.m_name == object.m_name) {
      ndb_log_info("%s is currently excluded and needs to be synced manually",
                   object_type_and_name_str(excluded_object).c_str());
      return true;
    }
  }
  return false;
}

bool Ndb_metadata_sync::add_logfile_group(const std::string &lfg_name) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj("", lfg_name,
                            object_detected_type::LOGFILE_GROUP_OBJECT);
  if (object_sync_pending(obj) || object_excluded(obj)) {
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
  if (object_sync_pending(obj) || object_excluded(obj)) {
    return false;
  }
  m_objects.emplace_back(obj);
  ndb_log_info(
      "Tablespace '%s' added to queue of objects waiting to be "
      "synchronized",
      tablespace_name.c_str());
  return true;
}

bool Ndb_metadata_sync::add_schema(const std::string &schema_name) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj(schema_name, "",
                            object_detected_type::SCHEMA_OBJECT);
  if (object_sync_pending(obj) || object_excluded(obj)) {
    return false;
  }
  m_objects.emplace_back(obj);
  ndb_log_info(
      "Schema '%s' added to queue of objects waiting to be synchronized",
      schema_name.c_str());
  return true;
}

bool Ndb_metadata_sync::add_table(const std::string &schema_name,
                                  const std::string &table_name) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj(schema_name, table_name,
                            object_detected_type::TABLE_OBJECT);
  if (object_sync_pending(obj) || object_excluded(obj)) {
    return false;
  }
  m_objects.emplace_back(obj);
  ndb_log_info(
      "Table '%s.%s' added to queue of objects waiting to be "
      "synchronized",
      schema_name.c_str(), table_name.c_str());
  return true;
}

void Ndb_metadata_sync::retrieve_pending_objects(
    Ndb_sync_pending_objects_table *pending_table) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  for (const Detected_object &obj : m_objects) {
    pending_table->add_pending_object(obj.m_schema_name, obj.m_name,
                                      static_cast<int>(obj.m_type));
  }
}

unsigned int Ndb_metadata_sync::get_pending_objects_count() {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  return m_objects.size();
}

bool Ndb_metadata_sync::object_queue_empty() const {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  return m_objects.empty();
}

void Ndb_metadata_sync::get_next_object(std::string &schema_name,
                                        std::string &name,
                                        object_detected_type &type) {
  std::lock_guard<std::mutex> guard(m_objects_mutex);
  const Detected_object obj = m_objects.front();
  schema_name = obj.m_schema_name;
  name = obj.m_name;
  type = obj.m_type;
  m_objects.pop_front();
}

static long long g_excluded_count =
    0;  // protected implicitly by m_excluded_objects_mutex
static void increment_excluded_count() { g_excluded_count++; }
static void decrement_excluded_count() { g_excluded_count--; }
static void reset_excluded_count() { g_excluded_count = 0; }
static SHOW_VAR ndb_status_vars_excluded_count[] = {
    {"metadata_excluded_count", reinterpret_cast<char *>(&g_excluded_count),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_metadata_excluded_count(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = reinterpret_cast<char *>(&ndb_status_vars_excluded_count);
  return 0;
}

void Ndb_metadata_sync::exclude_object_from_sync(const std::string &schema_name,
                                                 const std::string &name,
                                                 object_detected_type type,
                                                 const std::string &reason) {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  const Detected_object obj(schema_name, name, type, reason);
  m_excluded_objects.emplace_back(obj);
  ndb_log_info("%s is excluded from detection",
               object_type_and_name_str(obj).c_str());
  increment_excluded_count();
}

bool Ndb_metadata_sync::get_excluded_object_for_validation(
    std::string &schema_name, std::string &name, object_detected_type &type) {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  for (Detected_object &obj : m_excluded_objects) {
    switch (obj.m_validation_state) {
      case object_validation_state::PENDING: {
        // Found object pending validation. Retrieve details and mark the object
        // as being validated
        schema_name = obj.m_schema_name;
        name = obj.m_name;
        type = obj.m_type;
        obj.m_validation_state = object_validation_state::IN_PROGRESS;
        return true;
      } break;
      case object_validation_state::DONE: {
      } break;
      case object_validation_state::IN_PROGRESS: {
        // Not possible since there can't be two objects being validated at once
        assert(false);
        return false;
      } break;
      default:
        // Unknown state, not possible
        assert(false);
        return false;
    }
  }
  // No objects pending validation
  return false;
}

bool Ndb_metadata_sync::check_object_mismatch(THD *thd,
                                              const std::string &schema_name,
                                              const std::string &name,
                                              object_detected_type type) const {
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  NdbDictionary::Dictionary *dict = thd_ndb->ndb->getDictionary();
  Ndb_dd_client dd_client(thd);
  switch (type) {
    case object_detected_type::LOGFILE_GROUP_OBJECT: {
      bool exists_in_NDB;
      if (!ndb_logfile_group_exists(dict, name, exists_in_NDB)) {
        ndb_log_info(
            "Failed to determine if logfile group '%s' exists in NDB, "
            "it is assumed that the mismatch still exists",
            name.c_str());
        return true;
      }

      if (!dd_client.mdl_lock_logfile_group(name.c_str(), true)) {
        ndb_log_info(
            "Failed to acquire MDL on logfile group '%s', it is assumed that "
            "the mismatch still exists",
            name.c_str());
        return true;
      }
      bool exists_in_DD;
      if (!dd_client.logfile_group_exists(name.c_str(), exists_in_DD)) {
        ndb_log_info(
            "Failed to determine if logfile group '%s' exists in DD, "
            "it is assumed that the mismatch still exists",
            name.c_str());
        return true;
      }

      if (exists_in_NDB == exists_in_DD) {
        ndb_log_info("Mismatch in logfile group '%s' doesn't exist anymore",
                     name.c_str());
        return false;
      }
      ndb_log_info("Mismatch in logfile group '%s' still exists", name.c_str());
      return true;
    } break;
    case object_detected_type::TABLESPACE_OBJECT: {
      bool exists_in_NDB;
      if (!ndb_tablespace_exists(dict, name, exists_in_NDB)) {
        ndb_log_info(
            "Failed to determine if tablespace '%s' exists in NDB, "
            "it is assumed that the mismatch still exists",
            name.c_str());
        return true;
      }

      if (!dd_client.mdl_lock_tablespace(name.c_str(), true)) {
        ndb_log_info(
            "Failed to acquire MDL on tablespace '%s', it is assumed that the "
            "mismatch still exists",
            name.c_str());
        return true;
      }
      bool exists_in_DD;
      if (!dd_client.tablespace_exists(name.c_str(), exists_in_DD)) {
        ndb_log_info(
            "Failed to determine if tablespace '%s' exists in DD, "
            "it is assumed that the mismatch still exists",
            name.c_str());
        return true;
      }

      if (exists_in_NDB == exists_in_DD) {
        ndb_log_info("Mismatch in tablespace '%s' doesn't exist anymore",
                     name.c_str());
        return false;
      }
      ndb_log_info("Mismatch in tablespace '%s' still exists", name.c_str());
      return true;
    } break;
    case object_detected_type::SCHEMA_OBJECT: {
      if (!dd_client.mdl_lock_schema(schema_name.c_str())) {
        ndb_log_info(
            "Failed to acquire MDL on schema '%s', it is assumed that the "
            "mismatch still exists",
            schema_name.c_str());
        return true;
      }
      bool exists_in_DD;
      if (!dd_client.schema_exists(schema_name.c_str(), &exists_in_DD)) {
        ndb_log_info(
            "Failed to determine if schema '%s' exists in DD, it is assumed "
            "that the mismatch still exists",
            schema_name.c_str());
        return true;
      }

      bool exists_in_NDB;
      if (!ndb_database_exists(dict, schema_name, exists_in_NDB)) {
        ndb_log_info(
            "Failed to determine if schema '%s' exists in NDB, it is assumed "
            "that the mismatch still exists",
            schema_name.c_str());
        return true;
      }

      if (exists_in_NDB && !exists_in_DD) {
        ndb_log_info("Mismatch in schema '%s' still exists",
                     schema_name.c_str());
        return true;
      }
      ndb_log_info("Mismatch in schema '%s' doesn't exist anymore",
                   schema_name.c_str());
      return false;
    } break;
    case object_detected_type::TABLE_OBJECT: {
      bool exists_in_NDB;
      if (!ndb_table_exists(dict, schema_name, name, exists_in_NDB)) {
        ndb_log_info(
            "Failed to determine if table '%s.%s' exists in NDB, "
            "it is assumed that the mismatch still exists",
            schema_name.c_str(), name.c_str());
        return true;
      }

      if (!dd_client.mdl_lock_table(schema_name.c_str(), name.c_str())) {
        ndb_log_info(
            "Failed to acquire MDL on table '%s.%s', it is assumed that the "
            "mismatch still exists",
            schema_name.c_str(), name.c_str());
        return true;
      }
      bool exists_in_DD;
      if (!dd_client.table_exists(schema_name.c_str(), name.c_str(),
                                  exists_in_DD)) {
        ndb_log_info(
            "Failed to determine if table '%s.%s' exists in DD, "
            "it is assumed that the mismatch still exists",
            schema_name.c_str(), name.c_str());
        return true;
      }

      if (exists_in_NDB == exists_in_DD) {
        ndb_log_info("Mismatch in table '%s.%s' doesn't exist anymore",
                     schema_name.c_str(), name.c_str());
        return false;
      }
      ndb_log_info("Mismatch in table '%s.%s' still exists",
                   schema_name.c_str(), name.c_str());
      return true;
    } break;
    default:
      ndb_log_error("Unknown object type found");
      assert(false);
  }
  return true;
}

void Ndb_metadata_sync::validate_excluded_object(bool check_mismatch_result) {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  for (auto it = m_excluded_objects.begin(); it != m_excluded_objects.end();
       it++) {
    Detected_object &obj = *it;
    if (obj.m_validation_state == object_validation_state::IN_PROGRESS) {
      if (!check_mismatch_result) {
        // Mismatch no longer exists, remove excluded object
        ndb_log_info("%s is no longer excluded from detection",
                     object_type_and_name_str(obj).c_str());
        m_excluded_objects.erase(it);
        decrement_excluded_count();
      } else {
        // Mark object as already validated for this cycle
        obj.m_validation_state = object_validation_state::DONE;
      }
      return;
    }
  }
  assert(false);
}

void Ndb_metadata_sync::reset_excluded_objects_state() {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  for (Detected_object &obj : m_excluded_objects) {
    obj.m_validation_state = object_validation_state::PENDING;
  }
}

void Ndb_metadata_sync::validate_excluded_objects(THD *thd) {
  ndb_log_info("Validating excluded objects");
  /*
    The validation is done by the change monitor thread at the beginning of
    each detection cycle. There's a possibility that the binlog thread is
    attempting to synchronize an object at the same time. Should the sync
    fail, the object has to be added to the back of the excluded objects list
    which could result in the binlog thread waiting to acquire
    m_excluded_objects_mutex. This is avoided by ensuring that the mutex is held
    by the validation code for short intervals of time per object. The mutex is
    acquired as the details of the object are retrieved and once again when it
    has been decided if the object should continue to remain remain excluded or
    not. This avoids holding the mutex during the object mismatch check which
    involves calls to DD and NDB Dictionary.
  */
  while (true) {
    std::string schema_name, name;
    object_detected_type type;
    if (!get_excluded_object_for_validation(schema_name, name, type)) {
      // No more objects pending validation
      break;
    }
    const bool check_mismatch_result =
        check_object_mismatch(thd, schema_name, name, type);
    validate_excluded_object(check_mismatch_result);
  }
  // Reset the states of all excluded objects
  reset_excluded_objects_state();
}

void Ndb_metadata_sync::clear_excluded_objects() {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  m_excluded_objects.clear();
  reset_excluded_count();
  ndb_log_info("Excluded objects cleared");
}

void Ndb_metadata_sync::retrieve_excluded_objects(
    Ndb_sync_excluded_objects_table *excluded_table) {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  for (const Detected_object &obj : m_excluded_objects) {
    excluded_table->add_excluded_object(obj.m_schema_name, obj.m_name,
                                        static_cast<int>(obj.m_type),
                                        obj.m_reason);
  }
}

unsigned int Ndb_metadata_sync::get_excluded_objects_count() {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  return m_excluded_objects.size();
}

extern bool opt_ndb_metadata_sync;

bool Ndb_metadata_sync::retry_limit_exceeded(const std::string &schema_name,
                                             const std::string &name,
                                             object_detected_type type) {
  if (!opt_ndb_metadata_sync) {
    /*
      The ndb_metadata_sync variable hasn't been set. This is then the default
      automatic sync mechanism where it's better to retry indefinitely under the
      assumption that the temporary error will have disappeared by the time the
      next discovery + sync attempt occurs.
    */
    return false;
  }
  /*
     The ndb_metadata_sync variable has been set. Check if the retry limit (10)
     has been hit in which case the object is excluded by the caller.
  */
  for (Detected_object &object : m_retry_objects) {
    if (object.m_type == type && object.m_schema_name == schema_name &&
        object.m_name == name) {
      object.m_retries++;
      ndb_log_info("%s retry count = %d",
                   object_type_and_name_str(object).c_str(), object.m_retries);
      return object.m_retries == 10;
    }
  }
  const Detected_object object(schema_name, name, type);
  m_retry_objects.emplace_back(object);
  ndb_log_info("%s retry count = 1", object_type_and_name_str(object).c_str());
  return false;
}

bool Ndb_metadata_sync::object_excluded(const std::string &schema_name,
                                        const std::string &name,
                                        object_detected_type type) const {
  std::lock_guard<std::mutex> guard(m_excluded_objects_mutex);
  for (const auto &excluded_object : m_excluded_objects) {
    if (excluded_object.m_type == type &&
        excluded_object.m_schema_name == schema_name &&
        excluded_object.m_name == name) {
      ndb_log_info("%s is currently excluded from detection",
                   object_type_and_name_str(excluded_object).c_str());
      return true;
    }
  }
  return false;
}

void Ndb_metadata_sync::clear_retry_objects() {
  m_retry_objects.clear();
  ndb_log_info("Retry objects cleared");
}

bool Ndb_metadata_sync::sync_logfile_group(THD *thd,
                                           const std::string &lfg_name,
                                           bool &temp_error,
                                           std::string &error_msg) const {
  if (DBUG_EVALUATE_IF("ndb_metadata_sync_fail", true, false)) {
    temp_error = false;
    error_msg = "Injected failure";
    return false;
  }
  Ndb_dd_client dd_client(thd);
  if (!dd_client.mdl_lock_logfile_group_exclusive(lfg_name.c_str(), true)) {
    ndb_log_info("Failed to acquire MDL on logfile group '%s'",
                 lfg_name.c_str());
    error_msg = "Failed to acquire MDL on logfile group";
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
    error_msg = "Failed to determine if object existed in NDB";
    return false;
  }

  bool exists_in_DD;
  if (!dd_client.logfile_group_exists(lfg_name.c_str(), exists_in_DD)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
    ndb_log_warning("Failed to determine if logfile group '%s' exists in DD",
                    lfg_name.c_str());
    error_msg = "Failed to determine if object existed in DD";
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
      error_msg = "Failed to drop object in DD";
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
    error_msg = "Failed to get undofiles assigned to logfile group";
    return false;
  }

  int ndb_id, ndb_version;
  if (!ndb_get_logfile_group_id_and_version(dict, lfg_name, ndb_id,
                                            ndb_version)) {
    ndb_log_error("Failed to get id and version of logfile group '%s'",
                  lfg_name.c_str());
    error_msg = "Failed to get object id and version";
    return false;
  }
  if (!dd_client.install_logfile_group(lfg_name.c_str(), undofile_names, ndb_id,
                                       ndb_version,
                                       false /* force_overwrite */)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to install logfile group '%s' in DD",
                  lfg_name.c_str());
    error_msg = "Failed to install object in DD";
    return false;
  }
  dd_client.commit();
  ndb_log_info("Logfile group '%s' installed in DD", lfg_name.c_str());
  return true;
}

bool Ndb_metadata_sync::sync_tablespace(THD *thd, const std::string &ts_name,
                                        bool &temp_error,
                                        std::string &error_msg) const {
  if (DBUG_EVALUATE_IF("ndb_metadata_sync_fail", true, false)) {
    temp_error = false;
    error_msg = "Injected failure";
    return false;
  }
  Ndb_dd_client dd_client(thd);
  if (!dd_client.mdl_lock_tablespace_exclusive(ts_name.c_str(), true)) {
    ndb_log_info("Failed to acquire MDL on tablespace '%s'", ts_name.c_str());
    error_msg = "Failed to acquire MDL on tablespace";
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
    error_msg = "Failed to determine if object existed in NDB";
    return false;
  }

  bool exists_in_DD;
  if (!dd_client.tablespace_exists(ts_name.c_str(), exists_in_DD)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
    ndb_log_warning("Failed to determine if tablespace '%s' exists in DD",
                    ts_name.c_str());
    error_msg = "Failed to determine if object existed in DD";
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
      error_msg = "Failed to drop object in DD";
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
    error_msg = "Failed to get datafiles assigned to tablespace";
    return false;
  }

  int ndb_id, ndb_version;
  if (!ndb_get_tablespace_id_and_version(dict, ts_name, ndb_id, ndb_version)) {
    ndb_log_error("Failed to get id and version of tablespace '%s'",
                  ts_name.c_str());
    error_msg = "Failed to get object id and version";
    return false;
  }
  if (!dd_client.install_tablespace(ts_name.c_str(), datafile_names, ndb_id,
                                    ndb_version, false /* force_overwrite */)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to install tablespace '%s' in DD", ts_name.c_str());
    error_msg = "Failed to install object in DD";
    return false;
  }
  dd_client.commit();
  ndb_log_info("Tablespace '%s' installed in DD", ts_name.c_str());
  return true;
}

bool Ndb_metadata_sync::sync_schema(THD *thd, const std::string &schema_name,
                                    bool &temp_error,
                                    std::string &error_msg) const {
  if (DBUG_EVALUATE_IF("ndb_metadata_sync_fail", true, false)) {
    temp_error = false;
    error_msg = "Injected failure";
    return false;
  }
  const std::string dd_schema_name = ndb_dd_fs_name_case(schema_name.c_str());
  Ndb_dd_client dd_client(thd);
  // Acquire exclusive MDL on the schema upfront. Note that this isn't strictly
  // necessary since the Ndb_local_connection is used further down the function.
  // But the binlog thread shouldn't stall while waiting for the MDL to be
  // acquired. Thus, there's an attempt to lock the schema with
  // lock_wait_timeout = 0 to ensure that the binlog thread can bail out early
  // should there be any conflicting locks
  if (!dd_client.mdl_lock_schema_exclusive(dd_schema_name.c_str(), true)) {
    ndb_log_info("Failed to acquire MDL on schema '%s'", schema_name.c_str());
    error_msg = "Failed to acquire MDL on schema";
    temp_error = true;
    // Since it's a temporary error, the THD conditions should be cleared but
    // not logged
    clear_thd_conditions(thd);
    return false;
  }

  ndb_log_info("Synchronizing schema '%s'", schema_name.c_str());
  // All errors beyond this point are not temporary errors
  temp_error = false;
  // Check if mismatch still exists
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  NdbDictionary::Dictionary *dict = thd_ndb->ndb->getDictionary();
  bool exists_in_NDB;
  if (!ndb_database_exists(dict, schema_name, exists_in_NDB)) {
    ndb_log_warning("Failed to determine if schema '%s' exists in NDB",
                    schema_name.c_str());
    error_msg = "Failed to determine if object existed in NDB";
    return false;
  }

  bool exists_in_DD;
  if (!dd_client.schema_exists(dd_schema_name.c_str(), &exists_in_DD)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
    ndb_log_warning("Failed to determine if schema '%s' exists in DD",
                    schema_name.c_str());
    error_msg = "Failed to determine if object existed in DD";
    return false;
  }

  // There are 3 possible scenarios:
  // 1. Exists in NDB but not in DD. This is dealt with by creating the schema
  //    in the DD
  // 2. Exists in DD but not NDB. This isn't a mismatch we're interested in
  //    fixing since the schema can contain tables of other storage engines
  // 3. Mismatch doesn't exist anymore
  // Scenarios 2 and 3 are handled by simply returning true denoting success
  if (exists_in_NDB && !exists_in_DD) {
    Ndb_local_connection local_connection(thd);
    if (local_connection.create_database(schema_name)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to create schema '%s'", schema_name.c_str());
      error_msg = "Failed to create schema";
      return false;
    }
    ndb_log_info("Schema '%s' installed in DD", schema_name.c_str());
  }
  return true;
}

void Ndb_metadata_sync::drop_ndb_share(const char *schema_name,
                                       const char *table_name) const {
  NDB_SHARE *share =
      NDB_SHARE::acquire_reference(schema_name, table_name, "table_sync");
  if (share) {
    NDB_SHARE::mark_share_dropped_and_release(share, "table_sync");
  }
}

bool Ndb_metadata_sync::sync_table(THD *thd, const std::string &schema_name,
                                   const std::string &table_name,
                                   bool &temp_error, std::string &error_msg) {
  if (DBUG_EVALUATE_IF("ndb_metadata_sync_fail", true, false)) {
    temp_error = false;
    error_msg = "Injected failure";
    return false;
  }
  Ndb_dd_client dd_client(thd);
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name.c_str(),
                                             table_name.c_str(), true)) {
    ndb_log_info("Failed to acquire MDL on table '%s.%s'", schema_name.c_str(),
                 table_name.c_str());
    error_msg = "Failed to acquire MDL on table";
    temp_error = true;
    // Since it's a temporary error, the THD conditions should be cleared but
    // not logged
    clear_thd_conditions(thd);
    return false;
  }

  ndb_log_info("Synchronizing table '%s.%s'", schema_name.c_str(),
               table_name.c_str());

  // Most of the errors detected after this are not temporary
  temp_error = false;

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  Ndb *ndb = thd_ndb->ndb;
  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  bool exists_in_NDB;
  if (!ndb_table_exists(dict, schema_name, table_name, exists_in_NDB)) {
    ndb_log_warning("Failed to determine if table '%s.%s' exists in NDB",
                    schema_name.c_str(), table_name.c_str());
    error_msg = "Failed to determine if object existed in NDB";
    return false;
  }

  bool exists_in_DD;
  if (!dd_client.table_exists(schema_name.c_str(), table_name.c_str(),
                              exists_in_DD)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::WARNING);
    ndb_log_warning("Failed to determine if table '%s.%s' exists in DD",
                    schema_name.c_str(), table_name.c_str());
    error_msg = "Failed to determine if object existed in DD";
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
    if (!dd_client.is_local_table(schema_name.c_str(), table_name.c_str(),
                                  local_table)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to determine if table '%s.%s' was a local table",
                    schema_name.c_str(), table_name.c_str());
      error_msg = "Failed to determine if object was a local table";
      return false;
    }
    if (local_table) {
      // Local table, the mismatch is expected
      return true;
    }

    // Remove the table from DD
    Ndb_referenced_tables_invalidator invalidator(thd, dd_client);
    if (!dd_client.remove_table(schema_name.c_str(), table_name.c_str(),
                                &invalidator)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to drop table '%s.%s' in DD", schema_name.c_str(),
                    table_name.c_str());
      error_msg = "Failed to drop object in DD";
      return false;
    }

    if (!invalidator.invalidate()) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error(
          "Failed to invalidate tables referencing table '%s.%s' in "
          "DD",
          schema_name.c_str(), table_name.c_str());
      error_msg = "Failed to invalidate table references";
      return false;
    }

    // Drop share if it exists
    drop_ndb_share(schema_name.c_str(), table_name.c_str());
    ndb_tdc_close_cached_table(thd, schema_name.c_str(), table_name.c_str());

    dd_client.commit();
    ndb_log_info("Table '%s.%s' dropped from DD", schema_name.c_str(),
                 table_name.c_str());

    // Invalidate the table in NdbApi
    Ndb_table_guard ndbtab_guard(ndb, schema_name.c_str(), table_name.c_str());
    ndbtab_guard.invalidate();
    return true;
  }

  // Table exists in NDB but not in DD. Correct this by installing the table in
  // the DD
  Ndb_table_guard ndbtab_guard(ndb, schema_name.c_str(), table_name.c_str());
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
                 schema_name.c_str(), table_name.c_str());
    error_msg = "Failed to get extra metadata of table";
    return false;
  }

  if (extra_metadata_version == 1) {
    // Table with "old" metadata found
    if (!dd_client.migrate_table(
            schema_name.c_str(), table_name.c_str(),
            static_cast<const unsigned char *>(unpacked_data), unpacked_len,
            false)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error(
          "Failed to migrate table '%s.%s' with extra metadata "
          "version 1",
          schema_name.c_str(), table_name.c_str());
      error_msg = "Failed to migrate table with extra metadata version 1";
      free(unpacked_data);
      return false;
    }
    free(unpacked_data);
    const dd::Table *dd_table;
    if (!dd_client.get_table(schema_name.c_str(), table_name.c_str(),
                             &dd_table)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error(
          "Failed to get table '%s.%s' from DD after it was installed",
          schema_name.c_str(), table_name.c_str());
      error_msg = "Failed to get object from DD";
      return false;
    }
    if (!Ndb_metadata::check_index_count(dict, ndbtab, dd_table)) {
      // Mismatch in terms of number of indexes in NDB Dictionary and DD. This
      // is likely due to the fact that a table has been created in NDB
      // Dictionary but the indexes haven't been created yet. The expectation is
      // that the indexes will be created by the next detection cycle so this is
      // treated as a temporary error
      log_and_clear_thd_conditions(thd, condition_logging_level::INFO);
      ndb_log_info("Table '%s.%s' not synced due to mismatch in indexes",
                   schema_name.c_str(), table_name.c_str());
      error_msg = "Mismatch in indexes detected";
      temp_error = true;
      return false;
    }
    if (!Ndb_metadata::compare(thd, ndb, schema_name.c_str(), ndbtab,
                               dd_table)) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Definition of table '%s.%s' in NDB Dictionary has changed",
                    schema_name.c_str(), table_name.c_str());
      error_msg = "Definition of table has changed in NDB Dictionary";
      return false;
    }
    if (ndbcluster_binlog_setup_table(thd, ndb, schema_name.c_str(),
                                      table_name.c_str(), dd_table) != 0) {
      log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
      ndb_log_error("Failed to setup binlogging for table '%s.%s'",
                    schema_name.c_str(), table_name.c_str());
      error_msg = "Failed to setup binlogging for table";
      return false;
    }
    dd_client.commit();
    ndb_log_info("Table '%s.%s' installed in DD", schema_name.c_str(),
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
      error_msg = "Failed to acquire MDL on tablespace";
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
      error_msg = "Failed to determine if object existed in DD";
      return false;
    }
    if (!tablespace_exists) {
      const Detected_object obj("", tablespace_name, TABLESPACE_OBJECT);
      if (object_excluded(obj)) {
        // The tablespace was detected but its sync failed. Such errors
        // shouldn't be treated as temporary errors and the table is excluded
        ndb_log_error("Tablespace '%s' is currently excluded",
                      tablespace_name.c_str());
        ndb_log_error("Failed to install disk data table '%s.%s'",
                      schema_name.c_str(), table_name.c_str());
        error_msg =
            "Failed to install disk data table since tablespace has been "
            "excluded";
        return false;
      } else {
        // There's a possibility (especially when ndb_restore is used) that a
        // disk data table is being synchronized before the tablespace has been
        // synchronized which is a temporary error since the next detection
        // cycle will detect and attempt to sync the tablespace before the table
        ndb_log_info(
            "Disk data table '%s.%s' not synced since tablespace '%s' "
            "hasn't been synced yet",
            schema_name.c_str(), table_name.c_str(), tablespace_name.c_str());
        error_msg = "Tablespace has not been synchronized yet";
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
          schema_name.c_str(), table_name.c_str(), sdi, ndbtab->getObjectId(),
          ndbtab->getObjectVersion(), ndbtab->getPartitionCount(),
          tablespace_name, false, &invalidator)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to install table '%s.%s' in DD", schema_name.c_str(),
                  table_name.c_str());
    error_msg = "Failed to install object in DD";
    return false;
  }

  if (!invalidator.invalidate()) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to invalidate tables referencing table '%s.%s' in DD",
                  schema_name.c_str(), table_name.c_str());
    error_msg = "Failed to invalidate table references";
    return false;
  }
  const dd::Table *dd_table;
  if (!dd_client.get_table(schema_name.c_str(), table_name.c_str(),
                           &dd_table)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to get table '%s.%s' from DD after it was installed",
                  schema_name.c_str(), table_name.c_str());
    error_msg = "Failed to get object from DD";
    return false;
  }
  if (!Ndb_metadata::check_index_count(dict, ndbtab, dd_table)) {
    // Mismatch in terms of number of indexes in NDB Dictionary and DD. This is
    // likely due to the fact that a table has been created in NDB Dictionary
    // but the indexes haven't been created yet. The expectation is that the
    // indexes will be created by the next detection cycle so this is treated
    // as a temporary error
    log_and_clear_thd_conditions(thd, condition_logging_level::INFO);
    ndb_log_info("Table '%s.%s' not synced due to mismatch in indexes",
                 schema_name.c_str(), table_name.c_str());
    error_msg = "Mismatch in indexes detected";
    temp_error = true;
    return false;
  }
  if (!Ndb_metadata::compare(thd, ndb, schema_name.c_str(), ndbtab, dd_table)) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Definition of table '%s.%s' in NDB Dictionary has changed",
                  schema_name.c_str(), table_name.c_str());
    error_msg = "Definition of table has changed in NDB Dictionary";
    return false;
  }
  if (ndbcluster_binlog_setup_table(thd, ndb, schema_name.c_str(),
                                    table_name.c_str(), dd_table) != 0) {
    log_and_clear_thd_conditions(thd, condition_logging_level::ERROR);
    ndb_log_error("Failed to setup binlogging for table '%s.%s'",
                  schema_name.c_str(), table_name.c_str());
    error_msg = "Failed to setup binlogging for table";
    return false;
  }
  dd_client.commit();
  ndb_log_info("Table '%s.%s' installed in DD", schema_name.c_str(),
               table_name.c_str());
  return true;
}
