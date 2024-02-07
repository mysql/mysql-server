/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/pfs_autosize.cc
  Private interface for the server (implementation).
*/

#include <assert.h>
#include <sys/types.h>
#include <algorithm>

#include "my_psi_config.h"
#include "my_thread.h" /* For pthread_t */
#include "sql/sql_const.h"
#include "sql/sys_vars.h"
#include "storage/perfschema/pfs_server.h"

using std::max;
using std::min;

/** Performance schema sizing heuristics. */
struct PFS_sizing_data {
  /** Default value for @c PFS_param.m_events_waits_history_sizing. */
  ulong m_events_waits_history_sizing;
  /** Default value for @c PFS_param.m_events_waits_history_long_sizing. */
  ulong m_events_waits_history_long_sizing;
  /** Default value for @c PFS_param.m_events_stages_history_sizing. */
  ulong m_events_stages_history_sizing;
  /** Default value for @c PFS_param.m_events_stages_history_long_sizing. */
  ulong m_events_stages_history_long_sizing;
  /** Default value for @c PFS_param.m_events_statements_history_sizing. */
  ulong m_events_statements_history_sizing;
  /** Default value for @c PFS_param.m_events_statements_history_long_sizing. */
  ulong m_events_statements_history_long_sizing;
  /** Default value for @c PFS_param.m_events_transactions_history_sizing. */
  ulong m_events_transactions_history_sizing;
  /** Default value for @c PFS_param.m_events_transactions_history_long_sizing.
   */
  ulong m_events_transactions_history_long_sizing;
  /** Default value for @c PFS_param.m_digest_sizing. */
  ulong m_digest_sizing;
  /** Default value for @c PFS_param.m_session_connect_attrs_sizing. */
  ulong m_session_connect_attrs_sizing;
};

PFS_sizing_data small_data = {
    /* History sizes */
    5, 100, 5, 100, 5, 100, 5, 100,
    /* Digests */
    1000,
    /* Session connect attrs. */
    512};

PFS_sizing_data medium_data = {
    /* History sizes */
    10, 1000, 10, 1000, 10, 1000, 10, 1000,
    /* Digests */
    5000,
    /* Session connect attrs. */
    512};

PFS_sizing_data large_data = {
    /* History sizes */
    10, 10000, 10, 10000, 10, 10000, 10, 10000,
    /* Digests */
    10000,
    /* Session connect attrs. */
    512};

static PFS_sizing_data *estimate_hints(PFS_global_param *param) {
  if ((param->m_hints.m_max_connections <= MAX_CONNECTIONS_DEFAULT) &&
      (param->m_hints.m_table_definition_cache <= TABLE_DEF_CACHE_DEFAULT) &&
      (param->m_hints.m_table_open_cache <= TABLE_OPEN_CACHE_DEFAULT)) {
    /* The my.cnf used is either unchanged, or lower than factory defaults. */
    return &small_data;
  }

  if ((param->m_hints.m_max_connections <= MAX_CONNECTIONS_DEFAULT * 2) &&
      (param->m_hints.m_table_definition_cache <=
       TABLE_DEF_CACHE_DEFAULT * 2) &&
      (param->m_hints.m_table_open_cache <= TABLE_OPEN_CACHE_DEFAULT * 2)) {
    /* Some defaults have been increased, to "moderate" values. */
    return &medium_data;
  }

  /* Looks like a server in production. */
  return &large_data;
}

static void apply_heuristic(PFS_global_param *p, PFS_sizing_data *h) {
  if (p->m_events_waits_history_sizing < 0) {
    p->m_events_waits_history_sizing = h->m_events_waits_history_sizing;
  }

  if (p->m_events_waits_history_long_sizing < 0) {
    p->m_events_waits_history_long_sizing =
        h->m_events_waits_history_long_sizing;
  }

  if (p->m_events_stages_history_sizing < 0) {
    p->m_events_stages_history_sizing = h->m_events_stages_history_sizing;
  }

  if (p->m_events_stages_history_long_sizing < 0) {
    p->m_events_stages_history_long_sizing =
        h->m_events_stages_history_long_sizing;
  }

  if (p->m_events_statements_history_sizing < 0) {
    p->m_events_statements_history_sizing =
        h->m_events_statements_history_sizing;
  }

  if (p->m_events_statements_history_long_sizing < 0) {
    p->m_events_statements_history_long_sizing =
        h->m_events_statements_history_long_sizing;
  }

  if (p->m_digest_sizing < 0) {
    p->m_digest_sizing = h->m_digest_sizing;
  }

  if (p->m_events_transactions_history_sizing < 0) {
    p->m_events_transactions_history_sizing =
        h->m_events_transactions_history_sizing;
  }

  if (p->m_events_transactions_history_long_sizing < 0) {
    p->m_events_transactions_history_long_sizing =
        h->m_events_transactions_history_long_sizing;
  }

  if (p->m_session_connect_attrs_sizing < 0) {
    p->m_session_connect_attrs_sizing = h->m_session_connect_attrs_sizing;
  }
}

void pfs_automated_sizing(PFS_global_param *param) {
  if (param->m_enabled) {
#ifndef HAVE_PSI_MUTEX_INTERFACE
    param->m_mutex_class_sizing = 0;
    param->m_mutex_sizing = 0;
#endif

#ifndef HAVE_PSI_RWLOCK_INTERFACE
    param->m_rwlock_class_sizing = 0;
    param->m_rwlock_sizing = 0;
#endif

#ifndef HAVE_PSI_COND_INTERFACE
    param->m_cond_class_sizing = 0;
    param->m_cond_sizing = 0;
#endif

#ifndef HAVE_PSI_FILE_INTERFACE
    param->m_file_class_sizing = 0;
    param->m_file_sizing = 0;
    param->m_file_handle_sizing = 0;
#endif

#ifndef HAVE_PSI_TABLE_INTERFACE
    param->m_table_share_sizing = 0;
    param->m_table_sizing = 0;
    param->m_table_lock_stat_sizing = 0;
    param->m_index_stat_sizing = 0;
#endif

#ifndef HAVE_PSI_SOCKET_INTERFACE
    param->m_socket_class_sizing = 0;
    param->m_socket_sizing = 0;
#endif

#ifndef HAVE_PSI_STAGE_INTERFACE
    param->m_stage_class_sizing = 0;
    param->m_events_stages_history_sizing = 0;
    param->m_events_stages_history_long_sizing = 0;
#endif

#ifndef HAVE_PSI_STATEMENT_INTERFACE
    param->m_statement_class_sizing = 0;
    param->m_events_statements_history_sizing = 0;
    param->m_events_statements_history_long_sizing = 0;
#endif

#ifndef HAVE_PSI_SP_INTERFACE
    param->m_program_sizing = 0;
    if (param->m_statement_stack_sizing > 1) {
      param->m_statement_stack_sizing = 1;
    }
#endif

#ifndef HAVE_PSI_PS_INTERFACE
    param->m_prepared_stmt_sizing = 0;
#endif

#ifndef HAVE_PSI_STATEMENT_DIGEST_INTERFACE
    param->m_digest_sizing = 0;
#endif

#ifndef HAVE_PSI_METADATA_INTERFACE
    param->m_metadata_lock_sizing = 0;
#endif

#ifndef HAVE_PSI_MEMORY_INTERFACE
    param->m_memory_class_sizing = 0;
#endif

#ifndef HAVE_PSI_METRICS_INTERFACE
    param->m_meter_class_sizing = 0;
    param->m_metric_class_sizing = 0;
#endif

    PFS_sizing_data *heuristic;
    heuristic = estimate_hints(param);
    apply_heuristic(param, heuristic);

    assert(param->m_events_waits_history_sizing >= 0);
    assert(param->m_events_waits_history_long_sizing >= 0);
    assert(param->m_events_stages_history_sizing >= 0);
    assert(param->m_events_stages_history_long_sizing >= 0);
    assert(param->m_events_statements_history_sizing >= 0);
    assert(param->m_events_statements_history_long_sizing >= 0);
    assert(param->m_events_transactions_history_sizing >= 0);
    assert(param->m_events_transactions_history_long_sizing >= 0);
    assert(param->m_session_connect_attrs_sizing >= 0);
  } else {
    /*
      The Performance Schema is disabled. Set the instrument sizings to zero to
      disable all instrumentation while retaining support for the status and
      system variable tables, the host cache table and the replication tables.
    */
    param->m_mutex_class_sizing = 0;
    param->m_rwlock_class_sizing = 0;
    param->m_cond_class_sizing = 0;
    param->m_thread_class_sizing = 0;
    param->m_table_share_sizing = 0;
    param->m_table_lock_stat_sizing = 0;
    param->m_index_stat_sizing = 0;
    param->m_file_class_sizing = 0;
    param->m_mutex_sizing = 0;
    param->m_rwlock_sizing = 0;
    param->m_cond_sizing = 0;
    param->m_thread_sizing = 0;
    param->m_table_sizing = 0;
    param->m_file_sizing = 0;
    param->m_file_handle_sizing = 0;
    param->m_socket_sizing = 0;
    param->m_socket_class_sizing = 0;
    param->m_events_waits_history_sizing = 0;
    param->m_events_waits_history_long_sizing = 0;
    param->m_setup_actor_sizing = 0;
    param->m_setup_object_sizing = 0;
    param->m_host_sizing = 0;
    param->m_user_sizing = 0;
    param->m_account_sizing = 0;
    param->m_stage_class_sizing = 0;
    param->m_events_stages_history_sizing = 0;
    param->m_events_stages_history_long_sizing = 0;
    param->m_statement_class_sizing = 0;
    param->m_events_statements_history_sizing = 0;
    param->m_events_statements_history_long_sizing = 0;
    param->m_digest_sizing = 0;
    param->m_program_sizing = 0;
    param->m_prepared_stmt_sizing = 0;
    param->m_events_transactions_history_sizing = 0;
    param->m_events_transactions_history_long_sizing = 0;
    param->m_session_connect_attrs_sizing = 0;
    param->m_statement_stack_sizing = 0;
    param->m_memory_class_sizing = 0;
    param->m_meter_class_sizing = 0;
    param->m_metric_class_sizing = 0;
    param->m_metadata_lock_sizing = 0;
    param->m_max_digest_length = 0;
    param->m_max_sql_text_length = 0;
  }
}
