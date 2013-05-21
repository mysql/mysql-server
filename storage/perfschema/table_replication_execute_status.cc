/*
      Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.
   
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
    {C_STRING_WITH_LEN("Service_State")},
    {C_STRING_WITH_LEN("enum('On','Off')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Remaining_Delay")},
    {C_STRING_WITH_LEN("varchar(11)")},
    {NULL, 0}
  },
};

TABLE_FIELD_DEF
table_replication_execute_status::m_field_def=
{ 2, field_types };

PFS_engine_table_share
table_replication_execute_status::m_share=
{
  { C_STRING_WITH_LEN("replication_execute_status") },
  &pfs_readonly_acl,
  &table_replication_execute_status::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  NULL,    
  1,
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
    m_filled(false), m_pos(0), m_next_pos(0)
{}

table_replication_execute_status::~table_replication_execute_status()
{}

void table_replication_execute_status::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}
#ifndef MYSQL_CLIENT
int table_replication_execute_status::rnd_next(void)
{
  Master_info *mi= active_mi;

  if (!m_filled)
  {
    if (mi->host[0])
      fill_rows(active_mi);
    else
      return HA_ERR_END_OF_FILE;
  }

  m_pos.set_at(&m_next_pos);
  m_next_pos.set_after(&m_pos);
  if (m_pos.m_index == m_share.m_records)
    return HA_ERR_END_OF_FILE;

  return 0;
}
#endif
int table_replication_execute_status::rnd_pos(const void *pos)
{
  Master_info *mi= active_mi;
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_share.m_records);

  if (!m_filled)
    fill_rows(mi);
  return 0;
}

void table_replication_execute_status::fill_rows(Master_info *mi)
{
  char *slave_sql_running_state= NULL;

  mysql_mutex_lock(&mi->rli->info_thd_lock);
  slave_sql_running_state= const_cast<char *>(mi->rli->info_thd ? mi->rli->info_thd->get_proc_info() : "");
  mysql_mutex_unlock(&mi->rli->info_thd_lock);


  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  
  if (mi->rli->slave_running)
    m_row.Service_State= PS_RPL_YES;
  else
    m_row.Service_State= PS_RPL_NO;
  
  // Remaining_Delay
  ulong remaining_delay_int;
  char remaining_delay_str[11];
    if (slave_sql_running_state == stage_sql_thd_waiting_until_delay.m_name)
    {
      time_t t= my_time(0), sql_delay_end= mi->rli->get_sql_delay_end();
      remaining_delay_int= (long int)(t < sql_delay_end ? sql_delay_end - t : 0);
      sprintf(remaining_delay_str, "%lu", remaining_delay_int);
      m_row.Remaining_Delay_length= strlen(remaining_delay_str) + 1;
      memcpy(m_row.Remaining_Delay, remaining_delay_str, m_row.Remaining_Delay_length);
    }
    else
    {
      m_row.Remaining_Delay_length= strlen("NULL") + 1;
      memcpy(m_row.Remaining_Delay, "NULL", m_row.Remaining_Delay_length);
    }

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
  
  m_filled= true;
}


int table_replication_execute_status::read_row_values(TABLE *table,
                                       unsigned char *,
                                       Field **fields,
                                       bool read_all)
{
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* Service_State */
        set_field_enum(f, m_row.Service_State);
        break;
      case 1: /* Remaining_Delay */
        set_field_varchar_utf8(f, m_row.Remaining_Delay, m_row.Remaining_Delay_length);
        break;    
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
