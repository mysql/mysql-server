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
#include "pfs_server.h"

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

static const ulong mutex_per_share= 3;
static const ulong rwlock_per_share= 3;
static const ulong cond_per_share= 1;
static const ulong file_per_share= 2;
static const ulong socket_per_share= 0;
static const ulong thread_per_share= 0;

struct PFS_sizing_data
{
  const char* m_name;
  ulong m_account_sizing;
  ulong m_user_sizing;
  ulong m_host_sizing;

  ulong m_events_waits_history_sizing;
  ulong m_events_waits_history_long_sizing;
  ulong m_events_stages_history_sizing;
  ulong m_events_stages_history_long_sizing;
  ulong m_events_statements_history_sizing;
  ulong m_events_statements_history_long_sizing;
  ulong m_digest_sizing;

  ulong m_max_number_of_tables;

  float m_load_factor_volatile;
  float m_load_factor_normal;
  float m_load_factor_static;
};

PFS_sizing_data tiny_data=
{
  "HEURISTIC 1",
  /* account / user / host */
  10, 5, 20,
  /* history sizes */
  5, 100, 5, 100, 5, 100,
  /* digests */
  1000,
  /* Max tables */
  200,
  /* Load factors */
  0.90, 0.90, 0.90
};

PFS_sizing_data medium_data=
{
  "HEURISTIC 2",
  /* account / user / host */
  100, 100, 100,
  /* history sizes */
  10, 1000, 10, 1000, 10, 1000,
  /* digests */
  5000,
  /* Max tables */
  500,
  /* Load factors */
  0.70, 0.80, 0.90
};

PFS_sizing_data big_data=
{
  "HEURISTIC 3",
  /* account / user / host */
  100, 100, 100,
  /* history sizes */
  10, 10000, 10, 10000, 10, 10000,
  /* digests */
  10000,
  /* Max tables */
  10000,
  /* Load factors */
  0.50, 0.65, 0.80
};

void enforce_range_long(long *value, long min, long max)
{
  if (*value < min)
  {
    *value = min;
  }
  else if (*value > max)
  {
    *value = max;
  }
}

PFS_sizing_data *estimate_hints(PFS_global_param *param)
{
  /*
    Sanitize hints, to avoid returning extremely high or low estimates.
    If the real configuration used is outside of these bounds,
    manual tuning will be preferable.
  */

  enforce_range_long(& param->m_hints.m_max_connections, 10, 65535);
  enforce_range_long(& param->m_hints.m_table_definition_cache, 100, 10000);
  enforce_range_long(& param->m_hints.m_table_open_cache, 100, 10000);

  if ((param->m_hints.m_max_connections <= MAX_CONNECTIONS_DEFAULT) &&
      (param->m_hints.m_table_definition_cache <= TABLE_DEF_CACHE_DEFAULT) &&
      (param->m_hints.m_table_open_cache <= TABLE_OPEN_CACHE_DEFAULT))
  {
    /* The my.cnf used is either unchanged, or lower that factory defaults. */
    return & tiny_data;
  }

  if ((param->m_hints.m_max_connections <= MAX_CONNECTIONS_DEFAULT) &&
      (param->m_hints.m_table_definition_cache <= TABLE_DEF_CACHE_DEFAULT) &&
      (param->m_hints.m_table_open_cache <= TABLE_OPEN_CACHE_DEFAULT))
  {
    /* Some defaults have been increased, to "moderate" values. */
    return & medium_data;
  }

  /* Looks like a server in production. */
  return & big_data;

}

static void apply_heuristic(PFS_global_param *p, PFS_sizing_data *h)
{
  ulong con = p->m_hints.m_max_connections;
  ulong handle = p->m_hints.m_table_open_cache;
  ulong share = p->m_hints.m_table_definition_cache;

  if (p->m_table_sizing < 0)
  {
    ulong count;
    count= handle;

    p->m_table_sizing= ceil(((float) count) / h->m_load_factor_volatile);
  }

  if (p->m_table_share_sizing < 0)
  {
    ulong count;
    count= share;

    p->m_table_share_sizing= ceil(((float) count) / h->m_load_factor_static);
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

  if (p->m_mutex_sizing < 0)
  {
    ulong count;
    count= fixed_mutex_instances
      + con * mutex_per_connection
      + handle * mutex_per_handle
      + share * mutex_per_share
      ;

    p->m_mutex_sizing= ceil(((float) count) / h->m_load_factor_volatile);
  }

  if (p->m_rwlock_sizing < 0)
  {
    ulong count;
    count= fixed_rwlock_instances
      + con * rwlock_per_connection
      + handle * rwlock_per_handle
      + share * rwlock_per_share
      ;

    p->m_rwlock_sizing= ceil(((float) count) / h->m_load_factor_volatile);
  }

  if (p->m_cond_sizing < 0)
  {
    ulong count;
    count= fixed_cond_instances
      + con * cond_per_connection
      + handle * cond_per_handle
      + share * cond_per_share
      ;

    p->m_cond_sizing= ceil(((float) count) / h->m_load_factor_volatile);
  }

  if (p->m_file_sizing < 0)
  {
    ulong count;
    count= fixed_file_instances
      + con * file_per_connection
      + handle * file_per_handle
      + share * file_per_share
      ;

    p->m_file_sizing= ceil(((float) count) / h->m_load_factor_normal);
  }

  if (p->m_socket_sizing < 0)
  {
    ulong count;
    count= fixed_socket_instances
      + con * socket_per_connection
      + handle * socket_per_handle
      + share * socket_per_share
      ;

    p->m_socket_sizing= ceil(((float) count) / h->m_load_factor_volatile);
  }

  if (p->m_thread_sizing < 0)
  {
    ulong count;
    count= fixed_thread_instances
      + con * thread_per_connection
      + handle * thread_per_handle
      + share * thread_per_share
      ;

    p->m_thread_sizing= ceil(((float) count) / h->m_load_factor_volatile);
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

  DBUG_ASSERT(param->m_mutex_sizing >= 0);
  DBUG_ASSERT(param->m_rwlock_sizing >= 0);
  DBUG_ASSERT(param->m_cond_sizing >= 0);
  DBUG_ASSERT(param->m_file_sizing >= 0);
  DBUG_ASSERT(param->m_socket_sizing >= 0);
  DBUG_ASSERT(param->m_thread_sizing >= 0);
  DBUG_ASSERT(param->m_table_sizing >= 0);
  DBUG_ASSERT(param->m_table_share_sizing >= 0);


}

