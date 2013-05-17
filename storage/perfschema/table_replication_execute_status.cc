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

//TODO: some values hard-coded below, replace them --shiv
static ST_STATUS_FIELD_INFO slave_field_info[]=
{
  {"Service_State", sizeof(ulonglong), MYSQL_TYPE_ENUM, FALSE},
  {"Remaining_Delay", 11, MYSQL_TYPE_STRING, 1},
};

table_replication_execute_status::table_replication_execute_status()
  : PFS_engine_table(&m_share, &m_pos),
    m_filled(false), m_pos(0), m_next_pos(0)
{
  for (int i= RPL_EXECUTION_CHANNEL_SERVICE_STATE; i <= _RPL_EXECUTION_CHANNEL_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING)
      m_fields[i].u.s.str= NULL;  // str_store() makes allocation
    if (slave_field_info[i].can_be_null)
      m_fields[i].is_null= false;
  }
}

table_replication_execute_status::~table_replication_execute_status()
{
  for (int i= RPL_EXECUTION_CHANNEL_SERVICE_STATE; i <= _RPL_EXECUTION_CHANNEL_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING &&
        m_fields[i].u.s.str != NULL)
      my_free(m_fields[i].u.s.str);
  }
}

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


void table_replication_execute_status::drop_null(enum enum_rpl_execution_channel_status_field_names name)
{
  if (slave_field_info[name].can_be_null)
    m_fields[name].is_null= false;
}

void table_replication_execute_status::set_null(enum enum_rpl_execution_channel_status_field_names name)
{
  DBUG_ASSERT(slave_field_info[name].can_be_null);
  m_fields[name].is_null= true;
}

void table_replication_execute_status::str_store(enum enum_rpl_execution_channel_status_field_names name, const char* val)
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

void table_replication_execute_status::int_store(enum enum_rpl_execution_channel_status_field_names name, longlong val)
{
  m_fields[name].u.n= val;
  drop_null(name);
}

void table_replication_execute_status::fill_rows(Master_info *mi)
{
  char *slave_sql_running_state= NULL;

  mysql_mutex_lock(&mi->rli->info_thd_lock);
  slave_sql_running_state= const_cast<char *>(mi->rli->info_thd ? mi->rli->info_thd->get_proc_info() : "");
  mysql_mutex_unlock(&mi->rli->info_thd_lock);


  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  
  enum_store(RPL_EXECUTION_CHANNEL_SERVICE_STATE, mi->rli->slave_running ? PS_RPL_YES : PS_RPL_NO);
  
  // Remaining_Delay
  long int remaining_delay_int;
  char remaining_delay_str[11];
    if (slave_sql_running_state == stage_sql_thd_waiting_until_delay.m_name)
    {
      time_t t= my_time(0), sql_delay_end= mi->rli->get_sql_delay_end();
      remaining_delay_int= (long int)(t < sql_delay_end ? sql_delay_end - t : 0);
      sprintf(remaining_delay_str, "%ld", remaining_delay_int);
      str_store(REMAINING_DELAY, remaining_delay_str);
    }
    else
      set_null(REMAINING_DELAY);

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
      if (slave_field_info[f->field_index].can_be_null)
      {
        if (m_fields[f->field_index].is_null)
        {
          if (f->field_index == REMAINING_DELAY)
            set_field_varchar_utf8(f, "NULL", 4);
          f->set_null();
          continue;
        }
        else
          f->set_notnull();
      }

      switch(f->field_index)
      {
      case REMAINING_DELAY:

        set_field_varchar_utf8(f,
                               m_fields[f->field_index].u.s.str,
                               m_fields[f->field_index].u.s.length);
        break;

      case RPL_EXECUTION_CHANNEL_SERVICE_STATE:

        set_field_enum(f, m_fields[f->field_index].u.n);
        break;
        
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
