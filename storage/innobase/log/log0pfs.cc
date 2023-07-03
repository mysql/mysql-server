/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cassert>
#include <memory>

#include <mysql/components/services/pfs_plugin_table_service.h>
#include <mysql/components/services/registry.h>
#include <mysql/service_plugin_registry.h>

#include "log0chkp.h"
#include "log0files_dict.h"
#include "log0files_io.h"
#include "log0log.h"
#include "log0pfs.h"
#include "log0sys.h"
#include "ut0mutex.h"

#include "sql/auto_thd.h"      /* Auto_THD */
#include "sql/pfs_priv_util.h" /* drop_native_table_for_pfs */
#include "sql/sql_plugin.h"    /* end_transaction */
#include "sql/table.h"         /* PERFORMANCE_SCHEMA_DB_NAME */
#include "sql/thd_raii.h"      /* Disable_autocommit_guard */

#include "sql/dd/cache/dictionary_client.h" /* Dictionary_client */

SERVICE_TYPE_NO_CONST(pfs_plugin_table_v1) * pfs_table{};
SERVICE_TYPE_NO_CONST(pfs_plugin_column_tiny_v1) * pfs_col_tinyint{};
SERVICE_TYPE_NO_CONST(pfs_plugin_column_integer_v1) * pfs_col_integer{};
SERVICE_TYPE_NO_CONST(pfs_plugin_column_bigint_v1) * pfs_col_bigint{};
SERVICE_TYPE_NO_CONST(pfs_plugin_column_string_v2) * pfs_col_string{};

static bool pfs_initialized{false};

static bool pfs_tables_created{false};

static constexpr const char *SVC_PFS_TABLE = "pfs_plugin_table_v1";

static constexpr const char *SVC_PFS_COLUMN_TINYINT =
    "pfs_plugin_column_tiny_v1";

static constexpr const char *SVC_PFS_COLUMN_INTEGER =
    "pfs_plugin_column_integer_v1";

static constexpr const char *SVC_PFS_COLUMN_BIGINT =
    "pfs_plugin_column_bigint_v1";

static constexpr const char *SVC_PFS_COLUMN_STRING =
    "pfs_plugin_column_string_v2";

/** PFS table with metadata of redo log files. */
class Log_files_pfs_table {
  /** Used to store data of a single row in the table. */
  struct Row {
    Log_file_id m_id;
    lsn_t m_start_lsn, m_end_lsn;
    os_offset_t m_size_in_bytes;
    bool m_is_full;
    int m_consumer_level;
  };

 public:
  static Log_files_pfs_table s_instance;

  /** Name of the PFS mysql table (belongs to performance_schema) */
  static constexpr const char *TABLE_NAME = "innodb_redo_log_files";

  Log_files_pfs_table() {
    m_pfs_table.m_table_name = TABLE_NAME;
    m_pfs_table.m_table_name_length = strlen(TABLE_NAME);
    m_pfs_table.m_table_definition =
        "`FILE_ID` BIGINT NOT NULL"
        " COMMENT 'Id of the file.',\n"

        "`FILE_NAME` VARCHAR(2000) NOT NULL"
        " COMMENT 'Path to the file.',\n"

        "`START_LSN` BIGINT NOT NULL"
        " COMMENT 'LSN of the first block in the file.',\n"

        "`END_LSN` BIGINT NOT NULL"
        " COMMENT 'LSN after the last block in the file.',\n"

        "`SIZE_IN_BYTES` BIGINT NOT NULL"
        " COMMENT 'Size of the file (in bytes).',\n"

        "`IS_FULL` TINYINT NOT NULL"
        " COMMENT '1 iff file has no free space inside.',\n"

        "`CONSUMER_LEVEL` INT NOT NULL"
        " COMMENT 'All redo log consumers registered on smaller levels"
        " than this value, have already consumed this file.'\n";

    m_pfs_table.m_ref_length = sizeof(m_position);
    m_pfs_table.m_acl = READONLY;
    m_pfs_table.delete_all_rows = nullptr;
    m_pfs_table.get_row_count = [] {
      log_t &log = *log_sys;
      IB_mutex_guard latch{&(log.m_files_mutex), UT_LOCATION_HERE};
      return 1ULL * log_files_number_of_existing_files(log.m_files);
    };

    auto &proxy_table = m_pfs_table.m_proxy_engine_table;

    proxy_table.open_table = [](PSI_pos **pos) {
      uint32_t **pos_ptr = reinterpret_cast<uint32_t **>(pos);
      *pos_ptr = &s_instance.m_position;
      return reinterpret_cast<PSI_table_handle *>(&s_instance);
    };

    proxy_table.close_table = [](PSI_table_handle *handle) {
      auto table = reinterpret_cast<Log_files_pfs_table *>(handle);
      table->close();
    };

    proxy_table.rnd_init = [](PSI_table_handle *handle, bool) {
      auto table = reinterpret_cast<Log_files_pfs_table *>(handle);
      return table->rnd_init();
    };

    proxy_table.rnd_next = [](PSI_table_handle *handle) {
      auto table = reinterpret_cast<Log_files_pfs_table *>(handle);
      return table->rnd_next();
    };

    proxy_table.rnd_pos = [](PSI_table_handle *handle) {
      auto table = reinterpret_cast<Log_files_pfs_table *>(handle);
      return table->rnd_pos();
    };

    proxy_table.read_column_value = [](PSI_table_handle *handle,
                                       PSI_field *field, uint32_t index) {
      auto table = reinterpret_cast<Log_files_pfs_table *>(handle);
      return table->read_column_value(field, index);
    };

    proxy_table.reset_position = [](PSI_table_handle *handle) {
      auto table = reinterpret_cast<Log_files_pfs_table *>(handle);
      table->reset_pos();
    };

    proxy_table.index_init = nullptr;
    proxy_table.index_read = nullptr;
    proxy_table.index_next = nullptr;

    proxy_table.write_column_value = nullptr;
    proxy_table.write_row_values = nullptr;
    proxy_table.update_column_value = nullptr;
    proxy_table.update_row_values = nullptr;
    proxy_table.delete_row_values = nullptr;
  }

  int read_column_value(PSI_field *field, uint32_t index) {
    const auto row_index = m_position;
    const bool is_null = row_index == 0;
    ut_a(row_index <= m_rows_n);
    switch (index) {
      case 0: { /* FILE_ID */
        PSI_ulonglong bigint_value;
        bigint_value.val = m_rows_array[row_index].m_id;
        bigint_value.is_null = is_null;
        pfs_col_bigint->set_unsigned(field, bigint_value);
        break;
      }
      case 1: /* FILE_NAME */ {
        const auto file_name =
            log_file_path(log_sys->m_files_ctx, m_rows_array[row_index].m_id);

        pfs_col_string->set_varchar_utf8mb4(
            field, is_null ? nullptr : file_name.c_str());
        break;
      }
      case 2: { /* START_LSN */
        PSI_ulonglong bigint_value;
        bigint_value.val = m_rows_array[row_index].m_start_lsn;
        bigint_value.is_null = is_null;
        pfs_col_bigint->set_unsigned(field, bigint_value);
        break;
      }
      case 3: { /* END_LSN */
        PSI_ulonglong bigint_value;
        bigint_value.val = m_rows_array[row_index].m_end_lsn;
        bigint_value.is_null = is_null;
        pfs_col_bigint->set_unsigned(field, bigint_value);
        break;
      }
      case 4: { /* SIZE_IN_BYTES */
        PSI_ulonglong bigint_value;
        bigint_value.val = m_rows_array[row_index].m_size_in_bytes;
        bigint_value.is_null = is_null;
        pfs_col_bigint->set_unsigned(field, bigint_value);
        break;
      }
      case 5: { /* IS_FULL */
        PSI_tinyint tiny_value;
        tiny_value.val = m_rows_array[row_index].m_is_full ? 1 : 0;
        tiny_value.is_null = is_null;
        pfs_col_tinyint->set(field, tiny_value);
        break;
      }
      case 6: { /* CONSUMER_LEVEL */
        PSI_uint int_value;
        int_value.val = m_rows_array[row_index].m_consumer_level;
        int_value.is_null = is_null;
        pfs_col_integer->set_unsigned(field, int_value);
        break;
      }
      default:
        assert(false);
    }
    return 0;
  }

  int rnd_init() {
    log_t &log = *log_sys;
    IB_mutex_guard latch{&(log.m_files_mutex), UT_LOCATION_HERE};
    const auto n_files = log_files_number_of_existing_files(log.m_files);
    m_rows_array.reset(new Row[n_files + 1]);
    size_t i = 0;
    for (const auto &file : log.m_files) {
      m_rows_array[++i] = {
          file.m_id,
          file.m_start_lsn,
          file.m_end_lsn,
          file.m_size_in_bytes,
          file.m_full,
          file.m_consumed ? 1 : 0  // XXX
      };
    }
    ut_a(i == n_files);
    m_rows_n = n_files;
    m_position = 0;
    return 0;
  }

  int rnd_next() {
    if (m_rows_n == 0) {
      return PFS_HA_ERR_END_OF_FILE;
    }
    ++m_position;
    if (m_position <= m_rows_n) {
      return 0;
    }
    ut_a(m_position == m_rows_n + 1);
    return PFS_HA_ERR_END_OF_FILE;
  }

  int rnd_pos() {
    if (0 < m_position && m_position <= m_rows_n) {
      return 0;
    }
    return PFS_HA_ERR_END_OF_FILE;
  }

  void reset_pos() { m_position = 0; }

  void close() { m_position = 0; }

  PFS_engine_table_share_proxy *get_proxy_share() { return &m_pfs_table; }

 private:
  uint32_t m_rows_n{};
  uint32_t m_position{};
  PFS_engine_table_share_proxy m_pfs_table{};
  std::unique_ptr<Row[]> m_rows_array;
};

Log_files_pfs_table Log_files_pfs_table::s_instance{};

template <typename T>
static bool acquire_service(SERVICE_TYPE(registry) * reg_srv, T *service,
                            const char *name) {
  my_h_service mysql_service;
  if (reg_srv->acquire(name, &mysql_service)) {
    return true;
  }
  *service = reinterpret_cast<T>(mysql_service);
  return false;
}

template <typename T>
static void release_service(SERVICE_TYPE(registry) * reg_srv, T **service) {
  if (*service != nullptr) {
    reg_srv->release(reinterpret_cast<my_h_service>(*service));
    *service = nullptr;
  }
}

void pfs_sdi_enable();
void pfs_sdi_disable();

static bool log_pfs_should_create_tables() { return !srv_read_only_mode; }

bool log_pfs_create_tables() {
  if (!log_pfs_should_create_tables()) {
    return true;
  }
  if (!pfs_initialized) {
    return false;
  }

  ut_a(pfs_table != nullptr);

  PFS_engine_table_share_proxy *pfs_proxy_tables[1];
  pfs_proxy_tables[0] = Log_files_pfs_table::s_instance.get_proxy_share();

  Auto_THD auto_thd;
  auto thd = auto_thd.thd;

  /*
    Set tx_read_only to false to allow installing PFS tables even
    if the server is started with --transaction-read-only=true.
  */
  thd->variables.transaction_read_only = false;
  thd->tx_read_only = false;

  pfs_sdi_disable();

  {
    Disable_autocommit_guard autocommit_guard{thd};
    Disable_binlog_guard disable_binlog(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    if (drop_native_table_for_pfs(PERFORMANCE_SCHEMA_DB_NAME.str,
                                  Log_files_pfs_table::TABLE_NAME)) {
      end_transaction(thd, true);
      pfs_sdi_enable();
      return false;
    }

    end_transaction(thd, false);
  }

  {
    Disable_autocommit_guard autocommit_guard{thd};
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    if (pfs_table->add_tables(pfs_proxy_tables, 1) != 0) {
      end_transaction(thd, true);
      pfs_sdi_enable();
      return false;
    }

    end_transaction(thd, false);
  }

  pfs_sdi_enable();

  pfs_tables_created = true;

  return true;
}

void log_pfs_delete_tables() {
  if (!pfs_tables_created) {
    return;
  }
  PFS_engine_table_share_proxy *pfs_proxy_tables[1];
  pfs_proxy_tables[0] = Log_files_pfs_table::s_instance.get_proxy_share();
  pfs_table->delete_tables(pfs_proxy_tables, 1);
  pfs_tables_created = false;
}

bool log_pfs_acquire_services(SERVICE_TYPE(registry) * reg_srv) {
  extern bool plugin_table_service_initialized;
  ut_a(plugin_table_service_initialized);

  ut_a(pfs_table == nullptr);

  if (reg_srv == nullptr) {
    return false;
  }

  if (acquire_service(reg_srv, &pfs_table, SVC_PFS_TABLE) ||
      acquire_service(reg_srv, &pfs_col_tinyint, SVC_PFS_COLUMN_TINYINT) ||
      acquire_service(reg_srv, &pfs_col_integer, SVC_PFS_COLUMN_INTEGER) ||
      acquire_service(reg_srv, &pfs_col_bigint, SVC_PFS_COLUMN_BIGINT) ||
      acquire_service(reg_srv, &pfs_col_string, SVC_PFS_COLUMN_STRING)) {
    log_pfs_release_services(reg_srv);
    return false;
  }

  pfs_initialized = true;

  return true;
}

void log_pfs_release_services(SERVICE_TYPE(registry) * reg_srv) {
  if (reg_srv == nullptr) {
    return;
  }

  release_service(reg_srv, &pfs_col_string);
  release_service(reg_srv, &pfs_col_bigint);
  release_service(reg_srv, &pfs_col_integer);
  release_service(reg_srv, &pfs_col_tinyint);
  release_service(reg_srv, &pfs_table);

  pfs_initialized = false;
}
