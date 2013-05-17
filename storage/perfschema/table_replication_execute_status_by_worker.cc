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
  @file storage/perfschema/table_replication_execute_status_by_worker.cc
  Table replication_execute_status_by_worker (implementation).
*/

#include "sql_priv.h"
#include "table_replication_execute_status_by_worker.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include  "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"
#include "rpl_rli_pdb.h"

THR_LOCK table_replication_execute_status_by_worker::m_table_lock;

/*
  numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("Worker_Id")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
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
    {C_STRING_WITH_LEN("Last_Executed_Transaction")},
    {C_STRING_WITH_LEN("char(57)")},
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
table_replication_execute_status_by_worker::m_field_def=
{ 7, field_types };

PFS_engine_table_share
table_replication_execute_status_by_worker::m_share=
{
  { C_STRING_WITH_LEN("replication_execute_status_by_worker") },
  &pfs_readonly_acl,
  &table_replication_execute_status_by_worker::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_execute_status_by_worker::get_row_count, /*TODO: get_row_count()*/   
  1000, /*records- used by optimizer*/
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_replication_execute_status_by_worker::create(void)
{
  return new table_replication_execute_status_by_worker();
}

//TODO: some values hard-coded below, replace them --shiv
static ST_STATUS_FIELD_INFO slave_field_info[]=
{
  {"Worker_Id", sizeof(ulonglong), MYSQL_TYPE_LONG, FALSE},
  {"Thread_Id", 21, MYSQL_TYPE_STRING, FALSE},
  {"Service_State", sizeof(ulonglong), MYSQL_TYPE_ENUM, FALSE},
  {"Last_Executed_Transaction", 57, MYSQL_TYPE_STRING, FALSE},
  {"Last_Error_Number", sizeof(ulonglong), MYSQL_TYPE_LONG, FALSE},
  {"Last_Error_Message", MAX_SLAVE_ERRMSG, MYSQL_TYPE_STRING, FALSE},
  {"Last_Error_Timestamp", 11, MYSQL_TYPE_STRING, FALSE}
};

table_replication_execute_status_by_worker::table_replication_execute_status_by_worker()
  : PFS_engine_table(&m_share, &m_pos),
    m_filled(false), m_pos(0), m_next_pos(0)
{
  for (int i= RPL_WORKER_ID; i <= _RPL_EXECUTE_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING)
      m_row.m_fields[i].u.s.str= NULL;  // str_store() makes allocation
    if (slave_field_info[i].can_be_null)
      m_row.m_fields[i].is_null= false;
  }
}

table_replication_execute_status_by_worker::~table_replication_execute_status_by_worker()
{
  for (int i= RPL_WORKER_ID; i <= _RPL_EXECUTE_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING &&
        m_row.m_fields[i].u.s.str != NULL)
      my_free(m_row.m_fields[i].u.s.str);
  }
}

void table_replication_execute_status_by_worker::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}
#ifndef MYSQL_CLIENT
int table_replication_execute_status_by_worker::rnd_next(void)
{
  Master_info *mi= active_mi;
  Slave_worker *w;

  if (!m_filled)
  {
    if (mi->host[0])
    {
      for (m_pos.set_at(&m_next_pos); m_pos.m_index < mi->rli->workers.elements; m_pos.next())
      {
        get_dynamic(&mi->rli->workers, (uchar *) &w, m_pos.m_index);
        fill_rows(w);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
    }
    else
      return HA_ERR_END_OF_FILE;
  }

  m_pos.set_at(&m_next_pos);
  m_next_pos.set_after(&m_pos);
  if (m_pos.m_index == m_share.get_row_count())
    return HA_ERR_END_OF_FILE;
  
  return 0;
}
#endif

ha_rows table_replication_execute_status_by_worker::get_row_count()
{
  return active_mi->rli->workers.elements; 
}
int table_replication_execute_status_by_worker::rnd_pos(const void *pos)
{
  Master_info *mi= active_mi;
  Slave_worker *w;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_share.get_row_count());

  get_dynamic(&mi->rli->workers, (uchar *) &w, m_pos.m_index);
  if (!m_filled)
  {
    fill_rows(w);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_replication_execute_status_by_worker::drop_null(enum enum_rpl_execute_field_names name)
{
  if (slave_field_info[name].can_be_null)
    m_row.m_fields[name].is_null= false;
}

void table_replication_execute_status_by_worker::set_null(enum enum_rpl_execute_field_names name)
{
  DBUG_ASSERT(slave_field_info[name].can_be_null);
  m_row.m_fields[name].is_null= true;
}

void table_replication_execute_status_by_worker::str_store(enum enum_rpl_execute_field_names name, const char* val)
{
  m_row.m_fields[name].u.s.length= strlen(val);
  DBUG_ASSERT(m_row.m_fields[name].u.s.length <= slave_field_info[name].max_size);
  if (m_row.m_fields[name].u.s.str == NULL)
    m_row.m_fields[name].u.s.str= (char *) my_malloc(m_row.m_fields[name].u.s.length, MYF(0));

  /*
    \0 may be stripped off since there is no need for \0-termination of
    m_row.m_fields[name].u.s.str
  */
  memcpy(m_row.m_fields[name].u.s.str, val, m_row.m_fields[name].u.s.length);
  m_row.m_fields[name].u.s.length= m_row.m_fields[name].u.s.length;

  drop_null(name);
}

void table_replication_execute_status_by_worker::int_store(enum enum_rpl_execute_field_names name, longlong val)
{
  m_row.m_fields[name].u.n= val;
  drop_null(name);
}

void table_replication_execute_status_by_worker::fill_rows(Slave_worker *w)
{
  //mysql_mutex_lock(&mi->data_lock);
  //mysql_mutex_lock(&mi->rli->data_lock);
  //mysql_mutex_lock(&mi->err_lock);
  //mysql_mutex_lock(&mi->rli->err_lock);
  
  int_store(RPL_WORKER_ID, w->id);
 
  mysql_mutex_lock(&w->jobs_lock);
  if (w->running_status == Slave_worker::RUNNING)
  {
    char thread_id_null_str[21];
    sprintf(thread_id_null_str, "%llu", (ulonglong) w->info_thd->thread_id);
    str_store(RPL_EXECUTE_THREAD_ID, thread_id_null_str);
  }
  else
    str_store(RPL_EXECUTE_THREAD_ID, "NULL");

  enum_store(RPL_EXECUTE_SERVICE_STATE, w->running_status == Slave_worker::RUNNING ? PS_RPL_YES: PS_RPL_NO);
  mysql_mutex_unlock(&w->jobs_lock);

  str_store(RPL_LAST_EXECUTED_TRANSACTION, "need to modify --shiv");
  int_store(RPL_EXECUTE_LAST_ERROR_NUMBER, (long int) w->last_error().number);
  str_store(RPL_EXECUTE_LAST_ERROR_MESSAGE, w->last_error().message);
  str_store(RPL_EXECUTE_LAST_ERROR_TIMESTAMP, w->last_error().timestamp);

  //mysql_mutex_unlock(&mi->rli->err_lock);
  //mysql_mutex_unlock(&mi->err_lock);
  //mysql_mutex_unlock(&mi->rli->data_lock);
  //mysql_mutex_unlock(&mi->data_lock);
  
  //m_filled= true;
}


int table_replication_execute_status_by_worker::read_row_values(TABLE *table,
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
        if (m_row.m_fields[f->field_index].is_null)
        {
          f->set_null();
          continue;
        }
        else
          f->set_notnull();
      }

      switch(f->field_index)
      {
      case RPL_EXECUTE_SERVICE_STATE:

        set_field_enum(f, m_row.m_fields[f->field_index].u.n);
        break;

      case RPL_EXECUTE_THREAD_ID:
      case RPL_EXECUTE_LAST_ERROR_MESSAGE:
      case RPL_EXECUTE_LAST_ERROR_TIMESTAMP:
      case RPL_LAST_EXECUTED_TRANSACTION:

        set_field_varchar_utf8(f,
                               m_row.m_fields[f->field_index].u.s.str,
                               m_row.m_fields[f->field_index].u.s.length);
        break;

      case RPL_WORKER_ID:
      case RPL_EXECUTE_LAST_ERROR_NUMBER:

        set_field_ulonglong(f, m_row.m_fields[f->field_index].u.n);
        break;

      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
