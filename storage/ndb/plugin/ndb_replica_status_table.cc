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

// Implements
#include "storage/ndb/plugin/ndb_replica_status_table.h"

#include <cassert>
#include <cstring>
#include <string>

static unsigned long long ndb_replica_channel_count() {
  // Return theoretical number of rows for optimizer
  static constexpr auto MANY_CHANNELS = 256;
  return MANY_CHANNELS;
}

static PSI_table_handle *ndb_replica_status_open_table(PSI_pos **pos) {
  // Constructs a table object and returns an opaque pointer
  auto *row_pos = reinterpret_cast<uint32_t **>(pos);
  /*
    Creates an instance of the table. Note that this is deallocated during the
    table close which is implemented in the base class. See the
    ndb_pfs_close_table() function in ndb_pfs_table.cc
  */
  std::unique_ptr<Ndb_replica_status_table> table =
      std::make_unique<Ndb_replica_status_table>();
  *row_pos = table->get_position_address();
  return reinterpret_cast<PSI_table_handle *>(table.release());
}

Ndb_replica_status_table_share::Ndb_replica_status_table_share() {
  m_table_name = "ndb_replication_applier_status";
  m_table_name_length = std::strlen(m_table_name);
  m_table_definition =
      "  CHANNEL_NAME CHAR(64) NOT NULL,\n"
      "  MAX_REPLICATED_EPOCH BIGINT UNSIGNED NOT NULL,\n"

      // NdbApi statistics
      "  API_WAIT_EXEC_COMPLETE_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_WAIT_SCAN_RESULT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_WAIT_META_REQUEST_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_WAIT_NANOS_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_BYTES_SENT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_BYTES_RECEIVED_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_TRANS_START_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_TRANS_COMMIT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_TRANS_ABORT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_TRANS_CLOSE_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_PK_OP_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_UK_OP_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_TABLE_SCAN_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_RANGE_SCAN_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_PRUNED_SCAN_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_SCAN_BATCH_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_READ_ROW_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_TRANS_LOCAL_READ_ROW_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_ADAPTIVE_SEND_FORCED_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_ADAPTIVE_SEND_UNFORCED_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  API_ADAPTIVE_SEND_DEFERRED_COUNT BIGINT UNSIGNED NOT NULL,\n"

      // Conflict violation counters
      "  CONFLICT_FN_MAX BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_OLD BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_MAX_DEL_WIN BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_MAX_INS BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_MAX_DEL_WIN_INS BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_EPOCH BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_EPOCH_TRANS BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_EPOCH2 BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_FN_EPOCH2_TRANS BIGINT UNSIGNED NOT NULL,\n"

      // Other conflict counters
      "  CONFLICT_TRANS_ROW_CONFLICT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_TRANS_ROW_REJECT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_TRANS_REJECT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_TRANS_DETECT_ITER_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_TRANS_CONFLICT_COMMIT_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_EPOCH_DELETE_DELETE_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_REFLECTED_OP_PREPARE_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_REFLECTED_OP_DISCARD_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_REFRESH_OP_COUNT BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_LAST_CONFLICT_EPOCH BIGINT UNSIGNED NOT NULL,\n"
      "  CONFLICT_LAST_STABLE_EPOCH BIGINT UNSIGNED NOT NULL";

  get_row_count = ndb_replica_channel_count;
  m_proxy_engine_table.open_table = ndb_replica_status_open_table;
}

int Ndb_replica_status_table::rnd_init() {
  // Build list of all channels in the replica.
  ndb_replica->get_channel_list(m_channel_list);
  set_num_rows(m_channel_list.size());
  reset_pos();
  return 0;
}

extern SERVICE_TYPE_NO_CONST(pfs_plugin_column_string_v2) * pfscol_string;
extern SERVICE_TYPE_NO_CONST(pfs_plugin_column_enum_v1) * pfscol_enum;
extern SERVICE_TYPE_NO_CONST(pfs_plugin_column_bigint_v1) * pfscol_bigint;

static void set_string(PSI_field *field, const std::string &value) {
  pfscol_string->set_char_utf8mb4(field, value.c_str(), value.length());
}

static void set_ubigint(PSI_field *field, Uint64 value) {
  const PSI_ubigint ubigint_value{value, false};
  pfscol_bigint->set_unsigned(field, ubigint_value);
}

int Ndb_replica_status_table::read_column_value(PSI_field *field,
                                                uint32_t index) {
  assert(!is_empty() && rows_pending_read());

  const unsigned int row_index = get_position();
  Ndb_replica::ChannelPtr &channel = m_channel_list[row_index - 1];
  const auto &info = channel->get_channel_info_ref();

  switch (index) {
    case 0:  //  CHANNEL_NAME
      set_string(field, channel->get_channel_name());
      break;

    // Epoch related
    case 1:  // MAX_REPLICATED_EPOCH
      set_ubigint(field, info.max_rep_epoch);
      break;

    // NdbApi statistics
    case 2:  // API_WAIT_EXEC_COMPLETE_COUNT
      set_ubigint(field, info.api_wait_exec_complete_count);
      break;
    case 3:  // API_WAIT_SCAN_RESULT_COUNT
      set_ubigint(field, info.api_wait_scan_result_count);
      break;
    case 4:  // API_WAIT_META_REQUEST_COUNT
      set_ubigint(field, info.api_wait_meta_request_count);
      break;
    case 5:  // API_WAIT_NANOS_COUNT
      set_ubigint(field, info.api_wait_nanos_count);
      break;
    case 6:  // API_BYTES_SENT_COUNT
      set_ubigint(field, info.api_bytes_sent_count);
      break;
    case 7:  // API_BYTES_RECEIVED_COUNT
      set_ubigint(field, info.api_bytes_received_count);
      break;
    case 8:  // API_TRANS_START_COUNT
      set_ubigint(field, info.api_trans_start_count);
      break;
    case 9:  // API_TRANS_COMMIT_COUNT
      set_ubigint(field, info.api_trans_commit_count);
      break;
    case 10:  // API_TRANS_ABORT_COUNT
      set_ubigint(field, info.api_trans_abort_count);
      break;
    case 11:  // API_TRANS_CLOSE_COUNT
      set_ubigint(field, info.api_trans_close_count);
      break;
    case 12:  // API_PK_OP_COUNT
      set_ubigint(field, info.api_pk_op_count);
      break;
    case 13:  // API_UK_OP_COUNT
      set_ubigint(field, info.api_uk_op_count);
      break;
    case 14:  // API_TABLE_SCAN_COUNT
      set_ubigint(field, info.api_table_scan_count);
      break;
    case 15:  // API_RANGE_SCAN_COUNT
      set_ubigint(field, info.api_range_scan_count);
      break;
    case 16:  // API_PRUNED_SCAN_COUNT
      set_ubigint(field, info.api_pruned_scan_count);
      break;
    case 17:  // API_SCAN_BATCH_COUNT
      set_ubigint(field, info.api_scan_batch_count);
      break;
    case 18:  // API_READ_ROW_COUNT
      set_ubigint(field, info.api_read_row_count);
      break;
    case 19:  // API_TRANS_LOCAL_READ_ROW_COUNT
      set_ubigint(field, info.api_trans_local_read_row_count);
      break;
    case 20:  // API_ADAPTIVE_SEND_FORCED_COUNT
      set_ubigint(field, info.api_adaptive_send_forced_count);
      break;
    case 21:  // API_ADAPTIVE_SEND_UNFORCED_COUNT
      set_ubigint(field, info.api_adaptive_send_unforced_count);
      break;
    case 22:  // API_ADAPTIVE_SEND_DEFERRED_COUNT
      set_ubigint(field, info.api_adaptive_send_deferred_count);
      break;

    // Conflict violation counters
    case 23:  // CONFLICT_FN_MAX
      set_ubigint(field, info.conflict_fn_max);
      break;
    case 24:  // CONFLICT_FN_OLD
      set_ubigint(field, info.conflict_fn_max);
      break;
    case 25:  // CONFLICT_FN_MAX_DEL_WIN
      set_ubigint(field, info.conflict_fn_max_del_win);
      break;
    case 26:  // CONFLICT_FN_MAX_INS
      set_ubigint(field, info.conflict_fn_max_ins);
      break;
    case 27:  // CONFLICT_FN_MAX_DEL_WIN_INS
      set_ubigint(field, info.conflict_fn_del_win_ins);
      break;
    case 28:  // CONFLICT_FN_EPOCH
      set_ubigint(field, info.conflict_fn_epoch);
      break;
    case 29:  // CONFLICT_FN_EPOCH_TRANS
      set_ubigint(field, info.conflict_fn_epoch_trans);
      break;
    case 30:  // CONFLICT_FN_EPOCH2
      set_ubigint(field, info.conflict_fn_epoch2);
      break;
    case 31:  // CONFLICT_FN_EPOCH2_TRANS
      set_ubigint(field, info.conflict_fn_epoch2_trans);
      break;

    // Other conflict counters
    case 32:  // CONFLICT_TRANS_ROW_CONFLICT_COUNT
      set_ubigint(field, info.conflict_trans_row_conflict_count);
      break;
    case 33:  // CONFLICT_TRANS_ROW_REJECT_COUNT
      set_ubigint(field, info.conflict_trans_row_reject_count);
      break;
    case 34:  // CONFLICT_TRANS_REJECT_COUNT
      set_ubigint(field, info.conflict_trans_in_conflict_count);
      break;
    case 35:  // CONFLICT_TRANS_DETECT_ITER_COUNT
      set_ubigint(field, info.conflict_trans_detect_iter_count);
      break;
    case 36:  // CONFLICT_TRANS_CONFLICT_COMMIT_COUNT
      set_ubigint(field, info.conflict_trans_conflict_commit_count);
      break;
    case 37:  // CONFLICT_EPOCH_DELETE_DELETE_COUNT
      set_ubigint(field, info.conflict_epoch_delete_delete_count);
      break;
    case 38:  // CONFLICT_REFLECTED_OP_PREPARE_COUNT
      set_ubigint(field, info.conflict_reflected_op_prepare_count);
      break;
    case 39:  // CONFLICT_REFLECTED_OP_DISCARD_COUNT
      set_ubigint(field, info.conflict_reflected_op_discard_count);
      break;
    case 40:  // CONFLICT_REFRESH_OP_COUNT
      set_ubigint(field, info.conflict_refresh_op_count);
      break;
    case 41:  // CONFLICT_LAST_CONFLICT_EPOCH
      set_ubigint(field, info.conflict_last_conflict_epoch);
      break;
    case 42:  // CONFLICT_LAST_STABLE_EPOCH
      set_ubigint(field, info.conflict_last_stable_epoch);
      break;
    default:
      // Unknown column index
      assert(false);
      break;
  }

  return 0;
}

void Ndb_replica_status_table::close() {
  m_channel_list.clear();
  reset_pos();
}

// Instantiate the table share
Ndb_replica_status_table_share replica_status_table_share;
PFS_engine_table_share_proxy *ndb_replica_status_table_share =
    &replica_status_table_share;
