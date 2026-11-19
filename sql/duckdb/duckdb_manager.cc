/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file sql/duckdb/duckdb_manager.cc
 */

#include "duckdb_manager.h"
#include <utility>
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/duckdb/duckdb_config.h"
#include "sql/duckdb/duckdb_mysql_udf.h"
#include "sql/duckdb/duckdb_timezone.h"
#include "sql/mysqld.h"

#include <sys/stat.h>

namespace myduck {

DuckdbManager *DuckdbManager::m_instance = nullptr;

DuckdbManager::DuckdbManager() = default;

bool DuckdbManager::Initialize() {
  if (current_thd != nullptr) {
    DEBUG_SYNC(current_thd, "duckdb_before_initialize_lock");
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  /** TODO: reinitialize m_database when it is invalidated. There is a problem:
  reinitialization need to close all connections, in order to destroy the duckdb
  instance saved in thd. */
  if (m_database != nullptr) {
    return false;
  }

  duckdb::DBConfig config;

  /** TODO: Use direct io to read/write data files. */
  /* Unfortunally, pread failed when MainHeader::CheckMagicBytes. */
  config.options.use_direct_io = global_use_dio;

  // scheduler_process_partial became a generic setting in duckdb v1.5; the
  // field on DBConfigOptions is gone. Route through the generic option API so
  // it lands in the user_settings slot the generic DUCKDB_SETTING form reads.
  config.SetOptionByName(
      "scheduler_process_partial",
      duckdb::Value::BOOLEAN(global_scheduler_process_partial));

  config.SetOptionByName(
      "enable_background_checkpoint",
      duckdb::Value::BOOLEAN(global_enable_background_checkpoint));

  config.SetOptionByName(
      "background_checkpoint_full_interval",
      duckdb::Value::UBIGINT(global_background_checkpoint_full_interval));

  // DuckDB v1.5.2 rejects non-integer literals in ORDER BY (e.g. ORDER BY
  // 'abc') by default. MySQL silently ignores them, and our plugin can rewrite
  // a GROUP_CONCAT positional ORDER BY into one that contains string literals,
  // so enable the legacy permissive behavior.
  config.SetOptionByName("order_by_non_integer_literal",
                         duckdb::Value::BOOLEAN(true));

  if (global_max_threads != 0) {
    config.options.maximum_threads = global_max_threads;
  }

  if (global_memory_limit != 0) {
    config.options.maximum_memory = global_memory_limit;
  }

  if (global_duckdb_temp_directory != nullptr)
    config.options.temporary_directory = global_duckdb_temp_directory;
  else {
    /* Default same as data directory. */
    char path[FN_REFLEN];
    fn_format(path, DUCKDB_DEFAULT_TMP_NAME, mysql_real_data_home, "", MYF(0));
    config.options.temporary_directory = path;
  }

  if (global_max_temp_directory_size != 0)
    config.options.maximum_swap_space = global_max_temp_directory_size;

  config.options.checkpoint_wal_size = checkpoint_threshold;

  // For now, we store all tables in one file
  char path[FN_REFLEN];
  fn_format(path, DUCKDB_FILE_NAME, mysql_real_data_home, "", MYF(0));
  auto database = std::make_unique<duckdb::DuckDB>(path, &config);

  if (current_thd != nullptr) {
    DEBUG_SYNC(current_thd, "duckdb_before_initialize_publish");
  }

  TimeZoneOffsetHelper::init_timezone();

  duckdb::Connection con(*database);
  register_mysql_udf(&con);

  m_database = std::move(database);

  LogErr(INFORMATION_LEVEL, ER_DUCKDB, "DuckdbManager::Initialize succeed.");

  return false;
}

bool DuckdbManager::CreateInstance() {
  assert(m_instance == nullptr);
  m_instance = new DuckdbManager();
  if (m_instance == nullptr) {
    LogErr(ERROR_LEVEL, ER_DUCKDB, "DuckdbManager::CreateInstance failed.");
    return true;
  }
  return false;
}

DuckdbManager::~DuckdbManager() = default;

void DuckdbManager::Cleanup() {
  if (m_instance == nullptr) return;

  delete m_instance;
  m_instance = nullptr;
}

DuckdbManager &DuckdbManager::Get() {
  assert(m_instance != nullptr);

  const bool ret [[maybe_unused]] = m_instance->Initialize();
  assert(!ret);

  return *m_instance;
}

std::shared_ptr<duckdb::Connection> DuckdbManager::CreateConnection() {
  auto &instance = Get();
  auto connection = std::make_shared<duckdb::Connection>(*instance.m_database);
  return connection;
}

bool DuckdbManager::DataFilesExist() {
  char path[FN_REFLEN];
  struct stat st;

  // Check for main database file: duckdb.db
  fn_format(path, DUCKDB_FILE_NAME, mysql_real_data_home, "", MYF(0));
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
    return true;
  }

  // Check for WAL file: duckdb.db.wal
  fn_format(path, DUCKDB_FILE_NAME, mysql_real_data_home, ".wal", MYF(0));
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
    return true;
  }

  return false;
}

bool DuckdbManager::InitializeIfNeeded() {
  if (m_instance == nullptr) {
    return true;
  }

  // Check if we need to initialize DuckDB at startup:
  // 1. DuckDB mode is ON, or
  // 2. DuckDB data files exist (need crash recovery)
  bool need_init = global_mode_on() || DataFilesExist();

  if (!need_init) {
    return false;
  }

  if (DataFilesExist()) {
    LogErr(INFORMATION_LEVEL, ER_DUCKDB,
           "DuckDB data files detected, initializing for crash recovery.");
  }

  // Trigger initialization (which includes crash recovery via WAL replay)
  bool ret = m_instance->Initialize();
  if (ret) {
    LogErr(ERROR_LEVEL, ER_DUCKDB, "DuckdbManager::InitializeIfNeeded failed.");
    return true;
  }

  return false;
}

}  // namespace myduck
