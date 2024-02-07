/*
   Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_REPLICATION_CONNECTION_STATUS_H
#define TABLE_REPLICATION_CONNECTION_STATUS_H

/**
  @file storage/perfschema/table_replication_connection_status.h
  Table replication_connection_status (declarations).
*/

#include <stddef.h>
#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_info.h"      /*CHANNEL_NAME_LENGTH */
#include "sql/rpl_reporting.h" /* MAX_SLAVE_ERRMSG */
#include "sql/sql_const.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Master_info;
class Plugin_table;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
enum enum_rpl_yes_no { PS_RPL_YES = 1, PS_RPL_NO };
#endif

enum enum_rpl_connect_status_service_state {
  PS_RPL_CONNECT_SERVICE_STATE_YES = 1,
  PS_RPL_CONNECT_SERVICE_STATE_NO,
  PS_RPL_CONNECT_SERVICE_STATE_CONNECTING
};

/*
  A row in the table. The fields with string values have an additional
  length field denoted by <field_name>_length.
*/
struct st_row_connect_status {
  char group_name[UUID_LENGTH];
  bool group_name_is_null;
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  char source_uuid[UUID_LENGTH];
  bool source_uuid_is_null;
  ulonglong thread_id{0};
  bool thread_id_is_null;
  enum_rpl_connect_status_service_state service_state;
  ulonglong count_received_heartbeats;
  ulonglong last_heartbeat_timestamp;
  char *received_transaction_set;
  int received_transaction_set_length;
  uint last_error_number;
  char last_error_message[MAX_SLAVE_ERRMSG];
  uint last_error_message_length;
  ulonglong last_error_timestamp;
  char last_queued_trx[Gtid::MAX_TEXT_LENGTH + 1];
  uint last_queued_trx_length;
  ulonglong last_queued_trx_original_commit_timestamp;
  ulonglong last_queued_trx_immediate_commit_timestamp;
  ulonglong last_queued_trx_start_queue_timestamp;
  ulonglong last_queued_trx_end_queue_timestamp;
  char queueing_trx[Gtid::MAX_TEXT_LENGTH + 1];
  uint queueing_trx_length;
  ulonglong queueing_trx_original_commit_timestamp;
  ulonglong queueing_trx_immediate_commit_timestamp;
  ulonglong queueing_trx_start_queue_timestamp;

  st_row_connect_status() : received_transaction_set(nullptr) {}

  void cleanup() {
    if (received_transaction_set != nullptr) {
      my_free(received_transaction_set);
      received_transaction_set = nullptr;
    }
  }
};

class PFS_index_rpl_connection_status : public PFS_engine_index {
 public:
  explicit PFS_index_rpl_connection_status(PFS_engine_key *key)
      : PFS_engine_index(key) {}

  ~PFS_index_rpl_connection_status() override = default;

  virtual bool match(Master_info *mi) = 0;
};

class PFS_index_rpl_connection_status_by_channel
    : public PFS_index_rpl_connection_status {
 public:
  PFS_index_rpl_connection_status_by_channel()
      : PFS_index_rpl_connection_status(&m_key), m_key("CHANNEL_NAME") {}

  ~PFS_index_rpl_connection_status_by_channel() override = default;

  bool match(Master_info *mi) override;

 private:
  PFS_key_name m_key;
};

class PFS_index_rpl_connection_status_by_thread
    : public PFS_index_rpl_connection_status {
 public:
  PFS_index_rpl_connection_status_by_thread()
      : PFS_index_rpl_connection_status(&m_key), m_key("THREAD_ID") {}

  ~PFS_index_rpl_connection_status_by_thread() override = default;

  bool match(Master_info *mi) override;

 private:
  PFS_key_thread_id m_key;
};

/** Table PERFORMANCE_SCHEMA.REPLICATION_CONNECTION_STATUS. */
class table_replication_connection_status : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

 private:
  int make_row(Master_info *mi);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_connect_status m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

 protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */

  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_replication_connection_status();

 public:
  ~table_replication_connection_status() override;

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();
  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 private:
  PFS_index_rpl_connection_status *m_opened_index;
};

/** @} */
#endif
