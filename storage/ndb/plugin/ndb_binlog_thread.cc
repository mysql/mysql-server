/*
   Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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
#include "storage/ndb/plugin/ndb_binlog_thread.h"

#include <cstdint>

// Using
#include "m_string.h"  // NullS
#include "my_dbug.h"
#include "mysql/status_var.h"  // enum_mysql_show_type
#include "sql/current_thd.h"   // current_thd
#include "storage/ndb/include/ndbapi/NdbError.hpp"
#include "storage/ndb/plugin/ndb_apply_status_table.h"
#include "storage/ndb/plugin/ndb_global_schema_lock_guard.h"  // Ndb_global_schema_lock_guard
#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_metadata_change_monitor.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_share.h"

int Ndb_binlog_thread::do_init() {
  if (!binlog_hooks.register_hooks(do_after_reset_master)) {
    ndb_log_error("Failed to register binlog hooks");
    return 1;
  }
  return 0;
}

int Ndb_binlog_thread::do_deinit() {
  binlog_hooks.unregister_all();
  return 0;
}

/*
  @brief Callback called when RESET MASTER has successfully removed binlog and
  reset index. This means that ndbcluster also need to clear its own binlog
  index(which is stored in the mysql.ndb_binlog_index table).

  @return 0 on success
*/
int Ndb_binlog_thread::do_after_reset_master(void *) {
  DBUG_TRACE;

  // Truncate the mysql.ndb_binlog_index table
  // - if table does not exist ignore the error as it is a
  // "consistent" behavior
  Ndb_local_connection mysqld(current_thd);
  const bool ignore_no_such_table = true;
  if (mysqld.truncate_table("mysql", "ndb_binlog_index",
                            ignore_no_such_table)) {
    // Failed to truncate table
    return 1;
  }
  return 0;
}

void Ndb_binlog_thread::validate_sync_excluded_objects(THD *thd) {
  metadata_sync.validate_excluded_objects(thd);
}

void Ndb_binlog_thread::clear_sync_excluded_objects() {
  metadata_sync.clear_excluded_objects();
}

void Ndb_binlog_thread::clear_sync_retry_objects() {
  metadata_sync.clear_retry_objects();
}

bool Ndb_binlog_thread::add_logfile_group_to_check(
    const std::string &lfg_name) {
  return metadata_sync.add_logfile_group(lfg_name);
}

bool Ndb_binlog_thread::add_tablespace_to_check(
    const std::string &tablespace_name) {
  return metadata_sync.add_tablespace(tablespace_name);
}

bool Ndb_binlog_thread::add_schema_to_check(const std::string &schema_name) {
  return metadata_sync.add_schema(schema_name);
}

bool Ndb_binlog_thread::add_table_to_check(const std::string &db_name,
                                           const std::string &table_name) {
  return metadata_sync.add_table(db_name, table_name);
}

void Ndb_binlog_thread::retrieve_sync_excluded_objects(
    Ndb_sync_excluded_objects_table *excluded_table) {
  metadata_sync.retrieve_excluded_objects(excluded_table);
}

unsigned int Ndb_binlog_thread::get_sync_excluded_objects_count() {
  return metadata_sync.get_excluded_objects_count();
}

void Ndb_binlog_thread::retrieve_sync_pending_objects(
    Ndb_sync_pending_objects_table *pending_table) {
  metadata_sync.retrieve_pending_objects(pending_table);
}

unsigned int Ndb_binlog_thread::get_sync_pending_objects_count() {
  return metadata_sync.get_pending_objects_count();
}

static int64_t g_metadata_synced_count = 0;
static void increment_metadata_synced_count() { g_metadata_synced_count++; }

static SHOW_VAR ndb_status_vars_metadata_synced[] = {
    {"metadata_synced_count",
     reinterpret_cast<char *>(&g_metadata_synced_count), SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_metadata_synced(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = reinterpret_cast<char *>(&ndb_status_vars_metadata_synced);
  return 0;
}

void Ndb_binlog_thread::synchronize_detected_object(THD *thd) {
  if (metadata_sync.object_queue_empty()) {
    // No objects pending sync
    Ndb_metadata_change_monitor::sync_done();
    return;
  }

  if (DBUG_EVALUATE_IF("skip_ndb_metadata_sync", true, false)) {
    // Injected failure
    return;
  }

  Ndb_global_schema_lock_guard global_schema_lock_guard(thd);
  if (!global_schema_lock_guard.try_lock()) {
    // Failed to obtain GSL
    return;
  }

  // Synchronize 1 object from the queue
  std::string schema_name, object_name;
  object_detected_type object_type;
  metadata_sync.get_next_object(schema_name, object_name, object_type);
  switch (object_type) {
    case object_detected_type::LOGFILE_GROUP_OBJECT: {
      bool temp_error;
      std::string error_msg;
      if (metadata_sync.sync_logfile_group(thd, object_name, temp_error,
                                           error_msg)) {
        log_info("Logfile group '%s' successfully synchronized",
                 object_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        if (metadata_sync.retry_limit_exceeded(schema_name, object_name,
                                               object_type)) {
          metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                                 object_type, error_msg);
        } else {
          log_info(
              "Failed to synchronize logfile group '%s' due to a temporary "
              "error",
              object_name.c_str());
        }
      } else {
        log_error("Failed to synchronize logfile group '%s'",
                  object_name.c_str());
        metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                               object_type, error_msg);
        increment_metadata_synced_count();
      }
      break;
    }
    case object_detected_type::TABLESPACE_OBJECT: {
      bool temp_error;
      std::string error_msg;
      if (metadata_sync.sync_tablespace(thd, object_name, temp_error,
                                        error_msg)) {
        log_info("Tablespace '%s' successfully synchronized",
                 object_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        if (metadata_sync.retry_limit_exceeded(schema_name, object_name,
                                               object_type)) {
          metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                                 object_type, error_msg);
        } else {
          log_info(
              "Failed to synchronize tablespace '%s' due to a temporary error",
              object_name.c_str());
        }
      } else {
        log_error("Failed to synchronize tablespace '%s'", object_name.c_str());
        metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                               object_type, error_msg);
        increment_metadata_synced_count();
      }
      break;
    }
    case object_detected_type::SCHEMA_OBJECT: {
      bool temp_error;
      std::string error_msg;
      if (metadata_sync.sync_schema(thd, schema_name, temp_error, error_msg)) {
        log_info("Schema '%s' successfully synchronized", schema_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        if (metadata_sync.retry_limit_exceeded(schema_name, object_name,
                                               object_type)) {
          metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                                 object_type, error_msg);
        } else {
          log_info("Failed to synchronize schema '%s' due to a temporary error",
                   schema_name.c_str());
        }
      } else {
        log_error("Failed to synchronize schema '%s'", schema_name.c_str());
        metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                               object_type, error_msg);
        increment_metadata_synced_count();
      }
      break;
    }
    case object_detected_type::TABLE_OBJECT: {
      bool temp_error;
      std::string error_msg;
      if (metadata_sync.sync_table(thd, schema_name, object_name, temp_error,
                                   error_msg)) {
        log_info("Table '%s.%s' successfully synchronized", schema_name.c_str(),
                 object_name.c_str());
        increment_metadata_synced_count();
      } else if (temp_error) {
        if (metadata_sync.retry_limit_exceeded(schema_name, object_name,
                                               object_type)) {
          metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                                 object_type, error_msg);
        } else {
          log_info(
              "Failed to synchronize table '%s.%s' due to a temporary error",
              schema_name.c_str(), object_name.c_str());
        }
      } else {
        log_error("Failed to synchronize table '%s.%s'", schema_name.c_str(),
                  object_name.c_str());
        metadata_sync.exclude_object_from_sync(schema_name, object_name,
                                               object_type, error_msg);
        increment_metadata_synced_count();
      }
      break;
    }
    default: {
      // Unexpected type, should never happen
      assert(false);
    }
  }
}

#ifndef NDEBUG
void Ndb_binlog_thread::dbug_sync_setting() const {
  char global_value[256];
  DBUG_EXPLAIN_INITIAL(global_value, sizeof(global_value));
  char local_value[256];
  DBUG_EXPLAIN(local_value, sizeof(local_value));

  // Detect change, log and set
  if (std::string(global_value) != std::string(local_value)) {
    log_info("Setting debug='%s'", global_value);
    DBUG_SET(global_value);
  }
}
#endif

void Ndb_binlog_thread::log_ndb_error(const NdbError &ndberr) const {
  log_error("Got NDB error '%d - %s'", ndberr.code, ndberr.message);
}

bool Ndb_binlog_thread::acquire_apply_status_reference() {
  DBUG_TRACE;

  m_apply_status_share = NDB_SHARE::acquire_reference(
      Ndb_apply_status_table::DB_NAME.c_str(),
      Ndb_apply_status_table::TABLE_NAME.c_str(), "m_apply_status_share");
  return m_apply_status_share != nullptr;
}

void Ndb_binlog_thread::release_apply_status_reference() {
  DBUG_TRACE;

  if (m_apply_status_share != nullptr) {
    NDB_SHARE::release_reference(m_apply_status_share, "m_apply_status_share");
    m_apply_status_share = nullptr;
  }
}

unsigned Ndb_binlog_thread::Metadata_cache::is_fk_parent(
    unsigned table_id) const {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("parent_id: %u", table_id));

  DBUG_EXECUTE("", {
    for (auto id : m_fk_parent_tables) {
      DBUG_PRINT("info", ("id: %u", id));
    }
  });

  return m_fk_parent_tables.count(table_id);
}

bool Ndb_binlog_thread::Metadata_cache::load_fk_parents(
    const NdbDictionary::Dictionary *dict) {
  DBUG_TRACE;
  std::unordered_set<unsigned> table_ids;
  if (!ndb_get_parent_table_ids_in_dictionary(dict, table_ids)) {
    return false;
  }
  m_fk_parent_tables = std::move(table_ids);
  return true;
}
