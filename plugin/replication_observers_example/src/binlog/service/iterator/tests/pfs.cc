/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "pfs.h"
#include <mysql/components/services/log_builtins.h>
#include <mysql/service_plugin_registry.h>
#include <mysqld_error.h>
#include <sstream>
#include <vector>
#include "include/my_byteorder.h"
#include "my_dbug.h"
#include "required_services.h"
#include "status_vars.h"
#ifndef NDEBUG
#include "sql/debug_sync.h"  // DBUG_SYNC
#endif

namespace binlog::service::iterators::tests {

my_h_service h_ret_table_svc = nullptr;
SERVICE_TYPE(pfs_plugin_table_v1) *table_srv = nullptr;

my_h_service h_ret_col_string_svc = nullptr;
SERVICE_TYPE(pfs_plugin_column_string_v2) *pc_string_srv = nullptr;

my_h_service h_ret_col_bigint_svc = nullptr;
SERVICE_TYPE(pfs_plugin_column_bigint_v1) *pc_bigint_srv = nullptr;

my_h_service h_ret_col_blob_svc = nullptr;
SERVICE_TYPE(pfs_plugin_column_blob_v1) *pc_blob_srv = nullptr;

my_h_service h_ret_binlog_iterator_svc = nullptr;
SERVICE_TYPE(binlog_storage_iterator) *binlog_iterator_svc = nullptr;

my_h_service h_ret_current_thd_svc = nullptr;
SERVICE_TYPE(mysql_current_thread_reader) *current_thd_srv = nullptr;

// extend the buffer 1KB on every increment
static const uint64_t DEFAULT_EXTENT{1024};

static PFS_engine_table_share_proxy table;
static PFS_engine_table_share_proxy *ptables[] = {nullptr};

static bool acquire_service_handles() {
  /* Acquire mysql_server's registry service */
  const auto *r = mysql_plugin_registry_acquire();
  if (r == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "mysql_plugin_registry_acquire() returns empty");
    return true;
    /* purecov: end */
  }

  /* Acquire pfs_plugin_table_v1 service */
  if (r->acquire("pfs_plugin_table_v1", &h_ret_table_svc) != 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find pfs_plugin_table_v1 service");
    return true;
    /* purecov: end */
  }

  /* Type cast this handler to proper service handle */
  table_srv =
      reinterpret_cast<SERVICE_TYPE(pfs_plugin_table_v1) *>(h_ret_table_svc);

  /* Acquire pfs_plugin_column_string_v2 service */
  if (r->acquire("pfs_plugin_column_string_v2", &h_ret_col_string_svc) != 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find pfs_plugin_column_string_v2 service");
    return true;
    /* purecov: end */
  }

  /* Type cast this handler to proper service handle */
  pc_string_srv = reinterpret_cast<SERVICE_TYPE(pfs_plugin_column_string_v2) *>(
      h_ret_col_string_svc);

  /* Acquire pfs_plugin_column_bigint_v1 service */
  if (r->acquire("pfs_plugin_column_bigint_v1", &h_ret_col_bigint_svc) != 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find pfs_plugin_column_bigint_v1 service");
    return true;
    /* purecov: end */
  }

  /* Type cast this handler to proper service handle */
  pc_bigint_srv = reinterpret_cast<SERVICE_TYPE(pfs_plugin_column_bigint_v1) *>(
      h_ret_col_bigint_svc);

  /* Acquire pfs_plugin_column_blob_v1 service */
  if (r->acquire("pfs_plugin_column_blob_v1", &h_ret_col_blob_svc) != 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find pfs_plugin_column_blob_v1 service");
    return true;
    /* purecov: end */
  }

  /* Type cast this handler to proper service handle */
  pc_blob_srv = reinterpret_cast<SERVICE_TYPE(pfs_plugin_column_blob_v1) *>(
      h_ret_col_blob_svc);

  /* Acquire pfs_plugin_column_blob_v1 service */
  if (r->acquire("binlog_storage_iterator", &h_ret_binlog_iterator_svc) != 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find binlog_storage_iterator service");
    return true;
    /* purecov: end */
  }

  /* Type cast this handler to proper service handle */
  binlog_iterator_svc =
      reinterpret_cast<SERVICE_TYPE(binlog_storage_iterator) *>(
          h_ret_binlog_iterator_svc);

  /* Acquire pfs_plugin_column_blob_v1 service */
  if (r->acquire("mysql_current_thread_reader", &h_ret_current_thd_svc) != 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find mysql_current_thread_reader service");
    return true;
    /* purecov: end */
  }

  /* Type cast this handler to proper service handle */
  current_thd_srv =
      reinterpret_cast<SERVICE_TYPE(mysql_current_thread_reader) *>(
          h_ret_current_thd_svc);

  /* Release registry service */
  mysql_plugin_registry_release(r);
  r = nullptr;

  return false;
}

static void release_service_handles() {
  const auto *r = mysql_plugin_registry_acquire();
  if (r == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "mysql_plugin_registry_acquire() returns empty");
    /* purecov: end */
  }

  if (r != nullptr) { /* purecov: inspected */
    if (h_ret_table_svc != nullptr) {
      /* Release pfs_plugin_table_v1 services */
      r->release(h_ret_table_svc);
      h_ret_table_svc = nullptr;
      table_srv = nullptr;
    }

    if (h_ret_col_blob_svc != nullptr) {
      /* Release pfs_plugin_column_blob_v1 services */
      r->release(h_ret_col_blob_svc);
      h_ret_col_blob_svc = nullptr;
      pc_blob_srv = nullptr;
    }

    if (h_ret_col_string_svc != nullptr) {
      /* Release pfs_plugin_column_string_v2 services */
      r->release(h_ret_col_string_svc);
      h_ret_col_string_svc = nullptr;
      pc_string_srv = nullptr;
    }

    if (h_ret_col_bigint_svc != nullptr) {
      /* Release pfs_plugin_column_bigint_v1 services */
      r->release(h_ret_col_bigint_svc);
      h_ret_col_bigint_svc = nullptr;
      pc_bigint_srv = nullptr;
    }

    if (h_ret_binlog_iterator_svc != nullptr) {
      /* Release binlog_storage_iterator services */
      r->release(h_ret_binlog_iterator_svc);
      h_ret_binlog_iterator_svc = nullptr;
      binlog_iterator_svc = nullptr;
    }

    if (h_ret_current_thd_svc != nullptr) {
      /* Release pfs_plugin_column_time_v1 services */
      r->release(h_ret_current_thd_svc);
      h_ret_current_thd_svc = nullptr;
      current_thd_srv = nullptr;
    }

    /* Release registry service */
    mysql_plugin_registry_release(r);
    r = nullptr;
  }
}

bool register_pfs_tables() {
  if (acquire_service_handles()) return true;
  init_share(&table);
  ptables[0] = &table;
  if (table_srv->add_tables(ptables, 1) != 0) {
    /* purecov: begin inspected */
    ptables[0] = nullptr;
    return true;
    /* purecov: end */
  }
  return false;
}

bool unregister_pfs_tables() {
  table_srv->delete_tables(ptables, 1);
  release_service_handles();
  return false;
}

void Cs_entries_table::delete_buffer() const { my_free(buffer); }

bool Cs_entries_table::extend_buffer_capacity(uint64_t size) {
  auto extent = size > 0 ? size : DEFAULT_EXTENT;
  // initial
  if (buffer_capacity == 0) {
    buffer = static_cast<unsigned char *>(
        my_malloc(PSI_NOT_INSTRUMENTED, extent, MYF(0)));
    if (buffer == nullptr) return true;
    buffer_capacity = extent;
    buffer_size = 0;
    global_status_var_sum_buffer_size_requested += buffer_capacity;
    return false;
  }

  assert(buffer != nullptr);
  auto new_size = buffer_capacity + extent;
  auto *ptr = static_cast<unsigned char *>(
      my_realloc(PSI_NOT_INSTRUMENTED, buffer, new_size, MYF(0)));
  if (ptr == nullptr) return true;
  buffer = ptr;
  global_status_var_sum_buffer_size_requested += new_size - buffer_capacity;
  global_status_var_count_buffer_reallocations++;
  buffer_capacity = new_size;
  return false;
}

PSI_table_handle *open_table(PSI_pos **pos [[maybe_unused]]) {
  Cs_entries_table *handle{new Cs_entries_table};
  if (handle->extend_buffer_capacity()) {
    /* purecov: begin inspected */
    handle->is_error = true;
    return reinterpret_cast<PSI_table_handle *>(handle);
    /* purecov: end */
  }
  std::string gtids_excluded{};
  DBUG_EXECUTE_IF("test_binlog_storage_iterator_filter_gtids", {
    gtids_excluded =
        "11111111-1111-1111-1111-111111111111:1,00000000-"
        "0000-0000-0000-000000000000:1-5:10-15:20-25";
  });
  if (binlog_iterator_svc->init(&handle->iterator, gtids_excluded.c_str()) !=
      0) {
    handle->is_error = true;
    return reinterpret_cast<PSI_table_handle *>(handle);
  }

#ifndef NDEBUG
  MYSQL_THD thd{nullptr};
  current_thd_srv->get(&thd);
  DEBUG_SYNC(thd, "test_binlog_storage_iterator_debug_sync_opened_iterator");
#endif
  return reinterpret_cast<PSI_table_handle *>(handle);
}

void close_table(PSI_table_handle *h) {
  assert(h);
  auto *handle = reinterpret_cast<Cs_entries_table *>(h);
  binlog_iterator_svc->deinit(handle->iterator);
  handle->delete_buffer();
  delete handle;
}

#ifndef NDEBUG
static Binlog_iterator_service_get_status dbug_retry_on_eof(
    Cs_entries_table *handle, Binlog_iterator_service_get_status get_return,
    uint64_t *bytes_read) {
  if (get_return == kBinlogIteratorGetEndOfChanges) {
    auto *iterator = handle->iterator;
    MYSQL_THD thd{nullptr};
    current_thd_srv->get(&thd);

    // wait until more transactions are produced into the log
    DEBUG_SYNC(thd, "test_binlog_storage_iterator_debug_sync_iterator_eof");
    while ((get_return = binlog_iterator_svc->get(
                iterator, handle->buffer, handle->buffer_capacity,
                bytes_read)) == kBinlogIteratorGetInsufficientBuffer) {
      uint64_t next_size{0};
      if (binlog_iterator_svc->get_next_entry_size(iterator, &next_size) != 0 ||
          handle->extend_buffer_capacity(next_size))
        return kBinlogIteratorGetErrorUnspecified;
    }
  }
  return get_return;
}

#endif

int rnd_next(PSI_table_handle *h) {
  assert(h);
  auto *handle = reinterpret_cast<Cs_entries_table *>(h);
  if (handle->is_error) return PFS_HA_ERR_WRONG_COMMAND;
  auto *iterator = handle->iterator;

  // engage the service and return the current event fetched if any
  // otherwise return PFS_HA_ERR_END_OF_FILE
  uint64_t bytes_read{0};
  Binlog_iterator_service_get_status get_return{kBinlogIteratorGetOk};
  while ((get_return = binlog_iterator_svc->get(
              iterator, handle->buffer, handle->buffer_capacity,
              &bytes_read)) == kBinlogIteratorGetInsufficientBuffer) {
    uint64_t next_size{0};
    if (binlog_iterator_svc->get_next_entry_size(iterator, &next_size) != 0 ||
        handle->extend_buffer_capacity(next_size))
      return 1; /* purecov: inspected */
  }

  DBUG_EXECUTE_IF("test_binlog_storage_iterator_try_again_on_eof", {
    get_return = dbug_retry_on_eof(handle, get_return, &bytes_read);
  });

  switch (get_return) {
    case kBinlogIteratorGetErrorUnspecified:
    case kBinlogIteratorGetErrorInvalid:
      assert(false);
      return PFS_HA_ERR_WRONG_COMMAND;
    case kBinlogIteratorGetInsufficientBuffer:
      /* purecov: begin inspected */
      assert(false);
      handle->row.reset();
      return PFS_HA_ERR_RECORD_FILE_FULL;
      /* purecov: end */
    case kBinlogIteratorGetOk: {
      uint64_t storage_details_buffer_size{MAX_STORAGE_NAME_SIZE};
      char storage_details_buffer[MAX_STORAGE_NAME_SIZE];
      memset(storage_details_buffer, 0, sizeof(storage_details_buffer));
      auto current_trx_tsid{handle->row.trx_tsid};
      auto current_trx_seqno{handle->row.trx_seqno};
      std::stringstream extra;
      auto &row = handle->row;
      const auto *buffer = reinterpret_cast<const char *>(handle->buffer);
      binlog_iterator_svc->get_storage_details(handle->iterator,
                                               storage_details_buffer,
                                               &storage_details_buffer_size);
      row.reset();
      row.storage_details.append(storage_details_buffer);
      row.end_position = storage_details_buffer_size;
      row.start_position =
          row.end_position - uint4korr(buffer + EVENT_LEN_OFFSET);

      extra << "{ ";

      row.event_type = static_cast<mysql::binlog::event::Log_event_type>(
          buffer[EVENT_TYPE_OFFSET]);
      row.event_name =
          mysql::binlog::event::get_event_type_as_string(row.event_type);

      switch (row.event_type) {
        case mysql::binlog::event::ANONYMOUS_GTID_LOG_EVENT:
        case mysql::binlog::event::FORMAT_DESCRIPTION_EVENT:
        case mysql::binlog::event::ROTATE_EVENT:
          row.trx_tsid = "";
          row.trx_seqno = 0;

          // TODO: extend to other events
          break;
        case mysql::binlog::event::GTID_LOG_EVENT:
        case mysql::binlog::event::GTID_TAGGED_LOG_EVENT: {
          mysql::binlog::event::Gtid_event gev(buffer, &handle->fde);
          row.trx_seqno = gev.get_gno();
          row.trx_tsid = gev.get_tsid().to_string();
          extra << "\"trx_size\" : \"" << gev.transaction_length << "\", ";
          extra << "\"trx_immediate_commit_ts\" : \""
                << gev.immediate_commit_timestamp << "\", ";
          extra << "\"trx_original_commit_ts\" : \""
                << gev.original_commit_timestamp << "\", ";
          break;
        }
        case mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT: {
          mysql::binlog::event::Previous_gtids_event pgev(buffer, &handle->fde);
          // TODO: show the contents on the extra part
          break;
        }
        default:
          row.trx_tsid = current_trx_tsid;
          row.trx_seqno = current_trx_seqno;
          break;
      }

      extra << "\"type\" : \"" << row.event_type << "\"";
      extra << " }";

      row.extra = extra.str();
    }
      return 0;
    default:
      handle->row.reset();
      return PFS_HA_ERR_END_OF_FILE;
  }
}  // namespace binlog::service::iterators::tests

int rnd_init(PSI_table_handle *h, bool scan [[maybe_unused]]) {
  assert(h);
  auto *handle = reinterpret_cast<Cs_entries_table *>(h);
  return handle->is_error ? PFS_HA_ERR_WRONG_COMMAND : 0;
}

/* Read current row from the current_row and display them in the table */
int read_column_value(PSI_table_handle *h, PSI_field *field,
                      unsigned int index) {
  assert(h);
  auto *handle = reinterpret_cast<Cs_entries_table *>(h);
  if (handle->is_error) return PFS_HA_ERR_WRONG_COMMAND;
  const auto &row = handle->row;
  switch (index) {
    case 0: /* EVENT_NAME */
      pc_string_srv->set_varchar_utf8mb4_len(field, row.event_name.c_str(),
                                             row.event_name.size());
      break;
    case 1: /* TRANSACTION_TSID */
      pc_string_srv->set_char_utf8mb4(field, row.trx_tsid.c_str(),
                                      row.trx_tsid.size());
      break;
    case 2: /* TRANSACTION_GNO */
      pc_bigint_srv->set_unsigned(
          field, PSI_ubigint{row.trx_seqno, row.trx_seqno == 0});
      break;
    case 3: /* STORAGE DETAILS */
      pc_blob_srv->set(field, row.storage_details.c_str(),
                       row.storage_details.size());
      break;
    case 4: /* SIZE */
      pc_bigint_srv->set_unsigned(
          field, PSI_ubigint{row.end_position - row.start_position, false});
      break;
    case 5: /* EXTRA */
      pc_blob_srv->set(field, row.extra.c_str(), row.extra.size());
      break;
    default: /* We should never reach here */
      /* purecov: begin inspected */
      assert(0);
      break;
      /* purecov: end */
  }

  return 0;
}

unsigned long long row_count() { return 0; }

int delete_all_rows() { return 0; } /* purecov: inspected */

void init_share(PFS_engine_table_share_proxy *share) {
  assert(share != nullptr);
  /* Instantiate and initialize PFS_engine_table_share_proxy */
  share->m_table_name = binlog::service::iterators::tests::TABLE_NAME.c_str();
  share->m_table_name_length =
      binlog::service::iterators::tests::TABLE_NAME.size();

  share->m_table_definition =
      "entry VARCHAR(1024), \n"
      "transaction_uuid CHAR(36), \n"
      "transaction_gno BIGINT, \n"
      "storage TEXT, \n"
      "size BIGINT UNSIGNED COMMENT 'Storage Size in Bytes', \n"
      "details TEXT\n";
  share->m_ref_length = sizeof(Cs_entries_table::s_current_row_pos);
  share->m_acl = READONLY;
  share->get_row_count = binlog::service::iterators::tests::row_count;
  share->delete_all_rows = binlog::service::iterators::tests::delete_all_rows;

  /* Initialize PFS_engine_table_proxy */
  share->m_proxy_engine_table = {
      binlog::service::iterators::tests::rnd_next,
      binlog::service::iterators::tests::rnd_init,
      nullptr /* rnd_pos */,
      nullptr /* index_init */,
      nullptr /* index_read */,
      nullptr /* index_next */,
      binlog::service::iterators::tests::read_column_value,
      nullptr /* reset_position */,
      nullptr /* write_column_value */,
      nullptr /* write_row_values */,
      nullptr /* update_column_value */,
      nullptr /* update_row_values */,
      nullptr /* delete_row_values */,
      binlog::service::iterators::tests::open_table,
      binlog::service::iterators::tests::close_table};
}

}  // namespace binlog::service::iterators::tests
