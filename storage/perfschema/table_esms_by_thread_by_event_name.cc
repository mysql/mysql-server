/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_esms_by_thread_by_event_name.cc
  Table EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_esms_by_thread_by_event_name.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_esms_by_thread_by_event_name::m_table_lock;

Plugin_table table_esms_by_thread_by_event_name::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_statements_summary_by_thread_by_event_name",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  SUM_LOCK_TIME BIGINT unsigned not null,\n"
    "  SUM_ERRORS BIGINT unsigned not null,\n"
    "  SUM_WARNINGS BIGINT unsigned not null,\n"
    "  SUM_ROWS_AFFECTED BIGINT unsigned not null,\n"
    "  SUM_ROWS_SENT BIGINT unsigned not null,\n"
    "  SUM_ROWS_EXAMINED BIGINT unsigned not null,\n"
    "  SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,\n"
    "  SUM_CREATED_TMP_TABLES BIGINT unsigned not null,\n"
    "  SUM_SELECT_FULL_JOIN BIGINT unsigned not null,\n"
    "  SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,\n"
    "  SUM_SELECT_RANGE BIGINT unsigned not null,\n"
    "  SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,\n"
    "  SUM_SELECT_SCAN BIGINT unsigned not null,\n"
    "  SUM_SORT_MERGE_PASSES BIGINT unsigned not null,\n"
    "  SUM_SORT_RANGE BIGINT unsigned not null,\n"
    "  SUM_SORT_ROWS BIGINT unsigned not null,\n"
    "  SUM_SORT_SCAN BIGINT unsigned not null,\n"
    "  SUM_NO_INDEX_USED BIGINT unsigned not null,\n"
    "  SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,\n"
    "  SUM_CPU_TIME BIGINT unsigned not null,\n"
    "  MAX_CONTROLLED_MEMORY BIGINT unsigned not null,\n"
    "  MAX_TOTAL_MEMORY BIGINT unsigned not null,\n"
    "  COUNT_SECONDARY BIGINT unsigned not null,\n"
    "  PRIMARY KEY (THREAD_ID, EVENT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_esms_by_thread_by_event_name::m_share = {
    &pfs_truncatable_acl,
    table_esms_by_thread_by_event_name::create,
    nullptr, /* write_row */
    table_esms_by_thread_by_event_name::delete_all_rows,
    table_esms_by_thread_by_event_name::get_row_count,
    sizeof(pos_esms_by_thread_by_event_name),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_esms_by_thread_by_event_name::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_esms_by_thread_by_event_name::match(PFS_statement_class *klass) {
  if (klass->is_mutable()) {
    return false;
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(klass)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_esms_by_thread_by_event_name::create(
    PFS_engine_table_share *) {
  return new table_esms_by_thread_by_event_name();
}

int table_esms_by_thread_by_event_name::delete_all_rows() {
  reset_events_statements_by_thread();
  return 0;
}

ha_rows table_esms_by_thread_by_event_name::get_row_count() {
  return global_thread_container.get_row_count() * statement_class_max;
}

table_esms_by_thread_by_event_name::table_esms_by_thread_by_event_name()
    : PFS_engine_table(&m_share, &m_pos), m_opened_index(nullptr) {
  m_normalizer = time_normalizer::get_statement();
}

void table_esms_by_thread_by_event_name::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

int table_esms_by_thread_by_event_name::rnd_init(bool) { return 0; }

int table_esms_by_thread_by_event_name::rnd_next() {
  PFS_thread *thread;
  PFS_statement_class *statement_class;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (thread != nullptr) {
      statement_class = find_statement_class(m_pos.m_index_2);
      if (statement_class) {
        m_next_pos.set_after(&m_pos);
        return make_row(thread, statement_class);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_esms_by_thread_by_event_name::rnd_pos(const void *pos) {
  set_position(pos);

  PFS_thread *thread = global_thread_container.get(m_pos.m_index_1);
  if (thread != nullptr) {
    PFS_statement_class *statement_class =
        find_statement_class(m_pos.m_index_2);
    if (statement_class) {
      return make_row(thread, statement_class);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_esms_by_thread_by_event_name::index_init(uint idx [[maybe_unused]],
                                                   bool) {
  assert(idx == 0);
  m_opened_index = PFS_NEW(PFS_index_esms_by_thread_by_event_name);
  m_index = m_opened_index;
  return 0;
}

int table_esms_by_thread_by_event_name::index_next() {
  PFS_thread *thread;
  PFS_statement_class *statement_class;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (thread != nullptr) {
      if (m_opened_index->match(thread)) {
        do {
          statement_class = find_statement_class(m_pos.m_index_2);
          if (statement_class != nullptr) {
            if (m_opened_index->match(statement_class)) {
              if (!make_row(thread, statement_class)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
            m_pos.m_index_2++;
          }
        } while (statement_class != nullptr);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_esms_by_thread_by_event_name::make_row(PFS_thread *thread,
                                                 PFS_statement_class *klass) {
  pfs_optimistic_state lock;

  if (klass->is_mutable()) {
    return HA_ERR_RECORD_DELETED;
  }

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id = thread->m_thread_internal_id;

  m_row.m_event_name.make_row(klass);

  PFS_connection_statement_visitor visitor(klass);
  PFS_connection_iterator::visit_thread(thread, &visitor);

  if (!thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_esms_by_thread_by_event_name::read_row_values(TABLE *table,
                                                        unsigned char *,
                                                        Field **fields,
                                                        bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* THREAD_ID */
          set_field_ulonglong(f, m_row.m_thread_internal_id);
          break;
        case 1: /* EVENT_NAME */
          m_row.m_event_name.set_field(f);
          break;
        default: /* 2, ... COUNT/SUM/MIN/AVG/MAX */
          m_row.m_stat.set_field(f->field_index() - 2, f);
          break;
      }
    }
  }

  return 0;
}
