/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "my_thread.h"
#include "hostname.h" /* For Host_entry */
#include "pfs_engine_table.h"
#include "pfs_buffer_container.h"

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
#include "table_esms_by_program.h"

#include "table_events_transactions.h"
#include "table_ets_by_thread_by_event_name.h"
#include "table_ets_by_host_by_event_name.h"
#include "table_ets_by_user_by_event_name.h"
#include "table_ets_by_account_by_event_name.h"
#include "table_ets_global_by_event_name.h"

#include "table_users.h"
#include "table_accounts.h"
#include "table_hosts.h"

#include "table_socket_instances.h"
#include "table_socket_summary_by_instance.h"
#include "table_socket_summary_by_event_name.h"
#include "table_session_connect_attrs.h"
#include "table_session_account_connect_attrs.h"
#include "table_mems_global_by_event_name.h"
#include "table_mems_by_account_by_event_name.h"
#include "table_mems_by_host_by_event_name.h"
#include "table_mems_by_thread_by_event_name.h"
#include "table_mems_by_user_by_event_name.h"

/* For replication related perfschema tables. */
#include "table_replication_connection_configuration.h"
#include "table_replication_group_members.h"
#include "table_replication_connection_status.h"
#include "table_replication_applier_configuration.h"
#include "table_replication_applier_status.h"
#include "table_replication_applier_status_by_coordinator.h"
#include "table_replication_applier_status_by_worker.h"
#include "table_replication_group_member_stats.h"

#include "table_prepared_stmt_instances.h"

#include "table_md_locks.h"
#include "table_table_handles.h"

#include "table_uvar_by_thread.h"

#include "table_status_by_account.h"
#include "table_status_by_host.h"
#include "table_status_by_thread.h"
#include "table_status_by_user.h"
#include "table_global_status.h"
#include "table_session_status.h"

#include "table_variables_by_thread.h"
#include "table_global_variables.h"
#include "table_session_variables.h"

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
#include "log.h"

/**
  @addtogroup Performance_schema_engine
  @{
*/

bool PFS_table_context::initialize(void)
{
  if (m_restore)
  {
    /* Restore context from TLS. */
    PFS_table_context *context= static_cast<PFS_table_context *>(my_get_thread_local(m_thr_key));
    DBUG_ASSERT(context != NULL);

    if(context)
    {
      m_last_version= context->m_current_version;
      m_map= context->m_map;
      DBUG_ASSERT(m_map_size == context->m_map_size);
      m_map_size= context->m_map_size;
      m_word_size= context->m_word_size;
    }
  }
  else
  {
    /* Check that TLS is not in use. */
    PFS_table_context *context= static_cast<PFS_table_context *>(my_get_thread_local(m_thr_key));
    //DBUG_ASSERT(context == NULL);

    context= this;

    /* Initialize a new context, store in TLS. */
    m_last_version= m_current_version;
    m_map= NULL;
    m_word_size= sizeof(ulong) * 8;

    /* Allocate a bitmap to record which threads are materialized. */
    if (m_map_size > 0)
    {
      THD *thd= current_thd;
      ulong words= m_map_size / m_word_size + (m_map_size % m_word_size > 0);
      m_map= (ulong *)thd->mem_calloc(words * m_word_size);
    }

    /* Write to TLS. */
    my_set_thread_local(m_thr_key, static_cast<void *>(context));
  }

  m_initialized= (m_map_size > 0) ? (m_map != NULL) : true;

  return m_initialized;
}

/* Constructor for global or single thread tables, map size = 0.  */
PFS_table_context::PFS_table_context(ulonglong current_version, bool restore, thread_local_key_t key) :
                   m_thr_key(key), m_current_version(current_version), m_last_version(0),
                   m_map(NULL), m_map_size(0), m_word_size(sizeof(ulong)),
                   m_restore(restore), m_initialized(false), m_last_item(0)
{
  initialize();
}

/* Constructor for by-thread or aggregate tables, map size = max thread/user/host/account. */
PFS_table_context::PFS_table_context(ulonglong current_version, ulong map_size, bool restore, thread_local_key_t key) :
                   m_thr_key(key), m_current_version(current_version), m_last_version(0),
                   m_map(NULL), m_map_size(map_size), m_word_size(sizeof(ulong)),
                   m_restore(restore), m_initialized(false), m_last_item(0)
{
  initialize();
}

PFS_table_context::~PFS_table_context(void)
{
  /* Clear TLS after final use. */ // TODO: How is that determined?
//  if (m_restore)
//  {
//    my_set_thread_local(m_thr_key, NULL);
//  }
}

void PFS_table_context::set_item(ulong n)
{
  if (n == m_last_item)
    return;
  ulong word= n / m_word_size;
  ulong bit= n % m_word_size;
  m_map[word] |= (1 << bit);
  m_last_item= n;
}

bool PFS_table_context::is_item_set(ulong n)
{
  ulong word= n / m_word_size;
  ulong bit= n % m_word_size;
  return (m_map[word] & (1 << bit));
}


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
  &table_esms_by_program::m_share,

  &table_events_transactions_current::m_share,
  &table_events_transactions_history::m_share,
  &table_events_transactions_history_long::m_share,
  &table_ets_by_thread_by_event_name::m_share,
  &table_ets_by_account_by_event_name::m_share,
  &table_ets_by_user_by_event_name::m_share,
  &table_ets_by_host_by_event_name::m_share,
  &table_ets_global_by_event_name::m_share,

  &table_users::m_share,
  &table_accounts::m_share,
  &table_hosts::m_share,

  &table_socket_instances::m_share,
  &table_socket_summary_by_instance::m_share,
  &table_socket_summary_by_event_name::m_share,

  &table_session_connect_attrs::m_share,
  &table_session_account_connect_attrs::m_share,

  &table_mems_global_by_event_name::m_share,
  &table_mems_by_account_by_event_name::m_share,
  &table_mems_by_host_by_event_name::m_share,
  &table_mems_by_thread_by_event_name::m_share,
  &table_mems_by_user_by_event_name::m_share,
  &table_table_handles::m_share,
  &table_metadata_locks::m_share,

  &table_replication_connection_configuration::m_share,
  &table_replication_group_members::m_share,
  &table_replication_connection_status::m_share,
  &table_replication_applier_configuration::m_share,
  &table_replication_applier_status::m_share,
  &table_replication_applier_status_by_coordinator::m_share,
  &table_replication_applier_status_by_worker::m_share,
  &table_replication_group_member_stats::m_share,

  &table_prepared_stmt_instances::m_share,

  &table_uvar_by_thread::m_share,
  &table_status_by_account::m_share,
  &table_status_by_host::m_share,
  &table_status_by_thread::m_share,
  &table_status_by_user::m_share,
  &table_global_status::m_share,
  &table_session_status::m_share,

  &table_variables_by_thread::m_share,
  &table_global_variables::m_share,
  &table_session_variables::m_share,

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

  if (! open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
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
  return m_get_row_count();
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
    return native_strcasecmp(name1, name2);
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

void PFS_engine_table::set_field_long(Field *f, long value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_LONG);
  Field_long *f2= (Field_long*) f;
  f2->store(value, false);
}

void PFS_engine_table::set_field_ulong(Field *f, ulong value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_LONG);
  Field_long *f2= (Field_long*) f;
  f2->store(value, true);
}

void PFS_engine_table::set_field_longlong(Field *f, longlong value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_LONGLONG);
  Field_longlong *f2= (Field_longlong*) f;
  f2->store(value, false);
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

void PFS_engine_table::set_field_varchar(Field *f,
                                         const CHARSET_INFO *cs,
                                         const char* str,
                                         uint len)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_VARCHAR);
  Field_varstring *f2= (Field_varstring*) f;
  f2->store(str, len, cs);
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

void PFS_engine_table::set_field_blob(Field *f, const char* val,
                                      uint len)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_BLOB);
  Field_blob *f2= (Field_blob*) f;
  f2->store(val, len, &my_charset_utf8_bin);
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

void PFS_engine_table::set_field_double(Field *f, double value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_DOUBLE);
  Field_double *f2= (Field_double*) f;
  f2->store(value);
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
    ACL_internal_schema_registry::register_schema(PERFORMANCE_SCHEMA_str,
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


PFS_readonly_world_acl pfs_readonly_world_acl;

ACL_internal_access_result
PFS_readonly_world_acl::check(ulong want_access, ulong *save_priv) const
{
  ACL_internal_access_result res= PFS_readonly_acl::check(want_access, save_priv);
  if (res == ACL_INTERNAL_ACCESS_CHECK_GRANT)
    res= ACL_INTERNAL_ACCESS_GRANTED;
  return res;
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


PFS_truncatable_world_acl pfs_truncatable_world_acl;

ACL_internal_access_result
PFS_truncatable_world_acl::check(ulong want_access, ulong *save_priv) const
{
  ACL_internal_access_result res= PFS_truncatable_acl::check(want_access, save_priv);
  if (res == ACL_INTERNAL_ACCESS_CHECK_GRANT)
    res= ACL_INTERNAL_ACCESS_GRANTED;
  return res;
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
  size_t size;

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

  size_t total_memory= 0;

  for (i=0; /* empty */; i++)
  {
    switch (i){
    case 0:
      name= "events_waits_current.size";
      size= sizeof(PFS_events_waits);
      break;
    case 1:
      name= "events_waits_current.count";
      size= WAIT_STACK_SIZE * global_thread_container.get_row_count();
      break;
    case 2:
      name= "events_waits_history.size";
      size= sizeof(PFS_events_waits);
      break;
    case 3:
      name= "events_waits_history.count";
      size= events_waits_history_per_thread * global_thread_container.get_row_count();
      break;
    case 4:
      name= "events_waits_history.memory";
      size= events_waits_history_per_thread * global_thread_container.get_row_count()
        * sizeof(PFS_events_waits);
      total_memory+= size;
      break;
    case 5:
      name= "events_waits_history_long.size";
      size= sizeof(PFS_events_waits);
      break;
    case 6:
      name= "events_waits_history_long.count";
      size= events_waits_history_long_size;
      break;
    case 7:
      name= "events_waits_history_long.memory";
      size= events_waits_history_long_size * sizeof(PFS_events_waits);
      total_memory+= size;
      break;
    case 8:
      name= "(pfs_mutex_class).size";
      size= sizeof(PFS_mutex_class);
      break;
    case 9:
      name= "(pfs_mutex_class).count";
      size= mutex_class_max;
      break;
    case 10:
      name= "(pfs_mutex_class).memory";
      size= mutex_class_max * sizeof(PFS_mutex_class);
      total_memory+= size;
      break;
    case 11:
      name= "(pfs_rwlock_class).size";
      size= sizeof(PFS_rwlock_class);
      break;
    case 12:
      name= "(pfs_rwlock_class).count";
      size= rwlock_class_max;
      break;
    case 13:
      name= "(pfs_rwlock_class).memory";
      size= rwlock_class_max * sizeof(PFS_rwlock_class);
      total_memory+= size;
      break;
    case 14:
      name= "(pfs_cond_class).size";
      size= sizeof(PFS_cond_class);
      break;
    case 15:
      name= "(pfs_cond_class).count";
      size= cond_class_max;
      break;
    case 16:
      name= "(pfs_cond_class).memory";
      size= cond_class_max * sizeof(PFS_cond_class);
      total_memory+= size;
      break;
    case 17:
      name= "(pfs_thread_class).size";
      size= sizeof(PFS_thread_class);
      break;
    case 18:
      name= "(pfs_thread_class).count";
      size= thread_class_max;
      break;
    case 19:
      name= "(pfs_thread_class).memory";
      size= thread_class_max * sizeof(PFS_thread_class);
      total_memory+= size;
      break;
    case 20:
      name= "(pfs_file_class).size";
      size= sizeof(PFS_file_class);
      break;
    case 21:
      name= "(pfs_file_class).count";
      size= file_class_max;
      break;
    case 22:
      name= "(pfs_file_class).memory";
      size= file_class_max * sizeof(PFS_file_class);
      total_memory+= size;
      break;
    case 23:
      name= "mutex_instances.size";
      size= global_mutex_container.get_row_size();
      break;
    case 24:
      name= "mutex_instances.count";
      size= global_mutex_container.get_row_count();
      break;
    case 25:
      name= "mutex_instances.memory";
      size= global_mutex_container.get_memory();
      total_memory+= size;
      break;
    case 26:
      name= "rwlock_instances.size";
      size= global_rwlock_container.get_row_size();
      break;
    case 27:
      name= "rwlock_instances.count";
      size= global_rwlock_container.get_row_count();
      break;
    case 28:
      name= "rwlock_instances.memory";
      size= global_rwlock_container.get_memory();
      total_memory+= size;
      break;
    case 29:
      name= "cond_instances.size";
      size= global_cond_container.get_row_size();
      break;
    case 30:
      name= "cond_instances.count";
      size= global_cond_container.get_row_count();
      break;
    case 31:
      name= "cond_instances.memory";
      size= global_cond_container.get_memory();
      total_memory+= size;
      break;
    case 32:
      name= "threads.size";
      size= global_thread_container.get_row_size();
      break;
    case 33:
      name= "threads.count";
      size= global_thread_container.get_row_count();
      break;
    case 34:
      name= "threads.memory";
      size= global_thread_container.get_memory();
      total_memory+= size;
      break;
    case 35:
      name= "file_instances.size";
      size= global_file_container.get_row_size();
      break;
    case 36:
      name= "file_instances.count";
      size= global_file_container.get_row_count();
      break;
    case 37:
      name= "file_instances.memory";
      size= global_file_container.get_memory();
      total_memory+= size;
      break;
    case 38:
      name= "(pfs_file_handle).size";
      size= sizeof(PFS_file*);
      break;
    case 39:
      name= "(pfs_file_handle).count";
      size= file_handle_max;
      break;
    case 40:
      name= "(pfs_file_handle).memory";
      size= file_handle_max * sizeof(PFS_file*);
      total_memory+= size;
      break;
    case 41:
      name= "events_waits_summary_by_thread_by_event_name.size";
      size= sizeof(PFS_single_stat);
      break;
    case 42:
      name= "events_waits_summary_by_thread_by_event_name.count";
      size= global_thread_container.get_row_count() * wait_class_max;
      break;
    case 43:
      name= "events_waits_summary_by_thread_by_event_name.memory";
      size= global_thread_container.get_row_count() * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 44:
      name= "(pfs_table_share).size";
      size= global_table_share_container.get_row_size();
      break;
    case 45:
      name= "(pfs_table_share).count";
      size= global_table_share_container.get_row_count();
      break;
    case 46:
      name= "(pfs_table_share).memory";
      size= global_table_share_container.get_memory();
      total_memory+= size;
      break;
    case 47:
      name= "(pfs_table).size";
      size= global_table_container.get_row_size();
      break;
    case 48:
      name= "(pfs_table).count";
      size= global_table_container.get_row_count();
      break;
    case 49:
      name= "(pfs_table).memory";
      size= global_table_container.get_memory();
      total_memory+= size;
      break;
    case 50:
      name= "setup_actors.size";
      size= global_setup_actor_container.get_row_size();
      break;
    case 51:
      name= "setup_actors.count";
      size= global_setup_actor_container.get_row_count();
      break;
    case 52:
      name= "setup_actors.memory";
      size= global_setup_actor_container.get_memory();
      total_memory+= size;
      break;
    case 53:
      name= "setup_objects.size";
      size= global_setup_object_container.get_row_size();
      break;
    case 54:
      name= "setup_objects.count";
      size= global_setup_object_container.get_row_count();
      break;
    case 55:
      name= "setup_objects.memory";
      size= global_setup_object_container.get_memory();
      total_memory+= size;
      break;
    case 56:
      name= "(pfs_account).size";
      size= global_account_container.get_row_size();
      break;
    case 57:
      name= "(pfs_account).count";
      size= global_account_container.get_row_count();
      break;
    case 58:
      name= "(pfs_account).memory";
      size= global_account_container.get_memory();
      total_memory+= size;
      break;
    case 59:
      name= "events_waits_summary_by_account_by_event_name.size";
      size= sizeof(PFS_single_stat);
      break;
    case 60:
      name= "events_waits_summary_by_account_by_event_name.count";
      size= global_account_container.get_row_count() * wait_class_max;
      break;
    case 61:
      name= "events_waits_summary_by_account_by_event_name.memory";
      size= global_account_container.get_row_count() * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 62:
      name= "events_waits_summary_by_user_by_event_name.size";
      size= sizeof(PFS_single_stat);
      break;
    case 63:
      name= "events_waits_summary_by_user_by_event_name.count";
      size= global_user_container.get_row_count() * wait_class_max;
      break;
    case 64:
      name= "events_waits_summary_by_user_by_event_name.memory";
      size= global_user_container.get_row_count() * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 65:
      name= "events_waits_summary_by_host_by_event_name.size";
      size= sizeof(PFS_single_stat);
      break;
    case 66:
      name= "events_waits_summary_by_host_by_event_name.count";
      size= global_host_container.get_row_count() * wait_class_max;
      break;
    case 67:
      name= "events_waits_summary_by_host_by_event_name.memory";
      size= global_host_container.get_row_count() * wait_class_max * sizeof(PFS_single_stat);
      total_memory+= size;
      break;
    case 68:
      name= "(pfs_user).size";
      size= global_user_container.get_row_size();
      break;
    case 69:
      name= "(pfs_user).count";
      size= global_user_container.get_row_count();
      break;
    case 70:
      name= "(pfs_user).memory";
      size= global_user_container.get_memory();
      total_memory+= size;
      break;
    case 71:
      name= "(pfs_host).size";
      size= global_host_container.get_row_size();
      break;
    case 72:
      name= "(pfs_host).count";
      size= global_host_container.get_row_count();
      break;
    case 73:
      name= "(pfs_host).memory";
      size= global_host_container.get_memory();
      total_memory+= size;
      break;
    case 74:
      name= "(pfs_stage_class).size";
      size= sizeof(PFS_stage_class);
      break;
    case 75:
      name= "(pfs_stage_class).count";
      size= stage_class_max;
      break;
    case 76:
      name= "(pfs_stage_class).memory";
      size= stage_class_max * sizeof(PFS_stage_class);
      total_memory+= size;
      break;
    case 77:
      name= "events_stages_history.size";
      size= sizeof(PFS_events_stages);
      break;
    case 78:
      name= "events_stages_history.count";
      size= events_stages_history_per_thread * global_thread_container.get_row_count();
      break;
    case 79:
      name= "events_stages_history.memory";
      size= events_stages_history_per_thread * global_thread_container.get_row_count()
        * sizeof(PFS_events_stages);
      total_memory+= size;
      break;
    case 80:
      name= "events_stages_history_long.size";
      size= sizeof(PFS_events_stages);
      break;
    case 81:
      name= "events_stages_history_long.count";
      size= events_stages_history_long_size;
      break;
    case 82:
      name= "events_stages_history_long.memory";
      size= events_stages_history_long_size * sizeof(PFS_events_stages);
      total_memory+= size;
      break;
    case 83:
      name= "events_stages_summary_by_thread_by_event_name.size";
      size= sizeof(PFS_stage_stat);
      break;
    case 84:
      name= "events_stages_summary_by_thread_by_event_name.count";
      size= global_thread_container.get_row_count() * stage_class_max;
      break;
    case 85:
      name= "events_stages_summary_by_thread_by_event_name.memory";
      size= global_thread_container.get_row_count() * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 86:
      name= "events_stages_summary_global_by_event_name.size";
      size= sizeof(PFS_stage_stat);
      break;
    case 87:
      name= "events_stages_summary_global_by_event_name.count";
      size= stage_class_max;
      break;
    case 88:
      name= "events_stages_summary_global_by_event_name.memory";
      size= stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 89:
      name= "events_stages_summary_by_account_by_event_name.size";
      size= sizeof(PFS_stage_stat);
      break;
    case 90:
      name= "events_stages_summary_by_account_by_event_name.count";
      size= global_account_container.get_row_count() * stage_class_max;
      break;
    case 91:
      name= "events_stages_summary_by_account_by_event_name.memory";
      size= global_account_container.get_row_count() * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 92:
      name= "events_stages_summary_by_user_by_event_name.size";
      size= sizeof(PFS_stage_stat);
      break;
    case 93:
      name= "events_stages_summary_by_user_by_event_name.count";
      size= global_user_container.get_row_count() * stage_class_max;
      break;
    case 94:
      name= "events_stages_summary_by_user_by_event_name.memory";
      size= global_user_container.get_row_count() * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 95:
      name= "events_stages_summary_by_host_by_event_name.size";
      size= sizeof(PFS_stage_stat);
      break;
    case 96:
      name= "events_stages_summary_by_host_by_event_name.count";
      size= global_host_container.get_row_count() * stage_class_max;
      break;
    case 97:
      name= "events_stages_summary_by_host_by_event_name.memory";
      size= global_host_container.get_row_count() * stage_class_max * sizeof(PFS_stage_stat);
      total_memory+= size;
      break;
    case 98:
      name= "(pfs_statement_class).size";
      size= sizeof(PFS_statement_class);
      break;
    case 99:
      name= "(pfs_statement_class).count";
      size= statement_class_max;
      break;
    case 100:
      name= "(pfs_statement_class).memory";
      size= statement_class_max * sizeof(PFS_statement_class);
      total_memory+= size;
      break;
    case 101:
      name= "events_statements_history.size";
      size= sizeof(PFS_events_statements);
      break;
    case 102:
      name= "events_statements_history.count";
      size= events_statements_history_per_thread * global_thread_container.get_row_count();
      break;
    case 103:
      name= "events_statements_history.memory";
      size= events_statements_history_per_thread * global_thread_container.get_row_count()
        * sizeof(PFS_events_statements);
      total_memory+= size;
      break;
    case 104:
      name= "events_statements_history_long.size";
      size= sizeof(PFS_events_statements);
      break;
    case 105:
      name= "events_statements_history_long.count";
      size= events_statements_history_long_size;
      break;
    case 106:
      name= "events_statements_history_long.memory";
      size= events_statements_history_long_size * (sizeof(PFS_events_statements));
      total_memory+= size;
      break;
    case 107:
      name= "events_statements_summary_by_thread_by_event_name.size";
      size= sizeof(PFS_statement_stat);
      break;
    case 108:
      name= "events_statements_summary_by_thread_by_event_name.count";
      size= global_thread_container.get_row_count() * statement_class_max;
      break;
    case 109:
      name= "events_statements_summary_by_thread_by_event_name.memory";
      size= global_thread_container.get_row_count() * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 110:
      name= "events_statements_summary_global_by_event_name.size";
      size= sizeof(PFS_statement_stat);
      break;
    case 111:
      name= "events_statements_summary_global_by_event_name.count";
      size= statement_class_max;
      break;
    case 112:
      name= "events_statements_summary_global_by_event_name.memory";
      size= statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 113:
      name= "events_statements_summary_by_account_by_event_name.size";
      size= sizeof(PFS_statement_stat);
      break;
    case 114:
      name= "events_statements_summary_by_account_by_event_name.count";
      size= global_account_container.get_row_count() * statement_class_max;
      break;
    case 115:
      name= "events_statements_summary_by_account_by_event_name.memory";
      size= global_account_container.get_row_count() * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 116:
      name= "events_statements_summary_by_user_by_event_name.size";
      size= sizeof(PFS_statement_stat);
      break;
    case 117:
      name= "events_statements_summary_by_user_by_event_name.count";
      size= global_user_container.get_row_count() * statement_class_max;
      break;
    case 118:
      name= "events_statements_summary_by_user_by_event_name.memory";
      size= global_user_container.get_row_count() * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 119:
      name= "events_statements_summary_by_host_by_event_name.size";
      size= sizeof(PFS_statement_stat);
      break;
    case 120:
      name= "events_statements_summary_by_host_by_event_name.count";
      size= global_host_container.get_row_count() * statement_class_max;
      break;
    case 121:
      name= "events_statements_summary_by_host_by_event_name.memory";
      size= global_host_container.get_row_count() * statement_class_max * sizeof(PFS_statement_stat);
      total_memory+= size;
      break;
    case 122:
      name= "events_statements_current.size";
      size= sizeof(PFS_events_statements);
      break;
    case 123:
      name= "events_statements_current.count";
      size= global_thread_container.get_row_count() * statement_stack_max;
      break;
    case 124:
      name= "events_statements_current.memory";
      size= global_thread_container.get_row_count() * statement_stack_max * sizeof(PFS_events_statements);
      total_memory+= size;
      break;
    case 125:
      name= "(pfs_socket_class).size";
      size= sizeof(PFS_socket_class);
      break;
    case 126:
      name= "(pfs_socket_class).count";
      size= socket_class_max;
      break;
    case 127:
      name= "(pfs_socket_class).memory";
      size= socket_class_max * sizeof(PFS_socket_class);
      total_memory+= size;
      break;
    case 128:
      name= "socket_instances.size";
      size= global_socket_container.get_row_size();
      break;
    case 129:
      name= "socket_instances.count";
      size= global_socket_container.get_row_count();
      break;
    case 130:
      name= "socket_instances.memory";
      size= global_socket_container.get_memory();
      total_memory+= size;
      break;
    case 131:
      name= "events_statements_summary_by_digest.size";
      size= sizeof(PFS_statements_digest_stat);
      break;
    case 132:
      name= "events_statements_summary_by_digest.count";
      size= digest_max;
      break;
    case 133:
      name= "events_statements_summary_by_digest.memory";
      size= digest_max * (sizeof(PFS_statements_digest_stat));
      total_memory+= size;
      break;
    case 134:
      name= "events_statements_summary_by_program.size";
      size= global_program_container.get_row_size();
      break;
    case 135:
      name= "events_statements_summary_by_program.count";
      size= global_program_container.get_row_count();
      break;
    case 136:
      name= "events_statements_summary_by_program.memory";
      size= global_program_container.get_memory();
      total_memory+= size;
      break;
    case 137:
      name= "session_connect_attrs.size";
      size= global_thread_container.get_row_count();
      break;
    case 138:
      name= "session_connect_attrs.count";
      size= session_connect_attrs_size_per_thread;
      break;
    case 139:
      name= "session_connect_attrs.memory";
      size= global_thread_container.get_row_count() * session_connect_attrs_size_per_thread;
      total_memory+= size;
      break;
    case 140:
      name= "prepared_statements_instances.size";
      size= global_prepared_stmt_container.get_row_size();
      break;
    case 141:
      name= "prepared_statements_instances.count";
      size= global_prepared_stmt_container.get_row_count();
      break;
    case 142:
      name= "prepared_statements_instances.memory";
      size= global_prepared_stmt_container.get_memory();
      total_memory+= size;
      break;

    case 143:
      name= "(account_hash).count";
      size= account_hash.count;
      break;
    case 144:
      name= "(account_hash).size";
      size= account_hash.size;
      break;
    case 145:
      name= "(digest_hash).count";
      size= digest_hash.count;
      break;
    case 146:
      name= "(digest_hash).size";
      size= digest_hash.size;
      break;
    case 147:
      name= "(filename_hash).count";
      size= filename_hash.count;
      break;
    case 148:
      name= "(filename_hash).size";
      size= filename_hash.size;
      break;
    case 149:
      name= "(host_hash).count";
      size= host_hash.count;
      break;
    case 150:
      name= "(host_hash).size";
      size= host_hash.size;
      break;
    case 151:
      name= "(setup_actor_hash).count";
      size= setup_actor_hash.count;
      break;
    case 152:
      name= "(setup_actor_hash).size";
      size= setup_actor_hash.size;
      break;
    case 153:
      name= "(setup_object_hash).count";
      size= setup_object_hash.count;
      break;
    case 154:
      name= "(setup_object_hash).size";
      size= setup_object_hash.size;
      break;
    case 155:
      name= "(table_share_hash).count";
      size= table_share_hash.count;
      break;
    case 156:
      name= "(table_share_hash).size";
      size= table_share_hash.size;
      break;
    case 157:
      name= "(user_hash).count";
      size= user_hash.count;
      break;
    case 158:
      name= "(user_hash).size";
      size= user_hash.size;
      break;
    case 159:
      name= "(program_hash).count";
      size= program_hash.count;
      break;
    case 160:
      name= "(program_hash).size";
      size= program_hash.size;
      break;
    case 161:
      /*
        This is not a performance_schema buffer,
        the data is maintained in the server,
        in hostname_cache.
        Print the size only, there are:
        - no host_cache.count
        - no host_cache.memory
      */
      name= "host_cache.size";
      size= sizeof(Host_entry);
      break;

    case 162:
      name= "(pfs_memory_class).row_size";
      size= sizeof(PFS_memory_class);
      break;
    case 163:
      name= "(pfs_memory_class).row_count";
      size= memory_class_max;
      break;
    case 164:
      name= "(pfs_memory_class).memory";
      size= memory_class_max * sizeof(PFS_memory_class);
      total_memory+= size;
      break;

    case 165:
      name= "memory_summary_by_thread_by_event_name.row_size";
      size= sizeof(PFS_memory_stat);
      break;
    case 166:
      name= "memory_summary_by_thread_by_event_name.row_count";
      size= global_thread_container.get_row_count() * memory_class_max;
      break;
    case 167:
      name= "memory_summary_by_thread_by_event_name.memory";
      size= global_thread_container.get_row_count() * memory_class_max * sizeof(PFS_memory_stat);
      total_memory+= size;
      break;
    case 168:
      name= "memory_summary_global_by_event_name.row_size";
      size= sizeof(PFS_memory_stat);
      break;
    case 169:
      name= "memory_summary_global_by_event_name.row_count";
      size= memory_class_max;
      break;
    case 170:
      name= "memory_summary_global_by_event_name.memory";
      size= memory_class_max * sizeof(PFS_memory_stat);
      total_memory+= size;
      break;
    case 171:
      name= "memory_summary_by_account_by_event_name.row_size";
      size= sizeof(PFS_memory_stat);
      break;
    case 172:
      name= "memory_summary_by_account_by_event_name.row_count";
      size= global_account_container.get_row_count() * memory_class_max;
      break;
    case 173:
      name= "memory_summary_by_account_by_event_name.memory";
      size= global_account_container.get_row_count() * memory_class_max * sizeof(PFS_memory_stat);
      total_memory+= size;
      break;
    case 174:
      name= "memory_summary_by_user_by_event_name.row_size";
      size= sizeof(PFS_memory_stat);
      break;
    case 175:
      name= "memory_summary_by_user_by_event_name.row_count";
      size= global_user_container.get_row_count() * memory_class_max;
      break;
    case 176:
      name= "memory_summary_by_user_by_event_name.memory";
      size= global_user_container.get_row_count() * memory_class_max * sizeof(PFS_memory_stat);
      total_memory+= size;
      break;
    case 177:
      name= "memory_summary_by_host_by_event_name.row_size";
      size= sizeof(PFS_memory_stat);
      break;
    case 178:
      name= "memory_summary_by_host_by_event_name.row_count";
      size= global_host_container.get_row_count() * memory_class_max;
      break;
    case 179:
      name= "memory_summary_by_host_by_event_name.memory";
      size= global_host_container.get_row_count() * memory_class_max * sizeof(PFS_memory_stat);
      total_memory+= size;
      break;
    case 180:
      name= "metadata_locks.row_size";
      size= global_mdl_container.get_row_size();
      break;
    case 181:
      name= "metadata_locks.row_count";
      size= global_mdl_container.get_row_count();
      break;
    case 182:
      name= "metadata_locks.memory";
      size= global_mdl_container.get_memory();
      total_memory+= size;
      break;
    case 183:
      name= "events_transactions_history.size";
      size= sizeof(PFS_events_transactions);
      break;
    case 184:
      name= "events_transactions_history.count";
      size= events_transactions_history_per_thread * global_thread_container.get_row_count();
      break;
    case 185:
      name= "events_transactions_history.memory";
      size= events_transactions_history_per_thread * global_thread_container.get_row_count()
        * sizeof(PFS_events_transactions);
      total_memory+= size;
      break;
    case 186:
      name= "events_transactions_history_long.size";
      size= sizeof(PFS_events_transactions);
      break;
    case 187:
      name= "events_transactions_history_long.count";
      size= events_transactions_history_long_size;
      break;
    case 188:
      name= "events_transactions_history_long.memory";
      size= events_transactions_history_long_size * sizeof(PFS_events_transactions);
      total_memory+= size;
      break;
    case 189:
      name= "events_transactions_summary_by_thread_by_event_name.size";
      size= sizeof(PFS_transaction_stat);
      break;
    case 190:
      name= "events_transactions_summary_by_thread_by_event_name.count";
      size= global_thread_container.get_row_count() * transaction_class_max;
      break;
    case 191:
      name= "events_transactions_summary_by_thread_by_event_name.memory";
      size= global_thread_container.get_row_count() * transaction_class_max * sizeof(PFS_transaction_stat);
      total_memory+= size;
      break;
    case 192:
      name= "events_transactions_summary_by_account_by_event_name.size";
      size= sizeof(PFS_transaction_stat);
      break;
    case 193:
      name= "events_transactions_summary_by_account_by_event_name.count";
      size= global_account_container.get_row_count() * transaction_class_max;
      break;
    case 194:
      name= "events_transactions_summary_by_account_by_event_name.memory";
      size= global_account_container.get_row_count() * transaction_class_max * sizeof(PFS_transaction_stat);
      total_memory+= size;
      break;
    case 195:
      name= "events_transactions_summary_by_user_by_event_name.size";
      size= sizeof(PFS_transaction_stat);
      break;
    case 196:
      name= "events_transactions_summary_by_user_by_event_name.count";
      size= global_user_container.get_row_count() * transaction_class_max;
      break;
    case 197:
      name= "events_transactions_summary_by_user_by_event_name.memory";
      size= global_user_container.get_row_count() * transaction_class_max * sizeof(PFS_transaction_stat);
      total_memory+= size;
      break;
    case 198:
      name= "events_transactions_summary_by_host_by_event_name.size";
      size= sizeof(PFS_transaction_stat);
      break;
    case 199:
      name= "events_transactions_summary_by_host_by_event_name.count";
      size= global_host_container.get_row_count() * transaction_class_max;
      break;
    case 200:
      name= "events_transactions_summary_by_host_by_event_name.memory";
      size= global_host_container.get_row_count() * transaction_class_max * sizeof(PFS_transaction_stat);
      total_memory+= size;
      break;
    case 201:
      name= "table_lock_waits_summary_by_table.size";
      size= global_table_share_lock_container.get_row_size();
      break;
    case 202:
      name= "table_lock_waits_summary_by_table.count";
      size= global_table_share_lock_container.get_row_count();
      break;
    case 203:
      name= "table_lock_waits_summary_by_table.memory";
      size= global_table_share_lock_container.get_memory();
      total_memory+= size;
      break;
    case 204:
      name= "table_io_waits_summary_by_index_usage.size";
      size= global_table_share_index_container.get_row_size();
      break;
    case 205:
      name= "table_io_waits_summary_by_index_usage.count";
      size= global_table_share_index_container.get_row_count();
      break;
    case 206:
      name= "table_io_waits_summary_by_index_usage.memory";
      size= global_table_share_index_container.get_memory();
      total_memory+= size;
      break;
    case 207:
      name= "(history_long_statements_digest_token_array).count";
      size= events_statements_history_long_size;
      break;
    case 208:
      name= "(history_long_statements_digest_token_array).size";
      size= pfs_max_digest_length;
      break;
    case 209:
      name= "(history_long_statements_digest_token_array).memory";
      size= events_statements_history_long_size * pfs_max_digest_length;
      total_memory+= size;
      break;
    case 210:
      name= "(history_statements_digest_token_array).count";
      size= global_thread_container.get_row_count() * events_statements_history_per_thread;
      break;
    case 211:
      name= "(history_statements_digest_token_array).size";
      size= pfs_max_digest_length;
      break;
    case 212:
      name= "(history_statements_digest_token_array).memory";
      size= global_thread_container.get_row_count() * events_statements_history_per_thread * pfs_max_digest_length;
      total_memory+= size;
      break;
    case 213:
      name= "(current_statements_digest_token_array).count";
      size= global_thread_container.get_row_count() * statement_stack_max;
      break;
    case 214:
      name= "(current_statements_digest_token_array).size";
      size= pfs_max_digest_length;
      break;
    case 215:
      name= "(current_statements_digest_token_array).memory";
      size= global_thread_container.get_row_count() * statement_stack_max * pfs_max_digest_length;
      total_memory+= size;
      break;
    case 216:
      name= "(history_long_statements_text_array).count";
      size= events_statements_history_long_size;
      break;
    case 217:
      name= "(history_long_statements_text_array).size";
      size= pfs_max_sqltext;
      break;
    case 218:
      name= "(history_long_statements_text_array).memory";
      size= events_statements_history_long_size * pfs_max_sqltext;
      total_memory+= size;
      break;
    case 219:
      name= "(history_statements_text_array).count";
      size= global_thread_container.get_row_count() * events_statements_history_per_thread;
      break;
    case 220:
      name= "(history_statements_text_array).size";
      size= pfs_max_sqltext;
      break;
    case 221:
      name= "(history_statements_text_array).memory";
      size= global_thread_container.get_row_count() * events_statements_history_per_thread * pfs_max_sqltext;
      total_memory+= size;
      break;
    case 222:
      name= "(current_statements_text_array).count";
      size= global_thread_container.get_row_count() * statement_stack_max;
      break;
    case 223:
      name= "(current_statements_text_array).size";
      size= pfs_max_sqltext;
      break;
    case 224:
      name= "(current_statements_text_array).memory";
      size= global_thread_container.get_row_count() * statement_stack_max * pfs_max_sqltext;
      total_memory+= size;
      break;
    case 225:
      name= "(statements_digest_token_array).count";
      size= digest_max;
      break;
    case 226:
      name= "(statements_digest_token_array).size";
      size= pfs_max_digest_length;
      break;
    case 227:
      name= "(statements_digest_token_array).memory";
      size= digest_max * pfs_max_digest_length;
      total_memory+= size;
      break;
    /*
      This case must be last,
      for aggregation in total_memory.
    */
    case 228:
      name= "performance_schema.memory";
      size= total_memory;
      break;
    default:
      goto end;
      break;
    }

    buflen= (uint)(longlong10_to_str(size, buf, 10) - buf);
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

