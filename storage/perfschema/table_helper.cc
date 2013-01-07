/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file storage/perfschema/table_helper.cc
  Performance schema table helpers (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_engine_table.h"
#include "table_helper.h"
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_account.h"

int PFS_host_row::make_row(PFS_host *pfs)
{
  m_hostname_length= pfs->m_hostname_length;
  if (m_hostname_length > sizeof(m_hostname))
    return 1;
  if (m_hostname_length > 0)
    memcpy(m_hostname, pfs->m_hostname, sizeof(m_hostname));
  return 0;
}

void PFS_host_row::set_field(Field *f)
{
  if (m_hostname_length > 0)
    PFS_engine_table::set_field_char_utf8(f, m_hostname, m_hostname_length);
  else
    f->set_null();
}

int PFS_user_row::make_row(PFS_user *pfs)
{
  m_username_length= pfs->m_username_length;
  if (m_username_length > sizeof(m_username))
    return 1;
  if (m_username_length > 0)
    memcpy(m_username, pfs->m_username, sizeof(m_username));
  return 0;
}

void PFS_user_row::set_field(Field *f)
{
  if (m_username_length > 0)
    PFS_engine_table::set_field_char_utf8(f, m_username, m_username_length);
  else
    f->set_null();
}

int PFS_account_row::make_row(PFS_account *pfs)
{
  m_username_length= pfs->m_username_length;
  if (m_username_length > sizeof(m_username))
    return 1;
  if (m_username_length > 0)
    memcpy(m_username, pfs->m_username, sizeof(m_username));

  m_hostname_length= pfs->m_hostname_length;
  if (m_hostname_length > sizeof(m_hostname))
    return 1;
  if (m_hostname_length > 0)
    memcpy(m_hostname, pfs->m_hostname, sizeof(m_hostname));

  return 0;
}

void PFS_account_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* USER */
      if (m_username_length > 0)
        PFS_engine_table::set_field_char_utf8(f, m_username, m_username_length);
      else
        f->set_null();
      break;
    case 1: /* HOST */
      if (m_hostname_length > 0)
        PFS_engine_table::set_field_char_utf8(f, m_hostname, m_hostname_length);
      else
        f->set_null();
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }
}

int PFS_digest_row::make_row(PFS_statements_digest_stat* pfs)
{
  /*
    "0" value for byte_count indicates special entry i.e. aggregated
    stats at index 0 of statements_digest_stat_array. So do not calculate
    digest/digest_text as it should always be "NULL".
  */
  if (pfs->m_digest_storage.m_byte_count != 0)
  {
    m_schema_name_length= pfs->m_digest_key.m_schema_name_length;
    if (m_schema_name_length > 0)
      memcpy(m_schema_name, pfs->m_digest_key.m_schema_name, m_schema_name_length);
    /*
      Calculate digest from MD5 HASH collected to be shown as
      DIGEST in this row.
    */
    MD5_HASH_TO_STRING(pfs->m_digest_key.m_md5, m_digest);
    m_digest_length= MD5_HASH_TO_STRING_LENGTH;

    /*
      Calculate digest_text information from the token array collected
      to be shown as DIGEST_TEXT column.
    */
    get_digest_text(m_digest_text, &pfs->m_digest_storage);
    m_digest_text_length= strlen(m_digest_text);
  }
  else
  {
    m_schema_name_length= 0;
    m_digest_length= 0;
    m_digest_text_length= 0;
  }

  return 0;
}

void PFS_digest_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* SCHEMA_NAME */
      if (m_schema_name_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_schema_name,
                                                 m_schema_name_length);
      else
        f->set_null();
      break;
    case 1: /* DIGEST */
      if (m_digest_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_digest,
                                                 m_digest_length);
      else
        f->set_null();
      break;
    case 2: /* DIGEST_TEXT */
      if (m_digest_text_length > 0)
        PFS_engine_table::set_field_longtext_utf8(f, m_digest_text,
                                                  m_digest_text_length);
      else
        f->set_null();
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }
}

int PFS_object_row::make_row(PFS_table_share *pfs)
{
  m_object_type= pfs->get_object_type();

  m_schema_name_length= pfs->m_schema_name_length;
  if (m_schema_name_length > sizeof(m_schema_name))
    return 1;
  if (m_schema_name_length > 0)
    memcpy(m_schema_name, pfs->m_schema_name, sizeof(m_schema_name));

  m_object_name_length= pfs->m_table_name_length;
  if (m_object_name_length > sizeof(m_object_name))
    return 1;
  if (m_object_name_length > 0)
    memcpy(m_object_name, pfs->m_table_name, sizeof(m_object_name));

  return 0;
}

void PFS_object_row::set_field(uint index, Field *f)
{
  switch(index)
  {
    case 0: /* OBJECT_TYPE */
      set_field_object_type(f, m_object_type);
      break;
    case 1: /* SCHEMA_NAME */
      PFS_engine_table::set_field_varchar_utf8(f, m_schema_name, m_schema_name_length);
      break;
    case 2: /* OBJECT_NAME */
      PFS_engine_table::set_field_varchar_utf8(f, m_object_name, m_object_name_length);
      break;
    default:
      DBUG_ASSERT(false);
  }
}

int PFS_index_row::make_row(PFS_table_share *pfs, uint table_index)
{
  if (m_object_row.make_row(pfs))
    return 1;

  if (table_index < MAX_INDEXES)
  {
    PFS_table_key *key= &pfs->m_keys[table_index];
    m_index_name_length= key->m_name_length;
    if (m_index_name_length > sizeof(m_index_name))
      return 1;
    memcpy(m_index_name, key->m_name, sizeof(m_index_name));
  }
  else
    m_index_name_length= 0;

  return 0;
}

void PFS_index_row::set_field(uint index, Field *f)
{
  switch(index)
  {
    case 0: /* OBJECT_TYPE */
    case 1: /* SCHEMA_NAME */
    case 2: /* OBJECT_NAME */
      m_object_row.set_field(index, f);
      break;
    case 3: /* INDEX_NAME */
      if (m_index_name_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_index_name, m_index_name_length);
      else
        f->set_null();
      break;
    default:
      DBUG_ASSERT(false);
  }
}

void PFS_statement_stat_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* COUNT_STAR */
    case 1: /* SUM_TIMER_WAIT */
    case 2: /* MIN_TIMER_WAIT */
    case 3: /* AVG_TIMER_WAIT */
    case 4: /* MAX_TIMER_WAIT */
      m_timer1_row.set_field(index, f);
      break;
    case 5: /* SUM_LOCK_TIME */
      PFS_engine_table::set_field_ulonglong(f, m_lock_time);
      break;
    case 6: /* SUM_ERRORS */
      PFS_engine_table::set_field_ulonglong(f, m_error_count);
      break;
    case 7: /* SUM_WARNINGS */
      PFS_engine_table::set_field_ulonglong(f, m_warning_count);
      break;
    case 8: /* SUM_ROWS_AFFECTED */
      PFS_engine_table::set_field_ulonglong(f, m_rows_affected);
      break;
    case 9: /* SUM_ROWS_SENT */
      PFS_engine_table::set_field_ulonglong(f, m_rows_sent);
      break;
    case 10: /* SUM_ROWS_EXAMINED */
      PFS_engine_table::set_field_ulonglong(f, m_rows_examined);
      break;
    case 11: /* SUM_CREATED_TMP_DISK_TABLES */
      PFS_engine_table::set_field_ulonglong(f, m_created_tmp_disk_tables);
      break;
    case 12: /* SUM_CREATED_TMP_TABLES */
      PFS_engine_table::set_field_ulonglong(f, m_created_tmp_tables);
      break;
    case 13: /* SUM_SELECT_FULL_JOIN */
      PFS_engine_table::set_field_ulonglong(f, m_select_full_join);
      break;
    case 14: /* SUM_SELECT_FULL_RANGE_JOIN */
      PFS_engine_table::set_field_ulonglong(f, m_select_full_range_join);
      break;
    case 15: /* SUM_SELECT_RANGE */
      PFS_engine_table::set_field_ulonglong(f, m_select_range);
      break;
    case 16: /* SUM_SELECT_RANGE_CHECK */
      PFS_engine_table::set_field_ulonglong(f, m_select_range_check);
      break;
    case 17: /* SUM_SELECT_SCAN */
      PFS_engine_table::set_field_ulonglong(f, m_select_scan);
      break;
    case 18: /* SUM_SORT_MERGE_PASSES */
      PFS_engine_table::set_field_ulonglong(f, m_sort_merge_passes);
      break;
    case 19: /* SUM_SORT_RANGE */
      PFS_engine_table::set_field_ulonglong(f, m_sort_range);
      break;
    case 20: /* SUM_SORT_ROWS */
      PFS_engine_table::set_field_ulonglong(f, m_sort_rows);
      break;
    case 21: /* SUM_SORT_SCAN */
      PFS_engine_table::set_field_ulonglong(f, m_sort_scan);
      break;
    case 22: /* SUM_NO_INDEX_USED */
      PFS_engine_table::set_field_ulonglong(f, m_no_index_used);
      break;
    case 23: /* SUM_NO_GOOD_INDEX_USED */
      PFS_engine_table::set_field_ulonglong(f, m_no_good_index_used);
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }
}

void PFS_connection_stat_row::set_field(uint index, Field *f)
{
  switch (index)
  {
    case 0: /* CURRENT_CONNECTIONS */
      PFS_engine_table::set_field_ulonglong(f, m_current_connections);
      break;
    case 1: /* TOTAL_CONNECTIONS */
      PFS_engine_table::set_field_ulonglong(f, m_total_connections);
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }
}

void set_field_object_type(Field *f, enum_object_type object_type)
{
  switch (object_type)
  {
  case OBJECT_TYPE_TABLE:
    PFS_engine_table::set_field_varchar_utf8(f, "TABLE", 5);
    break;
  case OBJECT_TYPE_TEMPORARY_TABLE:
    PFS_engine_table::set_field_varchar_utf8(f, "TEMPORARY TABLE", 15);
    break;
  default:
    DBUG_ASSERT(false);
  }
}

