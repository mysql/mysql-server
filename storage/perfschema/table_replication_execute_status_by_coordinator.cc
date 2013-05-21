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
  @file storage/perfschema/table_replication_execute_status_by_cordinator.cc
  Table replication_execute_status_by_coordinator (implementation).
*/

#include "sql_priv.h"
#include "table_replication_execute_status_by_coordinator.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include  "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"

THR_LOCK table_replication_execute_status_by_coordinator::m_table_lock;

/*
  numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("Thread_Id")},
    {C_STRING_WITH_LEN("char(21)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Service_State")},
    {C_STRING_WITH_LEN("enum('On','Off')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Last_Error_Number")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Last_Error_Message")},
    {C_STRING_WITH_LEN("varchar(1024)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Last_Error_Timestamp")},
    {C_STRING_WITH_LEN("varchar(16)")},
    {NULL, 0}
  },
};

TABLE_FIELD_DEF
table_replication_execute_status_by_coordinator::m_field_def=
{ 5, field_types };

PFS_engine_table_share
table_replication_execute_status_by_coordinator::m_share=
{
  { C_STRING_WITH_LEN("replication_execute_status_by_coordinator") },
  &pfs_readonly_acl,
  &table_replication_execute_status_by_coordinator::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  NULL,    
  1,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};


PFS_engine_table* table_replication_execute_status_by_coordinator::create(void)
{
  return new table_replication_execute_status_by_coordinator();
}

table_replication_execute_status_by_coordinator::table_replication_execute_status_by_coordinator()
  : PFS_engine_table(&m_share, &m_pos),
    m_filled(false), m_pos(0), m_next_pos(0)
{}

table_replication_execute_status_by_coordinator::~table_replication_execute_status_by_coordinator()
{}

void table_replication_execute_status_by_coordinator::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}
#ifndef MYSQL_CLIENT
int table_replication_execute_status_by_coordinator::rnd_next(void)
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
int table_replication_execute_status_by_coordinator::rnd_pos(const void *pos)
{
  Master_info *mi= active_mi;
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_share.m_records);

  if (!m_filled)
    fill_rows(mi);
  return 0;
}

void table_replication_execute_status_by_coordinator::fill_rows(Master_info *mi)
{
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  mysql_mutex_lock(&mi->err_lock);
  mysql_mutex_lock(&mi->rli->err_lock);

  if (mi->rli->slave_running)
  {  
    char thread_id_str[21];
    sprintf(thread_id_str, "%u", (uint) mi->rli->info_thd->thread_id);
    m_row.Thread_Id_length= strlen(thread_id_str);
    memcpy(m_row.Thread_Id, thread_id_str, m_row.Thread_Id_length);
  }
  else
  {
    m_row.Thread_Id_length= strlen("NULL");
    memcpy(m_row.Thread_Id, "NULL", m_row.Thread_Id_length+1);
  }
 
  if (mi->rli->slave_running)
    m_row.Service_State= PS_RPL_YES;
  else
    m_row.Service_State= PS_RPL_NO;

  m_row.Last_Error_Number= (long int) mi->rli->last_error().number;

  if (m_row.Last_Error_Number)
  {
    char *temp_store= (char*) mi->rli->last_error().message;
    m_row.Last_Error_Message_length= strlen(temp_store) + 1;
    memcpy(m_row.Last_Error_Message, temp_store, m_row.Last_Error_Message_length);
    temp_store= (char*) mi->rli->last_error().timestamp;
    m_row.Last_Error_Timestamp_length= strlen(temp_store) + 1;
    memcpy(m_row.Last_Error_Timestamp, temp_store, m_row.Last_Error_Timestamp_length);
  }

  mysql_mutex_unlock(&mi->rli->err_lock);
  mysql_mutex_unlock(&mi->err_lock);
  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
  
  m_filled= true;
}

int table_replication_execute_status_by_coordinator::read_row_values(TABLE *table,
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
      case 0: /*Thread_Id*/
        set_field_varchar_utf8(f, m_row.Thread_Id, m_row.Thread_Id_length);
        break;
      case 1: /*Service_State*/
        set_field_enum(f, m_row.Service_State);
        break;
      case 2: /*Last_Error_Number*/
        set_field_ulong(f, m_row.Last_Error_Number);
        break;
      case 3: /*Last_Error_Message*/
        set_field_varchar_utf8(f, m_row.Last_Error_Message, m_row.Last_Error_Message_length);
        break;
      case 4: /*Last_Error_Timestamp*/
        set_field_varchar_utf8(f, m_row.Last_Error_Timestamp, m_row.Last_Error_Timestamp_length);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
