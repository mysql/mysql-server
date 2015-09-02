/*
      Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "log.h"
#include "rpl_group_replication.h"

/*
  Callbacks implementation for GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS.
*/
static void set_channel_name(void* const context, const char& value,
                             size_t length)
{
  struct st_row_group_members* row=
      static_cast<struct st_row_group_members*>(context);
  const size_t max= CHANNEL_NAME_LENGTH;
  length= std::min(length, max);

  row->channel_name_length= length;
  memcpy(row->channel_name, &value, length);
}

static void set_member_id(void* const context, const char& value,
                          size_t length)
{
  struct st_row_group_members* row=
      static_cast<struct st_row_group_members*>(context);
  const size_t max= UUID_LENGTH;
  length= std::min(length, max);

  row->member_id_length= length;
  memcpy(row->member_id, &value, length);
}

static void set_member_host(void* const context, const char& value,
                            size_t length)
{
  struct st_row_group_members* row=
      static_cast<struct st_row_group_members*>(context);
  const size_t max= HOSTNAME_LENGTH;
  length= std::min(length, max);

  row->member_host_length= length;
  memcpy(row->member_host, &value, length);
}

static void set_member_port(void* const context, unsigned int value)
{
  struct st_row_group_members* row=
      static_cast<struct st_row_group_members*>(context);
  row->member_port= value;
}

static void set_member_state(void* const context, const char& value,
                             size_t length)
{
  struct st_row_group_members* row=
      static_cast<struct st_row_group_members*>(context);
  const size_t max= NAME_LEN;
  length= std::min(length, max);

  row->member_state_length= length;
  memcpy(row->member_state, &value, length);
}


THR_LOCK table_replication_group_members::m_table_lock;

/* Numbers in varchar count utf8 characters. */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("CHANNEL_NAME")},
    {C_STRING_WITH_LEN("char(64)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_ID")},
    {C_STRING_WITH_LEN("char(36)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_HOST")},
    {C_STRING_WITH_LEN("char(60)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_PORT")},
    {C_STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_STATE")},
    {C_STRING_WITH_LEN("char(64)")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_group_members::m_field_def=
{ 5, field_types };

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
  false, /* checked */
  false  /* perpetual */
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
  return get_group_replication_members_number_info();
}

int table_replication_group_members::rnd_next(void)
{
  if (!is_group_replication_plugin_loaded())
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
  if (!is_group_replication_plugin_loaded())
    return HA_ERR_END_OF_FILE;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < get_row_count());
  make_row(m_pos.m_index);

  return 0;
}

void table_replication_group_members::make_row(uint index)
{
  DBUG_ENTER("table_replication_group_members::make_row");
  // Set default values.
  m_row_exists= false;
  m_row.channel_name_length= 0;
  m_row.member_id_length= 0;
  m_row.member_host_length= 0;
  m_row.member_port= 0;
  m_row.member_state_length= 0;

  // Set callbacks on GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS.
  const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS callbacks=
  {
    &m_row,
    &set_channel_name,
    &set_member_id,
    &set_member_host,
    &set_member_port,
    &set_member_state,
  };

  // Query plugin and let callbacks do their job.
  if (get_group_replication_group_members_info(index, callbacks))
  {
    DBUG_PRINT("info", ("Group Replication stats not available!"));
  }
  else
  {
    m_row_exists= true;
  }

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

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** channel_name */
        set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
        break;
      case 1: /** member_id */
        set_field_char_utf8(f, m_row.member_id, m_row.member_id_length);
        break;
      case 2: /** member_host */
        set_field_char_utf8(f, m_row.member_host, m_row.member_host_length);
        break;
      case 3: /** member_port */
        if (m_row.member_port > 0)
          set_field_ulong(f, m_row.member_port);
        else
          f->set_null();
        break;
      case 4: /** member_state */
        set_field_char_utf8(f, m_row.member_state, m_row.member_state_length);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
