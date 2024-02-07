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
  @file storage/perfschema/table_performance_timers.cc
  Table PERFORMANCE_TIMERS (implementation).
*/

#include "storage/perfschema/table_performance_timers.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/table_helper.h"

THR_LOCK table_performance_timers::m_table_lock;

Plugin_table table_performance_timers::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "performance_timers",
    /* Definition */
    "  TIMER_NAME ENUM ("
    "    'CYCLE', 'NANOSECOND', 'MICROSECOND', 'MILLISECOND', 'THREAD_CPU') "
    "    NOT NULL,\n"
    "  TIMER_FREQUENCY BIGINT,\n"
    "  TIMER_RESOLUTION BIGINT,\n"
    "  TIMER_OVERHEAD BIGINT\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_performance_timers::m_share = {
    &pfs_readonly_acl,
    table_performance_timers::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_performance_timers::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_performance_timers::create(PFS_engine_table_share *) {
  return new table_performance_timers();
}

ha_rows table_performance_timers::get_row_count() { return COUNT_TIMER_NAME; }

table_performance_timers::table_performance_timers()
    : PFS_engine_table(&m_share, &m_pos),
      m_row(nullptr),
      m_pos(0),
      m_next_pos(0) {
  int index;

  index = (int)TIMER_NAME_CYCLE - FIRST_TIMER_NAME;
  m_data[index].m_timer_name = TIMER_NAME_CYCLE;
  m_data[index].m_info = pfs_timer_info.cycles;

  index = (int)TIMER_NAME_NANOSEC - FIRST_TIMER_NAME;
  m_data[index].m_timer_name = TIMER_NAME_NANOSEC;
  m_data[index].m_info = pfs_timer_info.nanoseconds;

  index = (int)TIMER_NAME_MICROSEC - FIRST_TIMER_NAME;
  m_data[index].m_timer_name = TIMER_NAME_MICROSEC;
  m_data[index].m_info = pfs_timer_info.microseconds;

  index = (int)TIMER_NAME_MILLISEC - FIRST_TIMER_NAME;
  m_data[index].m_timer_name = TIMER_NAME_MILLISEC;
  m_data[index].m_info = pfs_timer_info.milliseconds;

  index = (int)TIMER_NAME_THREAD_CPU - FIRST_TIMER_NAME;
  m_data[index].m_timer_name = TIMER_NAME_THREAD_CPU;
  m_data[index].m_info = pfs_timer_info.thread_cpu;
}

void table_performance_timers::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_performance_timers::rnd_next() {
  int result;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < COUNT_TIMER_NAME) {
    m_row = &m_data[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    result = 0;
  } else {
    m_row = nullptr;
    result = HA_ERR_END_OF_FILE;
  }

  return result;
}

int table_performance_timers::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < COUNT_TIMER_NAME);
  m_row = &m_data[m_pos.m_index];
  return 0;
}

int table_performance_timers::read_row_values(TABLE *table, unsigned char *buf,
                                              Field **fields, bool read_all) {
  Field *f;

  assert(m_row);

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* TIMER_NAME */
          set_field_enum(f, m_row->m_timer_name);
          break;
        case 1: /* TIMER_FREQUENCY */
          if (m_row->m_info.routine != 0) {
            set_field_ulonglong(f, m_row->m_info.frequency);
          } else {
            f->set_null();
          }
          break;
        case 2: /* TIMER_RESOLUTION */
          if (m_row->m_info.routine != 0) {
            set_field_ulonglong(f, m_row->m_info.resolution);
          } else {
            f->set_null();
          }
          break;
        case 3: /* TIMER_OVERHEAD */
          if (m_row->m_info.routine != 0) {
            set_field_ulonglong(f, m_row->m_info.overhead);
          } else {
            f->set_null();
          }
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
