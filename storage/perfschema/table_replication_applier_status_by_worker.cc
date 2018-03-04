/*
  Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_applier_status_by_worker.cc
  Table replication_applier_status_by_worker (implementation).
*/

#include "storage/perfschema/table_replication_applier_status_by_worker.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /*Multi source replication */
#include "sql/rpl_rli.h"
#include "sql/rpl_rli_pdb.h"
#include "sql/rpl_slave.h"
#include "sql/sql_parse.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_replication_applier_status_by_worker::m_table_lock;

Plugin_table table_replication_applier_status_by_worker::m_table_def(
  /* Schema name */
  "performance_schema",
  /* Name */
  "replication_applier_status_by_worker",
  /* Definition */
  "  CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,\n"
  "  WORKER_ID BIGINT UNSIGNED not null,\n"
  "  THREAD_ID BIGINT UNSIGNED,\n"
  "  SERVICE_STATE ENUM('ON','OFF') not null,\n"
  "  LAST_ERROR_NUMBER INTEGER not null,\n"
  "  LAST_ERROR_MESSAGE VARCHAR(1024) not null,\n"
  "  LAST_ERROR_TIMESTAMP TIMESTAMP(6) not null,\n"
  "  PRIMARY KEY (CHANNEL_NAME, WORKER_ID) USING HASH,\n"
  "  KEY (THREAD_ID) USING HASH,\n"
  "  LAST_APPLIED_TRANSACTION CHAR(57),\n"
  "  LAST_APPLIED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
  "                                                     not null,\n"
  "  LAST_APPLIED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
  "                                                      not null,\n"
  "  LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP TIMESTAMP(6)\n"
  "                                                 not null,\n"
  "  LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP TIMESTAMP(6)\n"
  "                                               not null,\n"
  "  APPLYING_TRANSACTION CHAR(57),\n"
  "  APPLYING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
  "                                                 not null,\n"
  "  APPLYING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP TIMESTAMP(6)\n"
  "                                                  not null,\n"
  "  APPLYING_TRANSACTION_START_APPLY_TIMESTAMP TIMESTAMP(6)\n"
  "                                             not null\n",
  /* Options */
  " ENGINE=PERFORMANCE_SCHEMA",
  /* Tablespace */
  nullptr);

PFS_engine_table_share table_replication_applier_status_by_worker::m_share = {
  &pfs_readonly_acl,
  table_replication_applier_status_by_worker::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_applier_status_by_worker::get_row_count, /*records*/
  sizeof(PFS_simple_index),                                  /* ref length */
  &m_table_lock,
  &m_table_def,
  true, /* perpetual */
  PFS_engine_table_proxy(),
  {0},
  false /* m_in_purgatory */
};

bool
PFS_index_rpl_applier_status_by_worker_by_channel::match(Master_info *mi)
{
  if (m_fields >= 1)
  {
    st_row_worker row;

    /* Mutex locks not necessary for channel name. */
    row.channel_name_length =
      mi->get_channel() ? (uint)strlen(mi->get_channel()) : 0;
    memcpy(row.channel_name, mi->get_channel(), row.channel_name_length);

    if (!m_key.match(row.channel_name, row.channel_name_length))
    {
      return false;
    }
  }

  return true;
}

bool
PFS_index_rpl_applier_status_by_worker_by_thread::match(Master_info *mi)
{
  if (m_fields >= 1)
  {
    st_row_worker row;
    row.thread_id_is_null = true;

    mysql_mutex_lock(&mi->rli->data_lock);

    if (mi->rli->slave_running)
    {
      /* STS will use SQL thread as workers on this table */
      if (mi->rli->get_worker_count() == 0)
      {
        PSI_thread *psi = thd_get_psi(mi->rli->info_thd);
        PFS_thread *pfs = reinterpret_cast<PFS_thread *>(psi);
        if (pfs)
        {
          row.thread_id = pfs->m_thread_internal_id;
          row.thread_id_is_null = false;
        }
      }
      /* MTS will have to check each channel worker for a match */
      else
      {
        for (int index = mi->rli->get_worker_count() - 1; index >= 0; index--)
        {
          Slave_worker *worker = mi->rli->get_worker(index);
          if (worker)
          {
            PSI_thread *psi = thd_get_psi(worker->info_thd);
            PFS_thread *pfs = reinterpret_cast<PFS_thread *>(psi);
            if (pfs && m_key.match(pfs->m_thread_internal_id))
            {
              row.thread_id = pfs->m_thread_internal_id;
              row.thread_id_is_null = false;
              break;
            }
          }
        }
      }
    }

    mysql_mutex_unlock(&mi->rli->data_lock);

    if (row.thread_id_is_null)
    {
      return false;
    }

    if (!m_key.match(row.thread_id))
    {
      return false;
    }
  }

  return true;
}

PFS_engine_table *
table_replication_applier_status_by_worker::create(PFS_engine_table_share *)
{
  return new table_replication_applier_status_by_worker();
}

table_replication_applier_status_by_worker::
  table_replication_applier_status_by_worker()
  : PFS_engine_table(&m_share, &m_pos),
    m_pos(),
    m_next_pos(),
    m_applier_pos(0),
    m_applier_next_pos(0)
{
}

table_replication_applier_status_by_worker::
  ~table_replication_applier_status_by_worker()
{
}

void
table_replication_applier_status_by_worker::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
  m_applier_pos.m_index = 0;
  m_applier_next_pos.m_index = 0;
}

ha_rows
table_replication_applier_status_by_worker::get_row_count()
{
  /*
    Return an estimate, number of master info's multipled by worker threads
  */
  return channel_map.get_max_channels() * 32;
}

int
table_replication_applier_status_by_worker::rnd_next(void)
{
  int res = HA_ERR_END_OF_FILE;

  Slave_worker *worker;
  Master_info *mi;

  channel_map.rdlock();

  /*
    For each SQL Thread in all channels get the respective Master_info and
    construct a row to display its status in
    'replication_applier_status_by_worker' table in case of single threaded
    slave mode.
  */
  for (m_applier_pos.set_at(&m_applier_next_pos);
       m_applier_pos.m_index < channel_map.get_max_channels();
       m_applier_pos.next())
  {
    mi = channel_map.get_mi_at_pos(m_applier_pos.m_index);

    if (mi && mi->host[0] && mi->rli && mi->rli->get_worker_count() == 0)
    {
      make_row(mi);
      m_applier_next_pos.set_after(&m_applier_pos);

      channel_map.unlock();
      return 0;
    }
  }

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_channels(channel_map.get_max_channels()) && res != 0;
       m_pos.next_channel())
  {
    mi = channel_map.get_mi_at_pos(m_pos.m_index_1);

    if (mi && mi->host[0])
    {
      worker = mi->rli->get_worker(m_pos.m_index_2);
      if (worker)
      {
        res = make_row(worker);
        m_next_pos.set_after(&m_pos);
      }
    }
  }

  channel_map.unlock();

  return res;
}

int
table_replication_applier_status_by_worker::rnd_pos(
  const void *pos MY_ATTRIBUTE((unused)))
{
  int res = HA_ERR_RECORD_DELETED;

  Slave_worker *worker;
  Master_info *mi;

  set_position(pos);

  channel_map.rdlock();

  mi = channel_map.get_mi_at_pos(m_pos.m_index_1);

  if (!mi || !mi->rli || !mi->host[0])
  {
    goto end;
  }

  DBUG_ASSERT(m_pos.m_index_1 < mi->rli->get_worker_count());
  /*
    Display SQL Thread's status only in the case of single threaded mode.
  */
  if (mi->rli->get_worker_count() == 0)
  {
    res = make_row(mi);
    goto end;
  }
  worker = mi->rli->get_worker(m_pos.m_index_2);

  if (worker != NULL)
  {
    res = make_row(worker);
  }

end:
  channel_map.unlock();

  return res;
}

int
table_replication_applier_status_by_worker::index_init(
  uint idx MY_ATTRIBUTE((unused)), bool)
{
  PFS_index_rpl_applier_status_by_worker *result = NULL;

  switch (idx)
  {
  case 0:
    result = PFS_NEW(PFS_index_rpl_applier_status_by_worker_by_channel);
    break;
  case 1:
    result = PFS_NEW(PFS_index_rpl_applier_status_by_worker_by_thread);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_replication_applier_status_by_worker::index_next(void)
{
  int res = HA_ERR_END_OF_FILE;

  Slave_worker *worker;
  Master_info *mi;

  channel_map.rdlock();

  /*
    For each SQL Thread in all channels get the respective Master_info and
    construct a row to display its status in
    'replication_applier_status_by_worker' table in case of single threaded
    slave mode.
  */
  for (m_applier_pos.set_at(&m_applier_next_pos);
       m_applier_pos.m_index < channel_map.get_max_channels();
       m_applier_pos.next())
  {
    mi = channel_map.get_mi_at_pos(m_applier_pos.m_index);

    if (mi && mi->host[0] && mi->rli && mi->rli->get_worker_count() == 0)
    {
      if (m_opened_index->match(mi))
      {
        make_row(mi);
        m_applier_next_pos.set_after(&m_applier_pos);

        channel_map.unlock();
        return 0;
      }
    }
  }

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_channels(channel_map.get_max_channels()) && res != 0;
       m_pos.next_channel())
  {
    mi = channel_map.get_mi_at_pos(m_pos.m_index_1);

    if (mi && mi->host[0])
    {
      if (m_opened_index->match(mi))
      {
        worker = mi->rli->get_worker(m_pos.m_index_2);
        if (worker)
        {
          make_row(worker);
          m_next_pos.set_after(&m_pos);
          res = 0;
        }
      }
    }
  }

  channel_map.unlock();

  return res;
}

/**
  Function to display SQL Thread's status as part of
  'replication_applier_status_by_worker' in single threaded slave mode.

   @param[in] mi Master_info
   @return 0 or HA_ERR_RECORD_DELETED
*/
int
table_replication_applier_status_by_worker::make_row(Master_info *mi)
{
  m_row.worker_id = 0;

  m_row.thread_id = 0;

  DBUG_ASSERT(mi != NULL);
  DBUG_ASSERT(mi->rli != NULL);

  mysql_mutex_lock(&mi->rli->data_lock);

  m_row.channel_name_length = strlen(mi->get_channel());
  memcpy(
    m_row.channel_name, (char *)mi->get_channel(), m_row.channel_name_length);

  if (mi->rli->slave_running)
  {
    PSI_thread *psi = thd_get_psi(mi->rli->info_thd);
    PFS_thread *pfs = reinterpret_cast<PFS_thread *>(psi);
    if (pfs)
    {
      m_row.thread_id = pfs->m_thread_internal_id;
      m_row.thread_id_is_null = false;
    }
    else
    {
      m_row.thread_id_is_null = true;
    }
  }
  else
  {
    m_row.thread_id_is_null = true;
  }

  if (mi->rli->slave_running)
  {
    m_row.service_state = PS_RPL_YES;
  }
  else
  {
    m_row.service_state = PS_RPL_NO;
  }

  mysql_mutex_lock(&mi->rli->err_lock);
  m_row.last_error_number = (long int)mi->rli->last_error().number;
  m_row.last_error_message_length = 0;
  m_row.last_error_timestamp = 0;

  /** if error, set error message and timestamp */
  if (m_row.last_error_number)
  {
    char *temp_store = (char *)mi->rli->last_error().message;
    m_row.last_error_message_length = strlen(temp_store);
    memcpy(
      m_row.last_error_message, temp_store, m_row.last_error_message_length);

    /** time in microsecond since epoch */
    m_row.last_error_timestamp = (ulonglong)mi->rli->last_error().skr;
  }

  mysql_mutex_unlock(&mi->rli->err_lock);

  /** The mi->rli->data_lock will be unlocked by populate_trx_info */
  populate_trx_info(mi->rli->get_gtid_monitoring_info(), &mi->rli->data_lock);

  return 0;
}

int
table_replication_applier_status_by_worker::make_row(Slave_worker *w)
{
  m_row.worker_id = w->get_internal_id();

  m_row.thread_id = 0;

  m_row.channel_name_length = strlen(w->get_channel());
  memcpy(
    m_row.channel_name, (char *)w->get_channel(), m_row.channel_name_length);

  mysql_mutex_lock(&w->jobs_lock);
  if (w->running_status == Slave_worker::RUNNING)
  {
    PSI_thread *psi = thd_get_psi(w->info_thd);
    PFS_thread *pfs = reinterpret_cast<PFS_thread *>(psi);
    if (pfs)
    {
      m_row.thread_id = pfs->m_thread_internal_id;
      m_row.thread_id_is_null = false;
    }
    else /* no instrumentation found */
    {
      m_row.thread_id_is_null = true;
    }
  }
  else
  {
    m_row.thread_id_is_null = true;
  }

  if (w->running_status == Slave_worker::RUNNING)
  {
    m_row.service_state = PS_RPL_YES;
  }
  else
  {
    m_row.service_state = PS_RPL_NO;
  }

  m_row.last_error_number = (unsigned int)w->last_error().number;
  m_row.last_error_message_length = 0;
  m_row.last_error_timestamp = 0;

  /** if error, set error message and timestamp */
  if (m_row.last_error_number)
  {
    char *temp_store = (char *)w->last_error().message;
    m_row.last_error_message_length = strlen(temp_store);
    memcpy(m_row.last_error_message,
           w->last_error().message,
           m_row.last_error_message_length);

    /** time in microsecond since epoch */
    m_row.last_error_timestamp = (ulonglong)w->last_error().skr;
  }

  /** The w->jobs_lock will be unlocked by populate_trx_info */
  populate_trx_info(w->get_gtid_monitoring_info(), &w->jobs_lock);

  return 0;
}

/**
  Auxiliary function to populate the transaction information fields.

  @param[in] monitoring_info   Gtid monitoring info about the transactions.
  @param[in] data_or_jobs_lock Lock to be released right after copying info.
*/
void
table_replication_applier_status_by_worker::populate_trx_info(
  Gtid_monitoring_info *monitoring_info, mysql_mutex_t *data_or_jobs_lock)
{
  Trx_monitoring_info applying_trx;
  Trx_monitoring_info last_applied_trx;

  monitoring_info->copy_info_to(&applying_trx, &last_applied_trx);

  mysql_mutex_unlock(data_or_jobs_lock);

  // The processing info is always visible
  applying_trx.copy_to_ps_table(global_sid_map,
                                m_row.applying_trx,
                                &m_row.applying_trx_length,
                                &m_row.applying_trx_original_commit_timestamp,
                                &m_row.applying_trx_immediate_commit_timestamp,
                                &m_row.applying_trx_start_apply_timestamp);

  last_applied_trx.copy_to_ps_table(
    global_sid_map,
    m_row.last_applied_trx,
    &m_row.last_applied_trx_length,
    &m_row.last_applied_trx_original_commit_timestamp,
    &m_row.last_applied_trx_immediate_commit_timestamp,
    &m_row.last_applied_trx_start_apply_timestamp,
    &m_row.last_applied_trx_end_apply_timestamp);
}

int
table_replication_applier_status_by_worker::read_row_values(
  TABLE *table MY_ATTRIBUTE((unused)),
  unsigned char *buf MY_ATTRIBUTE((unused)),
  Field **fields MY_ATTRIBUTE((unused)),
  bool read_all MY_ATTRIBUTE((unused)))
{
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /** channel_name */
        set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
        break;
      case 1: /*worker_id*/
        set_field_ulonglong(f, m_row.worker_id);
        break;
      case 2: /*thread_id*/
        if (m_row.thread_id_is_null)
        {
          f->set_null();
        }
        else
        {
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
        set_field_varchar_utf8(
          f, m_row.last_error_message, m_row.last_error_message_length);
        break;
      case 6: /*last_error_timestamp*/
        set_field_timestamp(f, m_row.last_error_timestamp);
        break;
      case 7: /*last_applied_trx*/
        set_field_char_utf8(
          f, m_row.last_applied_trx, m_row.last_applied_trx_length);
        break;
      case 8: /*last_applied_trx_original_commit_timestamp*/
        set_field_timestamp(f,
                            m_row.last_applied_trx_original_commit_timestamp);
        break;
      case 9: /*last_applied_trx_immediate_commit_timestamp*/
        set_field_timestamp(f,
                            m_row.last_applied_trx_immediate_commit_timestamp);
        break;
      case 10: /*last_applied_trx_start_apply_timestamp*/
        set_field_timestamp(f, m_row.last_applied_trx_start_apply_timestamp);
        break;
      case 11: /*last_applied_trx_end_apply_timestamp*/
        set_field_timestamp(f, m_row.last_applied_trx_end_apply_timestamp);
        break;
      case 12: /*applying_trx*/
        set_field_char_utf8(f, m_row.applying_trx, m_row.applying_trx_length);
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
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
