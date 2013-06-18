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
    {C_STRING_WITH_LEN("char(36)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Thread_Id")},
    {C_STRING_WITH_LEN("bigint(20)")},
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
    {C_STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Last_Error_Message")},
    {C_STRING_WITH_LEN("varchar(1024)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Last_Error_Timestamp")},
    {C_STRING_WITH_LEN("timestamp")},
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
    m_row_exists(false), m_pos(0), m_next_pos(0)
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
  if (!m_row_exists)
  {
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi->host[0])
    {
      make_row(active_mi);
      mysql_mutex_unlock(&LOCK_active_mi);
      return 0;
    }
    else
    {
      mysql_mutex_unlock(&LOCK_active_mi);
      return HA_ERR_RECORD_DELETED; /** A record is not there */
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_replication_connection_status::rnd_pos(const void *pos)
{
  set_position(pos);

  DBUG_ASSERT(m_pos.m_index < m_share.m_records);

  if (!m_row_exists)
  {
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi->host[0])
    {
      make_row(active_mi);
      mysql_mutex_unlock(&LOCK_active_mi);
      return 0;
    }
    else
    {
      mysql_mutex_unlock(&LOCK_active_mi);
      return HA_ERR_RECORD_DELETED; /** A record is not there */
    }
  }
  return HA_ERR_END_OF_FILE;
}

void table_replication_connection_status::make_row(Master_info *mi)
{
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  memcpy(m_row.Source_UUID, mi->master_uuid, UUID_LENGTH+1);

  m_row.Thread_Id= 0;

  if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
  {
    m_row.Thread_Id= (ulonglong) mi->info_thd->thread_id;
    m_row.Thread_Id_is_null= false;
  }
  else
    m_row.Thread_Id_is_null= true;

  if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
    m_row.Service_State= PS_RPL_CONNECT_SERVICE_STATE_YES;
  else
  {
    if (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT)
      m_row.Service_State= PS_RPL_CONNECT_SERVICE_STATE_CONNECTING;
    else
      m_row.Service_State= PS_RPL_CONNECT_SERVICE_STATE_NO;
  }

  mysql_mutex_lock(&mi->err_lock);
  mysql_mutex_lock(&mi->rli->err_lock);

  if (mi != NULL)
  {
    global_sid_lock->wrlock();

    const Gtid_set* io_gtid_set= mi->rli->get_gtid_set();

    if ((m_row.Received_Transaction_Set_length=
         io_gtid_set->to_string(&m_row.Received_Transaction_Set)) < 0)
    {
      my_free(m_row.Received_Transaction_Set);
      m_row.Received_Transaction_Set_length= 0;
      global_sid_lock->unlock();
      return;
    }
    global_sid_lock->unlock();
  }

  m_row.Last_Error_Number= (unsigned int) mi->last_error().number;
  m_row.Last_Error_Message_length= 0;
  m_row.Last_Error_Timestamp= 0;

  /** If error, set error message and timestamp */
  if (m_row.Last_Error_Number)
  {
    char* temp_store= (char*)mi->last_error().message;
    m_row.Last_Error_Message_length= strlen(temp_store);
    memcpy(m_row.Last_Error_Message, temp_store,
           m_row.Last_Error_Message_length);

    /** time in millisecond since epoch */
    m_row.Last_Error_Timestamp= mi->last_error().skr*1000000;
  }

  mysql_mutex_unlock(&mi->rli->err_lock);
  mysql_mutex_unlock(&mi->err_lock);
  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  m_row_exists= true;
}

int table_replication_connection_status::read_row_values(TABLE *table,
                                                         unsigned char *buf,
                                                         Field **fields,
                                                         bool read_all)
{
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** Source_UUID */
        set_field_char_utf8(f, m_row.Source_UUID, UUID_LENGTH);
        break;
      case 1: /** Thread_id */
        if(m_row.Thread_Id_is_null)
          f->set_null();
        else
          set_field_ulonglong(f, m_row.Thread_Id);
        break;
      case 2: /** Service_State */
        set_field_enum(f, m_row.Service_State);
        break;
      case 3: /** Received_Transaction_Set */
        set_field_longtext_utf8(f, m_row.Received_Transaction_Set,
                                m_row.Received_Transaction_Set_length);
        break;
      case 4: /*Last_Error_Number*/
        set_field_ulong(f, m_row.Last_Error_Number);
        break;
      case 5: /*Last_Error_Message*/
        set_field_varchar_utf8(f, m_row.Last_Error_Message,
                               m_row.Last_Error_Message_length);
        break;
      case 6: /*Last_Error_Timestamp*/
         set_field_timestamp(f, m_row.Last_Error_Timestamp);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
