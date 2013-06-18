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
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Service_State")},
    {C_STRING_WITH_LEN("enum('On','Off')")},
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
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
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

table_replication_execute_status_by_coordinator
  ::table_replication_execute_status_by_coordinator()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{}

table_replication_execute_status_by_coordinator
  ::~table_replication_execute_status_by_coordinator()
{}

void table_replication_execute_status_by_coordinator::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_replication_execute_status_by_coordinator::rnd_next(void)
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

int table_replication_execute_status_by_coordinator::rnd_pos(const void *pos)
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

void table_replication_execute_status_by_coordinator
  ::make_row(Master_info *mi)
{
  mysql_mutex_lock(&mi->rli->data_lock);

  if (mi->rli->slave_running)
  {
    m_row.Thread_Id_is_null= false;
    m_row.Thread_Id= (ulonglong)mi->rli->info_thd->thread_id;
  }
  else
    m_row.Thread_Id_is_null= true;

  if (mi->rli->slave_running)
    m_row.Service_State= PS_RPL_YES;
  else
    m_row.Service_State= PS_RPL_NO;

  mysql_mutex_lock(&mi->rli->err_lock);

  m_row.Last_Error_Number= (long int) mi->rli->last_error().number;
  m_row.Last_Error_Message_length= 0;
  m_row.Last_Error_Timestamp= 0;

  /** If error, set error message and timestamp */
  if (m_row.Last_Error_Number)
  {
    char *temp_store= (char*) mi->rli->last_error().message;
    m_row.Last_Error_Message_length= strlen(temp_store);
    memcpy(m_row.Last_Error_Message, temp_store,
           m_row.Last_Error_Message_length);

    /** time in millisecond since epoch */
    m_row.Last_Error_Timestamp= mi->rli->last_error().skr*1000000;
  }

  mysql_mutex_unlock(&mi->rli->err_lock);
  mysql_mutex_unlock(&mi->rli->data_lock);

  m_row_exists= true;
}

int table_replication_execute_status_by_coordinator
  ::read_row_values(TABLE *table, unsigned char *buf,
                    Field **fields, bool read_all)
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
      case 0: /*Thread_Id*/
        if (!m_row.Thread_Id_is_null)
          set_field_ulonglong(f, m_row.Thread_Id);
        else
          f->set_null();
        break;
      case 1: /*Service_State*/
        set_field_enum(f, m_row.Service_State);
        break;
      case 2: /*Last_Error_Number*/
        set_field_ulong(f, m_row.Last_Error_Number);
        break;
      case 3: /*Last_Error_Message*/
        set_field_varchar_utf8(f, m_row.Last_Error_Message,
                               m_row.Last_Error_Message_length);
        break;
      case 4: /*Last_Error_Timestamp*/
        set_field_timestamp(f, m_row.Last_Error_Timestamp);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
