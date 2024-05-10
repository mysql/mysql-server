/*
   Copyright (c) 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements
#include "storage/ndb/plugin/ndb_binlog_purger.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "my_dbug.h"
#include "nulls.h"          // NullS
#include "sql/handler.h"    // ISO_READ_COMMITTED
#include "sql/sql_class.h"  // THD
#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_sleep.h"
#include "storage/ndb/plugin/ndb_thd.h"

static long long purged_rows_count = 0;
static long long purged_files_count = 0;
static long long purge_queue_size = 0;

static SHOW_VAR ndb_status_vars_purger[] = {
    {"log_purged_files", reinterpret_cast<char *>(&purged_files_count),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"log_purged_rows", reinterpret_cast<char *>(&purged_rows_count),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"log_purge_queue_size", reinterpret_cast<char *>(&purge_queue_size),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_purger_stats(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = reinterpret_cast<char *>(&ndb_status_vars_purger);
  return 0;
}

Ndb_binlog_purger::Ndb_binlog_purger(bool *binlogging_on, ulong *log_purge_rate)
    : Ndb_component("Purger", "ndb_purger"),
      m_binlogging_on(binlogging_on),
      m_log_purge_rate(log_purge_rate) {}

Ndb_binlog_purger::~Ndb_binlog_purger() {}

void Ndb_binlog_purger::submit_purge_binlog_file(void *session,
                                                 const std::string &filename) {
  if (is_stop_requested()) {
    // Does not happen, but better not accept new work
    log_error("Binlog file '%s' submitted while stopping", filename.c_str());
    return;
  }

  std::unique_lock files_lock(m_purge_files_lock);

  // Don't allow adding existing filename
  if (std::ranges::any_of(m_purge_files, [filename](const auto &request) {
        return request.filename == filename;
      })) {
    log_info("Binlog file '%s' was already submitted for purge, skipping",
             filename.c_str());
    return;
  }

  m_purge_files.push_back({std::move(filename), session});
  purge_queue_size++;
  assert(purge_queue_size == static_cast<long long>(m_purge_files.size()));
  m_purge_file_added_cond.notify_one();
}

void Ndb_binlog_purger::wait_purge_completed_for_session(void *session) {
  std::unique_lock files_lock(m_purge_files_lock);

  m_purge_files_finished_cond.wait(files_lock, [this, session]() {
    return is_stop_requested() ||
           std::ranges::none_of(m_purge_files, [session](const auto &request) {
             return request.session == session;
           });
  });
}

void Ndb_binlog_purger::find_and_delete_orphan_purged_rows() {
  if (*m_binlogging_on == false) {
    // Can't list binary logs
    return;
  }

  Ndb_local_connection con(m_thd);

  // Build list of existing binary log files
  std::vector<std::string> existing;
  if (con.select_column("SHOW BINARY LOGS", existing)) {
    log_error("Failed to list binary logs");
    return;
  }

  // Lambda to check that file doesn't end with any existing binlog file
  //
  // Example when the file should be kept:
  //   file: ".\binlog.000001"
  //   existing: [ "binlog.000001", "binlog.000002", ... ]
  //
  auto not_existing_filter = [&existing](std::string_view file) {
    for (const auto &e : existing) {
      if (file.ends_with(e)) {
        return false;  // Keep
      }
    }
    return true;  // Remove
  };

  // Build list of binary log files referenced in ndb_binlog_index which does
  // not exist anymore
  std::vector<std::string> ndb_binlog_index_files_not_existing;
  const std::string query =
      "SELECT File FROM mysql.ndb_binlog_index "
      "GROUP BY File ORDER BY File";
  if (con.select_column_matching_filter(
          query, ndb_binlog_index_files_not_existing, not_existing_filter)) {
    log_error("Failed to get list of files referenced by ndb_binlog_index");
    return;
  }

  for (const auto &file : ndb_binlog_index_files_not_existing) {
    log_info("Found row(s) for '%s' which has been purged, removing it",
             file.c_str());
    submit_purge_binlog_file(nullptr, file);
  }
}

bool Ndb_binlog_purger::find_min_and_max_epochs(Ndb_local_connection &con,
                                                const std::string &filename,
                                                uint64_t &min_epoch,
                                                uint64_t &max_epoch) {
  // The idea with these queries is to avoid needing to scan the full table to
  // be able to get the min+max epoch bounds, instead they try to scan a subset
  // of all rows, reducing the data footprint->IO,MVCC,lock footprint->runtime
  // of the query.

  // Get MIN(epoch) for filename
  const std::string min_query =
      "SELECT CAST(epoch AS CHAR(20)) FROM mysql.ndb_binlog_index "
      "WHERE File = '" +
      filename + "' ORDER BY epoch LIMIT 1";
  std::vector<std::string> result;
  if (con.select_column(min_query, result)) {
    log_error("Failed to get MIN(epoch) for '%s'", filename.c_str());
    return true;
  }
  if (result.size() == 0) {
    // No rows found, return identical epochs
    min_epoch = max_epoch = 0;
    return false;
  }
  assert(result.size() == 1);
  min_epoch = std::stoull(result[0]);
  log_info("  min_epoch: %llu", static_cast<unsigned long long>(min_epoch));
  result.clear();

  // Get MAX(epoch) for filename by finding first row with different filename.
  // The max epoch returned is non-inclusive - e.g. it's the first epoch for the
  // next file, rather than the last epoch for this file.
  const std::string max_query =
      "SELECT CAST(epoch AS CHAR(20)) FROM mysql.ndb_binlog_index "
      "WHERE epoch >= " +
      std::to_string(min_epoch) + " AND File != '" + filename +
      "' ORDER BY epoch LIMIT 1";

  if (con.select_column(max_query, result)) {
    log_error("Failed to get MAX(epoch) for != '%s'", filename.c_str());
    return true;
  }
  if (result.size() == 0) {
    // No row with different filename found, fall back to simply find
    // MAX(epoch)+1 using FTS amongst the rows matching filename.
    const std::string max_query =
        "SELECT CAST(MAX(epoch)+1 AS CHAR(20)) FROM mysql.ndb_binlog_index "
        "WHERE epoch >= " +
        std::to_string(min_epoch) + " AND File = '" + filename + "'";
    if (con.select_column(max_query, result)) {
      log_error("Failed to get MAX(epoch) for '%s'", filename.c_str());
      return true;
    }

    if (result.size() == 0) {
      // No max row found, return max one higher than min
      max_epoch = min_epoch + 1;
      log_info("  max_epoch: %llu (min_epoch+1)",
               static_cast<unsigned long long>(max_epoch));
      return false;
    }
  }
  assert(result.size() == 1);
  max_epoch = std::stoull(result[0]);
  log_info("  max_epoch: %llu", static_cast<unsigned long long>(max_epoch));

  return false;
}

void Ndb_binlog_purger::process_purge_first_file_completed(
    const std::string &filename) {
  log_info("Completed purging binlog file: '%s'", filename.c_str());
#ifndef NDEBUG
  // Dobuble check that all rows for filename has been deleted.
  const std::string check_query =
      "SELECT CONCAT_WS(', ', epoch, File) FROM mysql.ndb_binlog_index "
      "WHERE File = '" +
      filename + "' ORDER BY epoch";
  std::vector<std::string> result;
  Ndb_local_connection con(m_thd);
  assert(!con.select_column(check_query, result));
  if (result.size()) {
    log_error("Found rows not deleted:");
    for (const auto &row : result) log_error(" - %s", row.c_str());
    assert(result.size() == 0);
  }
#endif
  std::unique_lock files_lock(m_purge_files_lock);
  assert(m_purge_files[0].filename == filename);
  m_purge_files.erase(m_purge_files.begin());
  purged_files_count++;
  purge_queue_size--;
  assert(purge_queue_size == static_cast<long long>(m_purge_files.size()));
  m_purge_files_finished_cond.notify_all();
}

bool Ndb_binlog_purger::process_purge_first_file(Ndb_local_connection &con) {
  std::unique_lock files_lock(m_purge_files_lock);
  if (m_purge_files.empty()) return false;

  // Start processing file to remove.
  const auto filename = m_purge_files[0].filename;
  log_info("Start purging binlog file: '%s'", filename.c_str());

  files_lock.unlock();

  // DELETE using epoch ranges rather than just the filename as it allows a
  // reduced data + lock footprint
  uint64_t min_epoch = 0;
  uint64_t max_epoch = 0;
  if (find_min_and_max_epochs(con, filename, min_epoch, max_epoch)) {
    log_error("Failed to find min or max epochs for the range to delete");
    return true;
  }

  if (min_epoch == 0 && max_epoch == 0) {
    // Special case for when there are no rows for the file
    process_purge_first_file_completed(filename);
    return false;
  }

  const ulong log_purge_rate = *m_log_purge_rate;
  while (true) {
    // Delete rows between min_epoch and max_epoch in order to efficiently use
    // the clustered primary key index, use limit to avoid redo log exhaustion.
    static constexpr bool ignore_no_such_table = true;
    const std::string where_order_by_limit =
        "epoch >= " + std::to_string(min_epoch) + " AND epoch < " +
        std::to_string(max_epoch) + " AND File='" + filename + "' " +
        "ORDER BY epoch LIMIT " + std::to_string(log_purge_rate);
    if (con.delete_rows("mysql", "ndb_binlog_index", ignore_no_such_table,
                        where_order_by_limit)) {
      log_error(
          "Failed to purge rows for binlog file '%s' from "
          "ndb_binlog_index",
          filename.c_str());
      return true;
    }

    ulong deleted_rows = 0;
    if (con.get_affected_rows(deleted_rows)) {
      log_error("Failed to get affected rows");
      return true;
    }
    log_info("Purged %lu rows for binlog file: '%s'", deleted_rows,
             filename.c_str());
    purged_rows_count += deleted_rows;

    if (deleted_rows < log_purge_rate) {
      process_purge_first_file_completed(filename);
      return false;
    }

    ndb_milli_sleep(DELETE_SLICE_DELAY_MILLIS);
  }
}

void Ndb_binlog_purger::process_purge_files_list() {
  int error_count = 0;
  constexpr int max_errors = 10;
  Ndb_local_connection con(m_thd);
  while (!m_purge_files.empty()) {
    const bool error = process_purge_first_file(con);

    if (is_stop_requested()) {
      return;
    }

    // Give up on purging file after too many errors
    if (error && error_count++ > max_errors) {
      std::unique_lock files_lock(m_purge_files_lock);
      log_error(
          "Got too many errors when removing rows for '%s' from "
          "ndb_binlog_index, skipping...",
          m_purge_files[0].filename.c_str());
      m_purge_files.erase(m_purge_files.begin());
      purge_queue_size--;
      assert(purge_queue_size == static_cast<long long>(m_purge_files.size()));
      m_purge_files_finished_cond.notify_all();
      return;
    }

    ndb_milli_sleep(DELETE_SLICE_DELAY_MILLIS);
  }
}

void Ndb_binlog_purger::do_run() {
  DBUG_TRACE;
  log_info("Starting...");
  if (!wait_for_server_started()) {
    return;
  }
  int stack_base = 0;
  create_thd(&stack_base);
  log_info("Started");

  // Check and delete 'orphan' purged rows
  find_and_delete_orphan_purged_rows();

  while (true) {
    Ndb_thd_memory_guard purger_loop_guard(m_thd);
    {
      std::unique_lock files_lock(m_purge_files_lock);

      m_purge_file_added_cond.wait(files_lock, [this]() {
        return is_stop_requested() || m_purge_files.size() != 0;
      });
    }
    if (is_stop_requested()) break;

    if (false) {
      // Delay for smoking out tests that need wait for asynch purge
      log_info("Sleeping before purge");
      ndb_milli_sleep(10000);
    };

    process_purge_files_list();
  }

  log_info("Stopped");
}

void Ndb_binlog_purger::create_thd(void *stackptr) {
  m_thd = new THD();
  m_thd->thread_stack = reinterpret_cast<const char *>(stackptr);
  m_thd->set_new_thread_id();
  m_thd->store_globals();

  m_thd->init_query_mem_roots();
  m_thd->set_command(COM_DAEMON);
  m_thd->security_context()->skip_grants();

  CHARSET_INFO *charset_connection =
      get_charset_by_csname("utf8mb3", MY_CS_PRIMARY, MYF(MY_WME));
  m_thd->variables.character_set_client = charset_connection;
  m_thd->variables.character_set_results = charset_connection;
  m_thd->variables.collation_connection = charset_connection;
  m_thd->update_charset();

  // Use read committed in order to avoid locking the whole table against
  // inserts while deleting rows
  m_thd->variables.transaction_isolation = ISO_READ_COMMITTED;

  // Ensure that file paths are escaped in a way that does not
  // interfere with path separator on Windows
  m_thd->variables.sql_mode |= MODE_NO_BACKSLASH_ESCAPES;
}

/*
  @brief Callback called when RESET BINARY LOGS AND GTIDS has successfully
  removed binlog and reset index. This means that ndbcluster also need to clear
  its own binlog index(which is stored in the mysql.ndb_binlog_index table).

  @return 0 on success
*/
int Ndb_binlog_purger::do_after_reset_master(void *) {
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

int Ndb_binlog_purger::do_init() {
  if (!m_binlog_hooks.register_hooks(do_after_reset_master)) {
    log_error("Failed to register binlog hooks");
    return 1;
  }
  return 0;
}

int Ndb_binlog_purger::do_deinit() {
  if (m_thd) {
    m_thd->release_resources();
    delete m_thd;
  }
  m_binlog_hooks.unregister_all();
  return 0;
}

void Ndb_binlog_purger::do_wakeup() {
  assert(is_stop_requested());
  log_info("Wakeup");
  m_purge_file_added_cond.notify_one();
  m_purge_files_finished_cond.notify_all();
}
