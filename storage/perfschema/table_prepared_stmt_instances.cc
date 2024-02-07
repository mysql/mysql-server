/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_prepared_stmt_instances.cc
  Table PREPARED_STATEMENTS_INSTANCES (implementation).
*/

#include "storage/perfschema/table_prepared_stmt_instances.h"

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
#include "storage/perfschema/pfs_prepared_stmt.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_prepared_stmt_instances::m_table_lock;

Plugin_table table_prepared_stmt_instances::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "prepared_statements_instances",
    /* Definition */
    "  OBJECT_INSTANCE_BEGIN bigint(20) unsigned NOT NULL,\n"
    "  STATEMENT_ID BIGINT(20) unsigned NOT NULL,\n"
    "  STATEMENT_NAME varchar(64) default NULL,\n"
    "  SQL_TEXT longtext NOT NULL,\n"
    "  OWNER_THREAD_ID bigint(20) unsigned NOT NULL,\n"
    "  OWNER_EVENT_ID bigint(20) unsigned NOT NULL,\n"
    "  OWNER_OBJECT_TYPE enum('EVENT','FUNCTION','PROCEDURE','TABLE',\n"
    "                         'TRIGGER') DEFAULT NULL,\n"
    "  OWNER_OBJECT_SCHEMA varchar(64) DEFAULT NULL,\n"
    "  OWNER_OBJECT_NAME varchar(64) DEFAULT NULL,\n"
    "  EXECUTION_ENGINE ENUM ('PRIMARY', 'SECONDARY'),\n"
    "  TIMER_PREPARE bigint(20) unsigned NOT NULL,\n"
    "  COUNT_REPREPARE bigint(20) unsigned NOT NULL,\n"
    "  COUNT_EXECUTE bigint(20) unsigned NOT NULL,\n"
    "  SUM_TIMER_EXECUTE bigint(20) unsigned NOT NULL,\n"
    "  MIN_TIMER_EXECUTE bigint(20) unsigned NOT NULL,\n"
    "  AVG_TIMER_EXECUTE bigint(20) unsigned NOT NULL,\n"
    "  MAX_TIMER_EXECUTE bigint(20) unsigned NOT NULL,\n"
    "  SUM_LOCK_TIME bigint(20) unsigned NOT NULL,\n"
    "  SUM_ERRORS bigint(20) unsigned NOT NULL,\n"
    "  SUM_WARNINGS bigint(20) unsigned NOT NULL,\n"
    "  SUM_ROWS_AFFECTED bigint(20) unsigned NOT NULL,\n"
    "  SUM_ROWS_SENT bigint(20) unsigned NOT NULL,\n"
    "  SUM_ROWS_EXAMINED bigint(20) unsigned NOT NULL,\n"
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
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  UNIQUE KEY (OWNER_THREAD_ID, OWNER_EVENT_ID) USING HASH,\n"
    "  KEY (STATEMENT_ID) USING HASH,\n"
    "  KEY (STATEMENT_NAME) USING HASH,\n"
    "  KEY (OWNER_OBJECT_TYPE, OWNER_OBJECT_SCHEMA,\n"
    "       OWNER_OBJECT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_prepared_stmt_instances::m_share = {
    &pfs_truncatable_acl,
    table_prepared_stmt_instances::create,
    nullptr, /* write_row */
    table_prepared_stmt_instances::delete_all_rows,
    table_prepared_stmt_instances::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_prepared_stmt_instances_by_instance::match(
    const PFS_prepared_stmt *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_prepared_stmt_instances_by_owner_thread::match(
    const PFS_prepared_stmt *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match_owner(pfs)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match_owner(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_prepared_stmt_instances_by_statement_id::match(
    const PFS_prepared_stmt *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_prepared_stmt_instances_by_statement_name::match(
    const PFS_prepared_stmt *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_prepared_stmt_instances_by_owner_object::match(
    const PFS_prepared_stmt *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
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

PFS_engine_table *table_prepared_stmt_instances::create(
    PFS_engine_table_share *) {
  return new table_prepared_stmt_instances();
}

int table_prepared_stmt_instances::delete_all_rows() {
  reset_prepared_stmt_instances();
  return 0;
}

ha_rows table_prepared_stmt_instances::get_row_count() {
  return global_prepared_stmt_container.get_row_count();
}

table_prepared_stmt_instances::table_prepared_stmt_instances()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {
  m_normalizer = time_normalizer::get_statement();
}

void table_prepared_stmt_instances::reset_position() {
  m_pos = 0;
  m_next_pos = 0;
}

int table_prepared_stmt_instances::rnd_next() {
  PFS_prepared_stmt *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_prepared_stmt_iterator it =
      global_prepared_stmt_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_prepared_stmt_instances::rnd_pos(const void *pos) {
  PFS_prepared_stmt *pfs;

  set_position(pos);

  pfs = global_prepared_stmt_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_prepared_stmt_instances::index_init(uint idx, bool) {
  PFS_index_prepared_stmt_instances *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_prepared_stmt_instances_by_instance);
      break;
    case 1:
      result = PFS_NEW(PFS_index_prepared_stmt_instances_by_owner_thread);
      break;
    case 2:
      result = PFS_NEW(PFS_index_prepared_stmt_instances_by_statement_id);
      break;
    case 3:
      result = PFS_NEW(PFS_index_prepared_stmt_instances_by_statement_name);
      break;
    case 4:
      result = PFS_NEW(PFS_index_prepared_stmt_instances_by_owner_object);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_prepared_stmt_instances::index_next() {
  PFS_prepared_stmt *pfs;
  bool has_more = true;

  for (m_pos.set_at(&m_next_pos); has_more; m_pos.next()) {
    pfs = global_prepared_stmt_container.get(m_pos.m_index, &has_more);

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

int table_prepared_stmt_instances::make_row(PFS_prepared_stmt *prepared_stmt) {
  pfs_optimistic_state lock;

  prepared_stmt->m_lock.begin_optimistic_lock(&lock);

  m_row.m_identity = prepared_stmt->m_identity;

  m_row.m_stmt_id = prepared_stmt->m_stmt_id;

  m_row.m_owner_thread_id = prepared_stmt->m_owner_thread_id;
  m_row.m_owner_event_id = prepared_stmt->m_owner_event_id;

  m_row.m_stmt_name_length = prepared_stmt->m_stmt_name_length;
  if (m_row.m_stmt_name_length > 0)
    memcpy(m_row.m_stmt_name, prepared_stmt->m_stmt_name,
           m_row.m_stmt_name_length);

  m_row.m_sql_text_length = prepared_stmt->m_sqltext_length;
  if (m_row.m_sql_text_length > 0) {
    memcpy(m_row.m_sql_text, prepared_stmt->m_sqltext, m_row.m_sql_text_length);
  }

  m_row.m_owner_object_type = prepared_stmt->m_owner_object_type;
  m_row.m_owner_object_name = prepared_stmt->m_owner_object_name;
  m_row.m_owner_object_schema = prepared_stmt->m_owner_object_schema;

  m_row.m_secondary = prepared_stmt->m_secondary;

  /* Get prepared statement prepare stats. */
  m_row.m_prepare_stat.set(m_normalizer, &prepared_stmt->m_prepare_stat);
  /* Get prepared statement re-prepare stats. */
  m_row.m_reprepare_stat.set(m_normalizer, &prepared_stmt->m_reprepare_stat);
  /* Get prepared statement execute stats. */
  m_row.m_execute_stat.set(m_normalizer, &prepared_stmt->m_execute_stat);

  if (!prepared_stmt->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_prepared_stmt_instances::read_row_values(TABLE *table,
                                                   unsigned char *buf,
                                                   Field **fields,
                                                   bool read_all) {
  Field *f;

  /*
    Set the null bits.
  */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* OBJECT_INSTANCE_BEGIN */
          set_field_ulonglong(f, (intptr)m_row.m_identity);
          break;
        case 1: /* STATEMENT_ID */
          set_field_ulonglong(f, m_row.m_stmt_id);
          break;
        case 2: /* STATEMENT_NAME */
          if (m_row.m_stmt_name_length > 0)
            set_field_varchar_utf8mb4(f, m_row.m_stmt_name,
                                      m_row.m_stmt_name_length);
          else {
            f->set_null();
          }
          break;
        case 3: /* SQL_TEXT */
          if (m_row.m_sql_text_length > 0) {
            set_field_blob(f, m_row.m_sql_text, m_row.m_sql_text_length);
          } else {
            f->set_null();
          }
          break;
        case 4: /* OWNER_THREAD_ID */
          set_field_ulonglong(f, m_row.m_owner_thread_id);
          break;
        case 5: /* OWNER_EVENT_ID */
          if (m_row.m_owner_event_id > 0) {
            set_field_ulonglong(f, m_row.m_owner_event_id);
          } else {
            f->set_null();
          }
          break;
        case 6: /* OWNER_OBJECT_TYPE */
          if (m_row.m_owner_object_type != 0) {
            set_field_enum(f, m_row.m_owner_object_type);
          } else {
            f->set_null();
          }
          break;
        case 7: /* OWNER_OBJECT_SCHEMA */
          set_nullable_field_schema_name(f, &m_row.m_owner_object_schema);
          break;
        case 8: /* OWNER_OBJECT_NAME */
          set_nullable_field_object_name(f, &m_row.m_owner_object_name);
          break;
        case 9: /* EXECUTION_ENGINE */
          set_field_enum(f, m_row.m_secondary ? ENUM_SECONDARY : ENUM_PRIMARY);
          break;
        case 10: /* TIMER_PREPARE */
          m_row.m_prepare_stat.set_field(1, f);
          break;
        case 11: /* COUNT_REPREPARE */
          m_row.m_reprepare_stat.set_field(0, f);
          break;
        default: /* 12, ... COUNT/SUM/MIN/AVG/MAX */
          m_row.m_execute_stat.set_field(f->field_index() - 12, f);
          break;
      }
    }
  }

  return 0;
}
