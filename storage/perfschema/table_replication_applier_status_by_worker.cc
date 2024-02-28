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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
  @file storage/perfschema/table_replication_applier_status_by_worker.cc
  Table replication_applier_status_by_worker (implementation).
*/

#include "storage/perfschema/table_replication_applier_status_by_worker.h"

#include <assert.h>
#include <stddef.h>

#include "my_compiler.h"

#include "mutex_lock.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/raii/read_write_lock_guard.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /*Multi source replication */
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"
#include "sql/rpl_rli_pdb.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_replication_applier_status_by_worker::m_table_lock;

Plugin_table table_replication_applier_status_by_worker::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_applier_status_by_worker",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) not null,\n"
    "  WORKER_ID BIGINT UNSIGNED not null,\n"
    "  THREAD_ID BIGINT UNSIGNED,\n"
    "  SERVICE_STATE ENUM('ON','OFF') not null,\n"
    "  LAST_ERROR_NUMBER INTEGER not null,\n"
    "  LAST_ERROR_MESSAGE VARCHAR(1024) not null,\n"
    "  LAST_ERROR_TIMESTAMP TIMESTAMP(6) not null,\n"
    "  PRIMARY KEY (CHANNEL_NAME, WORKER_ID) USING HASH,\n"
    "  KEY (THREAD_ID) USING HASH,\n"
    "  LAST_APPLIED_TRANSACTION CHAR(90),\n"
    "  LAST_APPLIED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                     not null,\n"
    "  LAST_APPLIED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                      not null,\n"
    "  LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP TIMESTAMP(6)\n"
    "                                                 not null,\n"
    "  LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP TIMESTAMP(6)\n"
    "                                               not null,\n"
    "  APPLYING_TRANSACTION CHAR(90),\n"
    "  APPLYING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                 not null,\n"
    "  APPLYING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
    "                                                  not null,\n"
    "  APPLYING_TRANSACTION_START_APPLY_TIMESTAMP TIMESTAMP(6)\n"
    "                                             not null,\n"
    "  LAST_APPLIED_TRANSACTION_RETRIES_COUNT BIGINT UNSIGNED not null,\n"
    "  LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER INTEGER not null,\n"
    "  LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE VARCHAR(1024),\n"
    "  LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP TIMESTAMP(6)\n"
    "                                                          not null,\n"
    "  APPLYING_TRANSACTION_RETRIES_COUNT BIGINT UNSIGNED not null,\n"
    "  APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER INTEGER not null,\n"
    "  APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE VARCHAR(1024),\n"
    "  APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP TIMESTAMP(6)\n"
    "                                                        not null\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_replication_applier_status_by_worker::m_share = {
    &pfs_readonly_acl,
    table_replication_applier_status_by_worker::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_replication_applier_status_by_worker::get_row_count, /*records*/
    sizeof(pos_t),                                             /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_rpl_applier_status_by_worker_by_channel::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_worker row;

    /* Mutex locks not necessary for channel name. */
    row.channel_name_length =
        mi->get_channel() ? (uint)strlen(mi->get_channel()) : 0;
    memcpy(row.channel_name, mi->get_channel(), row.channel_name_length);

    if (!m_key_1.match_not_null(row.channel_name, row.channel_name_length)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match_not_null(0)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_rpl_applier_status_by_worker_by_channel::match(
    Master_info *mi, Slave_worker *worker) {
  if (m_fields >= 1) {
    st_row_worker row;

    /* Mutex locks not necessary for channel name. */
    row.channel_name_length =
        mi->get_channel() ? (uint)strlen(mi->get_channel()) : 0;
    memcpy(row.channel_name, mi->get_channel(), row.channel_name_length);

    if (!m_key_1.match_not_null(row.channel_name, row.channel_name_length)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match_not_null(worker->get_internal_id())) {
      return false;
    }
  }

  return true;
}

bool PFS_index_rpl_applier_status_by_worker_by_thread::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_worker row;
    /* NULL THREAD_ID is represented by 0 */
    row.thread_id = 0;

#ifdef HAVE_PSI_THREAD_INTERFACE
    mysql_mutex_assert_owner(&mi->rli->data_lock);

    if (mi->rli->slave_running) {
      /* STS will use SQL thread as workers on this table */
      if (mi->rli->get_worker_count() == 0) {
        PSI_thread *psi = thd_get_psi(mi->rli->info_thd);
        if (psi != nullptr) {
          row.thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
        }
      }
    }
#endif /* HAVE_PSI_THREAD_INTERFACE */

    if (!m_key.match(row.thread_id)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_rpl_applier_status_by_worker_by_thread::match(
    Master_info *mi, Slave_worker *worker) {
  if (m_fields >= 1) {
    st_row_worker row;
    /* NULL THREAD_ID is represented by 0 */
    row.thread_id = 0;

#ifdef HAVE_PSI_THREAD_INTERFACE
    mysql_mutex_assert_owner(&mi->rli->data_lock);

    if (mi->rli->slave_running) {
      if (worker) {
        PSI_thread *psi = thd_get_psi(worker->info_thd);
        if (psi != nullptr) {
          row.thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
        }
      }
    }
#endif /* HAVE_PSI_THREAD_INTERFACE */

    if (!m_key.match(row.thread_id)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_replication_applier_status_by_worker::create(
    PFS_engine_table_share *) {
  return new table_replication_applier_status_by_worker();
}

table_replication_applier_status_by_worker::
    table_replication_applier_status_by_worker()
    : PFS_engine_table(&m_share, &m_pos), m_opened_index(nullptr) {}

table_replication_applier_status_by_worker::
    ~table_replication_applier_status_by_worker() = default;

void table_replication_applier_status_by_worker::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

ha_rows table_replication_applier_status_by_worker::get_row_count() {
  /*
    Return an estimate, number of master info's multiplied by worker threads
  */
  return channel_map.get_max_channels() * 32;
}

int table_replication_applier_status_by_worker::rnd_next() {
  Slave_worker *worker;
  Master_info *mi;
  size_t wc;

  const Rdlock_guard<Multisource_info> channel_map_guard{channel_map};

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_channels(channel_map.get_max_channels());
       m_pos.next_channel()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index_1);

    if (mi == nullptr) {
      continue;
    }

    if (!mi->host[0]) {
      continue;
    }

    if (mi->rli == nullptr) {
      continue;
    }

    // prevent worker deletion
    MUTEX_LOCK(lock, &mi->rli->data_lock);

    wc = mi->rli->get_worker_count();
    if (wc == 0) {
      /* Single Thread Slave */
      make_row(mi);
      m_next_pos.set_channel_after(&m_pos);
      return 0;
    }
    for (; m_pos.m_index_2 < wc; m_pos.next_worker()) {
      /* Multi Thread Slave */

      worker = mi->rli->get_worker(m_pos.m_index_2);
      if (worker) {
        make_row(worker);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_replication_applier_status_by_worker::rnd_pos(const void *pos) {
  int res = HA_ERR_RECORD_DELETED;

  Master_info *mi;
  size_t wc;

  set_position(pos);

  const Rdlock_guard<Multisource_info> channel_map_guard{channel_map};

  mi = channel_map.get_mi_at_pos(m_pos.m_index_1);

  if (!mi || !mi->rli || !mi->host[0]) {
    return res;
  }

  // prevent worker deletion
  MUTEX_LOCK(lock, &mi->rli->data_lock);

  wc = mi->rli->get_worker_count();

  if (wc == 0) {
    /* Single Thread Slave */
    make_row(mi);
    res = 0;
  } else {
    /* Multi Thread Slave */
    if (m_pos.m_index_2 < wc) {
      Slave_worker *worker = mi->rli->get_worker(m_pos.m_index_2);
      if (worker != nullptr) {
        make_row(worker);
        res = 0;
      }
    }
  }
  return res;
}

int table_replication_applier_status_by_worker::index_init(uint idx, bool) {
  PFS_index_rpl_applier_status_by_worker *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_rpl_applier_status_by_worker_by_channel);
      break;
    case 1:
      result = PFS_NEW(PFS_index_rpl_applier_status_by_worker_by_thread);
      break;
    default:
      assert(false);
      break;
  }
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_replication_applier_status_by_worker::index_next() {
  Slave_worker *worker;
  Master_info *mi;
  size_t wc;

  const Rdlock_guard<Multisource_info> channel_map_guard{channel_map};

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_channels(channel_map.get_max_channels());
       m_pos.next_channel()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index_1);

    if (mi == nullptr) {
      continue;
    }

    if (!mi->host[0]) {
      continue;
    }

    if (mi->rli == nullptr) {
      continue;
    }

    // prevent worker deletion

    MUTEX_LOCK(lock, &mi->rli->data_lock);

    wc = mi->rli->get_worker_count();

    for (; m_pos.m_index_2 < wc + 1; m_pos.m_index_2++) {
      if (m_pos.m_index_2 == 0) {
        /* Looking for Single Thread Slave */

        if (wc == 0) {
          if (m_opened_index->match(mi)) {
            if (!make_row(mi)) {
              m_next_pos.set_channel_after(&m_pos);
              return 0;
            }
          }
        }
      } else {
        /* Looking for Multi Thread Slave */

        if ((m_pos.m_index_2 >= 1) && (m_pos.m_index_2 <= wc)) {
          worker = mi->rli->get_worker(m_pos.m_index_2 - 1);
          if (worker) {
            if (m_opened_index->match(mi, worker)) {
              if (!make_row(worker)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
          }
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

/**
  Function to display SQL Thread's status as part of
  'replication_applier_status_by_worker' in single threaded slave mode.

   @param[in] mi Master_info
   @return 0 or HA_ERR_RECORD_DELETED
*/
int table_replication_applier_status_by_worker::make_row(Master_info *mi) {
  m_row.worker_id = 0;

  m_row.thread_id = 0;
  m_row.thread_id_is_null = true;

  assert(mi != nullptr);
  assert(mi->rli != nullptr);

  mysql_mutex_assert_owner(&mi->rli->data_lock);

  DEBUG_SYNC(current_thd,
             "rpl_pfs_replication_applier_status_by_worker_after_data_lock");

  m_row.channel_name_length = strlen(mi->get_channel());
  memcpy(m_row.channel_name, (char *)mi->get_channel(),
         m_row.channel_name_length);

#ifdef HAVE_PSI_THREAD_INTERFACE
  if (mi->rli->slave_running) {
    PSI_thread *psi = thd_get_psi(mi->rli->info_thd);
    if (psi != nullptr) {
      m_row.thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
      m_row.thread_id_is_null = false;
    }
  }
#endif /* HAVE_PSI_THREAD_INTERFACE */

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

  Trx_monitoring_info applying_trx;
  Trx_monitoring_info last_applied_trx;
  mi->rli->get_gtid_monitoring_info()->copy_info_to(&applying_trx,
                                                    &last_applied_trx);

  populate_trx_info(applying_trx, last_applied_trx);

  return 0;
}

int table_replication_applier_status_by_worker::make_row(Slave_worker *w) {
  m_row.worker_id = w->get_internal_id();

  m_row.thread_id = 0;
  m_row.thread_id_is_null = true;

  m_row.channel_name_length = strlen(w->get_channel());
  memcpy(m_row.channel_name, (char *)w->get_channel(),
         m_row.channel_name_length);

  DEBUG_SYNC(current_thd,
             "rpl_pfs_replication_applier_status_by_worker_after_data_lock");

  Trx_monitoring_info applying_trx;
  Trx_monitoring_info last_applied_trx;

  {
    MUTEX_LOCK(lock, &w->jobs_lock);

#ifdef HAVE_PSI_THREAD_INTERFACE
    if (w->running_status == Slave_worker::RUNNING) {
      PSI_thread *psi = thd_get_psi(w->info_thd);
      if (psi != nullptr) {
        m_row.thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
        m_row.thread_id_is_null = false;
      }
    }
#endif /* HAVE_PSI_THREAD_INTERFACE */

    if (w->running_status == Slave_worker::RUNNING) {
      m_row.service_state = PS_RPL_YES;
    } else {
      m_row.service_state = PS_RPL_NO;
    }

    m_row.last_error_number = (unsigned int)w->last_error().number;
    m_row.last_error_message_length = 0;
    m_row.last_error_timestamp = 0;

    /** if error, set error message and timestamp */
    if (m_row.last_error_number) {
      const char *temp_store = w->last_error().message;
      m_row.last_error_message_length = strlen(temp_store);
      memcpy(m_row.last_error_message, w->last_error().message,
             m_row.last_error_message_length);

      /** time in microsecond since epoch */
      m_row.last_error_timestamp = (ulonglong)w->last_error().skr;
    }

    w->get_gtid_monitoring_info()->copy_info_to(&applying_trx,
                                                &last_applied_trx);
  }

  populate_trx_info(applying_trx, last_applied_trx);

  return 0;
}

/**
  Auxiliary function to populate the transaction information fields.

  @param[in] applying_trx   info on the transaction being applied
  @param[in] last_applied_trx info on the last applied transaction
*/
void table_replication_applier_status_by_worker::populate_trx_info(
    Trx_monitoring_info const &applying_trx,
    Trx_monitoring_info const &last_applied_trx) {
  // The processing info is always visible
  applying_trx.copy_to_ps_table(global_tsid_map, m_row.applying_trx,
                                &m_row.applying_trx_length,
                                &m_row.applying_trx_original_commit_timestamp,
                                &m_row.applying_trx_immediate_commit_timestamp,
                                &m_row.applying_trx_start_apply_timestamp,
                                &m_row.applying_trx_last_retry_err_number,
                                m_row.applying_trx_last_retry_err_msg,
                                &m_row.applying_trx_last_retry_err_msg_length,
                                &m_row.applying_trx_last_retry_timestamp,
                                &m_row.applying_trx_retries_count);

  last_applied_trx.copy_to_ps_table(
      global_tsid_map, m_row.last_applied_trx, &m_row.last_applied_trx_length,
      &m_row.last_applied_trx_original_commit_timestamp,
      &m_row.last_applied_trx_immediate_commit_timestamp,
      &m_row.last_applied_trx_start_apply_timestamp,
      &m_row.last_applied_trx_end_apply_timestamp,
      &m_row.last_applied_trx_last_retry_err_number,
      m_row.last_applied_trx_last_retry_err_msg,
      &m_row.last_applied_trx_last_retry_err_msg_length,
      &m_row.last_applied_trx_last_retry_timestamp,
      &m_row.last_applied_trx_retries_count);
}

int table_replication_applier_status_by_worker::read_row_values(
    TABLE *table, unsigned char *buf, Field **fields, bool read_all) {
  Field *f;

  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /** channel_name */
          set_field_char_utf8mb4(f, m_row.channel_name,
                                 m_row.channel_name_length);
          break;
        case 1: /*worker_id*/
          set_field_ulonglong(f, m_row.worker_id);
          break;
        case 2: /*thread_id*/
          if (m_row.thread_id_is_null) {
            f->set_null();
          } else {
            set_field_ulonglong(f, m_row.thread_id);
          }
          break;
        case 3: /*service_state*/
          set_field_enum(f, m_row.service_state);
          break;
        case 4: /*last_error_number*/
          set_field_ulong(f, m_row.last_error_number);
          break;
        case 5: /*last_error_message*/
          set_field_varchar_utf8mb4(f, m_row.last_error_message,
                                    m_row.last_error_message_length);
          break;
        case 6: /*last_error_timestamp*/
          set_field_timestamp(f, m_row.last_error_timestamp);
          break;
        case 7: /*last_applied_trx*/
          set_field_char_utf8mb4(f, m_row.last_applied_trx,
                                 m_row.last_applied_trx_length);
          break;
        case 8: /*last_applied_trx_original_commit_timestamp*/
          set_field_timestamp(f,
                              m_row.last_applied_trx_original_commit_timestamp);
          break;
        case 9: /*last_applied_trx_immediate_commit_timestamp*/
          set_field_timestamp(
              f, m_row.last_applied_trx_immediate_commit_timestamp);
          break;
        case 10: /*last_applied_trx_start_apply_timestamp*/
          set_field_timestamp(f, m_row.last_applied_trx_start_apply_timestamp);
          break;
        case 11: /*last_applied_trx_end_apply_timestamp*/
          set_field_timestamp(f, m_row.last_applied_trx_end_apply_timestamp);
          break;
        case 12: /*applying_trx*/
          set_field_char_utf8mb4(f, m_row.applying_trx,
                                 m_row.applying_trx_length);
          break;
        case 13: /*applying_trx_original_commit_timestamp*/
          set_field_timestamp(f, m_row.applying_trx_original_commit_timestamp);
          break;
        case 14: /*applying_trx_immediate_commit_timestamp*/
          set_field_timestamp(f, m_row.applying_trx_immediate_commit_timestamp);
          break;
        case 15: /*applying_trx_start_apply_timestamp*/
          set_field_timestamp(f, m_row.applying_trx_start_apply_timestamp);
          break;
        case 16: /*last_applied_trx_retries_count*/
          set_field_ulonglong(f, m_row.last_applied_trx_retries_count);
          break;
        case 17: /*last_applied_trx_last_trans_errno*/
          set_field_ulong(f, m_row.last_applied_trx_last_retry_err_number);
          break;
        case 18: /*last_applied_trx_last_retry_err_msg*/
          set_field_varchar_utf8mb4(
              f, m_row.last_applied_trx_last_retry_err_msg,
              m_row.last_applied_trx_last_retry_err_msg_length);
          break;
        case 19: /*last_applied_trx_last_retry_timestamp*/
          set_field_timestamp(f, m_row.last_applied_trx_last_retry_timestamp);
          break;
        case 20: /*applying_trx_retries_count*/
          set_field_ulonglong(f, m_row.applying_trx_retries_count);
          break;
        case 21: /*applying_trx_last_trans_errno*/
          set_field_ulong(f, m_row.applying_trx_last_retry_err_number);
          break;
        case 22: /*applying_trx_last_retry_err_msg*/
          set_field_varchar_utf8mb4(
              f, m_row.applying_trx_last_retry_err_msg,
              m_row.applying_trx_last_retry_err_msg_length);
          break;
        case 23: /*applying_trx_last_retry_timestamp*/
          set_field_timestamp(f, m_row.applying_trx_last_retry_timestamp);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
