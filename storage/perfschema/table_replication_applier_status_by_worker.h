/*
   Copyright (c) 2013, 2023, Oracle and/or its affiliates.

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

#ifndef TABLE_REPLICATION_APPLIER_STATUS_BY_WORKER_H
#define TABLE_REPLICATION_APPLIER_STATUS_BY_WORKER_H

/**
  @file storage/perfschema/table_replication_applier_status_by_worker.h
  Table replication_applier_status_by_worker (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_info.h" /*CHANNEL_NAME_LENGTH*/
#include "sql/rpl_reporting.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Master_info;
class Plugin_table;
class Slave_worker;
struct TABLE;
struct THR_LOCK;
struct mysql_mutex_t;

/**
  @addtogroup performance_schema_tables
  @{
*/

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
/** enumerated values for service_state of worker thread*/
enum enum_rpl_yes_no {
  PS_RPL_YES = 1, /* service_state= on */
  PS_RPL_NO       /* service_state= off */
};
#endif

/*
  A row in worker's table. The fields with string values have an additional
  length field denoted by <field_name>_length.
*/
struct st_row_worker {
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  /*
    worker_id is added to the table because thread is killed at STOP SLAVE
    but the status needs to show up, so worker_id is used as a permanent
    identifier.
  */
  ulonglong worker_id;
  ulonglong thread_id;
  uint thread_id_is_null;
  enum_rpl_yes_no service_state;
  uint last_error_number;
  char last_error_message[MAX_SLAVE_ERRMSG];
  uint last_error_message_length;
  ulonglong last_error_timestamp;
  char last_applied_trx[Gtid::MAX_TEXT_LENGTH + 1];
  uint last_applied_trx_length;
  ulonglong last_applied_trx_original_commit_timestamp;
  ulonglong last_applied_trx_immediate_commit_timestamp;
  ulonglong last_applied_trx_start_apply_timestamp;
  ulonglong last_applied_trx_end_apply_timestamp;
  char applying_trx[Gtid::MAX_TEXT_LENGTH + 1];
  uint applying_trx_length;
  ulonglong applying_trx_original_commit_timestamp;
  ulonglong applying_trx_immediate_commit_timestamp;
  ulonglong applying_trx_start_apply_timestamp;
  ulong last_applied_trx_retries_count;
  uint last_applied_trx_last_retry_err_number;
  char last_applied_trx_last_retry_err_msg[MAX_SLAVE_ERRMSG];
  uint last_applied_trx_last_retry_err_msg_length;
  ulonglong last_applied_trx_last_retry_timestamp;
  ulong applying_trx_retries_count;
  uint applying_trx_last_retry_err_number;
  char applying_trx_last_retry_err_msg[MAX_SLAVE_ERRMSG];
  uint applying_trx_last_retry_err_msg_length;
  ulonglong applying_trx_last_retry_timestamp;
};

/**
  Position in table replication_applier_status_by_worker.
  Index 1 for replication channel.
  Index 2 for worker:
  - position [0] is for Single Thread Slave (Master_info)
  - position [1] .. [N] is for Multi Thread Slave (Slave_worker)
*/
struct pos_replication_applier_status_by_worker : public PFS_double_index {
  pos_replication_applier_status_by_worker() : PFS_double_index(0, 0) {}

  inline void reset() {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline bool has_more_channels(uint num) { return (m_index_1 < num); }

  inline void next_channel() {
    m_index_1++;
    m_index_2 = 0;
  }

  inline void next_worker() { m_index_2++; }

  inline void set_channel_after(
      const pos_replication_applier_status_by_worker *other) {
    m_index_1 = other->m_index_1 + 1;
    m_index_2 = 0;
  }
};

class PFS_index_rpl_applier_status_by_worker : public PFS_engine_index {
 public:
  explicit PFS_index_rpl_applier_status_by_worker(PFS_engine_key *key)
      : PFS_engine_index(key) {}

  PFS_index_rpl_applier_status_by_worker(PFS_engine_key *key_1,
                                         PFS_engine_key *key_2)
      : PFS_engine_index(key_1, key_2) {}

  ~PFS_index_rpl_applier_status_by_worker() override = default;

  virtual bool match(Master_info *mi) = 0;
  virtual bool match(Master_info *mi, Slave_worker *w) = 0;
};

class PFS_index_rpl_applier_status_by_worker_by_channel
    : public PFS_index_rpl_applier_status_by_worker {
 public:
  PFS_index_rpl_applier_status_by_worker_by_channel()
      : PFS_index_rpl_applier_status_by_worker(&m_key_1, &m_key_2),
        m_key_1("CHANNEL_NAME"),
        m_key_2("WORKER_ID") {}

  ~PFS_index_rpl_applier_status_by_worker_by_channel() override = default;

  bool match(Master_info *mi) override;
  bool match(Master_info *mi, Slave_worker *w) override;

 private:
  PFS_key_name m_key_1;
  PFS_key_worker_id m_key_2;
};

class PFS_index_rpl_applier_status_by_worker_by_thread
    : public PFS_index_rpl_applier_status_by_worker {
 public:
  PFS_index_rpl_applier_status_by_worker_by_thread()
      : PFS_index_rpl_applier_status_by_worker(&m_key), m_key("THREAD_ID") {}

  ~PFS_index_rpl_applier_status_by_worker_by_thread() override = default;

  bool match(Master_info *mi) override;
  bool match(Master_info *mi, Slave_worker *w) override;

 private:
  PFS_key_thread_id m_key;
};

/** Table PERFORMANCE_SCHEMA.replication_applier_status_by_worker */
class table_replication_applier_status_by_worker : public PFS_engine_table {
  typedef pos_replication_applier_status_by_worker pos_t;

 private:
  int make_row(Slave_worker *);
  /*
    Master_info to construct a row to display SQL Thread's status
    information in STS mode
  */
  int make_row(Master_info *);
  void populate_trx_info(Trx_monitoring_info const &applying_trx,
                         Trx_monitoring_info const &last_applied_trx);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** current row. */
  st_row_worker m_row;
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

  table_replication_applier_status_by_worker();

 public:
  ~table_replication_applier_status_by_worker() override;

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
  PFS_index_rpl_applier_status_by_worker *m_opened_index;
};

/** @} */
#endif
