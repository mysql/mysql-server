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

#pragma once

#include <memory>
#include <mutex>

#include "duckdb.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/main/appender.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/table_description.hpp"

namespace myduck {

constexpr char DUCKDB_FILE_NAME[] = "duckdb.db";
constexpr char DUCKDB_DEFAULT_TMP_NAME[] = "duckdb_tmp";

class DuckdbManager {
 public:
  DuckdbManager(const DuckdbManager &) = delete;
  DuckdbManager &operator=(const DuckdbManager &) = delete;

  static bool CreateInstance();

  static void Cleanup();

  static inline DuckdbManager &Get();

  static std::shared_ptr<duckdb::Connection> CreateConnection();

  /**
   * Initialize DuckDB if needed at startup.
   * This should be called after CreateInstance() to trigger crash recovery
   * when:
   * 1. DuckDB mode is ON (global_mode == DUCKDB_ON), or
   * 2. DuckDB data files exist in the data directory
   *
   * @return true on error, false on success
   */
  static bool InitializeIfNeeded();

 private:
  static DuckdbManager *m_instance;

  DuckdbManager();

  ~DuckdbManager();

  bool Initialize();

  /**
   * Check if DuckDB data files exist in the data directory.
   * Looks for duckdb.db file or WAL files.
   *
   * @return true if DuckDB data files exist
   */
  static bool DataFilesExist();

  std::mutex m_mutex;

  // Published under m_mutex only after all initialization has succeeded.
  // The instance remains unchanged until server shutdown drains its users.
  std::unique_ptr<duckdb::DuckDB> m_database;
};

}  // namespace myduck
