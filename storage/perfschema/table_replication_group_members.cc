/*
      Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_group_members.cc
  Table replication_group_members (implementation).
*/

#define HAVE_REPLICATION

#include "my_global.h"
#include "table_replication_group_members.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"
#include "gcs_replication.h"
#include "log.h"

THR_LOCK table_replication_group_members::m_table_lock;

/* Numbers in varchar count utf8 characters. */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("CHANNEL_NAME")},
    {C_STRING_WITH_LEN("varchar(256)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_ID")},
    {C_STRING_WITH_LEN("varchar(64)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_ADDRESS")},
    {C_STRING_WITH_LEN("varchar(64)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_STATE")},
    {C_STRING_WITH_LEN("enum('ONLINE','OFFLINE','RECOVERING')")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_group_members::m_field_def=
{ 4, field_types };

PFS_engine_table_share
table_replication_group_members::m_share=
{
  { C_STRING_WITH_LEN("replication_group_members") },
  &pfs_readonly_acl,
  &table_replication_group_members::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_group_members::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_replication_group_members::create(void)
{
  return new table_replication_group_members();
}

table_replication_group_members::table_replication_group_members()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{}

table_replication_group_members::~table_replication_group_members()
{}

void table_replication_group_members::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

ha_rows table_replication_group_members::get_row_count()
{
  return get_gcs_members_number_info();
}

int table_replication_group_members::rnd_next(void)
{
  if (!is_gcs_plugin_loaded())
    return HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < get_row_count();
       m_pos.next())
  {
    make_row(m_pos.m_index);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_replication_group_members::rnd_pos(const void *pos)
{
  if (!is_gcs_plugin_loaded())
    return HA_ERR_END_OF_FILE;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < get_row_count());
  make_row(m_pos.m_index);

  return 0;
}

void table_replication_group_members::make_row(uint index)
{
  DBUG_ENTER("table_replication_group_members::make_row");
  m_row_exists= false;

  RPL_GCS_GROUP_MEMBERS_INFO* gcs_info;
  if(!(gcs_info= (RPL_GCS_GROUP_MEMBERS_INFO*)my_malloc(PSI_NOT_INSTRUMENTED,
                                                  sizeof(RPL_GCS_GROUP_MEMBERS_INFO),
                                                  MYF(MY_WME))))
  {
    sql_print_error("Unable to allocate memory on"
                    " table_replication_group_members::make_row");
    DBUG_VOID_RETURN;
  }

  gcs_info->member_address= NULL;

  bool stats_not_available= get_gcs_group_members_info(index, gcs_info);
  if (stats_not_available)
    DBUG_PRINT("info", ("GCS stats not available!"));
  else
  {
    m_row.channel_name_length= strlen(gcs_info->channel_name);
    memcpy(m_row.channel_name, gcs_info->channel_name,
           m_row.channel_name_length);

    m_row.member_id_length= strlen(gcs_info->member_id);
    memcpy(m_row.member_id, gcs_info->member_id, m_row.member_id_length);

    m_row.member_address_length= strlen(gcs_info->member_address);
    memcpy(m_row.member_address, gcs_info->member_address,
           m_row.member_address_length);

    m_row.member_state= gcs_info->member_state;

    m_row_exists= true;
  }

  if(gcs_info->member_address != NULL)
    my_free((char*)gcs_info->member_address);

  if(gcs_info->member_id != NULL)
    my_free((char*)gcs_info->member_id);

  my_free(gcs_info);
  DBUG_VOID_RETURN;
}


int table_replication_group_members::read_row_values(TABLE *table,
                                                     unsigned char *buf,
                                                     Field **fields,
                                                     bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(table->s->null_bytes == 0);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** channel_name */
        set_field_varchar_utf8(f, m_row.channel_name, m_row.channel_name_length);
        break;
      case 1: /** member_id */
        set_field_varchar_utf8(f, m_row.member_id, m_row.member_id_length);
        break;
      case 2: /** member_host */
        set_field_varchar_utf8(f,
                               m_row.member_address,
                               m_row.member_address_length);
        break;
      case 3: /** node_state */
        set_field_enum(f, m_row.member_state);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
