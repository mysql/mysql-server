/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_esms_by_program.cc
  Table EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM (implementation).
*/

#include "storage/perfschema/table_esms_by_program.h"

#include <assert.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_program.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_esms_by_program::m_table_lock;

Plugin_table table_esms_by_program::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_statements_summary_by_program",
    /* Definition */
    "  OBJECT_TYPE enum('EVENT', 'FUNCTION', 'PROCEDURE', 'TABLE',\n"
    "                   'TRIGGER'),\n"
    "  OBJECT_SCHEMA VARCHAR(64) NOT NULL,\n"
    "  OBJECT_NAME VARCHAR(64) NOT NULL,\n"
    "  COUNT_STAR bigint(20) unsigned NOT NULL,\n"
    "  SUM_TIMER_WAIT bigint(20) unsigned NOT NULL,\n"
    "  MIN_TIMER_WAIT bigint(20) unsigned NOT NULL,\n"
    "  AVG_TIMER_WAIT bigint(20) unsigned NOT NULL,\n"
    "  MAX_TIMER_WAIT bigint(20) unsigned NOT NULL,\n"
    "  COUNT_STATEMENTS bigint(20) unsigned NOT NULL,\n"
    "  SUM_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,\n"
    "  MIN_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,\n"
    "  AVG_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,\n"
    "  MAX_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,\n"
    "  SUM_LOCK_TIME bigint(20) unsigned NOT NULL,\n"
    "  SUM_ERRORS bigint(20) unsigned NOT NULL,\n"
    "  SUM_WARNINGS bigint(20) unsigned NOT NULL,\n"
    "  SUM_ROWS_AFFECTED bigint(20) unsigned NOT NULL,\n"
    "  SUM_ROWS_SENT bigint(20) unsigned NOT NULL,\n"
    "  SUM_ROWS_EXAMINED bigint(20) UNSIGNED NOT NULL,\n"
    "  SUM_CREATED_TMP_DISK_TABLES bigint(20) unsigned NOT NULL,\n"
    "  SUM_CREATED_TMP_TABLES bigint(20) unsigned NOT NULL,\n"
    "  SUM_SELECT_FULL_JOIN bigint(20) unsigned NOT NULL,\n"
    "  SUM_SELECT_FULL_RANGE_JOIN bigint(20) unsigned NOT NULL,\n"
    "  SUM_SELECT_RANGE bigint(20) unsigned NOT NULL,\n"
    "  SUM_SELECT_RANGE_CHECK bigint(20) unsigned NOT NULL,\n"
    "  SUM_SELECT_SCAN bigint(20) unsigned NOT NULL,\n"
    "  SUM_SORT_MERGE_PASSES bigint(20) unsigned NOT NULL,\n"
    "  SUM_SORT_RANGE bigint(20) unsigned NOT NULL,\n"
    "  SUM_SORT_ROWS bigint(20) unsigned NOT NULL,\n"
    "  SUM_SORT_SCAN bigint(20) unsigned NOT NULL,\n"
    "  SUM_NO_INDEX_USED bigint(20) unsigned NOT NULL,\n"
    "  SUM_NO_GOOD_INDEX_USED bigint(20) unsigned NOT NULL,\n"
    "  SUM_CPU_TIME BIGINT unsigned not null,\n"
    "  MAX_CONTROLLED_MEMORY BIGINT unsigned not null,\n"
    "  MAX_TOTAL_MEMORY BIGINT unsigned not null,\n"
    "  COUNT_SECONDARY bigint(20) unsigned NOT NULL,\n"
    "  PRIMARY KEY (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_esms_by_program::m_share = {
    &pfs_truncatable_acl,
    table_esms_by_program::create,
    nullptr, /* write_row */
    table_esms_by_program::delete_all_rows,
    table_esms_by_program::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_esms_by_program::match(PFS_program *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs->m_key.m_type)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }

  if (m_fields >= 3) {
    if (!m_key_3.match(pfs)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_esms_by_program::create(PFS_engine_table_share *) {
  return new table_esms_by_program();
}

int table_esms_by_program::delete_all_rows() {
  reset_esms_by_program();
  return 0;
}

ha_rows table_esms_by_program::get_row_count() {
  return global_program_container.get_row_count();
}

table_esms_by_program::table_esms_by_program()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {
  m_normalizer = time_normalizer::get_statement();
}

void table_esms_by_program::reset_position() {
  m_pos = 0;
  m_next_pos = 0;
}

int table_esms_by_program::rnd_next() {
  PFS_program *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_program_iterator it = global_program_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_esms_by_program::rnd_pos(const void *pos) {
  PFS_program *pfs;

  set_position(pos);

  pfs = global_program_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_esms_by_program::index_init(uint idx [[maybe_unused]], bool) {
  PFS_index_esms_by_program *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_esms_by_program);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_esms_by_program::index_next() {
  PFS_program *pfs;
  bool has_more_program = true;

  for (m_pos.set_at(&m_next_pos); has_more_program; m_pos.next()) {
    pfs = global_program_container.get(m_pos.m_index, &has_more_program);

    if (pfs != nullptr) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_esms_by_program::make_row(PFS_program *program) {
  pfs_optimistic_state lock;

  program->m_lock.begin_optimistic_lock(&lock);

  m_row.m_object_type = program->m_key.m_type;
  m_row.m_schema_name = program->m_key.m_schema_name;
  m_row.m_object_name = program->m_key.m_object_name;

  /* Get stored program's over all stats. */
  m_row.m_sp_stat.set(m_normalizer, &program->m_sp_stat);
  /* Get sub statements' stats. */
  m_row.m_stmt_stat.set(m_normalizer, &program->m_stmt_stat);

  if (!program->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_esms_by_program::read_row_values(TABLE *table, unsigned char *buf,
                                           Field **fields, bool read_all) {
  Field *f;

  /*
    Set the null bits. It indicates how many fields could be null
    in the table.
  */
  assert(table->s->null_bytes == 0);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* OBJECT_TYPE */
          if (m_row.m_object_type != 0) {
            set_field_enum(f, m_row.m_object_type);
          } else {
            f->set_null();
          }
          break;
        case 1: /* OBJECT_SCHEMA */
          set_field_schema_name(f, &m_row.m_schema_name);
          break;
        case 2: /* OBJECT_NAME */
          set_field_routine_name(f, &m_row.m_object_name);
          break;
        case 3: /* COUNT_STAR */
        case 4: /* SUM_TIMER_WAIT */
        case 5: /* MIN_TIMER_WAIT */
        case 6: /* AVG_TIMER_WAIT */
        case 7: /* MAX_TIMER_WAIT */
          m_row.m_sp_stat.set_field(f->field_index() - 3, f);
          break;
        default: /* 8, ... COUNT/SUM/MIN/AVG/MAX */
          m_row.m_stmt_stat.set_field(f->field_index() - 8, f);
          break;
      }
    }
  }

  return 0;
}
