/*
      Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_applier_configuration.cc
  Table replication_applier_configuration (implementation).
*/

#define HAVE_REPLICATION

#include "my_global.h"
#include "table_replication_applier_configuration.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"
#include "rpl_msr.h"   /* Multisource replication */

THR_LOCK table_replication_applier_configuration::m_table_lock;

/*
  numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("CHANNEL_NAME")},
    {C_STRING_WITH_LEN("char(64)")},
    {NULL,0}
  },

  {
    {C_STRING_WITH_LEN("DESIRED_DELAY")},
    {C_STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_applier_configuration::m_field_def=
{ 2, field_types };

PFS_engine_table_share
table_replication_applier_configuration::m_share=
{
  { C_STRING_WITH_LEN("replication_applier_configuration") },
  &pfs_readonly_acl,
  table_replication_applier_configuration::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_applier_configuration::get_row_count,
  sizeof(pos_t), /* ref length */
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table* table_replication_applier_configuration::create(void)
{
  return new table_replication_applier_configuration();
}

table_replication_applier_configuration
  ::table_replication_applier_configuration()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{}

table_replication_applier_configuration
  ::~table_replication_applier_configuration()
{}

void table_replication_applier_configuration::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}


ha_rows table_replication_applier_configuration::get_row_count()
{
 return channel_map.get_max_channels();
}


int table_replication_applier_configuration::rnd_next(void)
{
  Master_info *mi;
  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels();
       m_pos.next())
  {
    mi= channel_map.get_mi_at_pos(m_pos.m_index);

    if (mi && mi->host[0])
    {
      make_row(mi);
      m_next_pos.set_after(&m_pos);
      channel_map.unlock();
      return 0;
    }
  }

  channel_map.unlock();
  return HA_ERR_END_OF_FILE;
}

int table_replication_applier_configuration::rnd_pos(const void *pos)
{
  Master_info *mi;
  int res= HA_ERR_RECORD_DELETED;

  set_position(pos);

  channel_map.rdlock();

  if ((mi= channel_map.get_mi_at_pos(m_pos.m_index)))
  {
    make_row(mi);
    res= 0;
  }

  channel_map.unlock();
  return res;
}

void table_replication_applier_configuration::make_row(Master_info *mi)
{
  m_row_exists= false;

  DBUG_ASSERT(mi != NULL);
  DBUG_ASSERT(mi->rli != NULL);

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  m_row.channel_name_length= mi->get_channel() ? strlen(mi->get_channel()):0;
  memcpy(m_row.channel_name, mi->get_channel(), m_row.channel_name_length);
  m_row.desired_delay= mi->rli->get_sql_delay();

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  m_row_exists= true;
}

int table_replication_applier_configuration::read_row_values(TABLE *table,
                                                             unsigned char *buf,
                                                             Field **fields,
                                                             bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /*
    Note:
    There are no NULL columns in this table,
    so there are no null bits reserved for NULL flags per column.
    There are no VARCHAR columns either, so the record is not
    in HA_OPTION_PACK_RECORD format as most other performance_schema tables.
    When HA_OPTION_PACK_RECORD is not set,
    the table record reserves an extra null byte, see open_binary_frm().
  */

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /**channel_name*/
        set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
        break;
      case 1: /** desired_delay */
        set_field_ulong(f, static_cast<ulong>(m_row.desired_delay));
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
