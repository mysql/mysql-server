/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_SERVER_H
#define PFS_SERVER_H

/**
  @file storage/perfschema/pfs_server.h
  Private interface for the server (declarations).
*/

#include <sys/types.h>

#include "my_psi_config.h"
#include "mysql/psi/psi_cond.h"
#include "mysql/psi/psi_data_lock.h"
#include "mysql/psi/psi_error.h"
#include "mysql/psi/psi_file.h"
#include "mysql/psi/psi_idle.h"
#include "mysql/psi/psi_mdl.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/psi/psi_mutex.h"
#include "mysql/psi/psi_rwlock.h"
#include "mysql/psi/psi_socket.h"
#include "mysql/psi/psi_stage.h"
#include "mysql/psi/psi_statement.h"
#include "mysql/psi/psi_system.h"
#include "mysql/psi/psi_table.h"
#include "mysql/psi/psi_thread.h"
#include "mysql/psi/psi_transaction.h"

#ifdef HAVE_PSI_INTERFACE

#define PFS_AUTOSCALE_VALUE (-1)
#define PFS_AUTOSIZE_VALUE (-1)

#ifndef PFS_MAX_MUTEX_CLASS
#define PFS_MAX_MUTEX_CLASS 300
#endif
#ifndef PFS_MAX_RWLOCK_CLASS
#define PFS_MAX_RWLOCK_CLASS 60
#endif
#ifndef PFS_MAX_COND_CLASS
#define PFS_MAX_COND_CLASS 80
#endif
#ifndef PFS_MAX_THREAD_CLASS
#define PFS_MAX_THREAD_CLASS 100
#endif
#ifndef PFS_MAX_FILE_CLASS
#define PFS_MAX_FILE_CLASS 80
#endif
#ifndef PFS_MAX_FILE_HANDLE
#define PFS_MAX_FILE_HANDLE 32768
#endif
#ifndef PFS_MAX_SOCKET_CLASS
#define PFS_MAX_SOCKET_CLASS 10
#endif
#ifndef PFS_MAX_STAGE_CLASS
#define PFS_MAX_STAGE_CLASS 150
#endif
#ifndef PFS_STATEMENTS_STACK_SIZE
#define PFS_STATEMENTS_STACK_SIZE 10
#endif
#ifndef PFS_MAX_MEMORY_CLASS
#define PFS_MAX_MEMORY_CLASS 450
#endif

#ifndef PFS_MAX_SERVER_ERRORS
#define PFS_MAX_SERVER_ERRORS ((total_error_count - obsolete_error_count) + 1)
#endif

/** Sizing hints, from the server configuration. */
struct PFS_sizing_hints {
  /** Value of @c Sys_table_def_size */
  ulong m_table_definition_cache;
  /** Value of @c Sys_table_cache_size */
  long m_table_open_cache;
  /** Value of @c Sys_max_connections */
  long m_max_connections;
  /** Value of @c Sys_open_files_limit */
  long m_open_files_limit;
  /** Value of @c Sys_max_prepared_stmt_count */
  long m_max_prepared_stmt_count;
};

/** Performance schema global sizing parameters. */
struct PFS_global_param {
  /** True if the performance schema is enabled. */
  bool m_enabled;
  /** Default values for SETUP_CONSUMERS. */
  bool m_consumer_events_stages_current_enabled;
  bool m_consumer_events_stages_history_enabled;
  bool m_consumer_events_stages_history_long_enabled;
  bool m_consumer_events_statements_current_enabled;
  bool m_consumer_events_statements_history_enabled;
  bool m_consumer_events_statements_history_long_enabled;
  bool m_consumer_events_transactions_current_enabled;
  bool m_consumer_events_transactions_history_enabled;
  bool m_consumer_events_transactions_history_long_enabled;
  bool m_consumer_events_waits_current_enabled;
  bool m_consumer_events_waits_history_enabled;
  bool m_consumer_events_waits_history_long_enabled;
  bool m_consumer_global_instrumentation_enabled;
  bool m_consumer_thread_instrumentation_enabled;
  bool m_consumer_statement_digest_enabled;

  /** Default instrument configuration option. */
  char *m_pfs_instrument;

  /**
    Maximum number of instrumented mutex classes.
    @sa mutex_class_lost.
  */
  ulong m_mutex_class_sizing;
  /**
    Maximum number of instrumented rwlock classes.
    @sa rwlock_class_lost.
  */
  ulong m_rwlock_class_sizing;
  /**
    Maximum number of instrumented cond classes.
    @sa cond_class_lost.
  */
  ulong m_cond_class_sizing;
  /**
    Maximum number of instrumented thread classes.
    @sa thread_class_lost.
  */
  ulong m_thread_class_sizing;
  /**
    Maximum number of instrumented table share.
    @sa table_share_lost.
  */
  long m_table_share_sizing;
  /**
    Maximum number of lock statistics collected for tables.
    @sa table_lock_stat_lost.
  */
  long m_table_lock_stat_sizing;
  /**
    Maximum number of index statistics collected for tables.
    @sa table_index_lost.
  */
  long m_index_stat_sizing;
  /**
    Maximum number of instrumented file classes.
    @sa file_class_lost.
  */
  ulong m_file_class_sizing;
  /**
    Maximum number of instrumented mutex instances.
    @sa mutex_lost.
  */
  long m_mutex_sizing;
  /**
    Maximum number of instrumented rwlock instances.
    @sa rwlock_lost.
  */
  long m_rwlock_sizing;
  /**
    Maximum number of instrumented cond instances.
    @sa cond_lost.
  */
  long m_cond_sizing;
  /**
    Maximum number of instrumented thread instances.
    @sa thread_lost.
  */
  long m_thread_sizing;
  /**
    Maximum number of instrumented table handles.
    @sa table_lost.
  */
  long m_table_sizing;
  /**
    Maximum number of instrumented file instances.
    @sa file_lost.
  */
  long m_file_sizing;
  /**
    Maximum number of instrumented file handles.
    @sa file_handle_lost.
  */
  long m_file_handle_sizing;
  /**
    Maximum number of instrumented socket instances
    @sa socket_lost
  */
  long m_socket_sizing;
  /**
    Maximum number of instrumented socket classes.
    @sa socket_class_lost.
  */
  ulong m_socket_class_sizing;
  /** Maximum number of rows per thread in table EVENTS_WAITS_HISTORY. */
  long m_events_waits_history_sizing;
  /** Maximum number of rows in table EVENTS_WAITS_HISTORY_LONG. */
  long m_events_waits_history_long_sizing;
  /** Maximum number of rows in table SETUP_ACTORS. */
  long m_setup_actor_sizing;
  /** Maximum number of rows in table SETUP_OBJECTS. */
  long m_setup_object_sizing;
  /** Maximum number of rows in table HOSTS. */
  long m_host_sizing;
  /** Maximum number of rows in table USERS. */
  long m_user_sizing;
  /** Maximum number of rows in table ACCOUNTS. */
  long m_account_sizing;
  /**
    Maximum number of instrumented stage classes.
    @sa stage_class_lost.
  */
  ulong m_stage_class_sizing;
  /** Maximum number of rows per thread in table EVENTS_STAGES_HISTORY. */
  long m_events_stages_history_sizing;
  /** Maximum number of rows in table EVENTS_STAGES_HISTORY_LONG. */
  long m_events_stages_history_long_sizing;
  /**
    Maximum number of instrumented statement classes.
    @sa statement_class_lost.
  */
  ulong m_statement_class_sizing;
  /** Maximum number of rows per thread in table EVENTS_STATEMENTS_HISTORY. */
  long m_events_statements_history_sizing;
  /** Maximum number of rows in table EVENTS_STATEMENTS_HISTORY_LONG. */
  long m_events_statements_history_long_sizing;
  /** Maximum number of digests to be captured */
  long m_digest_sizing;
  /** Maximum number of programs to be captured */
  long m_program_sizing;
  /** Maximum number of prepared statements to be captured */
  long m_prepared_stmt_sizing;
  /** Maximum number of rows per thread in table EVENTS_TRANSACTIONS_HISTORY. */
  long m_events_transactions_history_sizing;
  /** Maximum number of rows in table EVENTS_TRANSACTIONS_HISTORY_LONG. */
  long m_events_transactions_history_long_sizing;

  /** Maximum number of session attribute strings per thread */
  long m_session_connect_attrs_sizing;
  /** Maximum size of statement stack */
  ulong m_statement_stack_sizing;

  /**
    Maximum number of instrumented memory classes.
    @sa memory_class_lost.
  */
  ulong m_memory_class_sizing;

  long m_metadata_lock_sizing;

  long m_max_digest_length;
  ulong m_max_sql_text_length;

  /** Maximum age in seconds for a query sample. */
  ulong m_max_digest_sample_age;

  /** Maximum number of error instrumented */
  ulong m_error_sizing;

  /** Sizing hints, for auto tuning. */
  PFS_sizing_hints m_hints;
};

/**
  Performance schema sizing values for the server.
  This global variable is set when parsing server startup options.
*/
extern PFS_global_param pfs_param;

/**
  Null initialization.
  Disable all instrumentation, size all internal buffers to 0.
  This pre initialization step is needed to ensure that events can be collected
  and discarded, until such time @c initialize_performance_schema() is called.
*/
void pre_initialize_performance_schema();

/**
  Initialize the performance schema.
  The performance schema implement several instrumentation services.
  Each instrumentation service is versioned, and accessible through
  a bootstrap structure, returned as output parameter.
  @param param Size parameters to use.
  @param [out] thread_bootstrap Thread instrumentation service bootstrap
  @param [out] mutex_bootstrap Mutex instrumentation service bootstrap
  @param [out] rwlock_bootstrap Rwlock instrumentation service bootstrap
  @param [out] cond_bootstrap Condition instrumentation service bootstrap
  @param [out] file_bootstrap File instrumentation service bootstrap
  @param [out] socket_bootstrap Socket instrumentation service bootstrap
  @param [out] table_bootstrap Table instrumentation service bootstrap
  @param [out] mdl_bootstrap Metadata Lock instrumentation service bootstrap
  @param [out] idle_bootstrap Idle instrumentation service bootstrap
  @param [out] stage_bootstrap Stage instrumentation service bootstrap
  @param [out] statement_bootstrap Statement instrumentation service bootstrap
  @param [out] transaction_bootstrap Transaction instrumentation service
  bootstrap
  @param [out] memory_bootstrap Memory instrumentation service bootstrap
  @param [out] error_bootstrap Error instrumentation service bootstrap
  @param [out] data_lock_bootstrap Data Lock instrumentation service bootstrap
  @param [out] system_bootstrap System instrumentation service bootstrap
  @returns
    @retval 0 success
*/
int initialize_performance_schema(
    PFS_global_param *param, PSI_thread_bootstrap **thread_bootstrap,
    PSI_mutex_bootstrap **mutex_bootstrap,
    PSI_rwlock_bootstrap **rwlock_bootstrap,
    PSI_cond_bootstrap **cond_bootstrap, PSI_file_bootstrap **file_bootstrap,
    PSI_socket_bootstrap **socket_bootstrap,
    PSI_table_bootstrap **table_bootstrap, PSI_mdl_bootstrap **mdl_bootstrap,
    PSI_idle_bootstrap **idle_bootstrap, PSI_stage_bootstrap **stage_bootstrap,
    PSI_statement_bootstrap **statement_bootstrap,
    PSI_transaction_bootstrap **transaction_bootstrap,
    PSI_memory_bootstrap **memory_bootstrap,
    PSI_error_bootstrap **error_bootstrap,
    PSI_data_lock_bootstrap **data_lock_bootstrap,
    PSI_system_bootstrap **system_bootstrap);

void pfs_automated_sizing(PFS_global_param *param);

/**
  Initialize the performance schema ACL.
  ACL is strictly enforced when the server is running in normal mode,
  to enforce that only legal operations are allowed.
  When running in bootstrap mode, ACL restrictions are relaxed,
  to allow the bootstrap scripts to DROP / CREATE performance schema tables.
  @sa ACL_internal_schema_registry
  @param bootstrap True if the server is starting in bootstrap mode.
*/
void initialize_performance_schema_acl(bool bootstrap);

/**
  Reset the aggregated status counter stats.
*/
void reset_pfs_status_stats();

/**
  Initialize the dynamic array holding individual instrument settings collected
  from the server configuration options.
*/
void init_pfs_instrument_array();

/**
  Process one PFS_INSTRUMENT configuration string.
*/
int add_pfs_instr_to_array(const char *name, const char *value);

/**
  Register/unregister notification service.
*/
int register_pfs_notification_service();
int unregister_pfs_notification_service();

/**
  Register/unregister resource group service.
*/
int register_pfs_resource_group_service();
int unregister_pfs_resource_group_service();

/**
  Shutdown the performance schema.
*/
void shutdown_performance_schema();

#endif /* HAVE_PSI_INTERFACE */

#endif /* PFS_SERVER_H */
