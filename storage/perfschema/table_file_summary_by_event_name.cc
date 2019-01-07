/* Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_file_summary_by_event_name.cc
  Table FILE_SUMMARY_BY_EVENT_NAME(implementation).
*/

#include "storage/perfschema/table_file_summary_by_event_name.h"

#include <stddef.h>

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_file_summary_by_event_name::m_table_lock;

Plugin_table table_file_summary_by_event_name::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "file_summary_by_event_name",
    /* Definition */
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  COUNT_STAR BIGINT UNSIGNED not null,\n"
    "  SUM_TIMER_WAIT BIGINT UNSIGNED not null,\n"
    "  MIN_TIMER_WAIT BIGINT UNSIGNED not null,\n"
    "  AVG_TIMER_WAIT BIGINT UNSIGNED not null,\n"
    "  MAX_TIMER_WAIT BIGINT UNSIGNED not null,\n"
    "  COUNT_READ BIGINT UNSIGNED not null,\n"
    "  SUM_TIMER_READ BIGINT UNSIGNED not null,\n"
    "  MIN_TIMER_READ BIGINT UNSIGNED not null,\n"
    "  AVG_TIMER_READ BIGINT UNSIGNED not null,\n"
    "  MAX_TIMER_READ BIGINT UNSIGNED not null,\n"
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
    "  PRIMARY KEY (EVENT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_file_summary_by_event_name::m_share = {
    &pfs_truncatable_acl,
    table_file_summary_by_event_name::create,
    NULL, /* write_row */
    table_file_summary_by_event_name::delete_all_rows,
    table_file_summary_by_event_name::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_file_summary_by_event_name::match(const PFS_file_class *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_file_summary_by_event_name::create(
    PFS_engine_table_share *) {
  return new table_file_summary_by_event_name();
}

int table_file_summary_by_event_name::delete_all_rows(void) {
  reset_file_instance_io();
  reset_file_class_io();
  return 0;
}

ha_rows table_file_summary_by_event_name::get_row_count(void) {
  return file_class_max;
}

table_file_summary_by_event_name::table_file_summary_by_event_name()
    : PFS_engine_table(&m_share, &m_pos), m_pos(1), m_next_pos(1) {
  m_normalizer = time_normalizer::get_wait();
}

void table_file_summary_by_event_name::reset_position(void) {
  m_pos.m_index = 1;
  m_next_pos.m_index = 1;
}

int table_file_summary_by_event_name::rnd_next(void) {
  PFS_file_class *file_class;

  m_pos.set_at(&m_next_pos);

  file_class = find_file_class(m_pos.m_index);
  if (file_class) {
    m_next_pos.set_after(&m_pos);
    return make_row(file_class);
  }

  return HA_ERR_END_OF_FILE;
}

int table_file_summary_by_event_name::rnd_pos(const void *pos) {
  PFS_file_class *file_class;

  set_position(pos);

  file_class = find_file_class(m_pos.m_index);
  if (file_class) {
    make_row(file_class);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

int table_file_summary_by_event_name::index_init(
    uint idx MY_ATTRIBUTE((unused)), bool) {
  PFS_index_file_summary_by_event_name *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_file_summary_by_event_name);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_file_summary_by_event_name::index_next(void) {
  PFS_file_class *file_class;

  m_pos.set_at(&m_next_pos);

  do {
    file_class = find_file_class(m_pos.m_index);
    if (file_class) {
      if (m_opened_index->match(file_class)) {
        if (!make_row(file_class)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      m_pos.next();
    }
  } while (file_class != NULL);

  return HA_ERR_END_OF_FILE;
}

/**
  Build a row.
  @param file_class            the file class the cursor is reading
  @return 0 or HA_ERR_RECORD_DELETED
*/
int table_file_summary_by_event_name::make_row(PFS_file_class *file_class) {
  m_row.m_event_name.make_row(file_class);

  PFS_instance_file_io_stat_visitor visitor;
  PFS_instance_iterator::visit_file_instances(file_class, &visitor);

  /* Collect timer and byte count stats */
  m_row.m_io_stat.set(m_normalizer, &visitor.m_file_io_stat);

  return 0;
}

int table_file_summary_by_event_name::read_row_values(TABLE *table,
                                                      unsigned char *,
                                                      Field **fields,
                                                      bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* EVENT_NAME */
          m_row.m_event_name.set_field(f);
          break;
        case 1: /* COUNT_STAR */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_count);
          break;
        case 2: /* SUM_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_sum);
          break;
        case 3: /* MIN_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_min);
          break;
        case 4: /* AVG_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_avg);
          break;
        case 5: /* MAX_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_max);
          break;

        case 6: /* COUNT_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_count);
          break;
        case 7: /* SUM_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_sum);
          break;
        case 8: /* MIN_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_min);
          break;
        case 9: /* AVG_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_avg);
          break;
        case 10: /* MAX_TIMER_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_max);
          break;
        case 11: /* SUM_NUMBER_OF_BYTES_READ */
          set_field_ulonglong(f, m_row.m_io_stat.m_read.m_bytes);
          break;

        case 12: /* COUNT_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_count);
          break;
        case 13: /* SUM_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_sum);
          break;
        case 14: /* MIN_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_min);
          break;
        case 15: /* AVG_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_avg);
          break;
        case 16: /* MAX_TIMER_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_max);
          break;
        case 17: /* SUM_NUMBER_OF_BYTES_WRITE */
          set_field_ulonglong(f, m_row.m_io_stat.m_write.m_bytes);
          break;

        case 18: /* COUNT_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_count);
          break;
        case 19: /* SUM_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_sum);
          break;
        case 20: /* MIN_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_min);
          break;
        case 21: /* AVG_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_avg);
          break;
        case 22: /* MAX_TIMER_MISC */
          set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_max);
          break;

        default:
          DBUG_ASSERT(false);
          break;
      }
    }  // if
  }    // for

  return 0;
}
