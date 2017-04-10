/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "storage/perfschema/pfs_engine_table.h"

#include "current_thd.h"
#include "derror.h"
#include "lock.h"  // MYSQL_LOCK_IGNORE_TIMEOUT
#include "log.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_thread.h"
#include "mysqld.h" /* lower_case_table_names */
#include "pfs_buffer_container.h"
/* For show status */
#include "pfs_column_values.h"
#include "pfs_digest.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "pfs_instr_class.h"
#include "pfs_setup_actor.h"
#include "pfs_setup_object.h"
#include "sql_base.h"  // close_thread_tables
#include "sql_class.h"
#include "table_accounts.h"
#include "table_data_lock_waits.h"
#include "table_data_locks.h"
#include "table_ees_by_account_by_error.h"
#include "table_ees_by_host_by_error.h"
#include "table_ees_by_thread_by_error.h"
#include "table_ees_by_user_by_error.h"
#include "table_ees_global_by_error.h"
#include "table_esgs_by_account_by_event_name.h"
#include "table_esgs_by_host_by_event_name.h"
#include "table_esgs_by_thread_by_event_name.h"
#include "table_esgs_by_user_by_event_name.h"
#include "table_esgs_global_by_event_name.h"
#include "table_esms_by_account_by_event_name.h"
#include "table_esms_by_digest.h"
#include "table_esms_by_host_by_event_name.h"
#include "table_esms_by_program.h"
#include "table_esmh_global.h"
#include "table_esmh_by_digest.h"
#include "table_esms_by_thread_by_event_name.h"
#include "table_esms_by_user_by_event_name.h"
#include "table_esms_global_by_event_name.h"
#include "table_ets_by_account_by_event_name.h"
#include "table_ets_by_host_by_event_name.h"
#include "table_ets_by_thread_by_event_name.h"
#include "table_ets_by_user_by_event_name.h"
#include "table_ets_global_by_event_name.h"
#include "table_events_stages.h"
#include "table_events_statements.h"
#include "table_events_transactions.h"
#include "table_events_waits.h"
#include "table_events_waits_summary.h"
#include "table_ews_by_account_by_event_name.h"
#include "table_ews_by_host_by_event_name.h"
#include "table_ews_by_thread_by_event_name.h"
#include "table_ews_by_user_by_event_name.h"
#include "table_ews_global_by_event_name.h"
#include "table_file_instances.h"
#include "table_file_summary_by_event_name.h"
#include "table_file_summary_by_instance.h"
#include "table_global_status.h"
#include "table_global_variables.h"
#include "table_host_cache.h"
#include "table_hosts.h"
#include "table_md_locks.h"
#include "table_mems_by_account_by_event_name.h"
#include "table_mems_by_host_by_event_name.h"
#include "table_mems_by_thread_by_event_name.h"
#include "table_mems_by_user_by_event_name.h"
#include "table_mems_global_by_event_name.h"
#include "table_os_global_by_type.h"
#include "table_performance_timers.h"
#include "table_prepared_stmt_instances.h"
#include "table_replication_applier_configuration.h"
#include "table_replication_applier_status.h"
#include "table_replication_applier_status_by_coordinator.h"
#include "table_replication_applier_status_by_worker.h"
/* For replication related perfschema tables. */
#include "table_replication_connection_configuration.h"
#include "table_replication_connection_status.h"
#include "table_replication_group_member_stats.h"
#include "table_replication_group_members.h"
#include "table_replication_applier_filters.h"
#include "table_replication_applier_global_filters.h"
#include "table_session_account_connect_attrs.h"
#include "table_session_connect_attrs.h"
#include "table_session_status.h"
#include "table_session_variables.h"
#include "table_setup_actors.h"
#include "table_setup_consumers.h"
#include "table_setup_instruments.h"
#include "table_setup_objects.h"
#include "table_setup_timers.h"
#include "table_socket_instances.h"
#include "table_socket_summary_by_event_name.h"
#include "table_socket_summary_by_instance.h"
#include "table_status_by_account.h"
#include "table_status_by_host.h"
#include "table_status_by_thread.h"
#include "table_status_by_user.h"
#include "table_sync_instances.h"
#include "table_table_handles.h"
#include "table_threads.h"
#include "table_tiws_by_index_usage.h"
#include "table_tiws_by_table.h"
#include "table_tlws_by_table.h"
#include "table_users.h"
#include "table_uvar_by_thread.h"
#include "table_variables_by_thread.h"
#include "table_variables_info.h"
#include "table_persisted_variables.h"

/**
  @page PAGE_PFS_NEW_TABLE Implementing a new performance_schema table

  To implement a new performance schema table,
  two independent problems need to be resolved:
  - HOW to expose the data as a SQL table, usable to user queries.
  - WHAT data to expose, and where is this data coming from.

  The storage engine interface is used to implement the former.
  Various design patterns can be used to resolve the later.

  @section NEW_TABLE_INTERFACE Storage engine interface

  @startuml

  actor mon as "Monitoring Client"
  participant server as "MySQL server"
  participant pfs as "Performance schema\n engine"
  participant pfs_table as "Performance schema\n table"

  == query start ==
  mon -> server : performance_schema query

  == opening table ==
  server -> pfs : ha_perfschema::open()
  activate pfs_table
  pfs -> pfs_table : create()
  server -> pfs : ha_perfschema::rnd_init()
  pfs -> pfs_table : rnd_init()

  == for each row ==
  server -> pfs : ha_perfschema::rnd_next()
  pfs -> pfs_table : rnd_next()
  server -> pfs : ha_perfschema::read_row_values()
  pfs -> pfs_table : read_row_values()
  mon <-- server : result set row

  == closing table ==
  server -> pfs : ha_perfschema::rnd_end()
  pfs -> pfs_table : rnd_end()
  server -> pfs : ha_perfschema::close()
  pfs -> pfs_table : destroy()
  deactivate pfs_table

  == query end ==
  mon <-- server : query end

  @enduml

  The performance schema engine, @c ha_perfschema,
  exposes performance schema tables to the MySQL server.
  Tables are implemented by sub classing @c PFS_engine_table.

  The access pattern is always the same for all performance schema tables,
  and does not depend on what data is exposed, only on how it is exposed.

  @section NEW_TABLE_APPLICATION Application query

  @startuml

  actor client as "Application Client"
  participant server as "MySQL server"

  == query start ==
  client -> server : application query

  == server implementation ==

  server -> server : ...

  == query end ==
  client <-- server : query end

  @enduml

  An application query in general just executes code in the server.
  Data may of may not be collected in this code path, see next for possible
  patterns.

  @section NEW_TABLE_STATIC Table exposing static data

  @startuml

  participant server as "MySQL server"
  participant pfs as "Performance schema\n engine"
  participant pfs_table as "Performance schema\n table"
  participant buffer as "Performance schema\n static data"

  == opening table ==
  server -> pfs : ha_perfschema::open()
  activate pfs_table
  pfs -> pfs_table : create()
  server -> pfs : ha_perfschema::rnd_init()
  pfs -> pfs_table : rnd_init()

  == for each row ==
  server -> pfs : rnd_next()
  pfs -> pfs_table : rnd_next()
  pfs_table -> pfs_table : make_row()
  activate pfs_table
  pfs_table -> buffer : read()
  deactivate pfs_table

  server -> pfs : ha_perfschema::read_row_values()
  pfs -> pfs_table : read_row_values()

  == closing table ==
  server -> pfs : ha_perfschema::rnd_end()
  pfs -> pfs_table : rnd_end()
  server -> pfs : ha_perfschema::close()
  pfs -> pfs_table : destroy()
  deactivate pfs_table

  @enduml

  Static data does not need to be collected, because it is already known.
  In this simple case, the table implementation just exposes some internal
  structure.

  An example of table using this pattern is @c table_setup_consumers.

  @section NEW_TABLE_COLLECTED Table exposing collected data

  @startuml

  actor client as "Application Client"
  participant server_c as "MySQL server\n (application thread)"
  participant psi as "Performance schema\n instrumentation point"
  participant pfs_table as "Performance schema\n table"
  participant pfs as "Performance schema\n engine"
  participant server_m as "MySQL server\n (monitoring thread)"
  actor mon as "Monitoring Client"

  box "Application thread"
  actor client
  participant server_c
  participant psi
  end box

  participant buffer as "Performance schema\n collected data"

  box "Monitoring thread"
  participant pfs_table
  participant pfs
  participant server_m
  actor mon
  end box

  == application query start ==
  client -> server_c : application query

  == server implementation ==

  server_c -> psi : instrumentation call()
  psi -> buffer : write()

  == application query end ==
  client <-- server_c : application query end

  == monitoring query start ==
  mon -> server_m : performance schema query

  == opening table ==
  server_m -> pfs : ha_perfschema::open()
  activate pfs_table
  pfs -> pfs_table : create()
  server_m -> pfs : ha_perfschema::rnd_init()
  pfs -> pfs_table : rnd_init()

  == for each row ==
  server_m -> pfs : rnd_next()
  pfs -> pfs_table : rnd_next()
  pfs_table -> pfs_table : make_row()
  activate pfs_table
  pfs_table -> buffer : read()
  deactivate pfs_table
  server_m -> pfs : ha_perfschema::read_row_values()
  pfs -> pfs_table : read_row_values()
  mon <-- server_m : result set row

  == closing table ==
  server_m -> pfs : ha_perfschema::rnd_end()
  pfs -> pfs_table : rnd_end()
  server_m -> pfs : ha_perfschema::close()
  pfs -> pfs_table : destroy()
  deactivate pfs_table

  == monitoring query end ==
  mon <-- server_m : performance schema query end

  @enduml

  When a table implementation exposes collected data:
  - a memory buffer is used to represent the data
  - the server code path is modified, calls instrumentation points,
  that feed the memory buffer,
  - the performance schema table implementation reads from the memory buffer.

  Note that access to the internal buffer is lock-free.

  This pattern:
  - creates memory overhead, for the memory buffer
  - creates CPU overhead, with the instrumentation points.

  This pattern should be used only when data is not available by other means.

  An example of table using this pattern is @c table_mutex_instances.

  @section NEW_TABLE_INTERNAL Table exposing server internal data

  @startuml

  box "Application thread"
  actor client as "Application Client"
  participant server_c as "MySQL server\n (application thread)"
  participant server_state as "MySQL server\n internal state"
  end box

  == application query start ==
  client -> server_c : application query

  == server implementation ==

  server_c -> server_state : lock()
  server_c -> server_state : write()
  server_c -> server_state : unlock()

  == application query end ==
  client <-- server_c : application query end

  @enduml

  @startuml

  participant server_state as "MySQL server\n internal state"

  box "Monitoring thread"
  participant materialized as "Performance schema\n materialized data"
  participant pfs_table as "Performance schema\n table"
  participant pfs as "Performance schema\n engine"
  participant server_m as "MySQL server\n (monitoring thread)"
  actor mon as "Monitoring Client"
  end box

  == monitoring query start ==
  mon -> server_m : performance schema query

  == opening table ==
  server_m -> pfs : ha_perfschema::open()
  activate pfs_table
  pfs -> pfs_table : create()
  server_m -> pfs : ha_perfschema::rnd_init()
  pfs -> pfs_table : rnd_init()

  == materialize table ==

  pfs_table -> materialized : create()
  activate materialized
  pfs_table -> server_state : lock()
  pfs_table -> server_state : read()
  pfs_table -> materialized : write()
  pfs_table -> server_state : unlock()

  == for each row ==
  server_m -> pfs : rnd_next()
  pfs -> pfs_table : rnd_next()
  pfs_table -> pfs_table : make_row()
  activate pfs_table
  pfs_table -> materialized : read()
  deactivate pfs_table
  server_m -> pfs : ha_perfschema::read_row_values()
  pfs -> pfs_table : read_row_values()
  mon <-- server_m : result set row

  == closing table ==
  server_m -> pfs : ha_perfschema::rnd_end()
  pfs -> pfs_table : rnd_end()
  server_m -> pfs : ha_perfschema::close()
  pfs -> pfs_table : destroy()
  pfs_table -> materialized : destroy()
  deactivate materialized
  deactivate pfs_table

  == monitoring query end ==
  mon <-- server_m : performance schema query end

  @enduml

  When a table implementation exposes internal state:
  - some structure in the server already exists and contains the data,
  - the server code path is not modified,
  as it already maintains the data structure during the normal server operation,
  - access to the structure is most likely protected by locks,
  to maintain integrity,
  - the performance schema table implementation reads directly from the server
  internal memory,
  by inspecting it, using locks if necessary.

  This pattern:
  - creates no memory overhead,
  - creates no CPU overhead,
  - may cause a performance schema query to stall a client application query

  To prevent stalls, by locking the server structure for a long time,
  the data is typically copied once (the table is 'materialized') before
  iterating on it.

  If this pattern is possible (aka, the data already exists and can be
  inspected),
  is it the preferred implementation, which results in the least overhead.

  An example of table using this pattern is @c table_host_cache.
*/

/**
  @addtogroup performance_schema_engine
  @{
*/

bool
PFS_table_context::initialize(void)
{
  if (m_restore)
  {
    /* Restore context from TLS. */
    PFS_table_context *context =
      static_cast<PFS_table_context *>(my_get_thread_local(m_thr_key));
    DBUG_ASSERT(context != NULL);

    if (context)
    {
      m_last_version = context->m_current_version;
      m_map = context->m_map;
      DBUG_ASSERT(m_map_size == context->m_map_size);
      m_map_size = context->m_map_size;
      m_word_size = context->m_word_size;
    }
  }
  else
  {
    /* Check that TLS is not in use. */
    PFS_table_context *context =
      static_cast<PFS_table_context *>(my_get_thread_local(m_thr_key));
    // DBUG_ASSERT(context == NULL);

    context = this;

    /* Initialize a new context, store in TLS. */
    m_last_version = m_current_version;
    m_map = NULL;
    m_word_size = sizeof(ulong) * 8;

#if 0
    /* Disabled. */
    /* Allocate a bitmap to record which threads are materialized. */
    if (m_map_size > 0)
    {
      THD *thd= current_thd;
      ulong words= m_map_size / m_word_size + (m_map_size % m_word_size > 0);
      m_map= (ulong *)thd->mem_calloc(words * m_word_size);
    }
#endif

    /* Write to TLS. */
    my_set_thread_local(m_thr_key, static_cast<void *>(context));
  }

  m_initialized = (m_map_size > 0) ? (m_map != NULL) : true;

  return m_initialized;
}

/* Constructor for global or single thread tables, map size = 0.  */
PFS_table_context::PFS_table_context(ulonglong current_version,
                                     bool restore,
                                     thread_local_key_t key)
  : m_thr_key(key),
    m_current_version(current_version),
    m_last_version(0),
    m_map(NULL),
    m_map_size(0),
    m_word_size(sizeof(ulong)),
    m_restore(restore),
    m_initialized(false),
    m_last_item(0)
{
  initialize();
}

/* Constructor for by-thread or aggregate tables, map size = max
 * thread/user/host/account. */
PFS_table_context::PFS_table_context(ulonglong current_version,
                                     ulong map_size,
                                     bool restore,
                                     thread_local_key_t key)
  : m_thr_key(key),
    m_current_version(current_version),
    m_last_version(0),
    m_map(NULL),
    m_map_size(map_size),
    m_word_size(sizeof(ulong)),
    m_restore(restore),
    m_initialized(false),
    m_last_item(0)
{
  initialize();
}

PFS_table_context::~PFS_table_context(void)
{
  /* Clear TLS after final use. */
  //  if (m_restore)
  //  {
  //    my_set_thread_local(m_thr_key, NULL);
  //  }
}

void
PFS_table_context::set_item(ulong n)
{
  if (n == m_last_item)
  {
    return;
  }
  ulong word = n / m_word_size;
  ulong bit = n % m_word_size;
  m_map[word] |= (1UL << bit);
  m_last_item = n;
}

bool
PFS_table_context::is_item_set(ulong n)
{
  ulong word = n / m_word_size;
  ulong bit = n % m_word_size;
  return (m_map[word] & (1UL << bit));
}

static PFS_engine_table_share *all_shares[] = {
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
  &table_esmh_global::m_share,
  &table_esmh_by_digest::m_share,

  &table_events_transactions_current::m_share,
  &table_events_transactions_history::m_share,
  &table_events_transactions_history_long::m_share,
  &table_ets_by_thread_by_event_name::m_share,
  &table_ets_by_account_by_event_name::m_share,
  &table_ets_by_user_by_event_name::m_share,
  &table_ets_by_host_by_event_name::m_share,
  &table_ets_global_by_event_name::m_share,

  &table_ees_by_user_by_error::m_share,
  &table_ees_by_host_by_error::m_share,
  &table_ees_by_account_by_error::m_share,
  &table_ees_by_thread_by_error::m_share,
  &table_ees_global_by_error::m_share,

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
  &table_data_locks::m_share,
  &table_data_lock_waits::m_share,

  &table_replication_connection_configuration::m_share,
  &table_replication_group_members::m_share,
  &table_replication_connection_status::m_share,
  &table_replication_applier_configuration::m_share,
  &table_replication_applier_status::m_share,
  &table_replication_applier_status_by_coordinator::m_share,
  &table_replication_applier_status_by_worker::m_share,
  &table_replication_group_member_stats::m_share,
  &table_replication_applier_filters::m_share,
  &table_replication_applier_global_filters::m_share,

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
  &table_variables_info::m_share,
  &table_persisted_variables::m_share,

  NULL};

/**
  Check all the tables structure.
  @param thd              current thread
*/
void
PFS_engine_table_share::check_all_tables(THD *thd)
{
  PFS_engine_table_share **current;

  DBUG_EXECUTE_IF("tampered_perfschema_table1", {
    /* Hack SETUP_INSTRUMENT, incompatible change. */
    all_shares[20]->m_field_def->count++;
  });

  for (current = &all_shares[0]; (*current) != NULL; current++)
  {
    (*current)->check_one_table(thd);
  }
}

/** Error reporting for schema integrity checks. */
class PFS_check_intact : public Table_check_intact
{
protected:
  virtual void report_error(uint code, const char *fmt, ...);

public:
  PFS_check_intact()
  {
  }

  ~PFS_check_intact()
  {
  }
};

void
PFS_check_intact::report_error(uint, const char *fmt, ...)
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
  The actual table schema is compared to the expected schema.
  @param thd              current thread
*/
void
PFS_engine_table_share::check_one_table(THD *thd)
{
  TABLE_LIST tables;

  tables.init_one_table(PERFORMANCE_SCHEMA_str.str,
                        PERFORMANCE_SCHEMA_str.length,
                        m_name.str,
                        m_name.length,
                        m_name.str,
                        TL_READ);

  /* Work around until Bug#32115 is backported. */
  LEX dummy_lex;
  LEX *old_lex = thd->lex;
  thd->lex = &dummy_lex;
  lex_start(thd);

  if (!open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    PFS_check_intact checker;

    if (!checker.check(thd, tables.table, m_field_def))
    {
      m_checked = true;
    }
    close_thread_tables(thd);
  }
  else
    sql_print_error(ER_DEFAULT(ER_WRONG_NATIVE_TABLE_STRUCTURE),
                    PERFORMANCE_SCHEMA_str.str,
                    m_name.str);

  lex_end(&dummy_lex);
  thd->lex = old_lex;
}

/** Initialize all the table share locks. */
void
PFS_engine_table_share::init_all_locks(void)
{
  PFS_engine_table_share **current;

  for (current = &all_shares[0]; (*current) != NULL; current++)
  {
    thr_lock_init((*current)->m_thr_lock_ptr);
  }
}

/** Delete all the table share locks. */
void
PFS_engine_table_share::delete_all_locks(void)
{
  PFS_engine_table_share **current;

  for (current = &all_shares[0]; (*current) != NULL; current++)
  {
    thr_lock_delete((*current)->m_thr_lock_ptr);
  }
}

ha_rows
PFS_engine_table_share::get_row_count(void) const
{
  return m_get_row_count();
}

int
PFS_engine_table_share::write_row(TABLE *table,
                                  unsigned char *buf,
                                  Field **fields) const
{
  my_bitmap_map *org_bitmap;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in m_write_row.
  */
  if (!m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  if (m_write_row == NULL)
  {
    return HA_ERR_WRONG_COMMAND;
  }

  /* We internally read from Fields to support the write interface */
  org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  int result = m_write_row(table, buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

static int
compare_table_names(const char *name1, const char *name2)
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
    Once the server is changed to be able to discover a table in a storage
    engine
    and then open the table without storing a FRM file on disk, this constraint
    on the performance schema will be lifted, and the naming logic can be
    relaxed
    to be simply my_strcasecmp(system_charset_info, name1, name2).
  */
  if (lower_case_table_names)
  {
    return native_strcasecmp(name1, name2);
  }
  return strcmp(name1, name2);
}

/**
  Find a table share by name.
  @param name             The table name
  @return table share
*/
const PFS_engine_table_share *
PFS_engine_table::find_engine_table_share(const char *name)
{
  DBUG_ENTER("PFS_engine_table::find_table_share");

  PFS_engine_table_share **current;

  for (current = &all_shares[0]; (*current) != NULL; current++)
  {
    if (compare_table_names(name, (*current)->m_name.str) == 0)
    {
      DBUG_RETURN(*current);
    }
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
int
PFS_engine_table::read_row(TABLE *table, unsigned char *buf, Field **fields)
{
  my_bitmap_map *org_bitmap;
  Field *f;
  Field **fields_reset;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in read_row_values.
  */
  if (!m_share_ptr->m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  /* We must read all columns in case a table is opened for update */
  bool read_all = !bitmap_is_clear_all(table->write_set);

  /* We internally write to Fields to support the read interface */
  org_bitmap = dbug_tmp_use_all_columns(table, table->write_set);

  /*
    Some callers of the storage engine interface do not honor the
    f->is_null() flag, and will attempt to read the data itself.
    A known offender is mysql_checksum_table().
    For robustness, reset every field.
  */
  for (fields_reset = fields; (f = *fields_reset); fields_reset++)
  {
    f->reset();
  }

  int result = read_row_values(table, buf, fields, read_all);
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
int
PFS_engine_table::update_row(TABLE *table,
                             const unsigned char *old_buf,
                             unsigned char *new_buf,
                             Field **fields)
{
  my_bitmap_map *org_bitmap;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in update_row_values.
  */
  if (!m_share_ptr->m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  /* We internally read from Fields to support the write interface */
  org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  int result = update_row_values(table, old_buf, new_buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

int
PFS_engine_table::delete_row(TABLE *table,
                             const unsigned char *buf,
                             Field **fields)
{
  my_bitmap_map *org_bitmap;

  /*
    Make sure the table structure is as expected before mapping
    hard wired columns in delete_row_values.
  */
  if (!m_share_ptr->m_checked)
  {
    return HA_ERR_TABLE_NEEDS_UPGRADE;
  }

  /* We internally read from Fields to support the delete interface */
  org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  int result = delete_row_values(table, buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

int
PFS_engine_table::delete_row_values(TABLE *, const unsigned char *, Field **)
{
  return HA_ERR_WRONG_COMMAND;
}

/**
  Get the position of the current row.
  @param [out] ref        position
*/
void
PFS_engine_table::get_position(void *ref)
{
  memcpy(ref, m_pos_ptr, m_share_ptr->m_ref_length);
}

/**
  Set the table cursor at a given position.
  @param [in] ref         position
*/
void
PFS_engine_table::set_position(const void *ref)
{
  memcpy(m_pos_ptr, ref, m_share_ptr->m_ref_length);
}

/**
  Get the timer normalizer and class type for the current row.
  @param [in] instr_class    class
*/
void
PFS_engine_table::get_normalizer(PFS_instr_class *instr_class)
{
  if (instr_class->m_type != m_class_type)
  {
    m_normalizer = time_normalizer::get(*instr_class->m_timer);
    m_class_type = instr_class->m_type;
  }
}

int
PFS_engine_table::update_row_values(TABLE *,
                                    const unsigned char *,
                                    unsigned char *,
                                    Field **)
{
  return HA_ERR_WRONG_COMMAND;
}

/**
  Positions an index cursor to the index specified in the handle. Fetches the
  row if any.
  @return 0, HA_ERR_KEY_NOT_FOUND, or error
*/
int
PFS_engine_table::index_read(KEY *key_infos,
                             uint index,
                             const uchar *key,
                             uint key_len,
                             enum ha_rkey_function find_flag)
{
  // DBUG_ASSERT(m_index != NULL);
  if (m_index == NULL)
  {
    return HA_ERR_END_OF_FILE;
  }

  // FIXME: Unclear what to do here
  DBUG_ASSERT(find_flag != HA_READ_PREFIX_LAST);
  DBUG_ASSERT(find_flag != HA_READ_PREFIX_LAST_OR_PREV);

  // No GIS here
  DBUG_ASSERT(find_flag != HA_READ_MBR_CONTAIN);
  DBUG_ASSERT(find_flag != HA_READ_MBR_INTERSECT);
  DBUG_ASSERT(find_flag != HA_READ_MBR_WITHIN);
  DBUG_ASSERT(find_flag != HA_READ_MBR_DISJOINT);
  DBUG_ASSERT(find_flag != HA_READ_MBR_EQUAL);

  KEY *key_info = key_infos + index;
  m_index->set_key_info(key_info);
  m_index->read_key(key, key_len, find_flag);
  reset_position();
  return index_next();
}

/**
  Reads the next row matching the given key value.
  @return 0, HA_ERR_END_OF_FILE, or error
*/
int
PFS_engine_table::index_next_same(const uchar *, uint)
{
  return index_next();
}

/** Implementation of internal ACL checks, for the performance schema. */
class PFS_internal_schema_access : public ACL_internal_schema_access
{
public:
  PFS_internal_schema_access()
  {
  }

  ~PFS_internal_schema_access()
  {
  }

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;

  const ACL_internal_table_access *lookup(const char *name) const;
};

ACL_internal_access_result
PFS_internal_schema_access::check(ulong want_access, ulong *) const
{
  const ulong always_forbidden =
    /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL |
    EXECUTE_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | CREATE_PROC_ACL |
    ALTER_PROC_ACL | EVENT_ACL | TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden))
  {
    return ACL_INTERNAL_ACCESS_DENIED;
  }

  /*
    Proceed with regular grant tables,
    to give administrative control to the DBA.
  */
  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

const ACL_internal_table_access *
PFS_internal_schema_access::lookup(const char *name) const
{
  const PFS_engine_table_share *share;
  share = PFS_engine_table::find_engine_table_share(name);
  if (share)
  {
    return share->m_acl;
  }
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

void
initialize_performance_schema_acl(bool bootstrap)
{
  /*
    ACL is always enforced, even if the performance schema
    is not enabled (the tables are still visible).
  */
  if (!bootstrap)
  {
    ACL_internal_schema_registry::register_schema(PERFORMANCE_SCHEMA_str,
                                                  &pfs_internal_access);
  }
}

PFS_readonly_acl pfs_readonly_acl;

ACL_internal_access_result
PFS_readonly_acl::check(ulong want_access, ulong *) const
{
  const ulong always_forbidden = INSERT_ACL | UPDATE_ACL | DELETE_ACL |
                                 /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL |
                                 ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL |
                                 TRIGGER_ACL | LOCK_TABLES_ACL;

  if (unlikely(want_access & always_forbidden))
  {
    return ACL_INTERNAL_ACCESS_DENIED;
  }

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_readonly_world_acl pfs_readonly_world_acl;

ACL_internal_access_result
PFS_readonly_world_acl::check(ulong want_access, ulong *save_priv) const
{
  ACL_internal_access_result res =
    PFS_readonly_acl::check(want_access, save_priv);
  if (res == ACL_INTERNAL_ACCESS_CHECK_GRANT)
  {
    res = ACL_INTERNAL_ACCESS_GRANTED;
  }
  return res;
}

PFS_truncatable_acl pfs_truncatable_acl;

ACL_internal_access_result
PFS_truncatable_acl::check(ulong want_access, ulong *) const
{
  const ulong always_forbidden = INSERT_ACL | UPDATE_ACL | DELETE_ACL |
                                 /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL |
                                 ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL |
                                 TRIGGER_ACL | LOCK_TABLES_ACL;

  if (unlikely(want_access & always_forbidden))
  {
    return ACL_INTERNAL_ACCESS_DENIED;
  }

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_truncatable_world_acl pfs_truncatable_world_acl;

ACL_internal_access_result
PFS_truncatable_world_acl::check(ulong want_access, ulong *save_priv) const
{
  ACL_internal_access_result res =
    PFS_truncatable_acl::check(want_access, save_priv);
  if (res == ACL_INTERNAL_ACCESS_CHECK_GRANT)
  {
    res = ACL_INTERNAL_ACCESS_GRANTED;
  }
  return res;
}

PFS_updatable_acl pfs_updatable_acl;

ACL_internal_access_result
PFS_updatable_acl::check(ulong want_access, ulong *) const
{
  const ulong always_forbidden =
    INSERT_ACL | DELETE_ACL | /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL |
    ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden))
  {
    return ACL_INTERNAL_ACCESS_DENIED;
  }

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_editable_acl pfs_editable_acl;

ACL_internal_access_result
PFS_editable_acl::check(ulong want_access, ulong *) const
{
  const ulong always_forbidden = /* CREATE_ACL | */ REFERENCES_ACL | INDEX_ACL |
                                 ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL |
                                 TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden))
  {
    return ACL_INTERNAL_ACCESS_DENIED;
  }

  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

PFS_unknown_acl pfs_unknown_acl;

ACL_internal_access_result
PFS_unknown_acl::check(ulong want_access, ulong *) const
{
  const ulong always_forbidden = CREATE_ACL | REFERENCES_ACL | INDEX_ACL |
                                 ALTER_ACL | CREATE_VIEW_ACL | TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden))
  {
    return ACL_INTERNAL_ACCESS_DENIED;
  }

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

enum ha_rkey_function
PFS_key_reader::read_uchar(enum ha_rkey_function find_flag,
                           bool &isnull,
                           uchar *value)
{
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len)
  {
    size_t data_size = 1;
    DBUG_ASSERT(m_remaining_key_part_info->type == HA_KEYTYPE_BINARY);
    DBUG_ASSERT(m_remaining_key_part_info->store_length >= data_size);

    isnull = false;
    if (m_remaining_key_part_info->field->real_maybe_null())
    {
      if (m_remaining_key[0])
      {
        isnull = true;
      }

      m_remaining_key += HA_KEY_NULL_LENGTH;
      m_remaining_key_len -= HA_KEY_NULL_LENGTH;
    }

    uchar data = mi_uint1korr(m_remaining_key);
    m_remaining_key += data_size;
    m_remaining_key_len -= (uint)data_size;
    m_parts_found++;
    m_remaining_key_part_info++;

    *value = data;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  DBUG_ASSERT(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

enum ha_rkey_function
PFS_key_reader::read_long(enum ha_rkey_function find_flag,
                          bool &isnull,
                          long *value)
{
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len)
  {
    size_t data_size = sizeof(int32);
    DBUG_ASSERT(m_remaining_key_part_info->type == HA_KEYTYPE_LONG_INT);
    DBUG_ASSERT(m_remaining_key_part_info->store_length >= data_size);

    isnull = false;
    if (m_remaining_key_part_info->field->real_maybe_null())
    {
      if (m_remaining_key[0])
      {
        isnull = true;
      }

      m_remaining_key += HA_KEY_NULL_LENGTH;
      m_remaining_key_len -= HA_KEY_NULL_LENGTH;
    }

    int32 data = sint4korr(m_remaining_key);
    m_remaining_key += data_size;
    m_remaining_key_len -= (uint)data_size;
    m_parts_found++;
    m_remaining_key_part_info++;

    *value = data;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  DBUG_ASSERT(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

enum ha_rkey_function
PFS_key_reader::read_ulong(enum ha_rkey_function find_flag,
                           bool &isnull,
                           ulong *value)
{
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len)
  {
    size_t data_size = sizeof(int32);
    DBUG_ASSERT(m_remaining_key_part_info->type == HA_KEYTYPE_ULONG_INT);
    DBUG_ASSERT(m_remaining_key_part_info->store_length >= data_size);

    isnull = false;
    if (m_remaining_key_part_info->field->real_maybe_null())
    {
      if (m_remaining_key[0])
      {
        isnull = true;
      }

      m_remaining_key += HA_KEY_NULL_LENGTH;
      m_remaining_key_len -= HA_KEY_NULL_LENGTH;
    }

    uint32 data = uint4korr(m_remaining_key);
    m_remaining_key += data_size;
    m_remaining_key_len -= (uint)data_size;
    m_parts_found++;
    m_remaining_key_part_info++;

    *value = data;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  DBUG_ASSERT(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

enum ha_rkey_function
PFS_key_reader::read_ulonglong(enum ha_rkey_function find_flag,
                               bool &isnull,
                               ulonglong *value)
{
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len)
  {
    size_t data_size = sizeof(ulonglong);
    DBUG_ASSERT(m_remaining_key_part_info->type == HA_KEYTYPE_ULONGLONG);
    DBUG_ASSERT(m_remaining_key_part_info->store_length >= data_size);

    isnull = false;
    if (m_remaining_key_part_info->field->real_maybe_null())
    {
      if (m_remaining_key[0])
      {
        isnull = true;
      }

      m_remaining_key += HA_KEY_NULL_LENGTH;
      m_remaining_key_len -= HA_KEY_NULL_LENGTH;
    }

    ulonglong data = uint8korr(m_remaining_key);
    m_remaining_key += data_size;
    m_remaining_key_len -= (uint)data_size;
    m_parts_found++;
    m_remaining_key_part_info++;

    *value = data;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  DBUG_ASSERT(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

enum ha_rkey_function
PFS_key_reader::read_varchar_utf8(enum ha_rkey_function find_flag,
                                  bool &isnull,
                                  char *buffer,
                                  uint *buffer_length,
                                  uint buffer_capacity)
{
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len)
  {
    /*
      Stored as:
      - 0 or 1 null byte.
      - a always 2 byte length prefix (storage order), even for
      HA_KEYTYPE_VARTEXT1
      - followed by data
    */

    size_t length_offset = 0;
    size_t data_offset = 2;
    isnull = false;
    if (m_remaining_key_part_info->field->real_maybe_null())
    {
      DBUG_ASSERT(HA_KEY_NULL_LENGTH <= m_remaining_key_len);

      length_offset++;
      data_offset++;
      if (m_remaining_key[0])
      {
        isnull = true;
      }
    }

    DBUG_ASSERT(m_remaining_key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
                m_remaining_key_part_info->type == HA_KEYTYPE_VARTEXT2);

    DBUG_ASSERT(data_offset <= m_remaining_key_len);
    size_t string_len = uint2korr(m_remaining_key + length_offset);
    DBUG_ASSERT(data_offset + string_len <=
                m_remaining_key_part_info->store_length);
    DBUG_ASSERT(data_offset + string_len <= m_remaining_key_len);

    // DBUG_ASSERT(string_len <= buffer_capacity);
    if (string_len > buffer_capacity)
    {
      string_len = buffer_capacity;
    }

    memcpy(buffer, m_remaining_key + data_offset, string_len);
    *buffer_length = (uint)string_len;

    uchar *pos = (uchar *)buffer;
#if 0
    const CHARSET_INFO *cs= &my_charset_utf8_bin; // FIXME
    if (cs->mbmaxlen > 1)
    {
      size_t char_length;
      char_length= my_charpos(cs, pos, pos + string_len, string_len/cs->mbmaxlen);
      set_if_smaller(string_len, char_length);
    }
#endif
    const uchar *end = skip_trailing_space(pos, string_len);
    *buffer_length = (uint)(end - pos);

    m_remaining_key += m_remaining_key_part_info->store_length;
    m_remaining_key_len -= m_remaining_key_part_info->store_length;
    m_parts_found++;
    m_remaining_key_part_info++;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  DBUG_ASSERT(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

enum ha_rkey_function
PFS_key_reader::read_text_utf8(enum ha_rkey_function find_flag,
                               bool &isnull,
                               char *buffer,
                               uint *buffer_length,
                               uint buffer_capacity)
{
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len)
  {
    /*
      Stored as:
      - 0 or 1 null byte
      - followed by data
      - Length determined by key definition
    */
    DBUG_ASSERT(m_remaining_key_part_info->type == HA_KEYTYPE_TEXT);

    size_t length_offset = 0;
    size_t data_offset = 0;
    isnull = false;
    if (m_remaining_key_part_info->field->real_maybe_null())
    {
      DBUG_ASSERT(HA_KEY_NULL_LENGTH <= m_remaining_key_len);

      length_offset++;
      data_offset++;
      if (m_remaining_key[0])
      {
        isnull = true;
      }
    }

    DBUG_ASSERT(data_offset <= m_remaining_key_len);
    size_t string_len = m_remaining_key_part_info->length;
    DBUG_ASSERT(data_offset + string_len <=
                m_remaining_key_part_info->store_length);
    DBUG_ASSERT(data_offset + string_len <= m_remaining_key_len);

    // DBUG_ASSERT(string_len <= buffer_capacity);
    if (string_len > buffer_capacity)  // FIXME
    {
      string_len = buffer_capacity;
    }

    memcpy(buffer, m_remaining_key + data_offset, string_len);
    *buffer_length = (uint)string_len;

    const CHARSET_INFO *cs = &my_charset_utf8_bin;  // FIXME
    uchar *pos = (uchar *)buffer;
    if (cs->mbmaxlen > 1)
    {
      size_t char_length;
      char_length =
        my_charpos(cs, pos, pos + string_len, string_len / cs->mbmaxlen);
      set_if_smaller(string_len, char_length);
    }
    const uchar *end = skip_trailing_space(pos, string_len);
    *buffer_length = (uint)(end - pos);

    m_remaining_key += m_remaining_key_part_info->store_length;
    m_remaining_key_len -= m_remaining_key_part_info->store_length;
    m_parts_found++;
    m_remaining_key_part_info++;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  DBUG_ASSERT(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

void
PFS_engine_index::read_key(const uchar *key,
                           uint key_len,
                           enum ha_rkey_function find_flag)
{
  PFS_key_reader reader(m_key_info, key, key_len);

  if (m_key_ptr_1 != NULL)
  {
    DBUG_ASSERT(native_strcasecmp(m_key_info->key_part[0].field->field_name,
                                  m_key_ptr_1->m_name) == 0);
    m_key_ptr_1->read(reader, find_flag);
  }

  if (m_key_ptr_2 != NULL)
  {
    DBUG_ASSERT(native_strcasecmp(m_key_info->key_part[1].field->field_name,
                                  m_key_ptr_2->m_name) == 0);
    m_key_ptr_2->read(reader, find_flag);
  }

  if (m_key_ptr_3 != NULL)
  {
    DBUG_ASSERT(native_strcasecmp(m_key_info->key_part[2].field->field_name,
                                  m_key_ptr_3->m_name) == 0);
    m_key_ptr_3->read(reader, find_flag);
  }

  if (m_key_ptr_4 != NULL)
  {
    DBUG_ASSERT(native_strcasecmp(m_key_info->key_part[3].field->field_name,
                                  m_key_ptr_4->m_name) == 0);
    m_key_ptr_4->read(reader, find_flag);
  }

  m_fields = reader.m_parts_found;
}

/** @} */
