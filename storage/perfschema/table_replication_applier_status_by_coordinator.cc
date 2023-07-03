/*
  Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/table_replication_applier_status_by_coordinator.cc
  Table replication_applier_status_by_coordinator (implementation).
*/

#include "storage/perfschema/table_replication_applier_status_by_coordinator.h"

#include <assert.h>
#include "my_compiler.h"

#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /* Multisource replication */
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"

THR_LOCK table_replication_applier_status_by_coordinator::m_table_lock;

Plugin_table table_replication_applier_status_by_coordinator::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_applier_status_by_coordinator",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) not null,\n"
    "  THREAD_ID BIGINT UNSIGNED,\n"
    "  SERVICE_STATE ENUM('ON','OFF') not null,\n"
    "  LAST_ERROR_NUMBER INTEGER not null,\n"
    "  LAST_ERROR_MESSAGE VARCHAR(1024) not null,\n"
    "  LAST_ERROR_TIMESTAMP TIMESTAMP(6) not null,\n"
    "  PRIMARY KEY (CHANNEL_NAME) USING HASH,\n"
    "  KEY (THREAD_ID) USING HASH,\n"
    "  LAST_PROCESSED_TRANSACTION CHAR(57),\n"
    "  LAST_PROCESSED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                       not null,\n"
    "  LAST_PROCESSED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                        not null,\n"
    "  LAST_PROCESSED_TRANSACTION_START_BUFFER_TIMESTAMP TIMESTAMP(6)\n"
    "                                                    not null,\n"
    "  LAST_PROCESSED_TRANSACTION_END_BUFFER_TIMESTAMP TIMESTAMP(6)\n"
    "                                                  not null,\n"
    "  PROCESSING_TRANSACTION CHAR(57),\n"
    "  PROCESSING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                   not null,\n"
    "  PROCESSING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                    not null,\n"
    "  PROCESSING_TRANSACTION_START_BUFFER_TIMESTAMP TIMESTAMP(6) not null\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share
    table_replication_applier_status_by_coordinator::m_share = {
        &pfs_readonly_acl,
        table_replication_applier_status_by_coordinator::create,
        nullptr, /* write_row */
        nullptr, /* delete_all_rows */
        table_replication_applier_status_by_coordinator::get_row_count,
        sizeof(pos_t), /* ref length */
        &m_table_lock,
        &m_table_def,
        true, /* perpetual */
        PFS_engine_table_proxy(),
        {0},
        false /* m_in_purgatory */
};

bool PFS_index_rpl_applier_status_by_coord_by_channel::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_coordinator row;

    /* Mutex locks not necessary for channel name. */
    row.channel_name_length =
        mi->get_channel() ? (uint)strlen(mi->get_channel()) : 0;
    memcpy(row.channel_name, mi->get_channel(), row.channel_name_length);

    if (!m_key.match_not_null(row.channel_name, row.channel_name_length)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_rpl_applier_status_by_coord_by_thread::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_coordinator row;
    /* NULL THREAD_ID is represented by 0 */
    row.thread_id = 0;

    mysql_mutex_lock(&mi->rli->data_lock);

    if (mi->rli->slave_running) {
      PSI_thread *psi [[maybe_unused]] = thd_get_psi(mi->rli->info_thd);
#ifdef HAVE_PSI_THREAD_INTERFACE
      if (psi != nullptr) {
        row.thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
      }
#endif /* HAVE_PSI_THREAD_INTERFACE */
    }

    mysql_mutex_unlock(&mi->rli->data_lock);

    if (!m_key.match(row.thread_id)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_replication_applier_status_by_coordinator::create(
    PFS_engine_table_share *) {
  return new table_replication_applier_status_by_coordinator();
}

table_replication_applier_status_by_coordinator::
    table_replication_applier_status_by_coordinator()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_replication_applier_status_by_coordinator::
    ~table_replication_applier_status_by_coordinator() = default;

void table_replication_applier_status_by_coordinator::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_replication_applier_status_by_coordinator::get_row_count() {
  return channel_map.get_max_channels();
}

int table_replication_applier_status_by_coordinator::rnd_next(void) {
  Master_info *mi;
  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels(); m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    /*
      Construct and display SQL Thread's (Coordinator) information in
      'replication_applier_status_by_coordinator' table only in the case of
      multi threaded slave mode. Code should do nothing in the case of single
      threaded slave mode. In case of single threaded slave mode SQL Thread's
      status will be reported as part of
      'replication_applier_status_by_worker' table.
    */
    if (mi && mi->host[0] && mi->rli && mi->rli->get_worker_count() > 0) {
      make_row(mi);
      m_next_pos.set_after(&m_pos);
      channel_map.unlock();
      return 0;
    }
  }

  channel_map.unlock();
  return HA_ERR_END_OF_FILE;
}

int table_replication_applier_status_by_coordinator::rnd_pos(const void *pos) {
  int res = HA_ERR_RECORD_DELETED;

  Master_info *mi = nullptr;

  set_position(pos);

  channel_map.rdlock();

  if ((mi = channel_map.get_mi_at_pos(m_pos.m_index))) {
    res = make_row(mi);
  }

  channel_map.unlock();

  return res;
}

int table_replication_applier_status_by_coordinator::index_init(uint idx,
                                                                bool) {
  PFS_index_rpl_applier_status_by_coord *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_rpl_applier_status_by_coord_by_channel);
      break;
    case 1:
      result = PFS_NEW(PFS_index_rpl_applier_status_by_coord_by_thread);
      break;
    default:
      assert(false);
      break;
  }
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_replication_applier_status_by_coordinator::index_next(void) {
  int res = HA_ERR_END_OF_FILE;

  Master_info *mi;

  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels() && res != 0;
       m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    /*
      Construct and display SQL Thread's (Coordinator) information in
      'replication_applier_status_by_coordinator' table only in the case of
      multi threaded slave mode. Code should do nothing in the case of single
      threaded slave mode. In case of single threaded slave mode SQL Thread's
      status will be reported as part of
      'replication_applier_status_by_worker' table.
    */
    if (mi && mi->host[0] && mi->rli && mi->rli->get_worker_count() > 0) {
      if (m_opened_index->match(mi)) {
        res = make_row(mi);
        m_next_pos.set_after(&m_pos);
      }
    }
  }

  channel_map.unlock();

  return res;
}

int table_replication_applier_status_by_coordinator::make_row(Master_info *mi) {
  assert(mi != nullptr);
  assert(mi->rli != nullptr);

  mysql_mutex_lock(&mi->rli->data_lock);

  m_row.channel_name_length = strlen(mi->get_channel());
  memcpy(m_row.channel_name, (char *)mi->get_channel(),
         m_row.channel_name_length);

  m_row.thread_id = 0;
  m_row.thread_id_is_null = true;

  if (mi->rli->slave_running) {
    PSI_thread *psi [[maybe_unused]] = thd_get_psi(mi->rli->info_thd);
#ifdef HAVE_PSI_THREAD_INTERFACE
    if (psi != nullptr) {
      m_row.thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
      m_row.thread_id_is_null = false;
    }
#endif /* HAVE_PSI_THREAD_INTERFACE */
  }

  if (mi->rli->slave_running) {
    m_row.service_state = PS_RPL_YES;
  } else {
    m_row.service_state = PS_RPL_NO;
  }

  mysql_mutex_lock(&mi->rli->err_lock);

  m_row.last_error_number = (long int)mi->rli->last_error().number;
  m_row.last_error_message_length = 0;
  m_row.last_error_timestamp = 0;

  /** if error, set error message and timestamp */
  if (m_row.last_error_number) {
    const char *temp_store = mi->rli->last_error().message;
    m_row.last_error_message_length = strlen(temp_store);
    memcpy(m_row.last_error_message, temp_store,
           m_row.last_error_message_length);

    /** time in microsecond since epoch */
    m_row.last_error_timestamp = (ulonglong)mi->rli->last_error().skr;
  }

  mysql_mutex_unlock(&mi->rli->err_lock);

  Trx_monitoring_info last_processed_trx;
  Trx_monitoring_info processing_trx;

  mi->rli->get_gtid_monitoring_info()->copy_info_to(&processing_trx,
                                                    &last_processed_trx);

  mysql_mutex_unlock(&mi->rli->data_lock);

  last_processed_trx.copy_to_ps_table(
      global_sid_map, m_row.last_processed_trx,
      &m_row.last_processed_trx_length,
      &m_row.last_processed_trx_original_commit_timestamp,
      &m_row.last_processed_trx_immediate_commit_timestamp,
      &m_row.last_processed_trx_start_buffer_timestamp,
      &m_row.last_processed_trx_end_buffer_timestamp);

  processing_trx.copy_to_ps_table(
      global_sid_map, m_row.processing_trx, &m_row.processing_trx_length,
      &m_row.processing_trx_original_commit_timestamp,
      &m_row.processing_trx_immediate_commit_timestamp,
      &m_row.processing_trx_start_buffer_timestamp);

  return 0;
}

int table_replication_applier_status_by_coordinator::read_row_values(
    TABLE *table, unsigned char *buf, Field **fields, bool read_all) {
  Field *f;

  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* channel_name */
          set_field_char_utf8mb4(f, m_row.channel_name,
                                 m_row.channel_name_length);
          break;
        case 1: /*thread_id*/
          if (!m_row.thread_id_is_null) {
            set_field_ulonglong(f, m_row.thread_id);
          } else {
            f->set_null();
          }
          break;
        case 2: /*service_state*/
          set_field_enum(f, m_row.service_state);
          break;
        case 3: /*last_error_number*/
          set_field_ulong(f, m_row.last_error_number);
          break;
        case 4: /*last_error_message*/
          set_field_varchar_utf8mb4(f, m_row.last_error_message,
                                    m_row.last_error_message_length);
          break;
        case 5: /*last_error_timestamp*/
          set_field_timestamp(f, m_row.last_error_timestamp);
          break;
        case 6: /*last_processed_trx*/
          set_field_char_utf8mb4(f, m_row.last_processed_trx,
                                 m_row.last_processed_trx_length);
          break;
        case 7: /*last_processed_trx_original_commit_timestamp*/
          set_field_timestamp(
              f, m_row.last_processed_trx_original_commit_timestamp);
          break;
        case 8: /*last_processed_trx_immediate_commit_timestamp*/
          set_field_timestamp(
              f, m_row.last_processed_trx_immediate_commit_timestamp);
          break;
        case 9: /*last_processed_trx_start_buffer_timestamp*/
          set_field_timestamp(f,
                              m_row.last_processed_trx_start_buffer_timestamp);
          break;
        case 10: /*last_processed_trx_end_buffer_timestamp*/
          set_field_timestamp(f, m_row.last_processed_trx_end_buffer_timestamp);
          break;
        case 11: /*processing_trx*/
          set_field_char_utf8mb4(f, m_row.processing_trx,
                                 m_row.processing_trx_length);
          break;
        case 12: /*processing_trx_original_commit_timestamp*/
          set_field_timestamp(f,
                              m_row.processing_trx_original_commit_timestamp);
          break;
        case 13: /*processing_trx_immediate_commit_timestamp*/
          set_field_timestamp(f,
                              m_row.processing_trx_immediate_commit_timestamp);
          break;
        case 14: /*processing_trx_start_buffer_timestamp*/
          set_field_timestamp(f, m_row.processing_trx_start_buffer_timestamp);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
