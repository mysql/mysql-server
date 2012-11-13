/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_engine_table.cc
  Performance schema tables (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_engine_table.h"

#include "table_events_waits.h"
#include "table_setup_actors.h"
#include "table_setup_consumers.h"
#include "table_setup_instruments.h"
#include "table_setup_objects.h"
#include "table_setup_timers.h"
#include "table_performance_timers.h"
#include "table_events_waits_summary.h"
#include "table_ews_by_thread_by_event_name.h"
#include "table_ews_global_by_event_name.h"
#include "table_host_cache.h"
#include "table_os_global_by_type.h"
#include "table_sync_instances.h"
#include "table_file_instances.h"
#include "table_file_summary_by_instance.h"
#include "table_file_summary_by_event_name.h"
#include "table_threads.h"

#include "table_ews_by_host_by_event_name.h"
#include "table_ews_by_user_by_event_name.h"
#include "table_ews_by_account_by_event_name.h"
#include "table_tiws_by_index_usage.h"
#include "table_tiws_by_table.h"
#include "table_tlws_by_table.h"

#include "table_events_stages.h"
#include "table_esgs_by_thread_by_event_name.h"
#include "table_esgs_by_host_by_event_name.h"
#include "table_esgs_by_user_by_event_name.h"
#include "table_esgs_by_account_by_event_name.h"
#include "table_esgs_global_by_event_name.h"

#include "table_events_statements.h"
#include "table_esms_by_thread_by_event_name.h"
#include "table_esms_by_host_by_event_name.h"
#include "table_esms_by_user_by_event_name.h"
#include "table_esms_by_account_by_event_name.h"
#include "table_esms_global_by_event_name.h"
#include "table_esms_by_digest.h"

#include "table_users.h"
#include "table_accounts.h"
#include "table_hosts.h"

#include "table_socket_instances.h"
#include "table_socket_summary_by_instance.h"
#include "table_socket_summary_by_event_name.h"
#include "table_session_connect_attrs.h"
#include "table_session_account_connect_attrs.h"

/* For show status */
#include "pfs_column_values.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"
#include "pfs_setup_object.h"
#include "pfs_global.h"
#include "pfs_digest.h"

#include "sql_base.h"                           // close_thread_tables
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT

/**
  @addtogroup Performance_schema_engine
  @{
*/

static PFS_engine_table_share *all_shares[]=
{
  &table_cond_instances::m_share,
  &table_events_waits_current::m_share,
  &table_events_waits_history::m_share,
  &table_events_waits_history_long::m_share,
  &table_ews_by_host_by_event_name::m_share,
  &table_events_waits_summary_by_instance::m_share,
  &table_ews_by_thread_by_event_name::m_share,
  &table_ews_by_user_by_event_name::m_share,
  &table_ews_by_account_by_event_name::m_share,
  &table_ews_global_by_event_name::m_share,
  &table_file_instances::m_share,
  &table_file_summary_by_event_name::m_share,
  &table_file_summary_by_instance::m_share,
  &table_host_cache::m_share,
  &table_mutex_instances::m_share,
  &table_os_global_by_type::m_share,
  &table_performance_timers::m_share,
  &table_rwlock_instances::m_share,
  &table_setup_actors::m_share,
  &table_setup_consumers::m_share,
  &table_setup_instruments::m_share,
  &table_setup_objects::m_share,
  &table_setup_timers::m_share,
  &table_tiws_by_index_usage::m_share,
  &table_tiws_by_table::m_share,
  &table_tlws_by_table::m_share,
  &table_threads::m_share,

  &table_events_stages_current::m_share,
  &table_events_stages_history::m_share,
  &table_events_stages_history_long::m_share,
  &table_esgs_by_thread_by_event_name::m_share,
  &table_esgs_by_account_by_event_name::m_share,
  &table_esgs_by_user_by_event_name::m_share,
  &table_esgs_by_host_by_event_name::m_share,
  &table_esgs_global_by_event_name::m_share,

  &table_events_statements_current::m_share,
  &table_events_statements_history::m_share,
  &table_events_statements_history_long::m_share,
  &table_esms_by_thread_by_event_name::m_share,
  &table_esms_by_account_by_event_name::m_share,
  &table_esms_by_user_by_event_name::m_share,
  &table_esms_by_host_by_event_name::m_share,
  &table_esms_global_by_event_name::m_share,
  &table_esms_by_digest::m_share,

  &table_users::m_share,
  &table_accounts::m_share,
  &table_hosts::m_share,

  &table_socket_instances::m_share,
  &table_socket_summary_by_instance::m_share,
  &table_socket_summary_by_event_name::m_share,
  &table_session_connect_attrs::m_share,
  &table_session_account_connect_attrs::m_share,
  NULL
};

/**
  Check all the tables structure.
  @param thd              current thread
*/
void PFS_engine_table_share::check_all_tables(THD *thd)
{
  PFS_engine_table_share **current;

  DBUG_EXECUTE_IF("tampered_perfschema_table1",
                  {
                    /* Hack SETUP_INSTRUMENT, incompatible change. */
                    all_shares[20]->m_field_def->count++;
                  });

  for (current= &all_shares[0]; (*current) != NULL; current++)
    (*current)->check_one_table(thd);
}

/** Error reporting for schema integrity checks. */
class PFS_check_intact : public Table_check_intact
{
protected:
  virtual void report_error(uint code, const char *fmt, ...);

public:
  PFS_check_intact()
  {}

  ~PFS_check_intact()
  {}
};

void PFS_check_intact::report_error(uint code, const char *fmt, ...)
{
  va_list args;
  char buff[MYSQL_ERRMSG_SIZE];

  va_start(args, fmt);
  my_vsnprintf(buff, sizeof(buff), fmt, args);
  va_end(args);

  /*
    This is an install/upgrade issue:
    - do not report it in the user connection, there is none in main(),
    - report it in the server error log.
  */
  sql_print_error("%s", buff);
}

/**
  Check integrity of the actual table schema.
  The actual table schema (.frm) is compared to the expected schema.
  @param thd              current thread
*/
void PFS_engine_table_share::check_one_table(THD *thd)
{
  TABLE_LIST tables;

  tables.init_one_table(PERFORMANCE_SCHEMA_str.str,
                        PERFORMANCE_SCHEMA_str.length,
                        m_name.str, m_name.length,
                        m_name.str, TL_READ);

  /* Work around until Bug#32115 is backported. */
  LEX dummy_lex;
  LEX *old_lex= thd->lex;
  thd->lex= &dummy_lex;
  lex_start(thd);

  if (! open_and_lock_tables(thd, &tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    PFS_check_intact checker;

    if (!checker.check(tables.table, m_field_def))
      m_checked= true;
    close_thread_tables(thd);
  }
  else
    sql_print_error(ER(ER_WRONG_NATIVE_TABLE_STRUCTURE),
                    PERFORMANCE_SCHEMA_str.str, m_name.str);

  lex_end(&dummy_lex);
  thd->lex= old_lex;
}

/** Initialize all the table share locks. */
void PFS_engine_table_share::init_all_locks(void)
{
  PFS_engine_table_share **current;

  for (current= &all_shares[0]; (*current) != NULL; current++)
    thr_lock_init((*current)->m_thr_lock_ptr);
}

/** Delete all the table share locks. */
void PFS_engine_table_share::delete_all_locks(void)
{
  PFS_engine_table_share **current;

  for (current= &all_shares[0]; (*current) != NULL; current++)
    thr_lock_delete((*current)->m_thr_lock_ptr);
}

ha_rows PFS_engine_table_share::get_row_count(void) const
{
  /* If available, count the exact number or records */
  if (m_get_row_count)
    return m_get_row_count();
  /* Otherwise, return an estimate */
  return m_records;
}

int PFS_engine_table_share::write_row(TABLE *table, unsigned char *buf,
                                      Field **fields) const
{
  my_bitmap_map *org_bitmap;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in m_write_row.
  */
  if (! m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  if (m_write_row == NULL)
  {
    return HA_ERR_WRONG_COMMAND;
  }

  /* We internally read from Fields to support the write interface */
  org_bitmap= dbug_tmp_use_all_columns(table, table->read_set);
  int result= m_write_row(table, buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

static int compare_table_names(const char *name1, const char *name2)
{
  /*
    The performance schema is implemented as a storage engine, in memory.
    The current storage engine interface exposed by the server,
    and in particular handlerton::discover, uses 'FRM' files to describe a
    table structure, which are later stored on disk, by the server,
    in ha_create_table_from_engine().
    Because the table metadata is stored on disk, the table naming rules
    used by the performance schema then have to comply with the constraints
    imposed by the disk storage, and in particular with lower_case_table_names.
    Once the server is changed to be able to discover a table in a storage engine
    and then open the table without storing a FRM file on disk, this constraint
    on the performance schema will be lifted, and the naming logic can be relaxed
    to be simply my_strcasecmp(system_charset_info, name1, name2).
  */
  if (lower_case_table_names)
    return strcasecmp(name1, name2);
  return strcmp(name1, name2);
}

/**
  Find a table share by name.
  @param name             The table name
  @return table share
*/
const PFS_engine_table_share*
PFS_engine_table::find_engine_table_share(const char *name)
{
  DBUG_ENTER("PFS_engine_table::find_table_share");

  PFS_engine_table_share **current;

  for (current= &all_shares[0]; (*current) != NULL; current++)
  {
    if (compare_table_names(name, (*current)->m_name.str) == 0)
      DBUG_RETURN(*current);
  }

  DBUG_RETURN(NULL);
}

/**
  Read a table row.
  @param table            Table handle
  @param buf              Row buffer
  @param fields           Table fields
  @return 0 on success
*/
int PFS_engine_table::read_row(TABLE *table,
                               unsigned char *buf,
                               Field **fields)
{
  my_bitmap_map *org_bitmap;
  Field *f;
  Field **fields_reset;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in read_row_values.
  */
  if (! m_share_ptr->m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  /* We must read all columns in case a table is opened for update */
  bool read_all= !bitmap_is_clear_all(table->write_set);

  /* We internally write to Fields to support the read interface */
  org_bitmap= dbug_tmp_use_all_columns(table, table->write_set);

  /*
    Some callers of the storage engine interface do not honor the
    f->is_null() flag, and will attempt to read the data itself.
    A known offender is mysql_checksum_table().
    For robustness, reset every field.
  */
  for (fields_reset= fields; (f= *fields_reset) ; fields_reset++)
    f->reset();

  int result= read_row_values(table, buf, fields, read_all);
  dbug_tmp_restore_column_map(table->write_set, org_bitmap);

  return result;
}

/**
  Update a table row.
  @param table            Table handle
  @param old_buf          old row buffer
  @param new_buf          new row buffer
  @param fields           Table fields
  @return 0 on success
*/
int PFS_engine_table::update_row(TABLE *table,
                                 const unsigned char *old_buf,
                                 unsigned char *new_buf,
                                 Field **fields)
{
  my_bitmap_map *org_bitmap;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in update_row_values.
  */
  if (! m_share_ptr->m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  /* We internally read from Fields to support the write interface */
  org_bitmap= dbug_tmp_use_all_columns(table, table->read_set);
  int result= update_row_values(table, old_buf, new_buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

int PFS_engine_table::delete_row(TABLE *table,
                                 const unsigned char *buf,
                                 Field **fields)
{
  my_bitmap_map *org_bitmap;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in delete_row_values.
  */
  if (! m_share_ptr->m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  /* We internally read from Fields to support the delete interface */
  org_bitmap= dbug_tmp_use_all_columns(table, table->read_set);
  int result= delete_row_values(table, buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

int PFS_engine_table::delete_row_values(TABLE *,
                                        const unsigned char *,
                                        Field **)
{
  return HA_ERR_WRONG_COMMAND;
}

/**
  Get the position of the current row.
  @param [out] ref        position
*/
void PFS_engine_table::get_position(void *ref)
{
  memcpy(ref, m_pos_ptr, m_share_ptr->m_ref_length);
}

/**
  Set the table cursor at a given position.
  @param [in] ref         position
*/
void PFS_engine_table::set_position(const void *ref)
{
  memcpy(m_pos_ptr, ref, m_share_ptr->m_ref_length);
}

/**
  Get the timer normalizer and class type for the current row.
  @param [in] instr_class    class
*/
void PFS_engine_table::get_normalizer(PFS_instr_class *instr_class)
{
  if (instr_class->m_type != m_class_type)
  {
    m_normalizer= time_normalizer::get(*instr_class->m_timer);
    m_class_type= instr_class->m_type;
  }
}

void PFS_engine_table::set_field_ulong(Field *f, ulong value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_LONG);
  Field_long *f2= (Field_long*) f;
  f2->store(value, true);
}

void PFS_engine_table::set_field_ulonglong(Field *f, ulonglong value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_LONGLONG);
  Field_longlong *f2= (Field_longlong*) f;
  f2->store(value, true);
}

void PFS_engine_table::set_field_char_utf8(Field *f, const char* str,
                                           uint len)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_STRING);
  Field_string *f2= (Field_string*) f;
  f2->store(str, len, &my_charset_utf8_bin);
}

void PFS_engine_table::set_field_varchar_utf8(Field *f, const char* str,
                                              uint len)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_VARCHAR);
  Field_varstring *f2= (Field_varstring*) f;
  f2->store(str, len, &my_charset_utf8_bin);
}

void PFS_engine_table::set_field_longtext_utf8(Field *f, const char* str,
                                               uint len)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_BLOB);
  Field_blob *f2= (Field_blob*) f;
  f2->store(str, len, &my_charset_utf8_bin);
}

void PFS_engine_table::set_field_enum(Field *f, ulonglong value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_ENUM);
  Field_enum *f2= (Field_enum*) f;
  f2->store_type(value);
}

void PFS_engine_table::set_field_timestamp(Field *f, ulonglong value)
{
  struct timeval tm;
  tm.tv_sec= (long)(value / 1000000);
  tm.tv_usec= (long)(value % 1000000);
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_TIMESTAMP2);
  Field_timestampf *f2= (Field_timestampf*) f;
  f2->store_timestamp(& tm);
}

ulonglong PFS_engine_table::get_field_enum(Field *f)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_ENUM);
  Field_enum *f2= (Field_enum*) f;
  return f2->val_int();
}

String*
PFS_engine_table::get_field_char_utf8(Field *f, String *val)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_STRING);
  Field_string *f2= (Field_string*) f;
  val= f2->val_str(NULL, val);
  return val;
}

String*
PFS_engine_table::get_field_varchar_utf8(Field *f, String *val)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_VARCHAR);
  Field_varstring *f2= (Field_varstring*) f;
  val= f2->val_str(NULL, val);
  return val;
}

int PFS_engine_table::update_row_values(TABLE *,
                                        const unsigned char *,
                                        unsigned char *,
                                        Field **)
{
  return HA_ERR_WRONG_COMMAND;
}

/** Implementation of internal ACL checks, for the performance schema. */
class PFS_internal_schema_access : public ACL_internal_schema_access
{
public:
  PFS_internal_schema_access()
  {}

  ~PFS_internal_schema_access()
  {}

  ACL_internal_access_result check(ulong want_access,
                                   ulong *save_priv) const;

  const ACL_internal_table_access *lookup(const char *name) const;
};

ACL_internal_access_result
PFS_internal_schema_access::check(ulong want_access,
                                  ulong *save_priv)  const
{
  const ulong always_forbidden= /* CREATE_ACL | */ REFERENCES_ACL
    | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL | EXECUTE_ACL
    | CREATE_VIEW_ACL | SHOW_VIEW_ACL | CREATE_PROC_ACL | ALTER_PROC_ACL
    | EVENT_ACL | TRIGGER_ACL ;

  if (unlikely(want_access & always_forbidden))
    return ACL_INTERNAL_ACCESS_DENIED;

  /*
    Proceed with regular grant tables,
    to give administrative control to the DBA.
  */
  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

const ACL_internal_table_access *
PFS_internal_schema_access::lookup(const char *name) const
{
  const PFS_engine_table_share* share;
  share= PFS_engine_table::find_engine_table_share(name);
  if (share)
    return share->m_acl;
  /*
    Do not return NULL, it would mean we are not interested
    in privilege checks for unknown tables.
    Instead, return an object that denies every actions,
    to prevent users for creating their own tables in the
    performance_schema database schema.
  */
  return &pfs_unknown_acl;
}

PFS_internal_schema_access pfs_internal_access;

void initialize_performance_schema_acl(bool bootstrap)
{
  /*
    ACL is always enforced, even if the performance schema
    is not enabled (the tables are still visible).
  */
  if (! bootstrap)
  {
    ACL_internal_schema_registry::register_schema(&PERFORMANCE_SCHEMA_str,
                                                  &pfs_internal_access);
  }
}

PFS_readonly_acl pfs_readonly_acl;

ACL_internal_access_result
PFS_readonly_acl::check(ulong want_access, ulong *save_priv) const
{
  const ulong always_forbidden= INSERT_ACL | UPDATE_ACL | DELETE_ACL
    | /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL | ALTER_ACL
    | CREATE_VIEW_ACL | SHOW_VIEW_ACL | TRIGGER_ACL | LOCK_TABLES_ACL;

  if (unlikely(want_access & always_forbidden))
    return ACL_INTERNAL_ACCESS_DENIED;

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_truncatable_acl pfs_truncatable_acl;

ACL_internal_access_result
PFS_truncatable_acl::check(ulong want_access, ulong *save_priv) const
{
  const ulong always_forbidden= INSERT_ACL | UPDATE_ACL | DELETE_ACL
    | /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL | ALTER_ACL
    | CREATE_VIEW_ACL | SHOW_VIEW_ACL | TRIGGER_ACL | LOCK_TABLES_ACL;

  if (unlikely(want_access & always_forbidden))
    return ACL_INTERNAL_ACCESS_DENIED;

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_updatable_acl pfs_updatable_acl;

ACL_internal_access_result
PFS_updatable_acl::check(ulong want_access, ulong *save_priv) const
{
  const ulong always_forbidden= INSERT_ACL | DELETE_ACL
    | /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL | ALTER_ACL
    | CREATE_VIEW_ACL | SHOW_VIEW_ACL | TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden))
    return ACL_INTERNAL_ACCESS_DENIED;

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_editable_acl pfs_editable_acl;

ACL_internal_access_result
PFS_editable_acl::check(ulong want_access, ulong *save_priv) const
{
  const ulong always_forbidden= /* CREATE_ACL | */ REFERENCES_ACL
    | INDEX_ACL | ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden))
    return ACL_INTERNAL_ACCESS_DENIED;

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_unknown_acl pfs_unknown_acl;

ACL_internal_access_result
PFS_unknown_acl::check(ulong want_access, ulong *save_priv) const
{
  const ulong always_forbidden= CREATE_ACL
    | REFERENCES_ACL | INDEX_ACL | ALTER_ACL
    | CREATE_VIEW_ACL | TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden))
    return ACL_INTERNAL_ACCESS_DENIED;

  /*
    There is no point in hiding (by enforcing ACCESS_DENIED for SELECT_ACL
    on performance_schema.*) tables that do not exist anyway.
    When SELECT_ACL is granted on performance_schema.* or *.*,
    SELECT * from performance_schema.wrong_table
    will fail with a more understandable ER_NO_SUCH_TABLE error,
    instead of ER_TABLEACCESS_DENIED_ERROR.
    The same goes for other DML (INSERT_ACL | UPDATE_ACL | DELETE_ACL),
    for ease of use: error messages will be less surprising.
  */
  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

/**
  SHOW ENGINE PERFORMANCE_SCHEMA STATUS.
  @param hton               Storage engine handler
  @param thd                Current thread
  @param print              Print function
  @param stat               status to show
*/
bool pfs_show_status(handlerton *hton, THD *thd,
                     stat_print_fn *print, enum ha_stat_type stat)
{
  char buf[1024];
  uint buflen;
  const char *name;
  int i;
  uint size;

  DBUG_ENTER("pfs_show_status");

  /*
    Note about naming conventions:
    - Internal buffers exposed as a table in the performance schema are named
    after the table, as in 'events_waits_current'
    - Internal buffers not exposed by a table are named with parenthesis,
    as in '(pfs_mutex_class)'.
  */
  if (stat != HA_ENGINE_STATUS)
    DBUG_RETURN(false);

  uint total_memory= 0;

  for (i=0; /* empty */; i++)
  {
    switch (i){
    case 0:
      name= "events_waits_current.row_size";
      size= sizeof(PFS_events_waits);
      break;
    case 1:
      name= "events_waits_current.row_count";
      size= WAIT_STACK_SIZE * thread_max;
      break;
    case 2:
      name= "events_waits_history.row_size";
      size= sizeof(PFS_events_waits);
      break;
    case 3:
      name= "events_waits_history.row_count";
      size= events_waits_history_per_thread * thread_max;
      break;
    case 4:
      name= "events_waits_history.memory";
      size= events_waits_history_per_thread * thread_max
        * sizeof(PFS_events_waits);
      total_memory+= size;
      break;
    case 5:
      name= "events_waits_history_long.row_size";
      size= sizeof(PFS_events_waits);
      break;
    case 6:
      name= "events_waits_history_long.row_count";
      size= events_waits_history_long_size;
      break;
    case 7:
      name= "events_waits_history_long.memory";
      size= events_waits_history_long_size * sizeof(PFS_events_waits);
      total_memory+= size;
      break;
    case 8:
      name= "(pfs_mutex_class).row_size";
      size= sizeof(PFS_mutex_class);
      break;
    case 9:
      name= "(pfs_mutex_class).row_count";
      size= mutex_class_max;
      break;
    case 10:
      name= "(pfs_mutex_class).memory";
      size= mutex_class_max * sizeof(PFS_mutex_class);
      total_memory+= size;
      break;
    case 11:
      name= "(pfs_rwlock_class).row_size";
      size= sizeof(PFS_rwlock_class);
      break;
    case 12:
      name= "(pfs_rwlock_class).row_count";
      size= rwlock_class_max;
      break;
    case 13:
      name= "(pfs_rwlock_class).memory";
      size= rwlock_class_max * sizeof(PFS_rwlock_class);
      total_memory+= size;
      break;
    case 14:
      name= "(pfs_cond_class).row_size";
      size= sizeof(PFS_cond_class);
      break;
    case 15:
      name= "(pfs_cond_class).row_count";
      size= cond_class_max;
      break;
    case 16:
      name= "(pfs_cond_class).memory";
      size= cond_class_max * sizeof(PFS_cond_class);
      total_memory+= size;
      break;
    case 17:
      name= "(pfs_thread_class).row_size";
      size= sizeof(PFS_thread_class);
      break;
    case 18:
      name= "(pfs_thread_class).row_count";
      size= thread_class_max;
      break;
    case 19:
      name= "(pfs_thread_class).memory";
      size= thread_class_max * sizeof(PFS_thread_class);
      total_memory+= size;
      break;
    case 20:
      name= "(pfs_file_class).row_size";
      size= sizeof(PFS_file_class);
      break;
    case 21:
      name= "(pfs_file_class).row_count";
      size= file_class_max;
      break;
    case 22:
      name= "(pfs_file_class).memory";
      size= file_class_max * sizeof(PFS_file_class);
      total_memory+= size;
      break;
    case 23:
      name= "mutex_instances.row_size";
      size= sizeof(PFS_mutex);
      break;
    case 24:
      name= "mutex_instances.row_count";
      size= mutex_max;
      break;
    case 25:
      name= "mutex_instances.memory";
      size= mutex_max * sizeof(PFS_mutex);
      total_memory+= size;
      break;
    case 26:
      name= "rwlock_instances.row_size";
      size= sizeof(PFS_rwlock);
      break;
    case 27:
      name= "rwlock_instances.row_count";
      size= rwlock_max;
      break;
    case 28:
      name= "rwlock_instances.memory";
      size= rwlock_max * sizeof(PFS_rwlock);
      total_memory+= size;
      break;
    case 29:
      name= "cond_instances.row_size";
      size= sizeof(PFS_cond);
      break;
    case 30:
      name= "cond_instances.row_count";
      size= cond_max;
      break;
    case 31:
      name= "cond_instances.memory";
      size= cond_max * sizeof(PFS_cond);
      total_memory+= size;
      break;
    case 32:
      name= "threads.row_size";
      size= sizeof(PFS_thread);
      break;
    case 33:
      name= "threads.row_count";
      size= thread_max;
      break;
    case 34:
      name= "threads.memory";
      size= thread_max * sizeof(PFS_thread);
      total_memory+= size;
      break;
    case 35:
      name= "file_instances.row_size";
      size= sizeof(PFS_file);
      break;
    case 36:
      name= "file_instances.row_count";
      size= file_max;
      break;
    case 37:
      name= "file_instances.memory";
      size= file_max * sizeof(PFS_file);
      total_memory+= size;
      break;
    case 38:
      name= "(pfs_file_handle).row_size";
      size= sizeof(PFS_file*);
      break;
    case 39:
      name= "(pfs_file_handle).row_count";
      size= file_handle_max;
      break;
    case 40:
      name= "(pfs_file_handle).memory";
      size= file_handle_max * sizeof(PFS_file*);
      total_memory+= size;
      break;
    case 41:
      name= "events_waits_summary_by_thread_by_event_name.row_size";
      size= sizeof(PFS_single_stat);
      break;
    case 42:
      name= "events_waits_summary_by_thread_by_event_name.row_count";
      size= thread_max * wait_class_max;
      break;
    case 43:
      name= "events_waits_summary_by_thread_by_event_name.memory";
      size= thread_max * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 44:
      name= "(pfs_table_share).row_size";
      size= sizeof(PFS_table_share);
      break;
    case 45:
      name= "(pfs_table_share).row_count";
      size= table_share_max;
      break;
    case 46:
      name= "(pfs_table_share).memory";
      size= table_share_max * sizeof(PFS_table_share);
      total_memory+= size;
      break;
    case 47:
      name= "(pfs_table).row_size";
      size= sizeof(PFS_table);
      break;
    case 48:
      name= "(pfs_table).row_count";
      size= table_max;
      break;
    case 49:
      name= "(pfs_table).memory";
      size= table_max * sizeof(PFS_table);
      total_memory+= size;
      break;
    case 50:
      name= "setup_actors.row_size";
      size= sizeof(PFS_setup_actor);
      break;
    case 51:
      name= "setup_actors.row_count";
      size= setup_actor_max;
      break;
    case 52:
      name= "setup_actors.memory";
      size= setup_actor_max * sizeof(PFS_setup_actor);
      total_memory+= size;
      break;
    case 53:
      name= "setup_objects.row_size";
      size= sizeof(PFS_setup_object);
      break;
    case 54:
      name= "setup_objects.row_count";
      size= setup_object_max;
      break;
    case 55:
      name= "setup_objects.memory";
      size= setup_object_max * sizeof(PFS_setup_object);
      total_memory+= size;
      break;
    case 56:
      name= "(pfs_account).row_size";
      size= sizeof(PFS_account);
      break;
    case 57:
      name= "(pfs_account).row_count";
      size= account_max;
      break;
    case 58:
      name= "(pfs_account).memory";
      size= account_max * sizeof(PFS_account);
      total_memory+= size;
      break;
    case 59:
      name= "events_waits_summary_by_account_by_event_name.row_size";
      size= sizeof(PFS_single_stat);
      break;
    case 60:
      name= "events_waits_summary_by_account_by_event_name.row_count";
      size= account_max * wait_class_max;
      break;
    case 61:
      name= "events_waits_summary_by_account_by_event_name.memory";
      size= account_max * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 62:
      name= "events_waits_summary_by_user_by_event_name.row_size";
      size= sizeof(PFS_single_stat);
      break;
    case 63:
      name= "events_waits_summary_by_user_by_event_name.row_count";
      size= user_max * wait_class_max;
      break;
    case 64:
      name= "events_waits_summary_by_user_by_event_name.memory";
      size= user_max * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 65:
      name= "events_waits_summary_by_host_by_event_name.row_size";
      size= sizeof(PFS_single_stat);
      break;
    case 66:
      name= "events_waits_summary_by_host_by_event_name.row_count";
      size= host_max * wait_class_max;
      break;
    case 67:
      name= "events_waits_summary_by_host_by_event_name.memory";
      size= host_max * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 68:
      name= "(pfs_user).row_size";
      size= sizeof(PFS_user);
      break;
    case 69:
      name= "(pfs_user).row_count";
      size= user_max;
      break;
    case 70:
      name= "(pfs_user).memory";
      size= user_max * sizeof(PFS_user);
      total_memory+= size;
      break;
    case 71:
      name= "(pfs_host).row_size";
      size= sizeof(PFS_host);
      break;
    case 72:
      name= "(pfs_host).row_count";
      size= host_max;
      break;
    case 73:
      name= "(pfs_host).memory";
      size= host_max * sizeof(PFS_host);
      total_memory+= size;
      break;
    case 74:
      name= "(pfs_stage_class).row_size";
      size= sizeof(PFS_stage_class);
      break;
    case 75:
      name= "(pfs_stage_class).row_count";
      size= stage_class_max;
      break;
    case 76:
      name= "(pfs_stage_class).memory";
      size= stage_class_max * sizeof(PFS_stage_class);
      total_memory+= size;
      break;
    case 77:
      name= "events_stages_history.row_size";
      size= sizeof(PFS_events_stages);
      break;
    case 78:
      name= "events_stages_history.row_count";
      size= events_stages_history_per_thread * thread_max;
      break;
    case 79:
      name= "events_stages_history.memory";
      size= events_stages_history_per_thread * thread_max
        * sizeof(PFS_events_stages);
      total_memory+= size;
      break;
    case 80:
      name= "events_stages_history_long.row_size";
      size= sizeof(PFS_events_stages);
      break;
    case 81:
      name= "events_stages_history_long.row_count";
      size= events_stages_history_long_size;
      break;
    case 82:
      name= "events_stages_history_long.memory";
      size= events_stages_history_long_size * sizeof(PFS_events_stages);
      total_memory+= size;
      break;
    case 83:
      name= "events_stages_summary_by_thread_by_event_name.row_size";
      size= sizeof(PFS_stage_stat);
      break;
    case 84:
      name= "events_stages_summary_by_thread_by_event_name.row_count";
      size= thread_max * stage_class_max;
      break;
    case 85:
      name= "events_stages_summary_by_thread_by_event_name.memory";
      size= thread_max * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 86:
      name= "events_stages_summary_global_by_event_name.row_size";
      size= sizeof(PFS_stage_stat);
      break;
    case 87:
      name= "events_stages_summary_global_by_event_name.row_count";
      size= stage_class_max;
      break;
    case 88:
      name= "events_stages_summary_global_by_event_name.memory";
      size= stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 89:
      name= "events_stages_summary_by_account_by_event_name.row_size";
      size= sizeof(PFS_stage_stat);
      break;
    case 90:
      name= "events_stages_summary_by_account_by_event_name.row_count";
      size= account_max * stage_class_max;
      break;
    case 91:
      name= "events_stages_summary_by_account_by_event_name.memory";
      size= account_max * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 92:
      name= "events_stages_summary_by_user_by_event_name.row_size";
      size= sizeof(PFS_stage_stat);
      break;
    case 93:
      name= "events_stages_summary_by_user_by_event_name.row_count";
      size= user_max * stage_class_max;
      break;
    case 94:
      name= "events_stages_summary_by_user_by_event_name.memory";
      size= user_max * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 95:
      name= "events_stages_summary_by_host_by_event_name.row_size";
      size= sizeof(PFS_stage_stat);
      break;
    case 96:
      name= "events_stages_summary_by_host_by_event_name.row_count";
      size= host_max * stage_class_max;
      break;
    case 97:
      name= "events_stages_summary_by_host_by_event_name.memory";
      size= host_max * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 98:
      name= "(pfs_statement_class).row_size";
      size= sizeof(PFS_statement_class);
      break;
    case 99:
      name= "(pfs_statement_class).row_count";
      size= statement_class_max;
      break;
    case 100:
      name= "(pfs_statement_class).memory";
      size= statement_class_max * sizeof(PFS_statement_class);
      total_memory+= size;
      break;
    case 101:
      name= "events_statements_history.row_size";
      size= sizeof(PFS_events_statements);
      break;
    case 102:
      name= "events_statements_history.row_count";
      size= events_statements_history_per_thread * thread_max;
      break;
    case 103:
      name= "events_statements_history.memory";
      size= events_statements_history_per_thread * thread_max
        * sizeof(PFS_events_statements);
      total_memory+= size;
      break;
    case 104:
      name= "events_statements_history_long.row_size";
      size= sizeof(PFS_events_statements);
      break;
    case 105:
      name= "events_statements_history_long.row_count";
      size= events_statements_history_long_size;
      break;
    case 106:
      name= "events_statements_history_long.memory";
      size= events_statements_history_long_size * sizeof(PFS_events_statements);
      total_memory+= size;
      break;
    case 107:
      name= "events_statements_summary_by_thread_by_event_name.row_size";
      size= sizeof(PFS_statement_stat);
      break;
    case 108:
      name= "events_statements_summary_by_thread_by_event_name.row_count";
      size= thread_max * statement_class_max;
      break;
    case 109:
      name= "events_statements_summary_by_thread_by_event_name.memory";
      size= thread_max * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 110:
      name= "events_statements_summary_global_by_event_name.row_size";
      size= sizeof(PFS_statement_stat);
      break;
    case 111:
      name= "events_statements_summary_global_by_event_name.row_count";
      size= statement_class_max;
      break;
    case 112:
      name= "events_statements_summary_global_by_event_name.memory";
      size= statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 113:
      name= "events_statements_summary_by_account_by_event_name.row_size";
      size= sizeof(PFS_statement_stat);
      break;
    case 114:
      name= "events_statements_summary_by_account_by_event_name.row_count";
      size= account_max * statement_class_max;
      break;
    case 115:
      name= "events_statements_summary_by_account_by_event_name.memory";
      size= account_max * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 116:
      name= "events_statements_summary_by_user_by_event_name.row_size";
      size= sizeof(PFS_statement_stat);
      break;
    case 117:
      name= "events_statements_summary_by_user_by_event_name.row_count";
      size= user_max * statement_class_max;
      break;
    case 118:
      name= "events_statements_summary_by_user_by_event_name.memory";
      size= user_max * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 119:
      name= "events_statements_summary_by_host_by_event_name.row_size";
      size= sizeof(PFS_statement_stat);
      break;
    case 120:
      name= "events_statements_summary_by_host_by_event_name.row_count";
      size= host_max * statement_class_max;
      break;
    case 121:
      name= "events_statements_summary_by_host_by_event_name.memory";
      size= host_max * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 122:
      name= "events_statements_current.row_size";
      size= sizeof(PFS_events_statements);
      break;
    case 123:
      name= "events_statements_current.row_count";
      size= thread_max * statement_stack_max;
      break;
    case 124:
      name= "events_statements_current.memory";
      size= thread_max * statement_stack_max * sizeof(PFS_events_statements);
      total_memory+= size;
      break;
    case 125:
      name= "(pfs_socket_class).row_size";
      size= sizeof(PFS_socket_class);
      break;
    case 126:
      name= "(pfs_socket_class).row_count";
      size= socket_class_max;
      break;
    case 127:
      name= "(pfs_socket_class).memory";
      size= socket_class_max * sizeof(PFS_socket_class);
      total_memory+= size;
      break;
    case 128:
      name= "socket_instances.row_size";
      size= sizeof(PFS_socket);
      break;
    case 129:
      name= "socket_instances.row_count";
      size= socket_max;
      break;
    case 130:
      name= "socket_instances.memory";
      size= socket_max * sizeof(PFS_socket);
      total_memory+= size;
      break;
    case 131:
      name= "events_statements_summary_by_digest.row_size";
      size= sizeof(PFS_statements_digest_stat);
      break;
    case 132:
      name= "events_statements_summary_by_digest.row_count";
      size= digest_max;
      break;
    case 133:
      name= "events_statements_summary_by_digest.memory";
      size= digest_max * sizeof(PFS_statements_digest_stat);
      total_memory+= size;
      break;
    case 134:
      name= "session_connect_attrs.row_size";
      size= thread_max;
      break;
    case 135:
      name= "session_connect_attrs.row_count";
      size= session_connect_attrs_size_per_thread;
      break;
    case 136:
      name= "session_connect_attrs.memory";
      size= thread_max * session_connect_attrs_size_per_thread;
      total_memory+= size;
      break;

    case 137:
      name= "(account_hash).count";
      size= account_hash.count;
      break;
    case 138:
      name= "(account_hash).size";
      size= account_hash.size;
      break;
    case 139:
      name= "(digest_hash).count";
      size= digest_hash.count;
      break;
    case 140:
      name= "(digest_hash).size";
      size= digest_hash.size;
      break;
    case 141:
      name= "(filename_hash).count";
      size= filename_hash.count;
      break;
    case 142:
      name= "(filename_hash).size";
      size= filename_hash.size;
      break;
    case 143:
      name= "(host_hash).count";
      size= host_hash.count;
      break;
    case 144:
      name= "(host_hash).size";
      size= host_hash.size;
      break;
    case 145:
      name= "(setup_actor_hash).count";
      size= setup_actor_hash.count;
      break;
    case 146:
      name= "(setup_actor_hash).size";
      size= setup_actor_hash.size;
      break;
    case 147:
      name= "(setup_object_hash).count";
      size= setup_object_hash.count;
      break;
    case 148:
      name= "(setup_object_hash).size";
      size= setup_object_hash.size;
      break;
    case 149:
      name= "(table_share_hash).count";
      size= table_share_hash.count;
      break;
    case 150:
      name= "(table_share_hash).size";
      size= table_share_hash.size;
      break;
    case 151:
      name= "(user_hash).count";
      size= user_hash.count;
      break;
    case 152:
      name= "(user_hash).size";
      size= user_hash.size;
      break;

    /*
      This case must be last,
      for aggregation in total_memory.
    */
    case 153:
      name= "performance_schema.memory";
      size= total_memory;
      /* This will fail if something is not advertised here */
      DBUG_ASSERT(size == pfs_allocated_memory);
      break;
    default:
      goto end;
      break;
    }

    buflen= int10_to_str(size, buf, 10) - buf;
    if (print(thd,
              PERFORMANCE_SCHEMA_str.str, PERFORMANCE_SCHEMA_str.length,
              name, strlen(name),
              buf, buflen))
      DBUG_RETURN(true);
  }

end:
  DBUG_RETURN(false);
}

/** @} */

