/*
  Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_connection_status.cc
  Table replication_connection_status (implementation).
*/

#include "storage/perfschema/table_replication_connection_status.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "mysql/plugin_group_replication.h"
#include "sql/log.h"
#include "sql/plugin_table.h"
#include "sql/rpl_group_replication.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /* Multi source replication */
#include "sql/rpl_rli.h"
#include "sql/rpl_slave.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

/*
  Callbacks implementation for GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS.
*/
static void set_channel_name(void *const, const char &, size_t) {}

static void set_group_name(void *const context, const char &value,
                           size_t length) {
  struct st_row_connect_status *row =
      static_cast<struct st_row_connect_status *>(context);
  const size_t max = UUID_LENGTH;
  length = std::min(length, max);

  row->group_name_is_null = false;
  memcpy(row->group_name, &value, length);
}

static void set_source_uuid(void *const context, const char &value,
                            size_t length) {
  struct st_row_connect_status *row =
      static_cast<struct st_row_connect_status *>(context);
  const size_t max = UUID_LENGTH;
  length = std::min(length, max);

  row->source_uuid_is_null = false;
  memcpy(row->source_uuid, &value, length);
}

static void set_service_state(void *const context, bool value) {
  struct st_row_connect_status *row =
      static_cast<struct st_row_connect_status *>(context);

  row->service_state = value ? PS_RPL_CONNECT_SERVICE_STATE_YES
                             : PS_RPL_CONNECT_SERVICE_STATE_NO;
}

THR_LOCK table_replication_connection_status::m_table_lock;

Plugin_table table_replication_connection_status::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_connection_status",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) not null,\n"
    "  GROUP_NAME CHAR(36) collate utf8mb4_bin not null,\n"
    "  SOURCE_UUID CHAR(36) collate utf8mb4_bin not null,\n"
    "  THREAD_ID BIGINT unsigned,\n"
    "  SERVICE_STATE ENUM('ON','OFF','CONNECTING') not null,\n"
    "  COUNT_RECEIVED_HEARTBEATS bigint unsigned NOT NULL DEFAULT 0,\n"
    "  LAST_HEARTBEAT_TIMESTAMP TIMESTAMP(6) not null\n"
    "  COMMENT 'Shows when the most recent heartbeat signal was received.',\n"
    "  RECEIVED_TRANSACTION_SET LONGTEXT not null,\n"
    "  LAST_ERROR_NUMBER INTEGER not null,\n"
    "  LAST_ERROR_MESSAGE VARCHAR(1024) not null,\n"
    "  LAST_ERROR_TIMESTAMP TIMESTAMP(6) not null,\n"
    "  PRIMARY KEY (CHANNEL_NAME) USING HASH,\n"
    "  KEY (THREAD_ID) USING HASH,\n"
    "  LAST_QUEUED_TRANSACTION CHAR(57),\n"
    "  LAST_QUEUED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                    not null,\n"
    "  LAST_QUEUED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                     not null,\n"
    "  LAST_QUEUED_TRANSACTION_START_QUEUE_TIMESTAMP TIMESTAMP(6) not null,\n"
    "  LAST_QUEUED_TRANSACTION_END_QUEUE_TIMESTAMP TIMESTAMP(6) not null,\n"
    "  QUEUEING_TRANSACTION CHAR(57),\n"
    "  QUEUEING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6) not null,\n"
    "  QUEUEING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                  not null,\n"
    "  QUEUEING_TRANSACTION_START_QUEUE_TIMESTAMP TIMESTAMP(6) not null\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_replication_connection_status::m_share = {
    &pfs_readonly_acl,
    table_replication_connection_status::create,
    NULL,                                               /* write_row */
    NULL,                                               /* delete_all_rows */
    table_replication_connection_status::get_row_count, /* records */
    sizeof(pos_t),                                      /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_rpl_connection_status_by_channel::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_connect_status row;

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

bool PFS_index_rpl_connection_status_by_thread::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_connect_status row;
    /* NULL THREAD_ID is represented by 0 */
    row.thread_id = 0;

    if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT) {
      PSI_thread *psi = thd_get_psi(mi->info_thd);
      PFS_thread *pfs = reinterpret_cast<PFS_thread *>(psi);
      if (pfs) {
        row.thread_id = pfs->m_thread_internal_id;
      }
    }

    if (!m_key.match(row.thread_id)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_replication_connection_status::create(
    PFS_engine_table_share *) {
  return new table_replication_connection_status();
}

table_replication_connection_status::table_replication_connection_status()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_replication_connection_status::~table_replication_connection_status() {}

void table_replication_connection_status::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_replication_connection_status::get_row_count() {
  /*A lock is not needed for an estimate */
  return channel_map.get_max_channels();
}

int table_replication_connection_status::rnd_next(void) {
  Master_info *mi = NULL;

  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels(); m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    if (mi && mi->host[0]) {
      if (!make_row(mi)) {
        m_next_pos.set_after(&m_pos);
        channel_map.unlock();
        return 0;
      }
    }
  }

  channel_map.unlock();

  return HA_ERR_END_OF_FILE;
}

int table_replication_connection_status::rnd_pos(const void *pos) {
  int res = HA_ERR_RECORD_DELETED;

  Master_info *mi;

  set_position(pos);

  channel_map.rdlock();

  if ((mi = channel_map.get_mi_at_pos(m_pos.m_index))) {
    res = make_row(mi);
  }

  channel_map.unlock();

  return res;
}

int table_replication_connection_status::index_init(uint idx, bool) {
  PFS_index_rpl_connection_status *result = NULL;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_rpl_connection_status_by_channel);
      break;
    case 1:
      result = PFS_NEW(PFS_index_rpl_connection_status_by_thread);
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_replication_connection_status::index_next(void) {
  Master_info *mi = NULL;

  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels(); m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    if (mi && mi->host[0]) {
      if (m_opened_index->match(mi)) {
        if (!make_row(mi)) {
          m_next_pos.set_after(&m_pos);
          channel_map.unlock();
          return 0;
        }
      }
    }
  }

  channel_map.unlock();

  return HA_ERR_END_OF_FILE;
}

int table_replication_connection_status::make_row(Master_info *mi) {
  DBUG_ENTER("table_replication_connection_status::make_row");
  bool error = false;
  Trx_monitoring_info queueing_trx;
  Trx_monitoring_info last_queued_trx;

  /* Default values */
  m_row.group_name_is_null = true;
  m_row.source_uuid_is_null = true;
  m_row.thread_id_is_null = true;
  m_row.service_state = PS_RPL_CONNECT_SERVICE_STATE_NO;

  DBUG_ASSERT(mi != NULL);
  DBUG_ASSERT(mi->rli != NULL);

  mysql_mutex_lock(&mi->data_lock);

  m_row.channel_name_length = mi->get_channel() ? strlen(mi->get_channel()) : 0;
  memcpy(m_row.channel_name, mi->get_channel(), m_row.channel_name_length);

  if (is_group_replication_plugin_loaded() &&
      channel_map.is_group_replication_channel_name(mi->get_channel(), true)) {
    /*
      Group Replication applier channel.
      Set callbacks on GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS.
    */
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS callbacks = {
        &m_row,           &set_channel_name,  &set_group_name,
        &set_source_uuid, &set_service_state,
    };

    // Query plugin and let callbacks do their job.
    if (get_group_replication_connection_status_info(callbacks)) {
      DBUG_PRINT("info", ("Group Replication stats not available!"));
    }
  } else {
    /* Slave channel. */
    if (mi->master_uuid[0] != 0) {
      memcpy(m_row.source_uuid, mi->master_uuid, UUID_LENGTH);
      m_row.source_uuid_is_null = false;
    }

    if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT) {
      m_row.service_state = PS_RPL_CONNECT_SERVICE_STATE_YES;
    } else {
      if (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT) {
        m_row.service_state = PS_RPL_CONNECT_SERVICE_STATE_CONNECTING;
      } else {
        m_row.service_state = PS_RPL_CONNECT_SERVICE_STATE_NO;
      }
    }
  }

  if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT) {
    PSI_thread *psi = thd_get_psi(mi->info_thd);
    PFS_thread *pfs = reinterpret_cast<PFS_thread *>(psi);
    if (pfs) {
      m_row.thread_id = pfs->m_thread_internal_id;
      m_row.thread_id_is_null = false;
    }
  }

  m_row.count_received_heartbeats = mi->received_heartbeats;
  // Time in microseconds since epoch.
  m_row.last_heartbeat_timestamp = (ulonglong)mi->last_heartbeat;

  {
    const Gtid_set *io_gtid_set = mi->rli->get_gtid_set();
    Checkable_rwlock *sid_lock = mi->rli->get_sid_lock();

    sid_lock->wrlock();
    m_row.received_transaction_set_length =
        io_gtid_set->to_string(&m_row.received_transaction_set);
    sid_lock->unlock();

    if (m_row.received_transaction_set_length < 0) {
      my_free(m_row.received_transaction_set);
      m_row.received_transaction_set_length = 0;
      mysql_mutex_unlock(&mi->data_lock);
      error = true;
      goto end;
    }
  }

  /* Errors */
  mysql_mutex_lock(&mi->err_lock);
  m_row.last_error_number = (unsigned int)mi->last_error().number;
  m_row.last_error_message_length = 0;
  m_row.last_error_timestamp = 0;

  /** If error, set error message and timestamp */
  if (m_row.last_error_number) {
    char *temp_store = (char *)mi->last_error().message;
    m_row.last_error_message_length = strlen(temp_store);
    memcpy(m_row.last_error_message, temp_store,
           m_row.last_error_message_length);

    // Time in microsecond since epoch
    m_row.last_error_timestamp = (ulonglong)mi->last_error().skr;
  }
  mysql_mutex_unlock(&mi->err_lock);

  mi->get_gtid_monitoring_info()->copy_info_to(&queueing_trx, &last_queued_trx);

  mysql_mutex_unlock(&mi->data_lock);

  queueing_trx.copy_to_ps_table(mi->rli->get_sid_map(), m_row.queueing_trx,
                                &m_row.queueing_trx_length,
                                &m_row.queueing_trx_original_commit_timestamp,
                                &m_row.queueing_trx_immediate_commit_timestamp,
                                &m_row.queueing_trx_start_queue_timestamp);

  last_queued_trx.copy_to_ps_table(
      mi->rli->get_sid_map(), m_row.last_queued_trx,
      &m_row.last_queued_trx_length,
      &m_row.last_queued_trx_original_commit_timestamp,
      &m_row.last_queued_trx_immediate_commit_timestamp,
      &m_row.last_queued_trx_start_queue_timestamp,
      &m_row.last_queued_trx_end_queue_timestamp);

end:
  if (error) {
    DBUG_RETURN(HA_ERR_RECORD_DELETED);
  }

  DBUG_RETURN(0);
}

int table_replication_connection_status::read_row_values(TABLE *table,
                                                         unsigned char *buf,
                                                         Field **fields,
                                                         bool read_all) {
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /** channel_name*/
          set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
          break;
        case 1: /** group_name */
          if (m_row.group_name_is_null) {
            f->set_null();
          } else {
            set_field_char_utf8(f, m_row.group_name, UUID_LENGTH);
          }
          break;
        case 2: /** source_uuid */
          if (m_row.source_uuid_is_null) {
            f->set_null();
          } else {
            set_field_char_utf8(f, m_row.source_uuid, UUID_LENGTH);
          }
          break;
        case 3: /** thread_id */
          if (m_row.thread_id_is_null) {
            f->set_null();
          } else {
            set_field_ulonglong(f, m_row.thread_id);
          }
          break;
        case 4: /** service_state */
          set_field_enum(f, m_row.service_state);
          break;
        case 5: /** number of heartbeat events received **/
          set_field_ulonglong(f, m_row.count_received_heartbeats);
          break;
        case 6: /** time of receipt of last heartbeat event **/
          set_field_timestamp(f, m_row.last_heartbeat_timestamp);
          break;
        case 7: /** received_transaction_set */
          set_field_blob(f, m_row.received_transaction_set,
                         m_row.received_transaction_set_length);
          break;
        case 8: /*last_error_number*/
          set_field_ulong(f, m_row.last_error_number);
          break;
        case 9: /*last_error_message*/
          set_field_varchar_utf8(f, m_row.last_error_message,
                                 m_row.last_error_message_length);
          break;
        case 10: /*last_error_timestamp*/
          set_field_timestamp(f, m_row.last_error_timestamp);
          break;
        case 11: /*last_queued_trx*/
          set_field_char_utf8(f, m_row.last_queued_trx,
                              m_row.last_queued_trx_length);
          break;
        case 12: /*last_queued_trx_original_commit_timestamp*/
          set_field_timestamp(f,
                              m_row.last_queued_trx_original_commit_timestamp);
          break;
        case 13: /*last_queued_trx_immediate_commit_timestamp*/
          set_field_timestamp(f,
                              m_row.last_queued_trx_immediate_commit_timestamp);
          break;
        case 14: /*last_queued_trx_start_queue_timestamp*/
          set_field_timestamp(f, m_row.last_queued_trx_start_queue_timestamp);
          break;
        case 15: /*last_queued_trx_end_queue_timestamp*/
          set_field_timestamp(f, m_row.last_queued_trx_end_queue_timestamp);
          break;
        case 16: /*queueing_trx*/
          set_field_char_utf8(f, m_row.queueing_trx, m_row.queueing_trx_length);
          break;
        case 17: /*queueing_trx_original_commit_timestamp*/
          set_field_timestamp(f, m_row.queueing_trx_original_commit_timestamp);
          break;
        case 18: /*queueing_trx_immediate_commit_timestamp*/
          set_field_timestamp(f, m_row.queueing_trx_immediate_commit_timestamp);
          break;
        case 19: /*queueing_trx_start_queue_timestamp*/
          set_field_timestamp(f, m_row.queueing_trx_start_queue_timestamp);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  m_row.cleanup();

  return 0;
}
