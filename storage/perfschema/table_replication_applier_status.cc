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
  @file storage/perfschema/table_replication_applier_status.cc
  Table replication_applier_status (implementation).
*/

#include "storage/perfschema/table_replication_applier_status.h"

#include <time.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/plugin_table.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /*Multi source replication */
#include "sql/rpl_rli.h"
#include "sql/rpl_slave.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_replication_applier_status::m_table_lock;

Plugin_table table_replication_applier_status::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_applier_status",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) not null,\n"
    "  SERVICE_STATE ENUM('ON','OFF') not null,\n"
    "  REMAINING_DELAY INTEGER unsigned,\n"
    "  COUNT_TRANSACTIONS_RETRIES BIGINT unsigned not null,\n"
    "  PRIMARY KEY (CHANNEL_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_replication_applier_status::m_share = {
    &pfs_readonly_acl,
    table_replication_applier_status::create,
    NULL,                                            /* write_row */
    NULL,                                            /* delete_all_rows */
    table_replication_applier_status::get_row_count, /* records */
    sizeof(pos_t),                                   /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_rpl_applier_status::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_applier_status row;

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

PFS_engine_table *table_replication_applier_status::create(
    PFS_engine_table_share *) {
  return new table_replication_applier_status();
}

table_replication_applier_status::table_replication_applier_status()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_replication_applier_status::~table_replication_applier_status() {}

void table_replication_applier_status::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_replication_applier_status::get_row_count() {
  return channel_map.get_max_channels();
}

int table_replication_applier_status::rnd_next(void) {
  int res = HA_ERR_END_OF_FILE;

  Master_info *mi;

  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels() && res != 0;
       m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    if (mi && mi->host[0]) {
      res = make_row(mi);
      m_next_pos.set_after(&m_pos);
    }
  }

  channel_map.unlock();

  return res;
}

int table_replication_applier_status::rnd_pos(const void *pos) {
  int res = HA_ERR_RECORD_DELETED;

  Master_info *mi = NULL;

  set_position(pos);

  channel_map.rdlock();

  if ((mi = channel_map.get_mi_at_pos(m_pos.m_index))) {
    res = make_row(mi);
  }

  channel_map.unlock();

  return res;
}

int table_replication_applier_status::index_init(
    uint idx MY_ATTRIBUTE((unused)), bool) {
  PFS_index_rpl_applier_status *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_rpl_applier_status);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_replication_applier_status::index_next(void) {
  int res = HA_ERR_END_OF_FILE;

  Master_info *mi;

  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels() && res != 0;
       m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    if (mi && mi->host[0]) {
      if (m_opened_index->match(mi)) {
        res = make_row(mi);
        m_next_pos.set_after(&m_pos);
      }
    }
  }

  channel_map.unlock();

  return res;
}

int table_replication_applier_status::make_row(Master_info *mi) {
  char *slave_sql_running_state = NULL;

  DBUG_ASSERT(mi != NULL);
  DBUG_ASSERT(mi->rli != NULL);

  m_row.channel_name_length =
      mi->get_channel() ? (uint)strlen(mi->get_channel()) : 0;
  memcpy(m_row.channel_name, mi->get_channel(), m_row.channel_name_length);

  mysql_mutex_lock(&mi->rli->info_thd_lock);

  slave_sql_running_state = const_cast<char *>(
      mi->rli->info_thd ? mi->rli->info_thd->get_proc_info() : "");
  mysql_mutex_unlock(&mi->rli->info_thd_lock);

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  if (mi->rli->slave_running) {
    m_row.service_state = PS_RPL_YES;
  } else {
    m_row.service_state = PS_RPL_NO;
  }

  m_row.remaining_delay = 0;
  if (slave_sql_running_state == stage_sql_thd_waiting_until_delay.m_name) {
    time_t t = my_time(0), sql_delay_end = mi->rli->get_sql_delay_end();
    m_row.remaining_delay = (uint)(t < sql_delay_end ? sql_delay_end - t : 0);
    m_row.remaining_delay_is_set = true;
  } else {
    m_row.remaining_delay_is_set = false;
  }

  m_row.count_transactions_retries = mi->rli->retried_trans;

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  return 0;
}

int table_replication_applier_status::read_row_values(TABLE *table,
                                                      unsigned char *buf,
                                                      Field **fields,
                                                      bool read_all) {
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /**channel_name*/
          set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
          break;
        case 1: /* service_state */
          set_field_enum(f, m_row.service_state);
          break;
        case 2: /* remaining_delay */
          if (m_row.remaining_delay_is_set) {
            set_field_ulong(f, m_row.remaining_delay);
          } else {
            f->set_null();
          }
          break;
        case 3: /* total number of times transactions were retried */
          set_field_ulonglong(f, m_row.count_transactions_retries);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
