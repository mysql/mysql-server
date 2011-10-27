/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "my_pthread.h"
#include "table_hosts.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_account.h"
#include "pfs_host.h"
#include "pfs_visitor.h"

THR_LOCK table_hosts::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("HOST") },
    { C_STRING_WITH_LEN("char(60)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CURRENT_CONNECTIONS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TOTAL_CONNECTIONS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_hosts::m_field_def=
{ 3, field_types };

PFS_engine_table_share
table_hosts::m_share=
{
  { C_STRING_WITH_LEN("hosts") },
  &pfs_truncatable_acl,
  &table_hosts::create,
  NULL, /* write_row */
  table_hosts::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_hosts::create()
{
  return new table_hosts();
}

int
table_hosts::delete_all_rows(void)
{
  reset_events_waits_by_thread();
  reset_events_waits_by_account();
  reset_events_waits_by_host();
  reset_events_stages_by_thread();
  reset_events_stages_by_account();
  reset_events_stages_by_host();
  reset_events_statements_by_thread();
  reset_events_statements_by_account();
  reset_events_statements_by_host();
  purge_all_account();
  purge_all_host();
  return 0;
}

table_hosts::table_hosts()
  : cursor_by_host(& m_share),
  m_row_exists(false)
{}

void table_hosts::make_row(PFS_host *pfs)
{
  pfs_lock lock;

  m_row_exists= false;
  pfs->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_host.make_row(pfs))
    return;

  PFS_connection_stat_visitor visitor;
  PFS_connection_iterator::visit_host(pfs, true, true, & visitor);

  if (! pfs->m_lock.end_optimistic_lock(& lock))
    return;

  m_row.m_connection_stat.set(& visitor.m_stat);
  m_row_exists= true;
}

int table_hosts::read_row_values(TABLE *table,
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
      case 1: /* CURRENT_CONNECTIONS */
      case 2: /* TOTAL_CONNECTIONS */
        m_row.m_connection_stat.set_field(f->field_index - 1, f);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

