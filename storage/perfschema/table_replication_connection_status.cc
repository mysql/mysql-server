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
  @file storage/perfschema/table_replication_connection_status.cc
  Table replication_connection_status (implementation).
*/

#include "sql_priv.h"
#include "table_replication_connection_status.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include  "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"

THR_LOCK table_replication_connection_status::m_table_lock;


//TODO : consider replacing with std::max
#define max(x, y) ((x) > (y) ? (x) : (y))

/*
  Numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("Source_UUID")},
    {C_STRING_WITH_LEN("varchar(36)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Thread_Id")},
    {C_STRING_WITH_LEN("char(21)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Service_State")},
    {C_STRING_WITH_LEN("enum('On','Off','Connecting')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Received_Transaction_Set")},
    {C_STRING_WITH_LEN("text")},
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
  }
};

TABLE_FIELD_DEF
table_replication_connection_status::m_field_def=
{ 7, field_types };

PFS_engine_table_share
table_replication_connection_status::m_share=
{
  { C_STRING_WITH_LEN("replication_connection_status") },
  &pfs_readonly_acl,
  &table_replication_connection_status::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  NULL,    
  1,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_replication_connection_status::create(void)
{
  return new table_replication_connection_status();
}

table_replication_connection_status::table_replication_connection_status()
  : PFS_engine_table(&m_share, &m_pos),
    m_filled(false), m_pos(0), m_next_pos(0)
{}

table_replication_connection_status::~table_replication_connection_status()
{}

void table_replication_connection_status::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_replication_connection_status::rnd_next(void)
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

int table_replication_connection_status::rnd_pos(const void *pos)
{
  Master_info *mi= active_mi;
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_share.m_records);

  if (!m_filled)
    fill_rows(mi);
  return 0;
}

void table_replication_connection_status::fill_rows(Master_info *mi)
{
  if (mi != NULL)
  {
    global_sid_lock->wrlock();

    const Gtid_set* io_gtid_set= mi->rli->get_gtid_set();
    if ((m_row.Received_Transaction_Set_length= io_gtid_set->to_string(&m_row.Received_Transaction_Set)) < 0)
    {
      my_free(m_row.Received_Transaction_Set);
      m_row.Received_Transaction_Set_length= 0;
      global_sid_lock->unlock();
      return;
    }
    global_sid_lock->unlock();
  }
  
  //TODO: get rid of mutexes that are not needed.
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  mysql_mutex_lock(&mi->err_lock);
  mysql_mutex_lock(&mi->rli->err_lock);
  
  memcpy(m_row.Source_UUID, mi->master_uuid, UUID_LENGTH+1);

  if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
  {
    char thread_id_str[21];
    sprintf(thread_id_str, "%llu", (ulonglong) mi->info_thd->thread_id);
    m_row.Thread_Id_length= strlen(thread_id_str) + 1;
    memcpy(m_row.Thread_Id, thread_id_str, m_row.Thread_Id_length);
  }
  else
  {
    m_row.Thread_Id_length= strlen("NULL");
    memcpy(m_row.Thread_Id, "NULL", m_row.Thread_Id_length+1);
  }

  if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
    m_row.Service_State= PS_RPL_CONNECT_SERVICE_STATE_YES;
  else
  {
    if (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT)
      m_row.Service_State= PS_RPL_CONNECT_SERVICE_STATE_CONNECTING;
    else
      m_row.Service_State= PS_RPL_CONNECT_SERVICE_STATE_NO;
  }  

  m_row.Last_Error_Number= (unsigned int) mi->last_error().number;
  
  m_row.Last_Error_Message_length= 0;
  m_row.Last_Error_Timestamp_length= 0;
  if (m_row.Last_Error_Number)
  {
    char* temp_store= (char*)mi->last_error().message;
    m_row.Last_Error_Message_length= strlen(temp_store) + 1;
    memcpy(m_row.Last_Error_Message, temp_store, m_row.Last_Error_Message_length);

    temp_store= (char*)mi->last_error().timestamp;
    m_row.Last_Error_Timestamp_length= strlen(temp_store) + 1;
    memcpy(m_row.Last_Error_Timestamp, temp_store, m_row.Last_Error_Timestamp_length);
  }

  mysql_mutex_unlock(&mi->rli->err_lock);
  mysql_mutex_unlock(&mi->err_lock);
  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
  
  m_filled= true;
}


int table_replication_connection_status::read_row_values(TABLE *table,
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
      case 0: /** Source_UUID */
        set_field_char_utf8(f, m_row.Source_UUID, UUID_LENGTH+1);
        break;
      case 1: /** Thread_id */
        set_field_varchar_utf8(f, m_row.Thread_Id, m_row.Thread_Id_length);
        break;
      case 2: /** Service_State */
        set_field_enum(f, m_row.Service_State);
        break;
      case 3: /** Received_Transaction_Set */
        set_field_longtext_utf8(f, m_row.Received_Transaction_Set,
                                m_row.Received_Transaction_Set_length);
      case 4: /*Last_Error_Number*/
        set_field_ulong(f, m_row.Last_Error_Number);
        break;
      case 5: /*Last_Error_Message*/
        set_field_varchar_utf8(f, m_row.Last_Error_Message,
                               m_row.Last_Error_Message_length);
        break;
      case 6: /*Last_Error_Timestamp*/
        set_field_varchar_utf8(f, m_row.Last_Error_Timestamp,
                               m_row.Last_Error_Timestamp_length);
        break;      
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
