/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_autosize.cc
  Private interface for the server (implementation).
*/

#include "my_global.h"
#include "sql_const.h"
#include "pfs_server.h"

#include <algorithm>
using std::min;
using std::max;

static const ulong fixed_mutex_instances= 500;
static const ulong fixed_rwlock_instances= 200;
static const ulong fixed_cond_instances= 50;
static const ulong fixed_file_instances= 200;
static const ulong fixed_socket_instances= 10;
static const ulong fixed_thread_instances= 50;

static const ulong mutex_per_connection= 3;
static const ulong rwlock_per_connection= 1;
static const ulong cond_per_connection= 2;
static const ulong file_per_connection= 0;
static const ulong socket_per_connection= 1;
static const ulong thread_per_connection= 1;

static const ulong mutex_per_handle= 0;
static const ulong rwlock_per_handle= 0;
static const ulong cond_per_handle= 0;
static const ulong file_per_handle= 0;
static const ulong socket_per_handle= 0;
static const ulong thread_per_handle= 0;

static const ulong mutex_per_share= 5;
static const ulong rwlock_per_share= 3;
static const ulong cond_per_share= 1;
static const ulong file_per_share= 3;
static const ulong socket_per_share= 0;
static const ulong thread_per_share= 0;

struct PFS_sizing_data
{
  /** Default value for @c PFS_param.m_account_sizing. */
  ulong m_account_sizing;
  /** Default value for @c PFS_param.m_user_sizing. */
  ulong m_user_sizing;
  /** Default value for @c PFS_param.m_host_sizing. */
  ulong m_host_sizing;

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
  /** Default value for @c PFS_param.m_digest_sizing. */
  ulong m_digest_sizing;
  /** Default value for @c PFS_param.m_session_connect_attrs_sizing. */
  ulong m_session_connect_attrs_sizing;

  /**
    Minimum number of tables to keep statistics for.
    On small deployments, all the tables can fit into the table definition cache,
    and this value can be 0.
    On big deployments, the table definition cache is only a subset of all the tables
    in the database, which are accounted for here.
  */
  ulong m_min_number_of_tables;

  /**
    Load factor for 'volatile' objects (mutexes, table handles, ...).
    Instrumented objects that:
    - use little memory
    - are created/destroyed very frequently
    should be stored in a low density (mostly empty) memory buffer,
    to optimize for speed.
  */
  float m_load_factor_volatile;
  /**
    Load factor for 'normal' objects (files).
    Instrumented objects that:
    - use a medium amount of memory
    - are created/destroyed 
    should be stored in a medium density memory buffer,
    as a trade off between space and speed.
  */
  float m_load_factor_normal;
  /**
    Load factor for 'static' objects (table shares).
    Instrumented objects that:
    - use a lot of memory
    - are created/destroyed very rarely
    can be stored in a high density (mostly packed) memory buffer,
    to optimize for space.
  */
  float m_load_factor_static;
};

PFS_sizing_data small_data=
{
  /* Account / user / host */
  10, 5, 20,
  /* History sizes */
  5, 100, 5, 100, 5, 100,
  /* Digests */
  1000,
  /* Session connect attrs. */
  512,
  /* Min tables */
  200,
  /* Load factors */
  0.90, 0.90, 0.90
};

PFS_sizing_data medium_data=
{
  /* Account / user / host */
  100, 100, 100,
  /* History sizes */
  10, 1000, 10, 1000, 10, 1000,
  /* Digests */
  5000,
  /* Session connect attrs. */
  512,
  /* Min tables */
  500,
  /* Load factors */
  0.70, 0.80, 0.90
};

PFS_sizing_data large_data=
{
  /* Account / user / host */
  100, 100, 100,
  /* History sizes */
  10, 10000, 10, 10000, 10, 10000,
  /* Digests */
  10000,
  /* Session connect attrs. */
  512,
  /* Min tables */
  10000,
  /* Load factors */
  0.50, 0.65, 0.80
};

static inline ulong apply_load_factor(ulong raw_value, float factor)
{
  float value = ((float) raw_value) / factor;
  return (ulong) ceil(value);
}

PFS_sizing_data *estimate_hints(PFS_global_param *param)
{
  if ((param->m_hints.m_max_connections <= MAX_CONNECTIONS_DEFAULT) &&
      (param->m_hints.m_table_definition_cache <= TABLE_DEF_CACHE_DEFAULT) &&
      (param->m_hints.m_table_open_cache <= TABLE_OPEN_CACHE_DEFAULT))
  {
    /* The my.cnf used is either unchanged, or lower than factory defaults. */
    return & small_data;
  }

  if ((param->m_hints.m_max_connections <= MAX_CONNECTIONS_DEFAULT * 2) &&
      (param->m_hints.m_table_definition_cache <= TABLE_DEF_CACHE_DEFAULT * 2) &&
      (param->m_hints.m_table_open_cache <= TABLE_OPEN_CACHE_DEFAULT * 2))
  {
    /* Some defaults have been increased, to "moderate" values. */
    return & medium_data;
  }

  /* Looks like a server in production. */
  return & large_data;
}

static void apply_heuristic(PFS_global_param *p, PFS_sizing_data *h)
{
  ulong count;
  ulong con = p->m_hints.m_max_connections;
  ulong handle = p->m_hints.m_table_open_cache;
  ulong share = p->m_hints.m_table_definition_cache;
  ulong file = p->m_hints.m_open_files_limit;

  if (p->m_table_sizing < 0)
  {
    count= handle;

    p->m_table_sizing= apply_load_factor(count, h->m_load_factor_volatile);
  }

  if (p->m_table_share_sizing < 0)
  {
    count= share;

    count= max<ulong>(count, h->m_min_number_of_tables);
    p->m_table_share_sizing= apply_load_factor(count, h->m_load_factor_static);
  }

  if (p->m_account_sizing < 0)
  {
    p->m_account_sizing= h->m_account_sizing;
  }

  if (p->m_user_sizing < 0)
  {
    p->m_user_sizing= h->m_user_sizing;
  }

  if (p->m_host_sizing < 0)
  {
    p->m_host_sizing= h->m_host_sizing;
  }

  if (p->m_events_waits_history_sizing < 0)
  {
    p->m_events_waits_history_sizing= h->m_events_waits_history_sizing;
  }

  if (p->m_events_waits_history_long_sizing < 0)
  {
    p->m_events_waits_history_long_sizing= h->m_events_waits_history_long_sizing;
  }

  if (p->m_events_stages_history_sizing < 0)
  {
    p->m_events_stages_history_sizing= h->m_events_stages_history_sizing;
  }

  if (p->m_events_stages_history_long_sizing < 0)
  {
    p->m_events_stages_history_long_sizing= h->m_events_stages_history_long_sizing;
  }

  if (p->m_events_statements_history_sizing < 0)
  {
    p->m_events_statements_history_sizing= h->m_events_statements_history_sizing;
  }

  if (p->m_events_statements_history_long_sizing < 0)
  {
    p->m_events_statements_history_long_sizing= h->m_events_statements_history_long_sizing;
  }

  if (p->m_digest_sizing < 0)
  {
    p->m_digest_sizing= h->m_digest_sizing;
  }

  if (p->m_session_connect_attrs_sizing < 0)
  {
    p->m_session_connect_attrs_sizing= h->m_session_connect_attrs_sizing;
  }

  if (p->m_mutex_sizing < 0)
  {
    count= fixed_mutex_instances
      + con * mutex_per_connection
      + handle * mutex_per_handle
      + share * mutex_per_share;

    p->m_mutex_sizing= apply_load_factor(count, h->m_load_factor_volatile);
  }

  if (p->m_rwlock_sizing < 0)
  {
    count= fixed_rwlock_instances
      + con * rwlock_per_connection
      + handle * rwlock_per_handle
      + share * rwlock_per_share;

    p->m_rwlock_sizing= apply_load_factor(count, h->m_load_factor_volatile);
  }

  if (p->m_cond_sizing < 0)
  {
    ulong count;
    count= fixed_cond_instances
      + con * cond_per_connection
      + handle * cond_per_handle
      + share * cond_per_share;

    p->m_cond_sizing= apply_load_factor(count, h->m_load_factor_volatile);
  }

  if (p->m_file_sizing < 0)
  {
    count= fixed_file_instances
      + con * file_per_connection
      + handle * file_per_handle
      + share * file_per_share;

    count= max<ulong>(count, file);
    p->m_file_sizing= apply_load_factor(count, h->m_load_factor_normal);
  }

  if (p->m_socket_sizing < 0)
  {
    count= fixed_socket_instances
      + con * socket_per_connection
      + handle * socket_per_handle
      + share * socket_per_share;

    p->m_socket_sizing= apply_load_factor(count, h->m_load_factor_volatile);
  }

  if (p->m_thread_sizing < 0)
  {
    count= fixed_thread_instances
      + con * thread_per_connection
      + handle * thread_per_handle
      + share * thread_per_share;

    p->m_thread_sizing= apply_load_factor(count, h->m_load_factor_volatile);
  }
}

void pfs_automated_sizing(PFS_global_param *param)
{
  PFS_sizing_data *heuristic;
  heuristic= estimate_hints(param);
  apply_heuristic(param, heuristic);

  DBUG_ASSERT(param->m_account_sizing >= 0);
  DBUG_ASSERT(param->m_digest_sizing >= 0);
  DBUG_ASSERT(param->m_host_sizing >= 0);
  DBUG_ASSERT(param->m_user_sizing >= 0);

  DBUG_ASSERT(param->m_events_waits_history_sizing >= 0);
  DBUG_ASSERT(param->m_events_waits_history_long_sizing >= 0);
  DBUG_ASSERT(param->m_events_stages_history_sizing >= 0);
  DBUG_ASSERT(param->m_events_stages_history_long_sizing >= 0);
  DBUG_ASSERT(param->m_events_statements_history_sizing >= 0);
  DBUG_ASSERT(param->m_events_statements_history_long_sizing >= 0);
  DBUG_ASSERT(param->m_session_connect_attrs_sizing >= 0);

  DBUG_ASSERT(param->m_mutex_sizing >= 0);
  DBUG_ASSERT(param->m_rwlock_sizing >= 0);
  DBUG_ASSERT(param->m_cond_sizing >= 0);
  DBUG_ASSERT(param->m_file_sizing >= 0);
  DBUG_ASSERT(param->m_socket_sizing >= 0);
  DBUG_ASSERT(param->m_thread_sizing >= 0);
  DBUG_ASSERT(param->m_table_sizing >= 0);
  DBUG_ASSERT(param->m_table_share_sizing >= 0);
}

