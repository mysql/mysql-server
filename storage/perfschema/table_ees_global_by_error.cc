/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_ees_global_by_error.cc
  Table EVENTS_ERRORS_SUMMARY_GLOBAL_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_ees_global_by_error.h"

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
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_ees_global_by_error::m_table_lock;

Plugin_table table_ees_global_by_error::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_errors_summary_global_by_error",
    /* Definition */
    "  ERROR_NUMBER INTEGER,\n"
    "  ERROR_NAME VARCHAR(64),\n"
    "  SQL_STATE VARCHAR(5),\n"
    "  SUM_ERROR_RAISED  BIGINT unsigned not null,\n"
    "  SUM_ERROR_HANDLED BIGINT unsigned not null,\n"
    "  FIRST_SEEN TIMESTAMP(0) null default 0,\n"
    "  LAST_SEEN TIMESTAMP(0) null default 0,\n"
    "  UNIQUE KEY (ERROR_NUMBER) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_ees_global_by_error::m_share = {
    &pfs_truncatable_acl,
    table_ees_global_by_error::create,
    NULL, /* write_row */
    table_ees_global_by_error::delete_all_rows,
    table_ees_global_by_error::get_row_count,
    sizeof(pos_ees_global_by_error),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_ees_global_by_error::match_error_index(uint error_index) {
  if (m_fields >= 1) {
    if (!m_key.match_error_index(error_index)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_ees_global_by_error::create(PFS_engine_table_share *) {
  return new table_ees_global_by_error();
}

int table_ees_global_by_error::delete_all_rows(void) {
  reset_events_errors_by_thread();
  reset_events_errors_by_account();
  reset_events_errors_by_user();
  reset_events_errors_by_host();
  reset_events_errors_global();
  return 0;
}

ha_rows table_ees_global_by_error::get_row_count(void) {
  return error_class_max * max_server_errors;
}

table_ees_global_by_error::table_ees_global_by_error()
    : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos() {}

void table_ees_global_by_error::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_ees_global_by_error::rnd_init(bool) { return 0; }

int table_ees_global_by_error::rnd_next(void) {
  m_pos.set_at(&m_next_pos);

  for (; m_pos.has_more_error(); m_pos.next_error()) {
    if (!make_row(m_pos.m_index)) {
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_ees_global_by_error::rnd_pos(const void *pos) {
  set_position(pos);

  for (; m_pos.has_more_error(); m_pos.next_error()) {
    if (!make_row(m_pos.m_index)) {
      return 0;
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_ees_global_by_error::index_init(uint idx MY_ATTRIBUTE((unused)),
                                          bool) {
  PFS_index_ees_global_by_error *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_ees_global_by_error);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_ees_global_by_error::index_next(void) {
  m_pos.set_at(&m_next_pos);

  for (; m_pos.has_more_error(); m_pos.next_error()) {
    if (m_opened_index->match_error_index(m_pos.m_index)) {
      if (!make_row(m_pos.m_index)) {
        m_next_pos.set_after(&m_pos);
        return 0;
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_ees_global_by_error::make_row(int error_index) {
  PFS_error_class *klass = &global_error_class;

  PFS_connection_error_visitor visitor(klass, error_index);
  PFS_connection_iterator::visit_global(true,  /* hosts */
                                        false, /* users */
                                        true,  /* accounts */
                                        true,  /* threads */
                                        false, /* THDs */
                                        &visitor);

  m_row.m_stat.set(&visitor.m_stat, error_index);

  return 0;
}

int table_ees_global_by_error::read_row_values(TABLE *table, unsigned char *buf,
                                               Field **fields, bool read_all) {
  Field *f;
  server_error *temp_error = NULL;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  if (m_row.m_stat.m_error_index > 0 &&
      m_row.m_stat.m_error_index < PFS_MAX_SERVER_ERRORS)
    temp_error =
        &error_names_array[pfs_to_server_error_map[m_row.m_stat.m_error_index]];

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* ERROR NUMBER */
        case 1: /* ERROR NAME */
        case 2: /* SQLSTATE */
        case 3: /* SUM_ERROR_RAISED */
        case 4: /* SUM_ERROR_HANDLED */
        case 5: /* FIRST_SEEN */
        case 6: /* LAST_SEEN */
          /** ERROR STATS */
          m_row.m_stat.set_field(f->field_index, f, temp_error);
          break;
        default:
          /** We should never reach here */
          DBUG_ASSERT(0);
          break;
      }
    }
  }

  return 0;
}
