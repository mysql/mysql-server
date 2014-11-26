/*
      Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_execute_status.cc
  Table replication_execute_status (implementation).
*/

#define HAVE_REPLICATION

#include "my_global.h"
#include "sql_priv.h"
#include "table_replication_execute_status.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include  "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"

THR_LOCK table_replication_execute_status::m_table_lock;

/*
  numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("SERVICE_STATE")},
    {C_STRING_WITH_LEN("enum('ON','OFF')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("REMAINING_DELAY")},
    {C_STRING_WITH_LEN("int")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("COUNT_TRANSACTIONS_RETRIES")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
};

TABLE_FIELD_DEF
table_replication_execute_status::m_field_def=
{ 3, field_types };

PFS_engine_table_share
table_replication_execute_status::m_share=
{
  { C_STRING_WITH_LEN("replication_execute_status") },
  &pfs_readonly_acl,
  table_replication_execute_status::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_execute_status::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};


PFS_engine_table* table_replication_execute_status::create(void)
{
  return new table_replication_execute_status();
}

table_replication_execute_status::table_replication_execute_status()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{}

table_replication_execute_status::~table_replication_execute_status()
{}

void table_replication_execute_status::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

ha_rows table_replication_execute_status::get_row_count()
{
  uint row_count= 0;

  mysql_mutex_lock(&LOCK_active_mi);

  if (active_mi && active_mi->host[0])
    row_count= 1;

  mysql_mutex_unlock(&LOCK_active_mi);

  return row_count;
}

int table_replication_execute_status::rnd_next(void)
{
  if(get_row_count() == 0)
    return HA_ERR_END_OF_FILE;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index == 0)
  {
    make_row();
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}


int table_replication_execute_status::rnd_pos(const void *pos)
{
 if(get_row_count() == 0)
  return HA_ERR_END_OF_FILE;

  set_position(pos);

  DBUG_ASSERT(m_pos.m_index < 1);

  make_row();

  return 0;
}

void table_replication_execute_status::make_row()
{
  char *slave_sql_running_state= NULL;

  m_row_exists= false;

  mysql_mutex_lock(&LOCK_active_mi);

  DBUG_ASSERT(active_mi != NULL);
  DBUG_ASSERT(active_mi->rli != NULL);

  mysql_mutex_lock(&active_mi->rli->info_thd_lock);
  slave_sql_running_state= const_cast<char *>
                           (active_mi->rli->info_thd ?
                            active_mi->rli->info_thd->get_proc_info() : "");
  mysql_mutex_unlock(&active_mi->rli->info_thd_lock);


  mysql_mutex_lock(&active_mi->data_lock);
  mysql_mutex_lock(&active_mi->rli->data_lock);

  if (active_mi->rli->slave_running)
    m_row.service_state= PS_RPL_YES;
  else
    m_row.service_state= PS_RPL_NO;

  m_row.remaining_delay= 0;
  if (slave_sql_running_state == stage_sql_thd_waiting_until_delay.m_name)
  {
    time_t t= my_time(0), sql_delay_end= active_mi->rli->get_sql_delay_end();
    m_row.remaining_delay= (uint)(t < sql_delay_end ?
                                      sql_delay_end - t : 0);
    m_row.remaining_delay_is_set= true;
  }
  else
    m_row.remaining_delay_is_set= false;

  m_row.count_transactions_retries= active_mi->rli->retried_trans;

  mysql_mutex_unlock(&active_mi->rli->data_lock);
  mysql_mutex_unlock(&active_mi->data_lock);
  mysql_mutex_unlock(&LOCK_active_mi);

  m_row_exists= true;
}

int table_replication_execute_status::read_row_values(TABLE *table,
                                       unsigned char *buf,
                                       Field **fields,
                                       bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* service_state */
        set_field_enum(f, m_row.service_state);
        break;
      case 1: /* remaining_delay */
        if (m_row.remaining_delay_is_set)
          set_field_ulong(f, m_row.remaining_delay);
        else
          f->set_null();
        break;
      case 2: /* total number of times transactions were retried */
        set_field_ulonglong(f, m_row.count_transactions_retries);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
