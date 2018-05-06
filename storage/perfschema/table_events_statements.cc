/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_events_statements.cc
  Table EVENTS_STATEMENTS_xxx (implementation).
*/

#include "storage/perfschema/table_events_statements.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "sql/plugin_table.h"
#include "sql/sp_head.h" /* TYPE_ENUM_FUNCTION, ... */
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_events_statements.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/table_helper.h"

THR_LOCK table_events_statements_current::m_table_lock;

Plugin_table table_events_statements_current::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_statements_current",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  EVENT_ID BIGINT unsigned not null,\n"
    "  END_EVENT_ID BIGINT unsigned,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  SOURCE VARCHAR(64),\n"
    "  TIMER_START BIGINT unsigned,\n"
    "  TIMER_END BIGINT unsigned,\n"
    "  TIMER_WAIT BIGINT unsigned,\n"
    "  LOCK_TIME BIGINT unsigned not null,\n"
    "  SQL_TEXT LONGTEXT,\n"
    "  DIGEST VARCHAR(64),\n"
    "  DIGEST_TEXT LONGTEXT,\n"
    "  CURRENT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned,\n"
    "  MYSQL_ERRNO INTEGER,\n"
    "  RETURNED_SQLSTATE VARCHAR(5),\n"
    "  MESSAGE_TEXT VARCHAR(128),\n"
    "  ERRORS BIGINT unsigned not null,\n"
    "  WARNINGS BIGINT unsigned not null,\n"
    "  ROWS_AFFECTED BIGINT unsigned not null,\n"
    "  ROWS_SENT BIGINT unsigned not null,\n"
    "  ROWS_EXAMINED BIGINT unsigned not null,\n"
    "  CREATED_TMP_DISK_TABLES BIGINT unsigned not null,\n"
    "  CREATED_TMP_TABLES BIGINT unsigned not null,\n"
    "  SELECT_FULL_JOIN BIGINT unsigned not null,\n"
    "  SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,\n"
    "  SELECT_RANGE BIGINT unsigned not null,\n"
    "  SELECT_RANGE_CHECK BIGINT unsigned not null,\n"
    "  SELECT_SCAN BIGINT unsigned not null,\n"
    "  SORT_MERGE_PASSES BIGINT unsigned not null,\n"
    "  SORT_RANGE BIGINT unsigned not null,\n"
    "  SORT_ROWS BIGINT unsigned not null,\n"
    "  SORT_SCAN BIGINT unsigned not null,\n"
    "  NO_INDEX_USED BIGINT unsigned not null,\n"
    "  NO_GOOD_INDEX_USED BIGINT unsigned not null,\n"
    "  NESTING_EVENT_ID BIGINT unsigned,\n"
    "  NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),\n"
    "  NESTING_EVENT_LEVEL INTEGER,\n"
    "  PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_events_statements_current::m_share = {
    &pfs_truncatable_acl,
    table_events_statements_current::create,
    NULL, /* write_row */
    table_events_statements_current::delete_all_rows,
    table_events_statements_current::get_row_count,
    sizeof(pos_events_statements_current), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

THR_LOCK table_events_statements_history::m_table_lock;

Plugin_table table_events_statements_history::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_statements_history",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  EVENT_ID BIGINT unsigned not null,\n"
    "  END_EVENT_ID BIGINT unsigned,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  SOURCE VARCHAR(64),\n"
    "  TIMER_START BIGINT unsigned,\n"
    "  TIMER_END BIGINT unsigned,\n"
    "  TIMER_WAIT BIGINT unsigned,\n"
    "  LOCK_TIME BIGINT unsigned not null,\n"
    "  SQL_TEXT LONGTEXT,\n"
    "  DIGEST VARCHAR(64),\n"
    "  DIGEST_TEXT LONGTEXT,\n"
    "  CURRENT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned,\n"
    "  MYSQL_ERRNO INTEGER,\n"
    "  RETURNED_SQLSTATE VARCHAR(5),\n"
    "  MESSAGE_TEXT VARCHAR(128),\n"
    "  ERRORS BIGINT unsigned not null,\n"
    "  WARNINGS BIGINT unsigned not null,\n"
    "  ROWS_AFFECTED BIGINT unsigned not null,\n"
    "  ROWS_SENT BIGINT unsigned not null,\n"
    "  ROWS_EXAMINED BIGINT unsigned not null,\n"
    "  CREATED_TMP_DISK_TABLES BIGINT unsigned not null,\n"
    "  CREATED_TMP_TABLES BIGINT unsigned not null,\n"
    "  SELECT_FULL_JOIN BIGINT unsigned not null,\n"
    "  SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,\n"
    "  SELECT_RANGE BIGINT unsigned not null,\n"
    "  SELECT_RANGE_CHECK BIGINT unsigned not null,\n"
    "  SELECT_SCAN BIGINT unsigned not null,\n"
    "  SORT_MERGE_PASSES BIGINT unsigned not null,\n"
    "  SORT_RANGE BIGINT unsigned not null,\n"
    "  SORT_ROWS BIGINT unsigned not null,\n"
    "  SORT_SCAN BIGINT unsigned not null,\n"
    "  NO_INDEX_USED BIGINT unsigned not null,\n"
    "  NO_GOOD_INDEX_USED BIGINT unsigned not null,\n"
    "  NESTING_EVENT_ID BIGINT unsigned,\n"
    "  NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),\n"
    "  NESTING_EVENT_LEVEL INTEGER,\n"
    "  PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_events_statements_history::m_share = {
    &pfs_truncatable_acl,
    table_events_statements_history::create,
    NULL, /* write_row */
    table_events_statements_history::delete_all_rows,
    table_events_statements_history::get_row_count,
    sizeof(pos_events_statements_history), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

THR_LOCK table_events_statements_history_long::m_table_lock;

Plugin_table table_events_statements_history_long::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_statements_history_long",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  EVENT_ID BIGINT unsigned not null,\n"
    "  END_EVENT_ID BIGINT unsigned,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  SOURCE VARCHAR(64),\n"
    "  TIMER_START BIGINT unsigned,\n"
    "  TIMER_END BIGINT unsigned,\n"
    "  TIMER_WAIT BIGINT unsigned,\n"
    "  LOCK_TIME BIGINT unsigned not null,\n"
    "  SQL_TEXT LONGTEXT,\n"
    "  DIGEST VARCHAR(64),\n"
    "  DIGEST_TEXT LONGTEXT,\n"
    "  CURRENT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned,\n"
    "  MYSQL_ERRNO INTEGER,\n"
    "  RETURNED_SQLSTATE VARCHAR(5),\n"
    "  MESSAGE_TEXT VARCHAR(128),\n"
    "  ERRORS BIGINT unsigned not null,\n"
    "  WARNINGS BIGINT unsigned not null,\n"
    "  ROWS_AFFECTED BIGINT unsigned not null,\n"
    "  ROWS_SENT BIGINT unsigned not null,\n"
    "  ROWS_EXAMINED BIGINT unsigned not null,\n"
    "  CREATED_TMP_DISK_TABLES BIGINT unsigned not null,\n"
    "  CREATED_TMP_TABLES BIGINT unsigned not null,\n"
    "  SELECT_FULL_JOIN BIGINT unsigned not null,\n"
    "  SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,\n"
    "  SELECT_RANGE BIGINT unsigned not null,\n"
    "  SELECT_RANGE_CHECK BIGINT unsigned not null,\n"
    "  SELECT_SCAN BIGINT unsigned not null,\n"
    "  SORT_MERGE_PASSES BIGINT unsigned not null,\n"
    "  SORT_RANGE BIGINT unsigned not null,\n"
    "  SORT_ROWS BIGINT unsigned not null,\n"
    "  SORT_SCAN BIGINT unsigned not null,\n"
    "  NO_INDEX_USED BIGINT unsigned not null,\n"
    "  NO_GOOD_INDEX_USED BIGINT unsigned not null,\n"
    "  NESTING_EVENT_ID BIGINT unsigned,\n"
    "  NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),\n"
    "  NESTING_EVENT_LEVEL INTEGER\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_events_statements_history_long::m_share = {
    &pfs_truncatable_acl,
    table_events_statements_history_long::create,
    NULL, /* write_row */
    table_events_statements_history_long::delete_all_rows,
    table_events_statements_history_long::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_events_statements::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_events_statements::match(PFS_events *pfs) {
  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }
  return true;
}

table_events_statements_common::table_events_statements_common(
    const PFS_engine_table_share *share, void *pos)
    : PFS_engine_table(share, pos) {
  m_normalizer = time_normalizer::get_statement();
}

/**
  Build a row, part 1.

  This method is used while holding optimist locks.

  @param statement    The statement the cursor is reading
  @param [out] digest Saved copy of the statement digest
  @return 0 on success or HA_ERR_RECORD_DELETED
*/
int table_events_statements_common::make_row_part_1(
    PFS_events_statements *statement, sql_digest_storage *digest) {
  const char *base;
  const char *safe_source_file;
  ulonglong timer_end;

  PFS_statement_class *unsafe = (PFS_statement_class *)statement->m_class;
  PFS_statement_class *klass = sanitize_statement_class(unsafe);
  if (unlikely(klass == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_thread_internal_id = statement->m_thread_internal_id;
  m_row.m_event_id = statement->m_event_id;
  m_row.m_end_event_id = statement->m_end_event_id;
  m_row.m_nesting_event_id = statement->m_nesting_event_id;
  m_row.m_nesting_event_type = statement->m_nesting_event_type;
  m_row.m_nesting_event_level = statement->m_nesting_event_level;

  if (m_row.m_end_event_id == 0) {
    timer_end = get_statement_timer();
  } else {
    timer_end = statement->m_timer_end;
  }

  m_normalizer->to_pico(statement->m_timer_start, timer_end,
                        &m_row.m_timer_start, &m_row.m_timer_end,
                        &m_row.m_timer_wait);
  m_row.m_lock_time = statement->m_lock_time * MICROSEC_TO_PICOSEC;

  m_row.m_name = klass->m_name;
  m_row.m_name_length = klass->m_name_length;

  m_row.m_current_schema_name_length = statement->m_current_schema_name_length;
  if (m_row.m_current_schema_name_length > 0)
    memcpy(m_row.m_current_schema_name, statement->m_current_schema_name,
           m_row.m_current_schema_name_length);

  m_row.m_object_type = statement->m_sp_type;

  m_row.m_schema_name_length = statement->m_schema_name_length;
  if (m_row.m_schema_name_length > 0)
    memcpy(m_row.m_schema_name, statement->m_schema_name,
           m_row.m_schema_name_length);

  m_row.m_object_name_length = statement->m_object_name_length;
  if (m_row.m_object_name_length > 0)
    memcpy(m_row.m_object_name, statement->m_object_name,
           m_row.m_object_name_length);

  safe_source_file = statement->m_source_file;
  if (unlikely(safe_source_file == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  base = base_name(safe_source_file);
  m_row.m_source_length = snprintf(m_row.m_source, sizeof(m_row.m_source),
                                   "%s:%d", base, statement->m_source_line);
  if (m_row.m_source_length > sizeof(m_row.m_source)) {
    m_row.m_source_length = sizeof(m_row.m_source);
  }

  memcpy(m_row.m_message_text, statement->m_message_text,
         sizeof(m_row.m_message_text));
  memcpy(m_row.m_sqlstate, statement->m_sqlstate, SQLSTATE_LENGTH);

  m_row.m_sql_errno = statement->m_sql_errno;
  m_row.m_error_count = statement->m_error_count;
  m_row.m_warning_count = statement->m_warning_count;
  m_row.m_rows_affected = statement->m_rows_affected;
  m_row.m_rows_sent = statement->m_rows_sent;
  m_row.m_rows_examined = statement->m_rows_examined;
  m_row.m_created_tmp_disk_tables = statement->m_created_tmp_disk_tables;
  m_row.m_created_tmp_tables = statement->m_created_tmp_tables;
  m_row.m_select_full_join = statement->m_select_full_join;
  m_row.m_select_full_range_join = statement->m_select_full_range_join;
  m_row.m_select_range = statement->m_select_range;
  m_row.m_select_range_check = statement->m_select_range_check;
  m_row.m_select_scan = statement->m_select_scan;
  m_row.m_sort_merge_passes = statement->m_sort_merge_passes;
  m_row.m_sort_range = statement->m_sort_range;
  m_row.m_sort_rows = statement->m_sort_rows;
  m_row.m_sort_scan = statement->m_sort_scan;
  m_row.m_no_index_used = statement->m_no_index_used;
  m_row.m_no_good_index_used = statement->m_no_good_index_used;

  /* Copy the digest storage. */
  digest->copy(&statement->m_digest_storage);

  /* Format the sqltext string for output. */
  format_sqltext(statement->m_sqltext, statement->m_sqltext_length,
                 get_charset(statement->m_sqltext_cs_number, MYF(0)),
                 statement->m_sqltext_truncated, m_row.m_sqltext);

  return 0;
}

/**
  Build a row, part 2.

  This method is used after all optimist locks have been released.

  @param [in] digest Statement digest to print in the row.
  @return 0 on success or HA_ERR_RECORD_DELETED
*/
int table_events_statements_common::make_row_part_2(
    const sql_digest_storage *digest) {
  /*
    Filling up statement digest information.
  */
  size_t safe_byte_count = digest->m_byte_count;
  if (safe_byte_count > 0 && safe_byte_count <= pfs_max_digest_length) {
    /* Generate the DIGEST string from the digest */
    DIGEST_HASH_TO_STRING(digest->m_hash, m_row.m_digest.m_digest);
    m_row.m_digest.m_digest_length = DIGEST_HASH_TO_STRING_LENGTH;

    /* Generate the DIGEST_TEXT string from the token array */
    compute_digest_text(digest, &m_row.m_digest.m_digest_text);

    if (m_row.m_digest.m_digest_text.length() == 0) {
      m_row.m_digest.m_digest_length = 0;
    }
  } else {
    m_row.m_digest.m_digest_length = 0;
    m_row.m_digest.m_digest_text.length(0);
  }

  return 0;
}

int table_events_statements_common::read_row_values(TABLE *table,
                                                    unsigned char *buf,
                                                    Field **fields,
                                                    bool read_all) {
  Field *f;
  uint len;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 3);
  buf[0] = 0;
  buf[1] = 0;
  buf[2] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* THREAD_ID */
          set_field_ulonglong(f, m_row.m_thread_internal_id);
          break;
        case 1: /* EVENT_ID */
          set_field_ulonglong(f, m_row.m_event_id);
          break;
        case 2: /* END_EVENT_ID */
          if (m_row.m_end_event_id > 0) {
            set_field_ulonglong(f, m_row.m_end_event_id - 1);
          } else {
            f->set_null();
          }
          break;
        case 3: /* EVENT_NAME */
          set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
          break;
        case 4: /* SOURCE */
          set_field_varchar_utf8(f, m_row.m_source, m_row.m_source_length);
          break;
        case 5: /* TIMER_START */
          if (m_row.m_timer_start != 0) {
            set_field_ulonglong(f, m_row.m_timer_start);
          } else {
            f->set_null();
          }
          break;
        case 6: /* TIMER_END */
          if (m_row.m_timer_end != 0) {
            set_field_ulonglong(f, m_row.m_timer_end);
          } else {
            f->set_null();
          }
          break;
        case 7: /* TIMER_WAIT */
          if (m_row.m_timer_wait != 0) {
            set_field_ulonglong(f, m_row.m_timer_wait);
          } else {
            f->set_null();
          }
          break;
        case 8: /* LOCK_TIME */
          if (m_row.m_lock_time != 0) {
            set_field_ulonglong(f, m_row.m_lock_time);
          } else {
            f->set_null();
          }
          break;
        case 9: /* SQL_TEXT */
          if (m_row.m_sqltext.length())
            set_field_text(f, m_row.m_sqltext.ptr(), m_row.m_sqltext.length(),
                           m_row.m_sqltext.charset());
          else {
            f->set_null();
          }
          break;
        case 10: /* DIGEST */
          if (m_row.m_digest.m_digest_length > 0)
            set_field_varchar_utf8(f, m_row.m_digest.m_digest,
                                   m_row.m_digest.m_digest_length);
          else {
            f->set_null();
          }
          break;
        case 11: /* DIGEST_TEXT */
          if (m_row.m_digest.m_digest_text.length() > 0)
            set_field_blob(f, m_row.m_digest.m_digest_text.ptr(),
                           (uint)m_row.m_digest.m_digest_text.length());
          else {
            f->set_null();
          }
          break;
        case 12: /* CURRENT_SCHEMA */
          if (m_row.m_current_schema_name_length)
            set_field_varchar_utf8(f, m_row.m_current_schema_name,
                                   m_row.m_current_schema_name_length);
          else {
            f->set_null();
          }
          break;
        case 13: /* OBJECT_TYPE */
          if (m_row.m_object_name_length > 0) {
            set_field_object_type(f, m_row.m_object_type);
          } else {
            f->set_null();
          }
          break;
        case 14: /* OBJECT_SCHEMA */
          if (m_row.m_schema_name_length)
            set_field_varchar_utf8(f, m_row.m_schema_name,
                                   m_row.m_schema_name_length);
          else {
            f->set_null();
          }
          break;
        case 15: /* OBJECT_NAME */
          if (m_row.m_object_name_length)
            set_field_varchar_utf8(f, m_row.m_object_name,
                                   m_row.m_object_name_length);
          else {
            f->set_null();
          }
          break;
        case 16: /* OBJECT_INSTANCE_BEGIN */
          f->set_null();
          break;
        case 17: /* MYSQL_ERRNO */
          set_field_ulong(f, m_row.m_sql_errno);
          break;
        case 18: /* RETURNED_SQLSTATE */
          if (m_row.m_sqlstate[0] != 0) {
            set_field_varchar_utf8(f, m_row.m_sqlstate, SQLSTATE_LENGTH);
          } else {
            f->set_null();
          }
          break;
        case 19: /* MESSAGE_TEXT */
          len = (uint)strlen(m_row.m_message_text);
          if (len) {
            set_field_varchar_utf8(f, m_row.m_message_text, len);
          } else {
            f->set_null();
          }
          break;
        case 20: /* ERRORS */
          set_field_ulonglong(f, m_row.m_error_count);
          break;
        case 21: /* WARNINGS */
          set_field_ulonglong(f, m_row.m_warning_count);
          break;
        case 22: /* ROWS_AFFECTED */
          set_field_ulonglong(f, m_row.m_rows_affected);
          break;
        case 23: /* ROWS_SENT */
          set_field_ulonglong(f, m_row.m_rows_sent);
          break;
        case 24: /* ROWS_EXAMINED */
          set_field_ulonglong(f, m_row.m_rows_examined);
          break;
        case 25: /* CREATED_TMP_DISK_TABLES */
          set_field_ulonglong(f, m_row.m_created_tmp_disk_tables);
          break;
        case 26: /* CREATED_TMP_TABLES */
          set_field_ulonglong(f, m_row.m_created_tmp_tables);
          break;
        case 27: /* SELECT_FULL_JOIN */
          set_field_ulonglong(f, m_row.m_select_full_join);
          break;
        case 28: /* SELECT_FULL_RANGE_JOIN */
          set_field_ulonglong(f, m_row.m_select_full_range_join);
          break;
        case 29: /* SELECT_RANGE */
          set_field_ulonglong(f, m_row.m_select_range);
          break;
        case 30: /* SELECT_RANGE_CHECK */
          set_field_ulonglong(f, m_row.m_select_range_check);
          break;
        case 31: /* SELECT_SCAN */
          set_field_ulonglong(f, m_row.m_select_scan);
          break;
        case 32: /* SORT_MERGE_PASSES */
          set_field_ulonglong(f, m_row.m_sort_merge_passes);
          break;
        case 33: /* SORT_RANGE */
          set_field_ulonglong(f, m_row.m_sort_range);
          break;
        case 34: /* SORT_ROWS */
          set_field_ulonglong(f, m_row.m_sort_rows);
          break;
        case 35: /* SORT_SCAN */
          set_field_ulonglong(f, m_row.m_sort_scan);
          break;
        case 36: /* NO_INDEX_USED */
          set_field_ulonglong(f, m_row.m_no_index_used);
          break;
        case 37: /* NO_GOOD_INDEX_USED */
          set_field_ulonglong(f, m_row.m_no_good_index_used);
          break;
        case 38: /* NESTING_EVENT_ID */
          if (m_row.m_nesting_event_id != 0) {
            set_field_ulonglong(f, m_row.m_nesting_event_id);
          } else {
            f->set_null();
          }
          break;
        case 39: /* NESTING_EVENT_TYPE */
          if (m_row.m_nesting_event_id != 0) {
            set_field_enum(f, m_row.m_nesting_event_type);
          } else {
            f->set_null();
          }
          break;
        case 40: /* NESTING_EVENT_LEVEL */
          set_field_ulong(f, m_row.m_nesting_event_level);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

PFS_engine_table *table_events_statements_current::create(
    PFS_engine_table_share *) {
  return new table_events_statements_current();
}

table_events_statements_current::table_events_statements_current()
    : table_events_statements_common(&m_share, &m_pos), m_pos(), m_next_pos() {}

void table_events_statements_current::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_statements_current::rnd_init(bool) { return 0; }

int table_events_statements_current::rnd_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      uint safe_events_statements_count = pfs_thread->m_events_statements_count;

      if (safe_events_statements_count == 0) {
        /* Display the last top level statement, when completed */
        if (m_pos.m_index_2 >= 1) {
          continue;
        }
      } else {
        /* Display all pending statements, when in progress */
        if (m_pos.m_index_2 >= safe_events_statements_count) {
          continue;
        }
      }

      statement = &pfs_thread->m_statement_stack[m_pos.m_index_2];

      m_next_pos.set_after(&m_pos);
      return make_row(pfs_thread, statement);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_current::rnd_pos(const void *pos) {
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;

  set_position(pos);

  pfs_thread = global_thread_container.get(m_pos.m_index_1);
  if (pfs_thread != NULL) {
    uint safe_events_statements_count = pfs_thread->m_events_statements_count;

    if (safe_events_statements_count == 0) {
      /* Display the last top level statement, when completed */
      if (m_pos.m_index_2 >= 1) {
        return HA_ERR_RECORD_DELETED;
      }
    } else {
      /* Display all pending statements, when in progress */
      if (m_pos.m_index_2 >= safe_events_statements_count) {
        return HA_ERR_RECORD_DELETED;
      }
    }

    DBUG_ASSERT(m_pos.m_index_2 < statement_stack_max);

    statement = &pfs_thread->m_statement_stack[m_pos.m_index_2];

    if (statement->m_class != NULL) {
      return make_row(pfs_thread, statement);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_events_statements_current::index_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      if (m_opened_index->match(pfs_thread)) {
        do {
          uint safe_events_statements_count =
              pfs_thread->m_events_statements_count;
          if (safe_events_statements_count == 0) {
            /* Display the last top level statement, when completed */
            if (m_pos.m_index_2 >= 1) {
              break;
            }
          } else {
            /* Display all pending statements, when in progress */
            if (m_pos.m_index_2 >= safe_events_statements_count) {
              break;
            }
          }

          statement = &pfs_thread->m_statement_stack[m_pos.m_index_2];
          if (statement->m_class != NULL) {
            if (m_opened_index->match(statement)) {
              if (!make_row(pfs_thread, statement)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
            m_pos.set_after(&m_pos);
          }
        } while (statement->m_class != NULL);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_current::make_row(
    PFS_thread *pfs_thread, PFS_events_statements *statement) {
  sql_digest_storage digest;
  pfs_optimistic_state lock;
  pfs_optimistic_state stmt_lock;

  digest.reset(m_token_array, MAX_DIGEST_STORAGE_SIZE);
  /* Protect this reader against thread termination. */
  pfs_thread->m_lock.begin_optimistic_lock(&lock);
  /* Protect this reader against writing on statement information. */
  pfs_thread->m_stmt_lock.begin_optimistic_lock(&stmt_lock);

  if (table_events_statements_common::make_row_part_1(statement, &digest) !=
      0) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!pfs_thread->m_stmt_lock.end_optimistic_lock(&stmt_lock) ||
      !pfs_thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return table_events_statements_common::make_row_part_2(&digest);
}

int table_events_statements_current::delete_all_rows(void) {
  reset_events_statements_current();
  return 0;
}

ha_rows table_events_statements_current::get_row_count(void) {
  return global_thread_container.get_row_count() * statement_stack_max;
}

int table_events_statements_current::index_init(uint idx MY_ATTRIBUTE((unused)),
                                                bool) {
  PFS_index_events_statements *result;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_events_statements);
  m_opened_index = result;
  m_index = result;
  return 0;
}

PFS_engine_table *table_events_statements_history::create(
    PFS_engine_table_share *) {
  return new table_events_statements_history();
}

table_events_statements_history::table_events_statements_history()
    : table_events_statements_common(&m_share, &m_pos), m_pos(), m_next_pos() {}

void table_events_statements_history::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_statements_history::rnd_init(bool) { return 0; }

int table_events_statements_history::rnd_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;
  bool has_more_thread = true;

  if (events_statements_history_per_thread == 0) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      if (m_pos.m_index_2 >= events_statements_history_per_thread) {
        /* This thread does not have more (full) history */
        continue;
      }

      if (!pfs_thread->m_statements_history_full &&
          (m_pos.m_index_2 >= pfs_thread->m_statements_history_index)) {
        /* This thread does not have more (not full) history */
        continue;
      }

      statement = &pfs_thread->m_statements_history[m_pos.m_index_2];

      if (statement->m_class != NULL) {
        /* Next iteration, look for the next history in this thread */
        m_next_pos.set_after(&m_pos);
        return make_row(pfs_thread, statement);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_history::rnd_pos(const void *pos) {
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;

  DBUG_ASSERT(events_statements_history_per_thread != 0);
  set_position(pos);

  pfs_thread = global_thread_container.get(m_pos.m_index_1);
  if (pfs_thread != NULL) {
    DBUG_ASSERT(m_pos.m_index_2 < events_statements_history_per_thread);

    if (!pfs_thread->m_statements_history_full &&
        (m_pos.m_index_2 >= pfs_thread->m_statements_history_index)) {
      return HA_ERR_RECORD_DELETED;
    }

    statement = &pfs_thread->m_statements_history[m_pos.m_index_2];
    if (statement->m_class != NULL) {
      return make_row(pfs_thread, statement);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_events_statements_history::index_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;
  bool has_more_thread = true;

  if (events_statements_history_per_thread == 0) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      if (m_opened_index->match(pfs_thread)) {
        do {
          if (m_pos.m_index_2 >= events_statements_history_per_thread) {
            /* This thread does not have more (full) history */
            break;
          }

          if (!pfs_thread->m_statements_history_full &&
              (m_pos.m_index_2 >= pfs_thread->m_statements_history_index)) {
            /* This thread does not have more (not full) history */
            break;
          }

          statement = &pfs_thread->m_statements_history[m_pos.m_index_2];
          if (statement->m_class != NULL) {
            if (m_opened_index->match(statement)) {
              if (!make_row(pfs_thread, statement)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
            m_pos.set_after(&m_pos);
          }
        } while (statement->m_class != NULL);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_history::make_row(
    PFS_thread *pfs_thread, PFS_events_statements *statement) {
  sql_digest_storage digest;
  pfs_optimistic_state lock;

  digest.reset(m_token_array, MAX_DIGEST_STORAGE_SIZE);
  /* Protect this reader against thread termination. */
  pfs_thread->m_lock.begin_optimistic_lock(&lock);

  if (table_events_statements_common::make_row_part_1(statement, &digest) !=
      0) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!pfs_thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return table_events_statements_common::make_row_part_2(&digest);
}

int table_events_statements_history::delete_all_rows(void) {
  reset_events_statements_history();
  return 0;
}

ha_rows table_events_statements_history::get_row_count(void) {
  return events_statements_history_per_thread *
         global_thread_container.get_row_count();
}

int table_events_statements_history::index_init(uint idx MY_ATTRIBUTE((unused)),
                                                bool) {
  PFS_index_events_statements *result;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_events_statements);
  m_opened_index = result;
  m_index = result;
  return 0;
}

PFS_engine_table *table_events_statements_history_long::create(
    PFS_engine_table_share *) {
  return new table_events_statements_history_long();
}

table_events_statements_history_long::table_events_statements_history_long()
    : table_events_statements_common(&m_share, &m_pos),
      m_pos(0),
      m_next_pos(0) {}

void table_events_statements_history_long::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_events_statements_history_long::rnd_init(bool) { return 0; }

int table_events_statements_history_long::rnd_next(void) {
  PFS_events_statements *statement;
  size_t limit;

  if (events_statements_history_long_size == 0) {
    return HA_ERR_END_OF_FILE;
  }

  if (events_statements_history_long_full) {
    limit = events_statements_history_long_size;
  } else
    limit = events_statements_history_long_index.m_u32 %
            events_statements_history_long_size;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < limit; m_pos.next()) {
    statement = &events_statements_history_long_array[m_pos.m_index];

    if (statement->m_class != NULL) {
      /* Next iteration, look for the next entry */
      m_next_pos.set_after(&m_pos);
      return make_row(statement);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_history_long::rnd_pos(const void *pos) {
  PFS_events_statements *statement;
  size_t limit;

  if (events_statements_history_long_size == 0) {
    return HA_ERR_RECORD_DELETED;
  }

  set_position(pos);

  if (events_statements_history_long_full) {
    limit = events_statements_history_long_size;
  } else
    limit = events_statements_history_long_index.m_u32 %
            events_statements_history_long_size;

  if (m_pos.m_index >= limit) {
    return HA_ERR_RECORD_DELETED;
  }

  statement = &events_statements_history_long_array[m_pos.m_index];

  if (statement->m_class == NULL) {
    return HA_ERR_RECORD_DELETED;
  }

  return make_row(statement);
}

int table_events_statements_history_long::make_row(
    PFS_events_statements *statement) {
  sql_digest_storage digest;

  digest.reset(m_token_array, MAX_DIGEST_STORAGE_SIZE);
  if (table_events_statements_common::make_row_part_1(statement, &digest) !=
      0) {
    return HA_ERR_RECORD_DELETED;
  }

  return table_events_statements_common::make_row_part_2(&digest);
}

int table_events_statements_history_long::delete_all_rows(void) {
  reset_events_statements_history_long();
  return 0;
}

ha_rows table_events_statements_history_long::get_row_count(void) {
  return events_statements_history_long_size;
}
