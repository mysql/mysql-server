/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_file_summary_by_instance.cc
  Table FILE_SUMMARY_BY_INSTANCE (implementation).
*/

#include "storage/perfschema/table_file_summary_by_instance.h"

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
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_file_summary_by_instance::m_table_lock;

Plugin_table table_file_summary_by_instance::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "file_summary_by_instance",
    /* Definition */
    "  FILE_NAME VARCHAR(512) not null,\n"
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
    "  SUM_NUMBER_OF_BYTES_READ BIGINT not null,\n"
    "  COUNT_WRITE BIGINT unsigned not null,\n"
    "  SUM_TIMER_WRITE BIGINT unsigned not null,\n"
    "  MIN_TIMER_WRITE BIGINT unsigned not null,\n"
    "  AVG_TIMER_WRITE BIGINT unsigned not null,\n"
    "  MAX_TIMER_WRITE BIGINT unsigned not null,\n"
    "  SUM_NUMBER_OF_BYTES_WRITE BIGINT not null,\n"
    "  COUNT_MISC BIGINT unsigned not null,\n"
    "  SUM_TIMER_MISC BIGINT unsigned not null,\n"
    "  MIN_TIMER_MISC BIGINT unsigned not null,\n"
    "  AVG_TIMER_MISC BIGINT unsigned not null,\n"
    "  MAX_TIMER_MISC BIGINT unsigned not null,\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  KEY (FILE_NAME) USING HASH,\n"
    "  KEY (EVENT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_file_summary_by_instance::m_share = {
    &pfs_truncatable_acl,
    table_file_summary_by_instance::create,
    nullptr, /* write_row */
    table_file_summary_by_instance::delete_all_rows,
    table_file_summary_by_instance::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_file_summary_by_instance_by_instance::match(
    const PFS_file *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_file_summary_by_instance_by_file_name::match(
    const PFS_file *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_file_summary_by_instance_by_event_name::match(
    const PFS_file *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_file_summary_by_instance::create(
    PFS_engine_table_share *) {
  return new table_file_summary_by_instance();
}

int table_file_summary_by_instance::delete_all_rows(void) {
  reset_file_instance_io();
  return 0;
}

ha_rows table_file_summary_by_instance::get_row_count(void) {
  return global_file_container.get_row_count();
}

table_file_summary_by_instance::table_file_summary_by_instance()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {
  m_normalizer = time_normalizer::get_wait();
}

void table_file_summary_by_instance::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_file_summary_by_instance::rnd_next(void) {
  PFS_file *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_file_iterator it = global_file_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_file_summary_by_instance::rnd_pos(const void *pos) {
  PFS_file *pfs;

  set_position(pos);

  pfs = global_file_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_file_summary_by_instance::index_init(uint idx, bool) {
  PFS_index_file_summary_by_instance *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_file_summary_by_instance_by_instance);
      break;
    case 1:
      result = PFS_NEW(PFS_index_file_summary_by_instance_by_file_name);
      break;
    case 2:
      result = PFS_NEW(PFS_index_file_summary_by_instance_by_event_name);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_file_summary_by_instance::index_next(void) {
  PFS_file *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_file_iterator it = global_file_container.iterate(m_pos.m_index);

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

/**
  Build a row.
  @param pfs              the file the cursor is reading
  @return 0 or HA_ERR_RECORD_DELETED
*/
int table_file_summary_by_instance::make_row(PFS_file *pfs) {
  pfs_optimistic_state lock;
  PFS_file_class *safe_class;

  /* Protect this reader against a file delete */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_file_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_file_name = pfs->m_file_name;
  m_row.m_event_name.make_row(safe_class);
  m_row.m_identity = pfs->m_identity;

  /* Collect timer and byte count stats */
  m_row.m_io_stat.set(m_normalizer, &pfs->m_file_stat.m_io_stat);

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_file_summary_by_instance::read_row_values(TABLE *table,
                                                    unsigned char *,
                                                    Field **fields,
                                                    bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* FILE_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_file_name.ptr(),
                                    m_row.m_file_name.length());
          break;
        case 1: /* EVENT_NAME */
          m_row.m_event_name.set_field(f);
          break;
        case 2: /* OBJECT_INSTANCE */
          set_field_ulonglong(f, (ulonglong)m_row.m_identity);
          break;

        case 3: /* COUNT_STAR */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_count);
          break;
        case 4: /* SUM_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_sum);
          break;
        case 5: /* MIN_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_min);
          break;
        case 6: /* AVG_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_avg);
          break;
        case 7: /* MAX_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_max);
          break;

        case 8: /* COUNT_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_count);
          break;
        case 9: /* SUM_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_sum);
          break;
        case 10: /* MIN_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_min);
          break;
        case 11: /* AVG_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_avg);
          break;
        case 12: /* MAX_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_max);
          break;
        case 13: /* SUM_NUMBER_OF_BYTES_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_bytes);
          break;

        case 14: /* COUNT_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_count);
          break;
        case 15: /* SUM_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_sum);
          break;
        case 16: /* MIN_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_min);
          break;
        case 17: /* AVG_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_avg);
          break;
        case 18: /* MAX_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_max);
          break;
        case 19: /* SUM_NUMBER_OF_BYTES_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_bytes);
          break;

        case 20: /* COUNT_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_count);
          break;
        case 21: /* SUM_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_sum);
          break;
        case 22: /* MIN_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_min);
          break;
        case 23: /* AVG_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_avg);
          break;
        case 24: /* MAX_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_max);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
