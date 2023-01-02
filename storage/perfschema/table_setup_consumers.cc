/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_setup_consumers.cc
  Table SETUP_CONSUMERS (implementation).
*/

#include "storage/perfschema/table_setup_consumers.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_digest.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_events_waits.h"
#include "storage/perfschema/pfs_instr.h"

#define COUNT_SETUP_CONSUMERS 16

static row_setup_consumers all_setup_consumers_data[COUNT_SETUP_CONSUMERS] = {
    {{STRING_WITH_LEN("events_stages_current")},
     &flag_events_stages_current,
     false,
     false},
    {{STRING_WITH_LEN("events_stages_history")},
     &flag_events_stages_history,
     false,
     true},
    {{STRING_WITH_LEN("events_stages_history_long")},
     &flag_events_stages_history_long,
     false,
     true},
    {{STRING_WITH_LEN("events_statements_cpu")},
     &flag_events_statements_cpu,
     false,
     false},
    {{STRING_WITH_LEN("events_statements_current")},
     &flag_events_statements_current,
     false,
     false},
    {{STRING_WITH_LEN("events_statements_history")},
     &flag_events_statements_history,
     false,
     true},
    {{STRING_WITH_LEN("events_statements_history_long")},
     &flag_events_statements_history_long,
     false,
     true},
    {{STRING_WITH_LEN("events_transactions_current")},
     &flag_events_transactions_current,
     false,
     false},
    {{STRING_WITH_LEN("events_transactions_history")},
     &flag_events_transactions_history,
     false,
     true},
    {{STRING_WITH_LEN("events_transactions_history_long")},
     &flag_events_transactions_history_long,
     false,
     true},
    {{STRING_WITH_LEN("events_waits_current")},
     &flag_events_waits_current,
     false,
     false},
    {{STRING_WITH_LEN("events_waits_history")},
     &flag_events_waits_history,
     false,
     true},
    {{STRING_WITH_LEN("events_waits_history_long")},
     &flag_events_waits_history_long,
     false,
     true},
    {{STRING_WITH_LEN("global_instrumentation")},
     &flag_global_instrumentation,
     true,
     true},
    {{STRING_WITH_LEN("thread_instrumentation")},
     &flag_thread_instrumentation,
     false,
     true},
    {{STRING_WITH_LEN("statements_digest")},
     &flag_statements_digest,
     false,
     false}};

THR_LOCK table_setup_consumers::m_table_lock;

Plugin_table table_setup_consumers::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "setup_consumers",
    /* Definition */
    "  NAME VARCHAR(64) not null,\n"
    "  ENABLED ENUM ('YES', 'NO') not null,\n"
    "  PRIMARY KEY (NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_setup_consumers::m_share = {
    &pfs_updatable_acl,
    table_setup_consumers::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_setup_consumers::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_setup_consumers::match(row_setup_consumers *row) {
  if (m_fields >= 1) {
    if (!m_key.match(&row->m_name)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_setup_consumers::create(PFS_engine_table_share *) {
  return new table_setup_consumers();
}

ha_rows table_setup_consumers::get_row_count() { return COUNT_SETUP_CONSUMERS; }

table_setup_consumers::table_setup_consumers()
    : PFS_engine_table(&m_share, &m_pos),
      m_row(nullptr),
      m_pos(0),
      m_next_pos(0) {}

void table_setup_consumers::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_setup_consumers::rnd_next() {
  int result;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < COUNT_SETUP_CONSUMERS) {
    m_row = &all_setup_consumers_data[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    result = 0;
  } else {
    m_row = nullptr;
    result = HA_ERR_END_OF_FILE;
  }

  return result;
}

int table_setup_consumers::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < COUNT_SETUP_CONSUMERS);
  m_row = &all_setup_consumers_data[m_pos.m_index];
  return 0;
}

int table_setup_consumers::index_init(uint idx [[maybe_unused]], bool) {
  PFS_index_setup_consumers *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_setup_consumers);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_setup_consumers::index_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < COUNT_SETUP_CONSUMERS;
       m_pos.next()) {
    m_row = &all_setup_consumers_data[m_pos.m_index];

    if (m_opened_index->match(m_row)) {
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  m_row = nullptr;
  return HA_ERR_END_OF_FILE;
}

int table_setup_consumers::read_row_values(TABLE *table, unsigned char *,
                                           Field **fields, bool read_all) {
  Field *f;

  assert(m_row);

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* NAME */
          set_field_varchar_utf8mb4(f, m_row->m_name.str, m_row->m_name.length);
          break;
        case 1: /* ENABLED */
          set_field_enum(f, (*m_row->m_enabled_ptr) ? ENUM_YES : ENUM_NO);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}

int table_setup_consumers::update_row_values(TABLE *table,
                                             const unsigned char *,
                                             unsigned char *, Field **fields) {
  Field *f;
  enum_yes_no value;

  assert(m_row);

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index())) {
      switch (f->field_index()) {
        case 1: /* ENABLED */
        {
          value = (enum_yes_no)get_field_enum(f);
          *m_row->m_enabled_ptr = (value == ENUM_YES);
          break;
        }
        default:
          return HA_ERR_WRONG_COMMAND;
      }
    }
  }

  if (m_row->m_instrument_refresh) {
    update_instruments_derived_flags();
  }

  if (m_row->m_thread_refresh) {
    update_thread_derived_flags();
  }

  return 0;
}
