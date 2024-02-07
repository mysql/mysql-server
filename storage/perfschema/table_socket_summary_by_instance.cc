/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/table_socket_summary_by_instance.cc
  Table SOCKET_SUMMARY_BY_INSTANCE (implementation).
*/

#include "storage/perfschema/table_socket_summary_by_instance.h"

#include <assert.h>
#include <stddef.h>

#include "my_compiler.h"

#include "my_inttypes.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"

THR_LOCK table_socket_summary_by_instance::m_table_lock;

Plugin_table table_socket_summary_by_instance::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "socket_summary_by_instance",
    /* Definition */
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  COUNT_READ BIGINT unsigned not null,\n"
    "  SUM_TIMER_READ BIGINT unsigned not null,\n"
    "  MIN_TIMER_READ BIGINT unsigned not null,\n"
    "  AVG_TIMER_READ BIGINT unsigned not null,\n"
    "  MAX_TIMER_READ BIGINT unsigned not null,\n"
    "  SUM_NUMBER_OF_BYTES_READ BIGINT unsigned not null,\n"
    "  COUNT_WRITE BIGINT unsigned not null,\n"
    "  SUM_TIMER_WRITE BIGINT unsigned not null,\n"
    "  MIN_TIMER_WRITE BIGINT unsigned not null,\n"
    "  AVG_TIMER_WRITE BIGINT unsigned not null,\n"
    "  MAX_TIMER_WRITE BIGINT unsigned not null,\n"
    "  SUM_NUMBER_OF_BYTES_WRITE BIGINT unsigned not null,\n"
    "  COUNT_MISC BIGINT unsigned not null,\n"
    "  SUM_TIMER_MISC BIGINT unsigned not null,\n"
    "  MIN_TIMER_MISC BIGINT unsigned not null,\n"
    "  AVG_TIMER_MISC BIGINT unsigned not null,\n"
    "  MAX_TIMER_MISC BIGINT unsigned not null,\n"
    "  PRIMARY KEY (object_instance_begin) USING HASH,\n"
    "  KEY (event_name) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_socket_summary_by_instance::m_share = {
    &pfs_truncatable_acl,
    table_socket_summary_by_instance::create,
    nullptr, /* write_row */
    table_socket_summary_by_instance::delete_all_rows,
    table_socket_summary_by_instance::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_socket_summary_by_instance_by_instance::match(
    const PFS_socket *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_socket_summary_by_instance_by_event_name::match(
    const PFS_socket *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_socket_summary_by_instance::create(
    PFS_engine_table_share *) {
  return new table_socket_summary_by_instance();
}

table_socket_summary_by_instance::table_socket_summary_by_instance()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {
  m_normalizer = time_normalizer::get_wait();
}

int table_socket_summary_by_instance::delete_all_rows() {
  reset_socket_instance_io();
  return 0;
}

ha_rows table_socket_summary_by_instance::get_row_count() {
  return global_socket_container.get_row_count();
}

void table_socket_summary_by_instance::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_socket_summary_by_instance::rnd_next() {
  PFS_socket *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_socket_iterator it = global_socket_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_socket_summary_by_instance::rnd_pos(const void *pos) {
  PFS_socket *pfs;

  set_position(pos);

  pfs = global_socket_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_socket_summary_by_instance::index_init(uint idx, bool) {
  PFS_index_socket_summary_by_instance *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_socket_summary_by_instance_by_instance);
      break;
    case 1:
      result = PFS_NEW(PFS_index_socket_summary_by_instance_by_event_name);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_socket_summary_by_instance::index_next() {
  PFS_socket *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_socket_iterator it = global_socket_container.iterate(m_pos.m_index);

  do {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != nullptr) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  } while (pfs != nullptr);

  return HA_ERR_END_OF_FILE;
}

int table_socket_summary_by_instance::make_row(PFS_socket *pfs) {
  pfs_optimistic_state lock;
  PFS_socket_class *safe_class;

  /* Protect this reader against a socket delete */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_socket_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_event_name.make_row(safe_class);
  m_row.m_identity = pfs->m_identity;

  /* Collect timer and byte count stats */
  m_row.m_io_stat.set(m_normalizer, &pfs->m_socket_stat.m_io_stat);

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_socket_summary_by_instance::read_row_values(TABLE *table,
                                                      unsigned char *,
                                                      Field **fields,
                                                      bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* EVENT_NAME */
          m_row.m_event_name.set_field(f);
          break;
        case 1: /* OBJECT_INSTANCE */
          set_field_ulonglong(f, (ulonglong)m_row.m_identity);
          break;

        case 2: /* COUNT_STAR */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_count);
          break;
        case 3: /* SUM_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_sum);
          break;
        case 4: /* MIN_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_min);
          break;
        case 5: /* AVG_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_avg);
          break;
        case 6: /* MAX_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_max);
          break;

        case 7: /* COUNT_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_count);
          break;
        case 8: /* SUM_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_sum);
          break;
        case 9: /* MIN_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_min);
          break;
        case 10: /* AVG_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_avg);
          break;
        case 11: /* MAX_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_max);
          break;
        case 12: /* SUM_NUMBER_OF_BYTES_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_bytes);
          break;

        case 13: /* COUNT_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_count);
          break;
        case 14: /* SUM_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_sum);
          break;
        case 15: /* MIN_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_min);
          break;
        case 16: /* AVG_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_avg);
          break;
        case 17: /* MAX_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_max);
          break;
        case 18: /* SUM_NUMBER_OF_BYTES_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_bytes);
          break;

        case 19: /* COUNT_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_count);
          break;
        case 20: /* SUM_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_sum);
          break;
        case 21: /* MIN_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_min);
          break;
        case 22: /* AVG_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_avg);
          break;
        case 23: /* MAX_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_max);
          break;
        default:
          assert(false);
          break;
      }
    }
  }

  return 0;
}
