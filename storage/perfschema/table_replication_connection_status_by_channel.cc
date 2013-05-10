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
  @file storage/perfschema/table_replication_connection_status_by_channel.cc
  Table replication_connection_status_by_channel (implementation).
*/

#include "sql_priv.h"
#include "table_replication_connection_status_by_channel.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include  "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"

THR_LOCK table_replication_connection_status_by_channel::m_table_lock;


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
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Service_State")},
    {C_STRING_WITH_LEN("enum('Yes','No','Connecting')")},
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
table_replication_connection_status_by_channel::m_field_def=
{ 7, field_types };

PFS_engine_table_share
table_replication_connection_status_by_channel::m_share=
{
  { C_STRING_WITH_LEN("replication_connection_status_by_channel") },
  &pfs_readonly_acl,
  &table_replication_connection_status_by_channel::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  NULL,    
  1,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_replication_connection_status_by_channel::create(void)
{
  return new table_replication_connection_status_by_channel();
}

//TODO: some values hard-coded below, replace them --shiv
static ST_STATUS_FIELD_INFO slave_field_info[]=
{
  {"Source_UUID", HOSTNAME_LENGTH, MYSQL_TYPE_STRING, FALSE},
  {"Thread_Id", sizeof(ulonglong), MYSQL_TYPE_LONG, FALSE},
  {"Service_State", sizeof(ulonglong), MYSQL_TYPE_ENUM, FALSE},
  {"Received_Transsaction_Set", 300, MYSQL_TYPE_STRING, FALSE},
  {"Last_Error_Number", sizeof(ulonglong), MYSQL_TYPE_LONG, FALSE},
  {"Last_Error_Message", MAX_SLAVE_ERRMSG, MYSQL_TYPE_STRING, FALSE},
  {"Last_Error_Timestamp", 16, MYSQL_TYPE_STRING, FALSE},
};

table_replication_connection_status_by_channel::table_replication_connection_status_by_channel()
  : PFS_engine_table(&m_share, &m_pos),
    m_filled(false), m_pos(0), m_next_pos(0)
{
  for (int i= SOURCE_UUID; i <= _RPL_CONNECT_STATUS_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING)
      m_fields[i].u.s.str= NULL;  // str_store() makes allocation
    if (slave_field_info[i].can_be_null)
      m_fields[i].is_null= false;
  }
}

table_replication_connection_status_by_channel::~table_replication_connection_status_by_channel()
{
  for (int i= SOURCE_UUID; i <= _RPL_CONNECT_STATUS_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING &&
        m_fields[i].u.s.str != NULL)
      my_free(m_fields[i].u.s.str);
  }
}

void table_replication_connection_status_by_channel::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_replication_connection_status_by_channel::rnd_next(void)
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

int table_replication_connection_status_by_channel::rnd_pos(const void *pos)
{
  Master_info *mi= active_mi;
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_share.m_records);

  if (!m_filled)
    fill_rows(mi);
  return 0;
}

void table_replication_connection_status_by_channel::drop_null(enum enum_rpl_connect_status_field_names name)
{
  if (slave_field_info[name].can_be_null)
    m_fields[name].is_null= false;
}

void table_replication_connection_status_by_channel::set_null(enum enum_rpl_connect_status_field_names name)
{
  DBUG_ASSERT(slave_field_info[name].can_be_null);
  m_fields[name].is_null= true;
}

void table_replication_connection_status_by_channel::str_store(enum enum_rpl_connect_status_field_names name, const char* val)
{
  m_fields[name].u.s.length= strlen(val);
  DBUG_ASSERT(m_fields[name].u.s.length <= slave_field_info[name].max_size);
  if (m_fields[name].u.s.str == NULL)
    m_fields[name].u.s.str= (char *) my_malloc(m_fields[name].u.s.length, MYF(0));

  /*
    \0 may be stripped off since there is no need for \0-termination of
    m_fields[name].u.s.str
  */
  memcpy(m_fields[name].u.s.str, val, m_fields[name].u.s.length);
  m_fields[name].u.s.length= m_fields[name].u.s.length;

  drop_null(name);
}

void table_replication_connection_status_by_channel::int_store(enum enum_rpl_connect_status_field_names name, longlong val)
{
  m_fields[name].u.n= val;
  drop_null(name);
}

void table_replication_connection_status_by_channel::fill_rows(Master_info *mi)
{
  char *io_gtid_set_buffer= NULL;
  int io_gtid_set_size= 0;
  
  if (mi != NULL)
  {
    global_sid_lock->wrlock();

    const Gtid_set* io_gtid_set= mi->rli->get_gtid_set();
    if ((io_gtid_set_size= io_gtid_set->to_string(&io_gtid_set_buffer)) < 0)
    {
      my_free(io_gtid_set_buffer);
      global_sid_lock->unlock();
      return;
    }
    global_sid_lock->unlock();
  }

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  mysql_mutex_lock(&mi->err_lock);
  mysql_mutex_lock(&mi->rli->err_lock);
  
  str_store(SOURCE_UUID, mi->master_uuid);

  //TODO: thread-id code pending, hardcoded below
  int_store(IO_THREAD_ID, 5);

  enum_store(RPL_CONNECT_SERVICE_STATE, mi->slave_running == MYSQL_SLAVE_RUN_CONNECT ?
             PS_RPL_CONNECT_SERVICE_STATE_YES:
             (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT ?
              PS_RPL_CONNECT_SERVICE_STATE_CONNECTING :PS_RPL_CONNECT_SERVICE_STATE_NO));

  str_store(RECEIVED_TRANSACTION_SET, io_gtid_set_buffer);
  int_store(RPL_CONNECT_LAST_ERROR_NUMBER, (long int) mi->last_error().number);
  str_store(RPL_CONNECT_LAST_ERROR_MESSAGE, mi->last_error().message);
  str_store(RPL_CONNECT_LAST_ERROR_TIMESTAMP, mi->last_error().timestamp);

  mysql_mutex_unlock(&mi->rli->err_lock);
  mysql_mutex_unlock(&mi->err_lock);
  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
  
  m_filled= true;
}


int table_replication_connection_status_by_channel::read_row_values(TABLE *table,
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
      if (slave_field_info[f->field_index].can_be_null)
      {
        if (m_fields[f->field_index].is_null)
        {
          f->set_null();
          continue;
        }
        else
          f->set_notnull();
      }

      switch(f->field_index)
      {
      case RPL_CONNECT_LAST_ERROR_MESSAGE:
      case RPL_CONNECT_LAST_ERROR_TIMESTAMP:
      case SOURCE_UUID:

        set_field_varchar_utf8(f,
                               m_fields[f->field_index].u.s.str,
                               m_fields[f->field_index].u.s.length);
        break;

      case IO_THREAD_ID:
      case RPL_CONNECT_LAST_ERROR_NUMBER:

        set_field_ulonglong(f, m_fields[f->field_index].u.n);
        break;

      case RPL_CONNECT_SERVICE_STATE:

        set_field_enum(f, m_fields[f->field_index].u.n);
        break;

      case RECEIVED_TRANSACTION_SET:

        set_field_longtext_utf8(f,
                               m_fields[f->field_index].u.s.str,
                               m_fields[f->field_index].u.s.length);
        break;
        
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

