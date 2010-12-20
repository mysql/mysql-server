/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "pfs_engine_table.h"

#include "table_events_waits.h"
#include "table_setup_consumers.h"
#include "table_setup_instruments.h"
#include "table_setup_timers.h"
#include "table_performance_timers.h"
#include "table_threads.h"
#include "table_events_waits_summary.h"
#include "table_ews_global_by_event_name.h"
#include "table_sync_instances.h"
#include "table_file_instances.h"
#include "table_file_summary.h"

/* For show status */
#include "pfs_column_values.h"
#include "pfs_instr.h"
#include "pfs_global.h"

#include "sql_base.h"                           // close_thread_tables
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT

/**
  @addtogroup Performance_schema_engine
  @{
*/

static PFS_engine_table_share *all_shares[]=
{
  &table_events_waits_current::m_share,
  &table_events_waits_history::m_share,
  &table_events_waits_history_long::m_share,
  &table_setup_consumers::m_share,
  &table_setup_instruments::m_share,
  &table_setup_timers::m_share,
  &table_performance_timers::m_share,
  &table_threads::m_share,
  &table_events_waits_summary_by_thread_by_event_name::m_share,
  &table_events_waits_summary_by_instance::m_share,
  &table_ews_global_by_event_name::m_share,
  &table_file_summary_by_event_name::m_share,
  &table_file_summary_by_instance::m_share,
  &table_mutex_instances::m_share,
  &table_rwlock_instances::m_share,
  &table_cond_instances::m_share,
  &table_file_instances::m_share,
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
                    all_shares[4]->m_field_def->count++;
                  });

  for (current= &all_shares[0]; (*current) != NULL; current++)
    (*current)->check_one_table(thd);
}

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

void PFS_engine_table::set_field_varchar_utf8(Field *f, const char* str,
                                              uint len)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_VARCHAR);
  Field_varstring *f2= (Field_varstring*) f;
  f2->store(str, len, &my_charset_utf8_bin);
}

void PFS_engine_table::set_field_enum(Field *f, ulonglong value)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_ENUM);
  Field_enum *f2= (Field_enum*) f;
  f2->store_type(value);
}

ulonglong PFS_engine_table::get_field_enum(Field *f)
{
  DBUG_ASSERT(f->real_type() == MYSQL_TYPE_ENUM);
  Field_enum *f2= (Field_enum*) f;
  return f2->val_int();
}

int PFS_engine_table::update_row_values(TABLE *,
                                        const unsigned char *,
                                        unsigned char *,
                                        Field **)
{
  return HA_ERR_WRONG_COMMAND;
}

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
  const ulong always_forbidden= INSERT_ACL | UPDATE_ACL | DELETE_ACL
    | CREATE_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL
    | CREATE_VIEW_ACL | TRIGGER_ACL | LOCK_TABLES_ACL;

  if (unlikely(want_access & always_forbidden))
    return ACL_INTERNAL_ACCESS_DENIED;

  /*
    There is no point in hidding (by enforcing ACCESS_DENIED for SELECT_ACL
    on performance_schema.*) tables that do not exist anyway.
    When SELECT_ACL is granted on performance_schema.* or *.*,
    SELECT * from performance_schema.wrong_table
    will fail with a more understandable ER_NO_SUCH_TABLE error,
    instead of ER_TABLEACCESS_DENIED_ERROR.
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
      size= sizeof(PFS_wait_locker);
      break;
    case 1:
      name= "events_waits_current.row_count";
      size= LOCKER_STACK_SIZE * thread_max;
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
      size= sizeof(PFS_single_stat_chain);
      break;
    case 42:
      name= "events_waits_summary_by_thread_by_event_name.row_count";
      size= thread_max * instr_class_per_thread;
      break;
    case 43:
      name= "events_waits_summary_by_thread_by_event_name.memory";
      size= thread_max * instr_class_per_thread * sizeof(PFS_single_stat_chain);
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
    /*
      This case must be last,
      for aggregation in total_memory.
    */
    case 50:
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


