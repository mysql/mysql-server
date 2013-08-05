/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/table_events_statements.cc
  Table EVENTS_STATEMENTS_xxx (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "table_events_statements.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_events_statements.h"
#include "pfs_timer.h"
#include "sp_head.h" /* TYPE_ENUM_FUNCTION, ... */
#include "table_helper.h"
#include "my_md5.h"

THR_LOCK table_events_statements_current::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("END_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SOURCE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_START") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_END") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOCK_TIME") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SQL_TEXT") },
    { C_STRING_WITH_LEN("longtext") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("DIGEST") },
    { C_STRING_WITH_LEN("varchar(32)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("DIGEST_TEXT") },
    { C_STRING_WITH_LEN("longtext") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CURRENT_SCHEMA") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_TYPE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_SCHEMA") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_INSTANCE_BEGIN") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MYSQL_ERRNO") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("RETURNED_SQLSTATE") },
    { C_STRING_WITH_LEN("varchar(5)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MESSAGE_TEXT") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ERRORS") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("WARNINGS") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ROWS_AFFECTED") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ROWS_SENT") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ROWS_EXAMINED") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CREATED_TMP_DISK_TABLES") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CREATED_TMP_TABLES") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SELECT_FULL_JOIN") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SELECT_FULL_RANGE_JOIN") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SELECT_RANGE") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SELECT_RANGE_CHECK") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SELECT_SCAN") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SORT_MERGE_PASSES") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SORT_RANGE") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SORT_ROWS") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SORT_SCAN") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NO_INDEX_USED") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NO_GOOD_INDEX_USED") },
    { C_STRING_WITH_LEN("bigint") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NESTING_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NESTING_EVENT_TYPE") },
    { C_STRING_WITH_LEN("enum(\'STATEMENT\',\'STAGE\',\'WAIT\'") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_events_statements_current::m_field_def=
{40 , field_types };

PFS_engine_table_share
table_events_statements_current::m_share=
{
  { C_STRING_WITH_LEN("events_statements_current") },
  &pfs_truncatable_acl,
  &table_events_statements_current::create,
  NULL, /* write_row */
  &table_events_statements_current::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

THR_LOCK table_events_statements_history::m_table_lock;

PFS_engine_table_share
table_events_statements_history::m_share=
{
  { C_STRING_WITH_LEN("events_statements_history") },
  &pfs_truncatable_acl,
  &table_events_statements_history::create,
  NULL, /* write_row */
  &table_events_statements_history::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(pos_events_statements_history), /* ref length */
  &m_table_lock,
  &table_events_statements_current::m_field_def,
  false /* checked */
};

THR_LOCK table_events_statements_history_long::m_table_lock;

PFS_engine_table_share
table_events_statements_history_long::m_share=
{
  { C_STRING_WITH_LEN("events_statements_history_long") },
  &pfs_truncatable_acl,
  &table_events_statements_history_long::create,
  NULL, /* write_row */
  &table_events_statements_history_long::delete_all_rows,
  NULL, /* get_row_count */
  10000, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &table_events_statements_current::m_field_def,
  false /* checked */
};

table_events_statements_common::table_events_statements_common
(const PFS_engine_table_share *share, void *pos)
  : PFS_engine_table(share, pos),
  m_row_exists(false)
{}

/**
  Build a row.
  @param statement                      the statement the cursor is reading
*/
void table_events_statements_common::make_row(PFS_events_statements *statement)
{
  const char *base;
  const char *safe_source_file;

  m_row_exists= false;

  PFS_statement_class *unsafe= (PFS_statement_class*) statement->m_class;
  PFS_statement_class *klass= sanitize_statement_class(unsafe);
  if (unlikely(klass == NULL))
    return;

  m_row.m_thread_internal_id= statement->m_thread_internal_id;
  m_row.m_event_id= statement->m_event_id;
  m_row.m_end_event_id= statement->m_end_event_id;
  m_row.m_nesting_event_id= statement->m_nesting_event_id;
  m_row.m_nesting_event_type= statement->m_nesting_event_type;

  m_normalizer->to_pico(statement->m_timer_start, statement->m_timer_end,
                      & m_row.m_timer_start, & m_row.m_timer_end, & m_row.m_timer_wait);
  m_row.m_lock_time= statement->m_lock_time * MICROSEC_TO_PICOSEC;

  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;

  m_row.m_sqltext_length= statement->m_sqltext_length;
  if (m_row.m_sqltext_length > 0)
    memcpy(m_row.m_sqltext, statement->m_sqltext, m_row.m_sqltext_length);

  m_row.m_current_schema_name_length= statement->m_current_schema_name_length;
  if (m_row.m_current_schema_name_length > 0)
    memcpy(m_row.m_current_schema_name, statement->m_current_schema_name, m_row.m_current_schema_name_length);

  safe_source_file= statement->m_source_file;
  if (unlikely(safe_source_file == NULL))
    return;

  base= base_name(safe_source_file);
  m_row.m_source_length= my_snprintf(m_row.m_source, sizeof(m_row.m_source),
                                     "%s:%d", base, statement->m_source_line);
  if (m_row.m_source_length > sizeof(m_row.m_source))
    m_row.m_source_length= sizeof(m_row.m_source);

  memcpy(m_row.m_message_text, statement->m_message_text, sizeof(m_row.m_message_text));
  m_row.m_sql_errno= statement->m_sql_errno;
  memcpy(m_row.m_sqlstate, statement->m_sqlstate, SQLSTATE_LENGTH);
  m_row.m_error_count= statement->m_error_count;
  m_row.m_warning_count= statement->m_warning_count;
  m_row.m_rows_affected= statement->m_rows_affected;

  m_row.m_rows_sent= statement->m_rows_sent;
  m_row.m_rows_examined= statement->m_rows_examined;
  m_row.m_created_tmp_disk_tables= statement->m_created_tmp_disk_tables;
  m_row.m_created_tmp_tables= statement->m_created_tmp_tables;
  m_row.m_select_full_join= statement->m_select_full_join;
  m_row.m_select_full_range_join= statement->m_select_full_range_join;
  m_row.m_select_range= statement->m_select_range;
  m_row.m_select_range_check= statement->m_select_range_check;
  m_row.m_select_scan= statement->m_select_scan;
  m_row.m_sort_merge_passes= statement->m_sort_merge_passes;
  m_row.m_sort_range= statement->m_sort_range;
  m_row.m_sort_rows= statement->m_sort_rows;
  m_row.m_sort_scan= statement->m_sort_scan;
  m_row.m_no_index_used= statement->m_no_index_used;
  m_row.m_no_good_index_used= statement->m_no_good_index_used;
  /* 
    Filling up statement digest information.
  */
  PSI_digest_storage *digest= & statement->m_digest_storage;

  int safe_byte_count= digest->m_byte_count;
  if (safe_byte_count > 0 &&
      safe_byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE)
  {
    PFS_digest_key md5;
    compute_md5_hash((char *) md5.m_md5,
                     (char *) digest->m_token_array,
                     safe_byte_count);

    /* Generate the DIGEST string from the MD5 digest  */
    MD5_HASH_TO_STRING(md5.m_md5,
                       m_row.m_digest.m_digest);
    m_row.m_digest.m_digest_length= MD5_HASH_TO_STRING_LENGTH;

    /* Generate the DIGEST_TEXT string from the token array */
    get_digest_text(m_row.m_digest.m_digest_text, digest);
    m_row.m_digest.m_digest_text_length= strlen(m_row.m_digest.m_digest_text);

    if (m_row.m_digest.m_digest_text_length == 0)
      m_row.m_digest.m_digest_length= 0;
  }
  else
  {
    m_row.m_digest.m_digest_length= 0;
    m_row.m_digest.m_digest_text_length= 0;
  }

  m_row_exists= true;
  return;
}

int table_events_statements_common::read_row_values(TABLE *table,
                                                    unsigned char *buf,
                                                    Field **fields,
                                                    bool read_all)
{
  Field *f;
  uint len;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 3);
  buf[0]= 0;
  buf[1]= 0;
  buf[2]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* THREAD_ID */
        set_field_ulonglong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* EVENT_ID */
        set_field_ulonglong(f, m_row.m_event_id);
        break;
      case 2: /* END_EVENT_ID */
        if (m_row.m_end_event_id > 0)
          set_field_ulonglong(f, m_row.m_end_event_id - 1);
        else
          f->set_null();
        break;
      case 3: /* EVENT_NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 4: /* SOURCE */
        set_field_varchar_utf8(f, m_row.m_source, m_row.m_source_length);
        break;
      case 5: /* TIMER_START */
        if (m_row.m_timer_start != 0)
          set_field_ulonglong(f, m_row.m_timer_start);
        else
          f->set_null();
        break;
      case 6: /* TIMER_END */
        if (m_row.m_timer_end != 0)
          set_field_ulonglong(f, m_row.m_timer_end);
        else
          f->set_null();
        break;
      case 7: /* TIMER_WAIT */
        if (m_row.m_timer_wait != 0)
          set_field_ulonglong(f, m_row.m_timer_wait);
        else
          f->set_null();
        break;
      case 8: /* LOCK_TIME */
        if (m_row.m_lock_time != 0)
          set_field_ulonglong(f, m_row.m_lock_time);
        else
          f->set_null();
        break;
      case 9: /* SQL_TEXT */
        if (m_row.m_sqltext_length)
          set_field_longtext_utf8(f, m_row.m_sqltext, m_row.m_sqltext_length);
        else
          f->set_null();
        break;
      case 10: /* DIGEST */
        if (m_row.m_digest.m_digest_length > 0)
          set_field_varchar_utf8(f, m_row.m_digest.m_digest,
                                 m_row.m_digest.m_digest_length);
        else
          f->set_null();
        break;
      case 11: /* DIGEST_TEXT */
        if (m_row.m_digest.m_digest_text_length > 0)
           set_field_longtext_utf8(f, m_row.m_digest.m_digest_text,
                                   m_row.m_digest.m_digest_text_length);
        else
          f->set_null();
        break;
      case 12: /* CURRENT_SCHEMA */
        if (m_row.m_current_schema_name_length)
          set_field_varchar_utf8(f, m_row.m_current_schema_name, m_row.m_current_schema_name_length);
        else
          f->set_null();
        break;
      case 13: /* OBJECT_TYPE */
        f->set_null();
        break;
      case 14: /* OBJECT_SCHEMA */
        f->set_null();
        break;
      case 15: /* OBJECT_NAME */
        f->set_null();
        break;
      case 16: /* OBJECT_INSTANCE_BEGIN */
        f->set_null();
        break;
      case 17: /* MYSQL_ERRNO */
        set_field_ulong(f, m_row.m_sql_errno);
        break;
      case 18: /* RETURNED_SQLSTATE */
        if (m_row.m_sqlstate[0] != 0)
          set_field_varchar_utf8(f, m_row.m_sqlstate, SQLSTATE_LENGTH);
        else
          f->set_null();
        break;
      case 19: /* MESSAGE_TEXT */
        len= strlen(m_row.m_message_text);
        if (len)
          set_field_varchar_utf8(f, m_row.m_message_text, len);
        else
          f->set_null();
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
        if (m_row.m_nesting_event_id != 0)
          set_field_ulonglong(f, m_row.m_nesting_event_id);
        else
          f->set_null();
        break;
      case 39: /* NESTING_EVENT_TYPE */
        if (m_row.m_nesting_event_id != 0)
          set_field_enum(f, m_row.m_nesting_event_type);
        else
          f->set_null();
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

PFS_engine_table* table_events_statements_current::create(void)
{
  return new table_events_statements_current();
}

table_events_statements_current::table_events_statements_current()
  : table_events_statements_common(&m_share, &m_pos),
  m_pos(), m_next_pos()
{}

void table_events_statements_current::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_statements_current::rnd_init(bool scan)
{
  m_normalizer= time_normalizer::get(statement_timer);
  return 0;
}

int table_events_statements_current::rnd_next(void)
{
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index_1 < thread_max;
       m_pos.next_thread())
  {
    pfs_thread= &thread_array[m_pos.m_index_1];

    if (! pfs_thread->m_lock.is_populated())
    {
      /* This thread does not exist */
      continue;
    }

    uint safe_events_statements_count= pfs_thread->m_events_statements_count;

    if (safe_events_statements_count == 0)
    {
      /* Display the last top level statement, when completed */
      if (m_pos.m_index_2 >= 1)
        continue;
    }
    else
    {
      /* Display all pending statements, when in progress */
      if (m_pos.m_index_2 >= safe_events_statements_count)
        continue;
    }

    statement= &pfs_thread->m_statement_stack[m_pos.m_index_2];

    make_row(statement);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_current::rnd_pos(const void *pos)
{
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);
  pfs_thread= &thread_array[m_pos.m_index_1];

  if (! pfs_thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  uint safe_events_statements_count= pfs_thread->m_events_statements_count;

  if (safe_events_statements_count == 0)
  {
    /* Display the last top level statement, when completed */
    if (m_pos.m_index_2 >= 1)
      return HA_ERR_RECORD_DELETED;
  }
  else
  {
    /* Display all pending statements, when in progress */
    if (m_pos.m_index_2 >= safe_events_statements_count)
      return HA_ERR_RECORD_DELETED;
  }

  DBUG_ASSERT(m_pos.m_index_2 < statement_stack_max);

  statement= &pfs_thread->m_statement_stack[m_pos.m_index_2];

  if (statement->m_class == NULL)
    return HA_ERR_RECORD_DELETED;

  make_row(statement);
  return 0;
}

int table_events_statements_current::delete_all_rows(void)
{
  reset_events_statements_current();
  return 0;
}

PFS_engine_table* table_events_statements_history::create(void)
{
  return new table_events_statements_history();
}

table_events_statements_history::table_events_statements_history()
  : table_events_statements_common(&m_share, &m_pos),
  m_pos(), m_next_pos()
{}

void table_events_statements_history::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_statements_history::rnd_init(bool scan)
{
  m_normalizer= time_normalizer::get(statement_timer);
  return 0;
}

int table_events_statements_history::rnd_next(void)
{
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;

  if (events_statements_history_per_thread == 0)
    return HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index_1 < thread_max;
       m_pos.next_thread())
  {
    pfs_thread= &thread_array[m_pos.m_index_1];

    if (! pfs_thread->m_lock.is_populated())
    {
      /* This thread does not exist */
      continue;
    }

    if (m_pos.m_index_2 >= events_statements_history_per_thread)
    {
      /* This thread does not have more (full) history */
      continue;
    }

    if ( ! pfs_thread->m_statements_history_full &&
        (m_pos.m_index_2 >= pfs_thread->m_statements_history_index))
    {
      /* This thread does not have more (not full) history */
      continue;
    }

    statement= &pfs_thread->m_statements_history[m_pos.m_index_2];

    if (statement->m_class != NULL)
    {
      make_row(statement);
      /* Next iteration, look for the next history in this thread */
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_history::rnd_pos(const void *pos)
{
  PFS_thread *pfs_thread;
  PFS_events_statements *statement;

  DBUG_ASSERT(events_statements_history_per_thread != 0);
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);
  pfs_thread= &thread_array[m_pos.m_index_1];

  if (! pfs_thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(m_pos.m_index_2 < events_statements_history_per_thread);

  if ( ! pfs_thread->m_statements_history_full &&
      (m_pos.m_index_2 >= pfs_thread->m_statements_history_index))
    return HA_ERR_RECORD_DELETED;

  statement= &pfs_thread->m_statements_history[m_pos.m_index_2];

  if (statement->m_class == NULL)
    return HA_ERR_RECORD_DELETED;

  make_row(statement);
  return 0;
}

int table_events_statements_history::delete_all_rows(void)
{
  reset_events_statements_history();
  return 0;
}

PFS_engine_table* table_events_statements_history_long::create(void)
{
  return new table_events_statements_history_long();
}

table_events_statements_history_long::table_events_statements_history_long()
  : table_events_statements_common(&m_share, &m_pos),
  m_pos(0), m_next_pos(0)
{}

void table_events_statements_history_long::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_events_statements_history_long::rnd_init(bool scan)
{
  m_normalizer= time_normalizer::get(statement_timer);
  return 0;
}

int table_events_statements_history_long::rnd_next(void)
{
  PFS_events_statements *statement;
  uint limit;

  if (events_statements_history_long_size == 0)
    return HA_ERR_END_OF_FILE;

  if (events_statements_history_long_full)
    limit= events_statements_history_long_size;
  else
    limit= events_statements_history_long_index % events_statements_history_long_size;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < limit; m_pos.next())
  {
    statement= &events_statements_history_long_array[m_pos.m_index];

    if (statement->m_class != NULL)
    {
      make_row(statement);
      /* Next iteration, look for the next entry */
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_statements_history_long::rnd_pos(const void *pos)
{
  PFS_events_statements *statement;
  uint limit;

  if (events_statements_history_long_size == 0)
    return HA_ERR_RECORD_DELETED;

  set_position(pos);

  if (events_statements_history_long_full)
    limit= events_statements_history_long_size;
  else
    limit= events_statements_history_long_index % events_statements_history_long_size;

  if (m_pos.m_index >= limit)
    return HA_ERR_RECORD_DELETED;

  statement= &events_statements_history_long_array[m_pos.m_index];

  if (statement->m_class == NULL)
    return HA_ERR_RECORD_DELETED;

  make_row(statement);
  return 0;
}

int table_events_statements_history_long::delete_all_rows(void)
{
  reset_events_statements_history_long();
  return 0;
}

