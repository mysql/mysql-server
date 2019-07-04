/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "sql/ndb_metadata_change_monitor.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "my_dbug.h"
#include "mysql/psi/mysql_cond.h"          // mysql_cond_t
#include "mysql/psi/mysql_mutex.h"         // mysql_mutex_t
#include "sql/ha_ndbcluster_binlog.h"      // ndb_binlog_is_read_only
#include "sql/ha_ndbcluster_connection.h"  // ndbcluster_is_connected
#include "sql/ndb_dd_client.h"             // Ndb_dd_client
#include "sql/ndb_ndbapi_util.h"           // ndb_get_*_names
#include "sql/ndb_sleep.h"                 // ndb_milli_sleep
#include "sql/ndb_thd.h"                   // thd_set_thd_ndb
#include "sql/ndb_thd_ndb.h"               // Thd_ndb
#include "sql/sql_class.h"                 // THD
#include "sql/table.h"  // is_infoschema_db() / is_perfschema_db()
#include "storage/ndb/include/ndbapi/Ndb.hpp"       // Ndb
#include "storage/ndb/include/ndbapi/NdbError.hpp"  // NdbError

Ndb_metadata_change_monitor::Ndb_metadata_change_monitor()
    : Ndb_component("Metadata") {}

Ndb_metadata_change_monitor::~Ndb_metadata_change_monitor() {}

int Ndb_metadata_change_monitor::do_init() {
  log_info("Initialization");
  mysql_mutex_init(PSI_INSTRUMENT_ME, &m_wait_mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(PSI_INSTRUMENT_ME, &m_wait_cond);
  return 0;
}

void Ndb_metadata_change_monitor::set_check_interval(
    unsigned long new_check_interval) {
  log_info("Check interval value changed to %lu", new_check_interval);
  mysql_mutex_lock(&m_wait_mutex);
  mysql_cond_signal(&m_wait_cond);
  mysql_mutex_unlock(&m_wait_mutex);
}

void Ndb_metadata_change_monitor::log_NDB_error(
    const NdbError &ndb_error) const {
  log_info("Got NDB error %u: %s", ndb_error.code, ndb_error.message);
}

// NOTE: Most return paths contain info level log messages even in the case of
// failing conditions. The rationale behind this is that during testing, the
// vast majority of the errors were the result of a normal MySQL server
// shutdown. Thus, we stick to info level messages here with the hope that
// "actual" errors are caught in the binlog thread during the synch

static long long g_metadata_detected_count = 0;

static void update_metadata_detected_count(int objects_detected) {
  g_metadata_detected_count += objects_detected;
}

static SHOW_VAR ndb_status_vars_metadata_check[] = {
    {"metadata_detected_count",
     reinterpret_cast<char *>(&g_metadata_detected_count), SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_metadata_check(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = reinterpret_cast<char *>(&ndb_status_vars_metadata_check);
  return 0;
}

bool Ndb_metadata_change_monitor::detect_logfile_group_changes(
    THD *thd, const Thd_ndb *thd_ndb) {
  // Fetch list of logfile groups from NDB
  NdbDictionary::Dictionary *dict = thd_ndb->ndb->getDictionary();
  std::unordered_set<std::string> lfg_in_NDB;
  if (!ndb_get_logfile_group_names(dict, lfg_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    log_info("Failed to fetch logfile group names from NDB");
    return false;
  }

  Ndb_dd_client dd_client(thd);
  // Fetch list of logfile groups from DD
  std::unordered_set<std::string> lfg_in_DD;
  if (!dd_client.fetch_ndb_logfile_group_names(lfg_in_DD)) {
    log_info("Failed to fetch logfile group names from DD");
    return false;
  }

  for (const auto logfile_group_name : lfg_in_NDB) {
    if (lfg_in_DD.find(logfile_group_name) == lfg_in_DD.end()) {
      // Exists in NDB but not in DD
      update_metadata_detected_count(1);
      if (!ndbcluster_binlog_check_logfile_group_asynch(logfile_group_name)) {
        log_info("Failed to submit logfile group '%s' for synchronization",
                 logfile_group_name.c_str());
      }
    } else {
      // Exists in both NDB and DD
      lfg_in_DD.erase(logfile_group_name);
    }
  }

  for (const auto logfile_group_name : lfg_in_DD) {
    // Exists in DD but not in NDB
    update_metadata_detected_count(1);
    if (!ndbcluster_binlog_check_logfile_group_asynch(logfile_group_name)) {
      log_info("Failed to submit logfile group '%s' for synchronization",
               logfile_group_name.c_str());
    }
  }

  return true;
}

bool Ndb_metadata_change_monitor::detect_tablespace_changes(
    THD *thd, const Thd_ndb *thd_ndb) {
  // Fetch list of tablespaces from NDB
  NdbDictionary::Dictionary *dict = thd_ndb->ndb->getDictionary();
  std::unordered_set<std::string> tablespaces_in_NDB;
  if (!ndb_get_tablespace_names(dict, tablespaces_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    log_info("Failed to fetch tablespace names from NDB");
    return false;
  }

  Ndb_dd_client dd_client(thd);
  // Fetch list of tablespaces from DD
  std::unordered_set<std::string> tablespaces_in_DD;
  if (!dd_client.fetch_ndb_tablespace_names(tablespaces_in_DD)) {
    log_info("Failed to fetch tablespace names from DD");
    return false;
  }

  for (const auto tablespace_name : tablespaces_in_NDB) {
    if (tablespaces_in_DD.find(tablespace_name) == tablespaces_in_DD.end()) {
      // Exists in NDB but not in DD
      update_metadata_detected_count(1);
      if (!ndbcluster_binlog_check_tablespace_asynch(tablespace_name)) {
        log_info("Failed to submit tablespace '%s' for synchronization",
                 tablespace_name.c_str());
      }
    } else {
      // Exists in both NDB and DD
      tablespaces_in_DD.erase(tablespace_name);
    }
  }

  for (const auto tablespace_name : tablespaces_in_DD) {
    // Exists in DD but not in NDB
    update_metadata_detected_count(1);
    if (!ndbcluster_binlog_check_tablespace_asynch(tablespace_name)) {
      log_info("Failed to submit tablespace '%s' for synchronization",
               tablespace_name.c_str());
    }
  }
  return true;
}

bool Ndb_metadata_change_monitor::detect_changes_in_schema(
    THD *thd, const Thd_ndb *thd_ndb, const std::string &schema_name) {
  // Fetch list of tables in NDB
  NdbDictionary::Dictionary *dict = thd_ndb->ndb->getDictionary();
  std::unordered_set<std::string> ndb_tables_in_NDB;
  if (!ndb_get_table_names_in_schema(dict, schema_name, ndb_tables_in_NDB)) {
    log_NDB_error(dict->getNdbError());
    log_info("Failed to get list of tables in schema '%s' from NDB",
             schema_name.c_str());
    return false;
  }

  // Lock the schema in DD
  Ndb_dd_client dd_client(thd);
  if (!dd_client.mdl_lock_schema(schema_name.c_str())) {
    log_info("Failed to MDL lock schema '%s'", schema_name.c_str());
    return false;
  }

  // Fetch list of NDB tables in DD, also acquire MDL lock on the tables
  std::unordered_set<std::string> ndb_tables_in_DD;
  if (!dd_client.get_ndb_table_names_in_schema(schema_name.c_str(),
                                               &ndb_tables_in_DD)) {
    log_info("Failed to get list of NDB tables in schema '%s' from DD",
             schema_name.c_str());
    return false;
  }

  // Special case when all tables belonging to a schema still exist in DD but
  // not in NDB
  if (ndb_tables_in_NDB.empty() && !ndb_tables_in_DD.empty()) {
    update_metadata_detected_count(ndb_tables_in_DD.size());
    if (!ndbcluster_binlog_check_schema_asynch(schema_name, "")) {
      log_info("Failed to submit schema '%s' for synchronization",
               schema_name.c_str());
      return false;
    }
    return true;
  }

  // Special case when all tables belonging to a schema still exist in NDB but
  // not in DD
  if (!ndb_tables_in_NDB.empty() && ndb_tables_in_DD.empty()) {
    update_metadata_detected_count(ndb_tables_in_NDB.size());
    if (!ndbcluster_binlog_check_schema_asynch(schema_name, "")) {
      log_info("Failed to submit schema '%s' for synchronization",
               schema_name.c_str());
      return false;
    }
    return true;
  }

  for (const auto ndb_table_name : ndb_tables_in_NDB) {
    if (ndb_tables_in_DD.find(ndb_table_name) == ndb_tables_in_DD.end()) {
      // Exists in NDB but not in DD
      update_metadata_detected_count(1);
      if (!ndbcluster_binlog_check_schema_asynch(schema_name, ndb_table_name)) {
        log_info("Failed to submit table '%s.%s' for synchronization",
                 schema_name.c_str(), ndb_table_name.c_str());
      }
    } else {
      // Exists in both NDB and DD
      ndb_tables_in_DD.erase(ndb_table_name);
    }
  }

  for (const auto ndb_table_name : ndb_tables_in_DD) {
    // Exists in DD but not in NDB
    update_metadata_detected_count(1);
    if (!ndbcluster_binlog_check_schema_asynch(schema_name, ndb_table_name)) {
      log_info("Failed to submit table '%s.%s' for synchronization",
               schema_name.c_str(), ndb_table_name.c_str());
    }
  }
  return true;
}

bool Ndb_metadata_change_monitor::detect_table_changes(THD *thd,
                                                       const Thd_ndb *thd_ndb) {
  // Fetch list of schemas in DD
  Ndb_dd_client dd_client(thd);
  std::vector<std::string> schema_names;
  if (!dd_client.fetch_schema_names(&schema_names)) {
    log_info("Failed to fetch schema names from DD");
    return false;
  }

  for (const auto name : schema_names) {
    if (is_infoschema_db(name.c_str()) || is_perfschema_db(name.c_str())) {
      // We do not expect user changes in these schemas so they can be skipped
      continue;
    }

    if (!detect_changes_in_schema(thd, thd_ndb, name)) {
      log_info("Failed to detect tables changes in schema '%s'", name.c_str());
      if (is_stop_requested()) {
        return false;
      }
    }
  }
  return true;
}

// RAII style class for THD
class Thread_handle_guard {
  THD *const m_thd;
  Thread_handle_guard(const Thread_handle_guard &) = delete;

 public:
  Thread_handle_guard() : m_thd(new THD()) {
    m_thd->system_thread = SYSTEM_THREAD_BACKGROUND;
    m_thd->thread_stack = reinterpret_cast<const char *>(&m_thd);
    m_thd->store_globals();
  }

  ~Thread_handle_guard() {
    if (m_thd) {
      m_thd->release_resources();
      delete m_thd;
    }
  }

  THD *get_thd() const { return m_thd; }
};

// RAII style class for Thd_ndb
class Thd_ndb_guard {
  THD *const m_thd;
  Thd_ndb *const m_thd_ndb;
  Thd_ndb_guard() = delete;
  Thd_ndb_guard(const Thd_ndb_guard &) = delete;

 public:
  Thd_ndb_guard(THD *thd) : m_thd(thd), m_thd_ndb(Thd_ndb::seize(m_thd)) {
    thd_set_thd_ndb(m_thd, m_thd_ndb);
  }

  ~Thd_ndb_guard() {
    Thd_ndb::release(m_thd_ndb);
    thd_set_thd_ndb(m_thd, nullptr);
  }

  const Thd_ndb *get_thd_ndb() const { return m_thd_ndb; }
};

extern bool opt_ndb_metadata_check;
extern unsigned long opt_ndb_metadata_check_interval;

void Ndb_metadata_change_monitor::do_run() {
  DBUG_ENTER("Ndb_metadata_change_monitor::do_run");

  log_info("Starting...");

  if (!wait_for_server_started()) {
    DBUG_VOID_RETURN;
  }

  Thread_handle_guard thd_guard;
  THD *thd = thd_guard.get_thd();
  if (thd == nullptr) {
    DBUG_ASSERT(false);
    log_error("Failed to allocate THD");
    DBUG_VOID_RETURN;
  }

  for (;;) {
    // Outer loop to ensure that if the connection to NDB is lost, a fresh
    // connection is established before the thread continues its processing
    while (!ndbcluster_is_connected(1)) {
      // No connection to NDB yet. Retry until connection is established while
      // checking if stop has been requested at 1 second intervals
      if (is_stop_requested()) {
        DBUG_VOID_RETURN;
      }
    }

    Thd_ndb_guard thd_ndb_guard(thd);
    const Thd_ndb *thd_ndb = thd_ndb_guard.get_thd_ndb();
    if (thd_ndb == nullptr) {
      DBUG_ASSERT(false);
      log_error("Failed to allocate Thd_ndb");
      DBUG_VOID_RETURN;
    }

    for (;;) {
      // Inner loop where each iteration represents one "lap" of the thread
      while (!opt_ndb_metadata_check) {
        // Sleep and then check for change of state i.e. has metadata check been
        // enabled or if a stop has been requested
        ndb_milli_sleep(1000);
        if (is_stop_requested()) {
          DBUG_VOID_RETURN;
        }
      }

      for (unsigned long check_interval = opt_ndb_metadata_check_interval,
                         elapsed_wait_time = 0;
           elapsed_wait_time < check_interval && !is_stop_requested();
           check_interval = opt_ndb_metadata_check_interval) {
        // Determine how long the next wait interval should be using the check
        // interval requested by the user and time spent waiting by the thread
        // already
        const auto wait_interval = check_interval - elapsed_wait_time;
        struct timespec abstime;
        set_timespec(&abstime, wait_interval);
        mysql_mutex_lock(&m_wait_mutex);
        const auto start = std::chrono::steady_clock::now();
        // Can be signalled from 2 places: do_wakeup() when a stop is requested
        // or set_check_interval() when the interval is changed by the user.
        // If a new interval is specified by the user, then the loop logic is
        // written such that if new value <= elapsed_wait time, then this loop
        // exits. Else, the thread waits for the remainder of the time that it
        // needs to as determined at the start of the loop using wait_interval
        mysql_cond_timedwait(&m_wait_cond, &m_wait_mutex, &abstime);
        const auto finish = std::chrono::steady_clock::now();
        mysql_mutex_unlock(&m_wait_mutex);

        // Add latest wait time to total elapsed wait time across different
        // iterations of the while loop
        elapsed_wait_time +=
            std::chrono::duration_cast<std::chrono::duration<unsigned long>>(
                finish - start)
                .count();
      }

      if (is_stop_requested()) {
        DBUG_VOID_RETURN;
      }

      // Check if metadata check is still enabled even after the wait
      if (!opt_ndb_metadata_check) {
        continue;
      }

      // It's pointless to try and monitor metadata changes if schema
      // synchronization is ongoing
      if (ndb_binlog_is_read_only()) {
        log_info(
            "Schema synchronization is ongoing, this iteration of metadata"
            " check is skipped");
        continue;
      }

      // Check if NDB connection is still valid
      if (!ndbcluster_is_connected(1)) {
        // Break out of inner loop
        log_info(
            "Connection to NDB was lost. Attempting to establish a new "
            "connection");
        break;
      }

      log_info("Metadata check started");

      if (!detect_logfile_group_changes(thd, thd_ndb)) {
        log_info("Failed to detect logfile group metadata changes");
      }
      log_info("Logfile group metadata check completed");

      if (is_stop_requested()) {
        DBUG_VOID_RETURN;
      }

      if (!detect_tablespace_changes(thd, thd_ndb)) {
        log_info("Failed to detect tablespace metadata changes");
      }
      log_info("Tablespace metadata check completed");

      if (is_stop_requested()) {
        DBUG_VOID_RETURN;
      }

      if (!detect_table_changes(thd, thd_ndb)) {
        log_info("Failed to detect table metadata changes");
      }
      log_info("Table metadata check completed");
      log_info("Metadata check completed");
    }
  }
}

int Ndb_metadata_change_monitor::do_deinit() {
  log_info("Deinitialization");
  mysql_mutex_destroy(&m_wait_mutex);
  mysql_cond_destroy(&m_wait_cond);
  return 0;
}

void Ndb_metadata_change_monitor::do_wakeup() {
  log_info("Wakeup");
  // Signal that a stop has been requested in case the thread is in the middle
  // of a wait
  mysql_mutex_lock(&m_wait_mutex);
  mysql_cond_signal(&m_wait_cond);
  mysql_mutex_unlock(&m_wait_mutex);
}
