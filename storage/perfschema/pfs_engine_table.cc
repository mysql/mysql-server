/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
  @file storage/perfschema/pfs_engine_table.cc
  Performance schema tables (implementation).
*/

#include "storage/perfschema/pfs_engine_table.h"

#include <string.h>
#include <algorithm>

#include "m_ctype.h"
#include "m_string.h"
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_sqlcommand.h"
#include "my_time.h"
#include "myisampack.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "sql/auth/auth_acls.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/mysqld.h" /* lower_case_table_names */
#include "sql/plugin_table.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/table.h"
/* For show status */
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/table_accounts.h"
#include "storage/perfschema/table_binary_log_transaction_compression_stats.h"
#include "storage/perfschema/table_data_lock_waits.h"
#include "storage/perfschema/table_data_locks.h"
#include "storage/perfschema/table_ees_by_account_by_error.h"
#include "storage/perfschema/table_ees_by_host_by_error.h"
#include "storage/perfschema/table_ees_by_thread_by_error.h"
#include "storage/perfschema/table_ees_by_user_by_error.h"
#include "storage/perfschema/table_ees_global_by_error.h"
#include "storage/perfschema/table_error_log.h"
#include "storage/perfschema/table_esgs_by_account_by_event_name.h"
#include "storage/perfschema/table_esgs_by_host_by_event_name.h"
#include "storage/perfschema/table_esgs_by_thread_by_event_name.h"
#include "storage/perfschema/table_esgs_by_user_by_event_name.h"
#include "storage/perfschema/table_esgs_global_by_event_name.h"
#include "storage/perfschema/table_esmh_by_digest.h"
#include "storage/perfschema/table_esmh_global.h"
#include "storage/perfschema/table_esms_by_account_by_event_name.h"
#include "storage/perfschema/table_esms_by_digest.h"
#include "storage/perfschema/table_esms_by_host_by_event_name.h"
#include "storage/perfschema/table_esms_by_program.h"
#include "storage/perfschema/table_esms_by_thread_by_event_name.h"
#include "storage/perfschema/table_esms_by_user_by_event_name.h"
#include "storage/perfschema/table_esms_global_by_event_name.h"
#include "storage/perfschema/table_ets_by_account_by_event_name.h"
#include "storage/perfschema/table_ets_by_host_by_event_name.h"
#include "storage/perfschema/table_ets_by_thread_by_event_name.h"
#include "storage/perfschema/table_ets_by_user_by_event_name.h"
#include "storage/perfschema/table_ets_global_by_event_name.h"
#include "storage/perfschema/table_events_stages.h"
#include "storage/perfschema/table_events_statements.h"
#include "storage/perfschema/table_events_transactions.h"
#include "storage/perfschema/table_events_waits.h"
#include "storage/perfschema/table_events_waits_summary.h"
#include "storage/perfschema/table_ews_by_account_by_event_name.h"
#include "storage/perfschema/table_ews_by_host_by_event_name.h"
#include "storage/perfschema/table_ews_by_thread_by_event_name.h"
#include "storage/perfschema/table_ews_by_user_by_event_name.h"
#include "storage/perfschema/table_ews_global_by_event_name.h"
#include "storage/perfschema/table_file_instances.h"
#include "storage/perfschema/table_file_summary_by_event_name.h"
#include "storage/perfschema/table_file_summary_by_instance.h"
#include "storage/perfschema/table_global_status.h"
#include "storage/perfschema/table_global_variables.h"
#include "storage/perfschema/table_host_cache.h"
#include "storage/perfschema/table_hosts.h"
#include "storage/perfschema/table_keyring_component_status.h"
#include "storage/perfschema/table_keyring_keys.h"
#include "storage/perfschema/table_md_locks.h"
#include "storage/perfschema/table_mems_by_account_by_event_name.h"
#include "storage/perfschema/table_mems_by_host_by_event_name.h"
#include "storage/perfschema/table_mems_by_thread_by_event_name.h"
#include "storage/perfschema/table_mems_by_user_by_event_name.h"
#include "storage/perfschema/table_mems_global_by_event_name.h"
#include "storage/perfschema/table_os_global_by_type.h"
#include "storage/perfschema/table_performance_timers.h"
#include "storage/perfschema/table_persisted_variables.h"
#include "storage/perfschema/table_prepared_stmt_instances.h"
#include "storage/perfschema/table_processlist.h"
#include "storage/perfschema/table_replication_applier_configuration.h"
#include "storage/perfschema/table_replication_applier_filters.h"
#include "storage/perfschema/table_replication_applier_global_filters.h"
#include "storage/perfschema/table_replication_applier_status.h"
#include "storage/perfschema/table_replication_applier_status_by_coordinator.h"
#include "storage/perfschema/table_replication_applier_status_by_worker.h"
/* For replication related perfschema tables. */
#include "storage/perfschema/table_log_status.h"
#include "storage/perfschema/table_replication_asynchronous_connection_failover.h"
#include "storage/perfschema/table_replication_connection_configuration.h"
#include "storage/perfschema/table_replication_connection_status.h"
#include "storage/perfschema/table_replication_group_member_stats.h"
#include "storage/perfschema/table_replication_group_members.h"
#include "storage/perfschema/table_rpl_async_connection_failover_managed.h"
#include "storage/perfschema/table_session_account_connect_attrs.h"
#include "storage/perfschema/table_session_connect_attrs.h"
#include "storage/perfschema/table_session_status.h"
#include "storage/perfschema/table_session_variables.h"
#include "storage/perfschema/table_setup_actors.h"
#include "storage/perfschema/table_setup_consumers.h"
#include "storage/perfschema/table_setup_instruments.h"
#include "storage/perfschema/table_setup_objects.h"
#include "storage/perfschema/table_setup_threads.h"
#include "storage/perfschema/table_socket_instances.h"
#include "storage/perfschema/table_socket_summary_by_event_name.h"
#include "storage/perfschema/table_socket_summary_by_instance.h"
#include "storage/perfschema/table_status_by_account.h"
#include "storage/perfschema/table_status_by_host.h"
#include "storage/perfschema/table_status_by_thread.h"
#include "storage/perfschema/table_status_by_user.h"
#include "storage/perfschema/table_sync_instances.h"
#include "storage/perfschema/table_table_handles.h"
#include "storage/perfschema/table_threads.h"
#include "storage/perfschema/table_tiws_by_index_usage.h"
#include "storage/perfschema/table_tiws_by_table.h"
#include "storage/perfschema/table_tls_channel_status.h"
#include "storage/perfschema/table_tlws_by_table.h"
#include "storage/perfschema/table_user_defined_functions.h"
#include "storage/perfschema/table_users.h"
#include "storage/perfschema/table_uvar_by_thread.h"
#include "storage/perfschema/table_variables_by_thread.h"
#include "storage/perfschema/table_variables_info.h"
#include "thr_lock.h"
#include "thr_mutex.h"

/* clang-format off */
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
/* clang-format on */

/**
  @addtogroup performance_schema_engine
  @{
*/

static PFS_engine_table_share *all_shares[] = {
    &table_cond_instances::m_share,
    &table_error_log::m_share,
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
    &table_processlist::m_share,
    &table_rwlock_instances::m_share,
    &table_setup_actors::m_share,
    &table_setup_consumers::m_share,
    &table_setup_instruments::m_share,
    &table_setup_objects::m_share,
    &table_setup_threads::m_share,
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

    &table_keyring_keys::s_share,

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
    &table_replication_asynchronous_connection_failover::m_share,
    &table_rpl_async_connection_failover_managed::m_share,
    &table_log_status::m_share,

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
    &table_user_defined_functions::m_share,
    &table_binary_log_transaction_compression_stats::m_share,
    &table_tls_channel_status::m_share,
    &table_keyring_component_status::m_share,
    nullptr};

static PSI_mutex_key key_LOCK_pfs_share_list;
static PSI_mutex_info info_LOCK_pfs_share_list = {
    &key_LOCK_pfs_share_list, "LOCK_pfs_share_list", PSI_VOLATILITY_PERMANENT,
    PSI_FLAG_SINGLETON,
    /* Doc */
    "Components can provide their own performance_schema tables. "
    "This lock protects the list of such tables definitions."};

void PFS_dynamic_table_shares::init_mutex() {
  /* This is called once at startup, ok to register here. */
  /* FIXME: Category "performance_schema" leads to a name too long. */
  mysql_mutex_register("pfs", &info_LOCK_pfs_share_list, 1);
  mysql_mutex_init(key_LOCK_pfs_share_list, &LOCK_pfs_share_list,
                   MY_MUTEX_INIT_FAST);
}

void PFS_dynamic_table_shares::destroy_mutex() {
  mysql_mutex_destroy(&LOCK_pfs_share_list);
}

PFS_dynamic_table_shares pfs_external_table_shares;

/** Get all the core performance schema tables. */
void PFS_engine_table_share::get_all_tables(List<const Plugin_table> *tables) {
  PFS_engine_table_share **current;

  for (current = &all_shares[0]; (*current) != nullptr; current++) {
    tables->push_back((*current)->m_table_def);
  }
}

/** Initialize all the table share locks. */
void PFS_engine_table_share::init_all_locks(void) {
  PFS_engine_table_share **current;

  for (current = &all_shares[0]; (*current) != nullptr; current++) {
    thr_lock_init((*current)->m_thr_lock_ptr);
  }
}

/** Delete all the table share locks. */
void PFS_engine_table_share::delete_all_locks(void) {
  PFS_engine_table_share **current;

  for (current = &all_shares[0]; (*current) != nullptr; current++) {
    thr_lock_delete((*current)->m_thr_lock_ptr);
  }
}

ha_rows PFS_engine_table_share::get_row_count(void) const {
  return m_get_row_count();
}

int PFS_engine_table_share::write_row(PFS_engine_table *pfs_table, TABLE *table,
                                      unsigned char *buf,
                                      Field **fields) const {
  my_bitmap_map *org_bitmap;

  if (m_write_row == nullptr) {
    return HA_ERR_WRONG_COMMAND;
  }

  /* We internally read from Fields to support the write interface */
  org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  int result = m_write_row(pfs_table, table, buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

int compare_table_names(const char *name1, const char *name2) {
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
  if (lower_case_table_names) {
    return native_strcasecmp(name1, name2);
  }
  return strcmp(name1, name2);
}

/**
  Find a table share by name.
  @param name             The table name
  @return table share
*/
PFS_engine_table_share *PFS_engine_table::find_engine_table_share(
    const char *name) {
  DBUG_TRACE;
  PFS_engine_table_share *result;

  /* First try to find in native performance schema table shares */
  PFS_engine_table_share **current;

  for (current = &all_shares[0]; (*current) != nullptr; current++) {
    if (compare_table_names(name, (*current)->m_table_def->get_name()) == 0) {
      return *current;
    }
  }

  /* Now try to find in non-native performance schema tables shares */
  result = pfs_external_table_shares.find_share(name, false);

  // FIXME : here we return an object that could be destroyed, unsafe.
  return result;
}

/**
  Read a table row.
  @param table            Table handle
  @param buf              Row buffer
  @param fields           Table fields
  @return 0 on success
*/
int PFS_engine_table::read_row(TABLE *table, unsigned char *buf,
                               Field **fields) {
  my_bitmap_map *org_bitmap;
  Field *f;
  Field **fields_reset;

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
  for (fields_reset = fields; (f = *fields_reset); fields_reset++) {
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
int PFS_engine_table::update_row(TABLE *table, const unsigned char *old_buf,
                                 unsigned char *new_buf, Field **fields) {
  my_bitmap_map *org_bitmap;

  /* We internally read from Fields to support the write interface */
  org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  int result = update_row_values(table, old_buf, new_buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

int PFS_engine_table::delete_row(TABLE *table, const unsigned char *buf,
                                 Field **fields) {
  my_bitmap_map *org_bitmap;

  /* We internally read from Fields to support the delete interface */
  org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  int result = delete_row_values(table, buf, fields);
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);

  return result;
}

int PFS_engine_table::delete_row_values(TABLE *, const unsigned char *,
                                        Field **) {
  return HA_ERR_WRONG_COMMAND;
}

/**
  Get the position of the current row.
  @param [out] ref        position
*/
void PFS_engine_table::get_position(void *ref) {
  memcpy(ref, m_pos_ptr, m_share_ptr->m_ref_length);
}

/**
  Set the table cursor at a given position.
  @param [in] ref         position
*/
void PFS_engine_table::set_position(const void *ref) {
  memcpy(m_pos_ptr, ref, m_share_ptr->m_ref_length);
}

int PFS_engine_table::update_row_values(TABLE *, const unsigned char *,
                                        unsigned char *, Field **) {
  return HA_ERR_WRONG_COMMAND;
}

/**
  Positions an index cursor to the index specified in the handle. Fetches the
  row if any.
  @return 0, HA_ERR_KEY_NOT_FOUND, or error
*/
int PFS_engine_table::index_read(KEY *key_infos, uint index, const uchar *key,
                                 uint key_len,
                                 enum ha_rkey_function find_flag) {
  // assert(m_index != NULL);
  if (m_index == nullptr) {
    return HA_ERR_END_OF_FILE;
  }

  // FIXME: Unclear what to do here
  assert(find_flag != HA_READ_PREFIX_LAST);
  assert(find_flag != HA_READ_PREFIX_LAST_OR_PREV);

  // No GIS here
  assert(find_flag != HA_READ_MBR_CONTAIN);
  assert(find_flag != HA_READ_MBR_INTERSECT);
  assert(find_flag != HA_READ_MBR_WITHIN);
  assert(find_flag != HA_READ_MBR_DISJOINT);
  assert(find_flag != HA_READ_MBR_EQUAL);

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
int PFS_engine_table::index_next_same(const uchar *, uint) {
  return index_next();
}

/**
  Find a share in the list
  @param table_name  name of the table
  @param is_dead_too if true, consider tables marked to be deleted

  @return if found table share or NULL
*/
PFS_engine_table_share *PFS_dynamic_table_shares::find_share(
    const char *table_name, bool is_dead_too) {
  if (!opt_initialize) {
    mysql_mutex_assert_owner(&LOCK_pfs_share_list);
  }

  for (auto it : shares_vector) {
    if ((compare_table_names(table_name, it->m_table_def->get_name()) == 0) &&
        (it->m_in_purgatory == false || is_dead_too)) {
      return it;
    }
  }
  return nullptr;
}

/**
  Remove a share from the list
  @param share  share to be removed
*/
void PFS_dynamic_table_shares::remove_share(PFS_engine_table_share *share) {
  mysql_mutex_assert_owner(&LOCK_pfs_share_list);

  std::vector<PFS_engine_table_share *>::iterator it;

  /* Search for the share in share list */
  it = std::find(shares_vector.begin(), shares_vector.end(), share);
  if (it != shares_vector.end()) {
    /* Remove the share from the share list */
    shares_vector.erase(it);
  }
}

/** Implementation of internal ACL checks, for the performance schema. */
class PFS_internal_schema_access : public ACL_internal_schema_access {
 public:
  PFS_internal_schema_access() = default;

  ~PFS_internal_schema_access() override = default;

  ACL_internal_access_result check(ulong want_access, ulong *save_priv,
                                   bool any_combination_will_do) const override;

  const ACL_internal_table_access *lookup(const char *name) const override;
};

static bool allow_drop_schema_privilege() {
  /*
    The same DROP_ACL privilege is used for different statements,
    in particular, as a schema level privilege:
    - DROP SCHEMA
    - GRANT DROP on performance_schema.*
    - REVOKE DROP ON performance_schema.*
    - DROP TABLE performance_schema.*

    As a table level privilege:
    - DROP TABLE performance_schema.foo
    - GRANT DROP on performance_schema.foo
    - REVOKE DROP on performance_schema.foo
    - TRUNCATE TABLE performance_schema.foo

    Here, we want to:
    - always prevent DROP SCHEMA (SQLCOM_DROP_DB)
    - allow GRANT/REVOKE to give the TRUNCATE on any tables
    - allow DROP TABLE checks to proceed further,
      in particular to drop unknown tables,
      see PFS_unknown_acl::check()
  */
  THD *thd = current_thd;
  if (thd == nullptr) {
    return false;
  }

  assert(thd->lex != nullptr);
  if ((thd->lex->sql_command != SQLCOM_TRUNCATE) &&
      (thd->lex->sql_command != SQLCOM_GRANT) &&
      (thd->lex->sql_command != SQLCOM_REVOKE) &&
      (thd->lex->sql_command != SQLCOM_DROP_TABLE)) {
    return false;
  }

  return true;
}

ACL_internal_access_result PFS_internal_schema_access::check(ulong want_access,
                                                             ulong *,
                                                             bool) const {
  const ulong always_forbidden =
      CREATE_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL |
      EXECUTE_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | CREATE_PROC_ACL |
      ALTER_PROC_ACL | EVENT_ACL | TRIGGER_ACL;

  if (unlikely(want_access & always_forbidden)) {
    return ACL_INTERNAL_ACCESS_DENIED;
  }

  if (want_access & DROP_ACL) {
    if (!allow_drop_schema_privilege()) {
      return ACL_INTERNAL_ACCESS_DENIED;
    }
  }

  /*
    Proceed with regular grant tables,
    to give administrative control to the DBA.
  */
  return ACL_INTERNAL_ACCESS_CHECK_GRANT;
}

const ACL_internal_table_access *PFS_internal_schema_access::lookup(
    const char *name) const {
  const PFS_engine_table_share *share;

  pfs_external_table_shares.lock_share_list();
  share = PFS_engine_table::find_engine_table_share(name);
  if (share) {
    const ACL_internal_table_access *acl = share->m_acl;
    pfs_external_table_shares.unlock_share_list();
    return acl;
  }

  pfs_external_table_shares.unlock_share_list();
  /*
    Do not return NULL, it would mean we are not interested
    in privilege checks for unknown tables.
    Instead, return an object that denies every actions,
    to prevent users from creating their own tables in the
    performance_schema database schema.
  */
  return &pfs_unknown_acl;
}

PFS_internal_schema_access pfs_internal_access;

void initialize_performance_schema_acl(bool bootstrap) {
  /*
    ACL is always enforced, even if the performance schema
    is not enabled (the tables are still visible).
  */
  if (!bootstrap) {
    ACL_internal_schema_registry::register_schema(PERFORMANCE_SCHEMA_str,
                                                  &pfs_internal_access);
  }
}

static bool allow_drop_table_privilege() {
  /*
    The same DROP_ACL privilege is used for different statements,
    in particular:
    - TRUNCATE TABLE
    - DROP TABLE
    - ALTER TABLE
    Here, we want to prevent DROP / ALTER  while allowing TRUNCATE.
    Note that we must also allow GRANT to transfer the truncate privilege.
  */
  THD *thd = current_thd;
  if (thd == nullptr) {
    return false;
  }

  assert(thd->lex != nullptr);
  if ((thd->lex->sql_command != SQLCOM_TRUNCATE) &&
      (thd->lex->sql_command != SQLCOM_GRANT)) {
    return false;
  }

  return true;
}

PFS_readonly_acl pfs_readonly_acl;

ACL_internal_access_result PFS_readonly_acl::check(
    ulong want_access, ulong *granted_access,
    bool any_combination_will_do) const {
  const ulong always_forbidden = INSERT_ACL | UPDATE_ACL | DELETE_ACL |
                                 CREATE_ACL | DROP_ACL | REFERENCES_ACL |
                                 INDEX_ACL | ALTER_ACL | CREATE_VIEW_ACL |
                                 SHOW_VIEW_ACL | TRIGGER_ACL | LOCK_TABLES_ACL;

  ulong can_be_allowed = TABLE_ACLS & (~always_forbidden);

  ulong want_forbidden = want_access & always_forbidden;

  ulong want_allowable = want_access & can_be_allowed;

  if (any_combination_will_do) {
    if (want_allowable != 0) {
      *granted_access = want_allowable;
      return ACL_INTERNAL_ACCESS_CHECK_GRANT;
    }

    return ACL_INTERNAL_ACCESS_DENIED;
  } else {
    if (want_forbidden != 0) {
      return ACL_INTERNAL_ACCESS_DENIED;
    }

    return ACL_INTERNAL_ACCESS_CHECK_GRANT;
  }
}

PFS_readonly_world_acl pfs_readonly_world_acl;

ACL_internal_access_result PFS_readonly_world_acl::check(
    ulong want_access, ulong *save_priv, bool any_combination_will_do) const {
  ACL_internal_access_result res =
      PFS_readonly_acl::check(want_access, save_priv, any_combination_will_do);
  if (res == ACL_INTERNAL_ACCESS_CHECK_GRANT) {
    res = ACL_INTERNAL_ACCESS_GRANTED;
  }
  return res;
}

PFS_readonly_processlist_acl pfs_readonly_processlist_acl;

ACL_internal_access_result PFS_readonly_processlist_acl::check(
    ulong want_access, ulong *save_priv, bool any_combination_will_do) const {
  ACL_internal_access_result res =
      PFS_readonly_acl::check(want_access, save_priv, any_combination_will_do);

  if ((res == ACL_INTERNAL_ACCESS_CHECK_GRANT) && (want_access & SELECT_ACL)) {
    return ACL_INTERNAL_ACCESS_GRANTED;
  }
  return res;
}

PFS_truncatable_acl pfs_truncatable_acl;

ACL_internal_access_result PFS_truncatable_acl::check(
    ulong want_access, ulong *granted_access,
    bool any_combination_will_do) const {
  const ulong always_forbidden = INSERT_ACL | UPDATE_ACL | DELETE_ACL |
                                 CREATE_ACL | REFERENCES_ACL | INDEX_ACL |
                                 ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL |
                                 TRIGGER_ACL | LOCK_TABLES_ACL;

  ulong can_be_allowed = TABLE_ACLS & (~always_forbidden);

  ulong want_allowable = want_access & can_be_allowed;

  ulong want_forbidden = want_access & always_forbidden;

  if (want_access & DROP_ACL) {
    if (!allow_drop_table_privilege()) {
      want_forbidden |= DROP_ACL;
      want_allowable &= ~DROP_ACL;
    }
  }

  if (any_combination_will_do) {
    if (want_allowable != 0) {
      *granted_access = want_allowable;
      return ACL_INTERNAL_ACCESS_CHECK_GRANT;
    }

    return ACL_INTERNAL_ACCESS_DENIED;
  } else {
    if (want_forbidden != 0) {
      return ACL_INTERNAL_ACCESS_DENIED;
    }

    return ACL_INTERNAL_ACCESS_CHECK_GRANT;
  }
}

PFS_truncatable_world_acl pfs_truncatable_world_acl;

ACL_internal_access_result PFS_truncatable_world_acl::check(
    ulong want_access, ulong *save_priv, bool any_combination_will_do) const {
  ACL_internal_access_result res = PFS_truncatable_acl::check(
      want_access, save_priv, any_combination_will_do);
  if (res == ACL_INTERNAL_ACCESS_CHECK_GRANT) {
    res = ACL_INTERNAL_ACCESS_GRANTED;
  }
  return res;
}

PFS_updatable_acl pfs_updatable_acl;

ACL_internal_access_result PFS_updatable_acl::check(
    ulong want_access, ulong *granted_access,
    bool any_combination_will_do) const {
  const ulong always_forbidden =
      INSERT_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | REFERENCES_ACL |
      INDEX_ACL | ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | TRIGGER_ACL;

  ulong can_be_allowed = TABLE_ACLS & (~always_forbidden);

  ulong want_forbidden = want_access & always_forbidden;

  ulong want_allowable = want_access & can_be_allowed;

  if (any_combination_will_do) {
    if (want_allowable != 0) {
      *granted_access = want_allowable;
      return ACL_INTERNAL_ACCESS_CHECK_GRANT;
    }

    return ACL_INTERNAL_ACCESS_DENIED;
  } else {
    if (want_forbidden != 0) {
      return ACL_INTERNAL_ACCESS_DENIED;
    }

    return ACL_INTERNAL_ACCESS_CHECK_GRANT;
  }
}

PFS_editable_acl pfs_editable_acl;

ACL_internal_access_result PFS_editable_acl::check(
    ulong want_access, ulong *granted_access,
    bool any_combination_will_do) const {
  const ulong always_forbidden = CREATE_ACL | REFERENCES_ACL | INDEX_ACL |
                                 ALTER_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL |
                                 TRIGGER_ACL;

  ulong can_be_allowed = TABLE_ACLS & (~always_forbidden);

  ulong want_forbidden = want_access & always_forbidden;

  ulong want_allowable = want_access & can_be_allowed;

  if (want_access & DROP_ACL) {
    if (!allow_drop_table_privilege()) {
      want_forbidden |= DROP_ACL;
      want_allowable &= ~DROP_ACL;
    }
  }

  if (any_combination_will_do) {
    if (want_allowable != 0) {
      *granted_access = want_allowable;
      return ACL_INTERNAL_ACCESS_CHECK_GRANT;
    }

    return ACL_INTERNAL_ACCESS_DENIED;
  } else {
    if (want_forbidden != 0) {
      return ACL_INTERNAL_ACCESS_DENIED;
    }

    return ACL_INTERNAL_ACCESS_CHECK_GRANT;
  }
}

PFS_unknown_acl pfs_unknown_acl;

ACL_internal_access_result PFS_unknown_acl::check(
    ulong want_access, ulong *granted_access,
    bool any_combination_will_do) const {
  /*
    Only enforce ACL_INTERNAL_ACCESS_DENIED
    for operations that can create unwanted SQL objects
    in the performance schema,
    relax error messages otherwise.
  */
  const ulong always_forbidden = CREATE_ACL | REFERENCES_ACL | INDEX_ACL |
                                 ALTER_ACL | CREATE_VIEW_ACL | TRIGGER_ACL;

  ulong can_be_allowed = TABLE_ACLS & (~always_forbidden);

  ulong want_forbidden = want_access & always_forbidden;

  ulong want_allowable = want_access & can_be_allowed;

  if (any_combination_will_do) {
    if (want_allowable != 0) {
      *granted_access = want_allowable;
      return ACL_INTERNAL_ACCESS_CHECK_GRANT;
    }

    return ACL_INTERNAL_ACCESS_DENIED;
  } else {
    if (want_forbidden != 0) {
      return ACL_INTERNAL_ACCESS_DENIED;
    }

    /*
      About SELECT_ACL:
      There is no point in hiding (by enforcing ACCESS_DENIED for SELECT_ACL
      on performance_schema.*) tables that do not exist anyway.
      When SELECT_ACL is granted on performance_schema.* or *.*,
      SELECT * from performance_schema.wrong_table
      will fail with a more understandable ER_NO_SUCH_TABLE error,
      instead of ER_TABLEACCESS_DENIED_ERROR.
      The same goes for other DML (INSERT_ACL | UPDATE_ACL | DELETE_ACL),
      for ease of use: error messages will be less surprising.

      About DROP_ACL:
      "Unknown" tables are not supposed to be here,
      so allowing DROP_ACL to make cleanup possible.
    */
    return ACL_INTERNAL_ACCESS_CHECK_GRANT;
  }
}

/*
  Common body for int key reads
  DS : data size in bytes
  KT : key type
  DT : data type
  KORR : storage/memory conversion function
*/
#define READ_INT_COMMON(DS, KT, DT, KORR)                                \
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len) {  \
    size_t data_size = DS;                                               \
    assert(m_remaining_key_part_info->type == KT);                       \
    assert(m_remaining_key_part_info->store_length >= data_size);        \
    isnull = false;                                                      \
    if (m_remaining_key_part_info->field->is_nullable()) {               \
      if (m_remaining_key[0]) {                                          \
        isnull = true;                                                   \
      }                                                                  \
      m_remaining_key += HA_KEY_NULL_LENGTH;                             \
      m_remaining_key_len -= HA_KEY_NULL_LENGTH;                         \
    }                                                                    \
    DT data = KORR(m_remaining_key);                                     \
    m_remaining_key += data_size;                                        \
    m_remaining_key_len -= (uint)data_size;                              \
    m_parts_found++;                                                     \
    m_remaining_key_part_info++;                                         \
    *value = data;                                                       \
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT); \
  }                                                                      \
  assert(m_remaining_key_len == 0);                                      \
  return HA_READ_INVALID

enum ha_rkey_function PFS_key_reader::read_int8(enum ha_rkey_function find_flag,
                                                bool &isnull, char *value) {
  READ_INT_COMMON(1, HA_KEYTYPE_INT8, char, mi_sint1korr);
}

enum ha_rkey_function PFS_key_reader::read_uint8(
    enum ha_rkey_function find_flag, bool &isnull, uchar *value) {
  READ_INT_COMMON(1, HA_KEYTYPE_BINARY, unsigned char, mi_uint1korr);
}

enum ha_rkey_function PFS_key_reader::read_int16(
    enum ha_rkey_function find_flag, bool &isnull, short *value) {
  READ_INT_COMMON(2, HA_KEYTYPE_SHORT_INT, short, sint2korr);
}

enum ha_rkey_function PFS_key_reader::read_uint16(
    enum ha_rkey_function find_flag, bool &isnull, ushort *value) {
  READ_INT_COMMON(2, HA_KEYTYPE_USHORT_INT, unsigned short, sint2korr);
}

enum ha_rkey_function PFS_key_reader::read_int24(
    enum ha_rkey_function find_flag, bool &isnull, long *value) {
  READ_INT_COMMON(3, HA_KEYTYPE_INT24, long, sint3korr);
}

enum ha_rkey_function PFS_key_reader::read_uint24(
    enum ha_rkey_function find_flag, bool &isnull, ulong *value) {
  READ_INT_COMMON(3, HA_KEYTYPE_UINT24, unsigned long, uint3korr);
}

enum ha_rkey_function PFS_key_reader::read_long(enum ha_rkey_function find_flag,
                                                bool &isnull, long *value) {
  READ_INT_COMMON(4, HA_KEYTYPE_LONG_INT, long, sint4korr);
}

enum ha_rkey_function PFS_key_reader::read_ulong(
    enum ha_rkey_function find_flag, bool &isnull, ulong *value) {
  READ_INT_COMMON(4, HA_KEYTYPE_ULONG_INT, long, uint4korr);
}

enum ha_rkey_function PFS_key_reader::read_longlong(
    enum ha_rkey_function find_flag, bool &isnull, longlong *value) {
  READ_INT_COMMON(8, HA_KEYTYPE_LONGLONG, long long, sint8korr);
}

enum ha_rkey_function PFS_key_reader::read_ulonglong(
    enum ha_rkey_function find_flag, bool &isnull, ulonglong *value) {
  READ_INT_COMMON(8, HA_KEYTYPE_ULONGLONG, unsigned long long, uint8korr);
}

enum ha_rkey_function PFS_key_reader::read_timestamp(
    enum ha_rkey_function find_flag, bool &isnull, ulonglong *value, uint dec) {
  size_t data_size = 4 + ((size_t)((dec + 1) / 2));
  my_timeval tm;

  if (m_remaining_key_part_info->store_length <= m_remaining_key_len) {
    assert(m_remaining_key_part_info->type == HA_KEYTYPE_BINARY);
    assert(m_remaining_key_part_info->store_length >= data_size);
    isnull = false;
    if (m_remaining_key_part_info->field->is_nullable()) {
      if (m_remaining_key[0]) {
        isnull = true;
      }
      m_remaining_key += HA_KEY_NULL_LENGTH;
      m_remaining_key_len -= HA_KEY_NULL_LENGTH;
    }
    my_timestamp_from_binary(&tm, m_remaining_key, dec);
    ulonglong data =
        static_cast<ulonglong>(tm.m_tv_sec) * 1000000ULL + tm.m_tv_usec;
    m_remaining_key += data_size;
    m_remaining_key_len -= (uint)data_size;
    m_parts_found++;
    m_remaining_key_part_info++;
    *value = data;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }
  assert(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

enum ha_rkey_function PFS_key_reader::read_varchar_utf8(
    enum ha_rkey_function find_flag, bool &isnull, char *buffer,
    uint *buffer_length, uint buffer_capacity [[maybe_unused]]) {
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len) {
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
    if (m_remaining_key_part_info->field->is_nullable()) {
      assert(HA_KEY_NULL_LENGTH <= m_remaining_key_len);

      length_offset++;
      data_offset++;
      if (m_remaining_key[0]) {
        isnull = true;
      }
    }

    assert(m_remaining_key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
           m_remaining_key_part_info->type == HA_KEYTYPE_VARTEXT2);

    assert(data_offset <= m_remaining_key_len);
    size_t string_len = uint2korr(m_remaining_key + length_offset);
    assert(data_offset + string_len <= m_remaining_key_part_info->store_length);
    assert(data_offset + string_len <= m_remaining_key_len);
    assert(string_len <= buffer_capacity);

    memcpy(buffer, m_remaining_key + data_offset, string_len);
    *buffer_length = (uint)string_len;

    uchar *pos = (uchar *)buffer;
    const uchar *end = skip_trailing_space(pos, string_len);
    *buffer_length = (uint)(end - pos);

    m_remaining_key += m_remaining_key_part_info->store_length;
    m_remaining_key_len -= m_remaining_key_part_info->store_length;
    m_parts_found++;
    m_remaining_key_part_info++;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  assert(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

enum ha_rkey_function PFS_key_reader::read_text_utf8(
    enum ha_rkey_function find_flag, bool &isnull, char *buffer,
    uint *buffer_length, uint buffer_capacity [[maybe_unused]]) {
  if (m_remaining_key_part_info->store_length <= m_remaining_key_len) {
    /*
      Stored as:
      - 0 or 1 null byte
      - followed by data
      - Length determined by key definition
    */
    assert(m_remaining_key_part_info->type == HA_KEYTYPE_TEXT);

    size_t data_offset = 0;
    isnull = false;
    if (m_remaining_key_part_info->field->is_nullable()) {
      assert(HA_KEY_NULL_LENGTH <= m_remaining_key_len);

      data_offset++;
      if (m_remaining_key[0]) {
        isnull = true;
      }
    }

    assert(data_offset <= m_remaining_key_len);
    size_t string_len = m_remaining_key_part_info->length;
    assert(data_offset + string_len <= m_remaining_key_part_info->store_length);
    assert(data_offset + string_len <= m_remaining_key_len);
    assert(string_len <= buffer_capacity);

    memcpy(buffer, m_remaining_key + data_offset, string_len);
    *buffer_length = (uint)string_len;

    const CHARSET_INFO *cs = &my_charset_utf8mb4_bin;
    uchar *pos = (uchar *)buffer;
    if (cs->mbmaxlen > 1) {
      size_t char_length;
      char_length =
          my_charpos(cs, pos, pos + string_len, string_len / cs->mbmaxlen);
      string_len = std::min(string_len, char_length);
    }
    const uchar *end = skip_trailing_space(pos, string_len);
    *buffer_length = (uint)(end - pos);

    m_remaining_key += m_remaining_key_part_info->store_length;
    m_remaining_key_len -= m_remaining_key_part_info->store_length;
    m_parts_found++;
    m_remaining_key_part_info++;
    return ((m_remaining_key_len == 0) ? find_flag : HA_READ_KEY_EXACT);
  }

  assert(m_remaining_key_len == 0);
  return HA_READ_INVALID;
}

void PFS_engine_index::read_key(const uchar *key, uint key_len,
                                enum ha_rkey_function find_flag) {
  PFS_key_reader reader(m_key_info, key, key_len);

  if (m_key_ptr_1 != nullptr) {
    assert(native_strcasecmp(m_key_info->key_part[0].field->field_name,
                             m_key_ptr_1->m_name) == 0);
    m_key_ptr_1->read(reader, find_flag);
  }

  if (m_key_ptr_2 != nullptr) {
    assert(native_strcasecmp(m_key_info->key_part[1].field->field_name,
                             m_key_ptr_2->m_name) == 0);
    m_key_ptr_2->read(reader, find_flag);
  }

  if (m_key_ptr_3 != nullptr) {
    assert(native_strcasecmp(m_key_info->key_part[2].field->field_name,
                             m_key_ptr_3->m_name) == 0);
    m_key_ptr_3->read(reader, find_flag);
  }

  if (m_key_ptr_4 != nullptr) {
    assert(native_strcasecmp(m_key_info->key_part[3].field->field_name,
                             m_key_ptr_4->m_name) == 0);
    m_key_ptr_4->read(reader, find_flag);
  }

  if (m_key_ptr_5 != nullptr) {
    assert(native_strcasecmp(m_key_info->key_part[4].field->field_name,
                             m_key_ptr_5->m_name) == 0);
    m_key_ptr_5->read(reader, find_flag);
  }

  m_fields = reader.m_parts_found;
}

/** @} */
