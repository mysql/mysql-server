/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_status_by_host.cc
  Table STATUS_BY_HOST (implementation).
*/

#include "my_global.h"
#include "table_status_by_host.h"
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_account.h"

THR_LOCK table_status_by_host::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("HOST") },
    { C_STRING_WITH_LEN("char(60)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("VARIABLE_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("VARIABLE_VALUE") },
    { C_STRING_WITH_LEN("varchar(1024)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_status_by_host::m_field_def=
{ 3, field_types };

PFS_engine_table_share
table_status_by_host::m_share=
{
  { C_STRING_WITH_LEN("status_by_host") },
  &pfs_truncatable_acl,
  table_status_by_host::create,
  NULL, /* write_row */
  table_status_by_host::delete_all_rows,
  table_status_by_host::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table*
table_status_by_host::create(void)
{
  return new table_status_by_host();
}

int table_status_by_host::delete_all_rows(void)
{
  mysql_mutex_lock(&LOCK_status);
  reset_status_by_thread();
  reset_status_by_account();
  reset_status_by_host();
  mysql_mutex_unlock(&LOCK_status);
  return 0;
}

ha_rows table_status_by_host::get_row_count(void)
{
  mysql_mutex_lock(&LOCK_status);
  size_t status_var_count= all_status_vars.size();
  mysql_mutex_unlock(&LOCK_status);
  return (global_host_container.get_row_count() * status_var_count);
}

table_status_by_host::table_status_by_host()
  : PFS_engine_table(&m_share, &m_pos),
    m_status_cache(true), m_row_exists(false), m_pos(), m_next_pos()
{}

void table_status_by_host::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_status_by_host::rnd_init(bool scan)
{
  if (show_compatibility_56)
    return 0;

  /*
    Build array of SHOW_VARs from the global status array prior to materializing
    threads in rnd_next() or rnd_pos().
  */
  m_status_cache.initialize_client_session();

  /* Use the current number of status variables to detect changes. */
  ulonglong status_version= m_status_cache.get_status_array_version();

  /*
    The table context holds the current version of the global status array
    and a record of which hosts were materialized. If scan == true, then
    allocate a new context from mem_root and store in TLS. If scan == false,
    then restore from TLS.
  */
  m_context= (table_status_by_host_context *)current_thd->alloc(sizeof(table_status_by_host_context));
  new(m_context) table_status_by_host_context(status_version, !scan);
  return 0;
}

int table_status_by_host::rnd_next(void)
{
  if (show_compatibility_56)
    return HA_ERR_END_OF_FILE;

  /* If status array changes, exit with warning. */ // TODO: Issue warning
  if (!m_context->versions_match())
    return HA_ERR_END_OF_FILE;

  /*
    For each user, build a cache of status variables using totals from all
    threads associated with the host.
  */
  bool has_more_host= true;

  for (m_pos.set_at(&m_next_pos);
       has_more_host;
       m_pos.next_host())
  {
    PFS_host *pfs_host= global_host_container.get(m_pos.m_index_1, &has_more_host);

    if (m_status_cache.materialize_host(pfs_host) == 0)
    {
      /* Mark this host as materialized. */
      m_context->set_item(m_pos.m_index_1);

      /* Get the next status variable. */
      const Status_variable *stat_var= m_status_cache.get(m_pos.m_index_2);
      if (stat_var != NULL)
      {
        make_row(pfs_host, stat_var);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int
table_status_by_host::rnd_pos(const void *pos)
{
  if (show_compatibility_56)
    return 0;

  /* If status array changes, exit with warning. */ // TODO: Issue warning
  if (!m_context->versions_match())
    return HA_ERR_END_OF_FILE;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < global_host_container.get_row_count());

  PFS_host *pfs_host= global_host_container.get(m_pos.m_index_1);

  /*
    Only materialize threads that were previously materialized by rnd_next().
    If a host cannot be rematerialized, then do nothing.
  */
  if (m_context->is_item_set(m_pos.m_index_1) &&
      m_status_cache.materialize_host(pfs_host) == 0)
  {
    const Status_variable *stat_var= m_status_cache.get(m_pos.m_index_2);
    if (stat_var != NULL)
    {
      make_row(pfs_host, stat_var);
      return 0;
    }
  }
  return HA_ERR_RECORD_DELETED;
}

void table_status_by_host
::make_row(PFS_host *pfs_host, const Status_variable *status_var)
{
  pfs_optimistic_state lock;
  m_row_exists= false;
  pfs_host->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_host.make_row(pfs_host))
    return;

  m_row.m_variable_name.make_row(status_var->m_name, status_var->m_name_length);
  m_row.m_variable_value.make_row(status_var);

  if (!pfs_host->m_lock.end_optimistic_lock(&lock))
    return;

  m_row_exists= true;
}

int table_status_by_host
::read_row_values(TABLE *table,
                  unsigned char *buf,
                  Field **fields,
                  bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* HOST */
        m_row.m_host.set_field(f);
        break;
      case 1: /* VARIABLE_NAME */
        set_field_varchar_utf8(f, m_row.m_variable_name.m_str, m_row.m_variable_name.m_length);
        break;
      case 2: /* VARIABLE_VALUE */
        m_row.m_variable_value.set_field(f);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

