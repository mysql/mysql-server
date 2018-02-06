/*
  Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/table_replication_group_member_stats.cc
  Table replication_group_member_stats (implementation).
*/

#include "storage/perfschema/table_replication_group_member_stats.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "mysql/plugin_group_replication.h"
#include "sql/field.h"
#include "sql/log.h"
#include "sql/plugin_table.h"
#include "sql/rpl_group_replication.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"
#include "thr_lock.h"

/*
  Callbacks implementation for GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS.
*/
static void set_channel_name(void *const context, const char &value,
                             size_t length) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  const size_t max = CHANNEL_NAME_LENGTH;
  length = std::min(length, max);

  row->channel_name_length = length;
  memcpy(row->channel_name, &value, length);
}

static void set_view_id(void *const context, const char &value, size_t length) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  const size_t max = HOSTNAME_LENGTH;
  length = std::min(length, max);

  row->view_id_length = length;
  memcpy(row->view_id, &value, length);
}

static void set_member_id(void *const context, const char &value,
                          size_t length) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  const size_t max = UUID_LENGTH;
  length = std::min(length, max);

  row->member_id_length = length;
  memcpy(row->member_id, &value, length);
}

static void set_transactions_committed(void *const context, const char &value,
                                       size_t length) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);

  if (row->trx_committed != NULL) {
    my_free(row->trx_committed);
  }

  row->trx_committed_length = length;
  row->trx_committed = (char *)my_malloc(PSI_NOT_INSTRUMENTED, length, MYF(0));
  memcpy(row->trx_committed, &value, length);
}

static void set_last_conflict_free_transaction(void *const context,
                                               const char &value,
                                               size_t length) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  const size_t max = Gtid::MAX_TEXT_LENGTH + 1;
  length = std::min(length, max);

  row->last_cert_trx_length = length;
  memcpy(row->last_cert_trx, &value, length);
}

static void set_transactions_in_queue(void *const context,
                                      unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_in_queue = value;
}

static void set_transactions_certified(void *const context,
                                       unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_checked = value;
}

static void set_transactions_conflicts_detected(void *const context,
                                                unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_conflicts = value;
}

static void set_transactions_rows_in_validation(void *const context,
                                                unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_rows_validating = value;
}

static void set_transactions_remote_applier_queue(
    void *const context, unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_remote_applier_queue = value;
}

static void set_transactions_remote_applied(void *const context,
                                            unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_remote_applied = value;
}

static void set_transactions_local_proposed(void *const context,
                                            unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_local_proposed = value;
}

static void set_transactions_local_rollback(void *const context,
                                            unsigned long long int value) {
  struct st_row_group_member_stats *row =
      static_cast<struct st_row_group_member_stats *>(context);
  row->trx_local_rollback = value;
}

THR_LOCK table_replication_group_member_stats::m_table_lock;

Plugin_table table_replication_group_member_stats::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_group_member_stats",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,\n"
    "  VIEW_ID CHAR(60) collate utf8_bin not null,\n"
    "  MEMBER_ID CHAR(36) collate utf8_bin not null,\n"
    "  COUNT_TRANSACTIONS_IN_QUEUE BIGINT unsigned not null,\n"
    "  COUNT_TRANSACTIONS_CHECKED BIGINT unsigned not null,\n"
    "  COUNT_CONFLICTS_DETECTED BIGINT unsigned not null,\n"
    "  COUNT_TRANSACTIONS_ROWS_VALIDATING BIGINT unsigned not null,\n"
    "  TRANSACTIONS_COMMITTED_ALL_MEMBERS LONGTEXT not null,\n"
    "  LAST_CONFLICT_FREE_TRANSACTION TEXT not null,\n"
    "  COUNT_TRANSACTIONS_REMOTE_IN_APPLIER_QUEUE BIGINT unsigned not null,\n"
    "  COUNT_TRANSACTIONS_REMOTE_APPLIED BIGINT unsigned not null,\n"
    "  COUNT_TRANSACTIONS_LOCAL_PROPOSED BIGINT unsigned not null,\n"
    "  COUNT_TRANSACTIONS_LOCAL_ROLLBACK BIGINT unsigned not null\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_replication_group_member_stats::m_share = {
    &pfs_readonly_acl,
    &table_replication_group_member_stats::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_replication_group_member_stats::get_row_count,
    sizeof(pos_t), /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_replication_group_member_stats::create(
    PFS_engine_table_share *) {
  return new table_replication_group_member_stats();
}

table_replication_group_member_stats::table_replication_group_member_stats()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {
  m_row.trx_committed = NULL;
}

table_replication_group_member_stats::~table_replication_group_member_stats() {
  if (m_row.trx_committed != NULL) {
    my_free(m_row.trx_committed);
    m_row.trx_committed = NULL;
  }
}

void table_replication_group_member_stats::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_replication_group_member_stats::get_row_count() {
  return get_group_replication_members_number_info();
}

int table_replication_group_member_stats::rnd_next(void) {
  if (!is_group_replication_plugin_loaded()) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < get_row_count();
       m_pos.next()) {
    make_row(m_pos.m_index);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_replication_group_member_stats::rnd_pos(const void *pos) {
  if (!is_group_replication_plugin_loaded()) {
    return HA_ERR_END_OF_FILE;
  }

  if (get_row_count() == 0) {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < get_row_count());
  return make_row(m_pos.m_index);
}

int table_replication_group_member_stats::make_row(uint index) {
  DBUG_ENTER("table_replication_group_member_stats::make_row");
  // Set default values.
  m_row.channel_name_length = 0;
  m_row.view_id_length = 0;
  m_row.member_id_length = 0;
  m_row.trx_committed_length = 0;
  m_row.last_cert_trx_length = 0;
  m_row.trx_in_queue = 0;
  m_row.trx_checked = 0;
  m_row.trx_conflicts = 0;
  m_row.trx_rows_validating = 0;
  m_row.trx_remote_applier_queue = 0;
  m_row.trx_remote_applied = 0;
  m_row.trx_local_proposed = 0;
  m_row.trx_local_rollback = 0;

  // Set callbacks on GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS.
  const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS callbacks = {
      &m_row,
      &set_channel_name,
      &set_view_id,
      &set_member_id,
      &set_transactions_committed,
      &set_last_conflict_free_transaction,
      &set_transactions_in_queue,
      &set_transactions_certified,
      &set_transactions_conflicts_detected,
      &set_transactions_rows_in_validation,
      &set_transactions_remote_applier_queue,
      &set_transactions_remote_applied,
      &set_transactions_local_proposed,
      &set_transactions_local_rollback,
  };

  // Query plugin and let callbacks do their job.
  if (get_group_replication_group_member_stats_info(index, callbacks)) {
    DBUG_PRINT("info", ("Group Replication stats not available!"));
  }

  DBUG_RETURN(0);
}

int table_replication_group_member_stats::read_row_values(TABLE *table,
                                                          unsigned char *buf,
                                                          Field **fields,
                                                          bool read_all) {
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 0);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /** channel_name */
          set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
          break;
        case 1: /** view id */
          set_field_char_utf8(f, m_row.view_id, m_row.view_id_length);
          break;
        case 2: /** member_id */
          set_field_char_utf8(f, m_row.member_id, m_row.member_id_length);
          break;
        case 3: /** transaction_in_queue */
          set_field_ulonglong(f, m_row.trx_in_queue);
          break;
        case 4: /** transactions_certified */
          set_field_ulonglong(f, m_row.trx_checked);
          break;
        case 5: /** negatively_certified_transaction */
          set_field_ulonglong(f, m_row.trx_conflicts);
          break;
        case 6: /** certification_db_size */
          set_field_ulonglong(f, m_row.trx_rows_validating);
          break;
        case 7: /** stable_set */
          set_field_blob(f, m_row.trx_committed, m_row.trx_committed_length);
          break;
        case 8: /** last_certified_transaction */
          set_field_blob(f, m_row.last_cert_trx, m_row.last_cert_trx_length);

          break;
        case 9:
          set_field_ulonglong(f, m_row.trx_remote_applier_queue);
          break;
        case 10:
          set_field_ulonglong(f, m_row.trx_remote_applied);
          break;
        case 11:
          set_field_ulonglong(f, m_row.trx_local_proposed);
          break;
        case 12:
          set_field_ulonglong(f, m_row.trx_local_rollback);
          break;

        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
