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
  @file storage/perfschema/table_replication_execute_configuration.cc
  Table replication_execute_configuration (implementation).
*/

#include "sql_priv.h"
#include "table_replication_execute_configuration.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include  "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"

THR_LOCK table_replication_execute_configuration::m_table_lock;

/*
  numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("Desired_Delay")},
    {C_STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_execute_configuration::m_field_def=
{ 1, field_types };

PFS_engine_table_share
table_replication_execute_configuration::m_share=
{
  { C_STRING_WITH_LEN("replication_execute_configuration") },
  &pfs_readonly_acl,
  &table_replication_execute_configuration::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  NULL,
  1,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_replication_execute_configuration::create(void)
{
  return new table_replication_execute_configuration();
}

table_replication_execute_configuration
  ::table_replication_execute_configuration()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{}

table_replication_execute_configuration
  ::~table_replication_execute_configuration()
{}

void table_replication_execute_configuration::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_replication_execute_configuration::rnd_next(void)
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

int table_replication_execute_configuration::rnd_pos(const void *pos)
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

void table_replication_execute_configuration::make_row(Master_info *mi)
{
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  m_row.Desired_Delay= mi->rli->get_sql_delay();

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  m_row_exists= true;
}

int table_replication_execute_configuration::read_row_values(TABLE *table,
                                                             unsigned char *,
                                                             Field **fields,
                                                             bool read_all)
{
  Field *f;

  //TODO: See why this comes as 1
  DBUG_ASSERT(table->s->null_bytes == 1);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** Desired_Delay */
        set_field_ulong(f, m_row.Desired_Delay);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
