/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/table_setup_actors.cc
  Table SETUP_ACTORS (implementation).
*/

#include "my_global.h"
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_setup_actor.h"
#include "table_setup_actors.h"
#include "pfs_global.h"
#include "pfs_buffer_container.h"
#include "field.h"

THR_LOCK table_setup_actors::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("HOST") },
    { C_STRING_WITH_LEN("char(60)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("USER") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ROLE") },
    { C_STRING_WITH_LEN("char(16)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ENABLED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("HISTORY") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_setup_actors::m_field_def=
{ 5, field_types };

PFS_engine_table_share
table_setup_actors::m_share=
{
  { C_STRING_WITH_LEN("setup_actors") },
  &pfs_editable_acl,
  table_setup_actors::create,
  table_setup_actors::write_row,
  table_setup_actors::delete_all_rows,
  table_setup_actors::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table* table_setup_actors::create()
{
  return new table_setup_actors();
}

int table_setup_actors::write_row(TABLE *table, unsigned char *buf,
                                  Field **fields)
{
  Field *f;
  String user_data("%", 1, &my_charset_utf8_bin);
  String host_data("%", 1, &my_charset_utf8_bin);
  String role_data("%", 1, &my_charset_utf8_bin);
  String *user= &user_data;
  String *host= &host_data;
  String *role= &role_data;
  enum_yes_no enabled_value= ENUM_YES;
  enum_yes_no history_value= ENUM_YES;
  bool enabled;
  bool history;

  for (; (f= *fields) ; fields++)
  {
    if (bitmap_is_set(table->write_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* HOST */
        host= get_field_char_utf8(f, &host_data);
        break;
      case 1: /* USER */
        user= get_field_char_utf8(f, &user_data);
        break;
      case 2: /* ROLE */
        role= get_field_char_utf8(f, &role_data);
        break;
      case 3: /* ENABLED */
        enabled_value= (enum_yes_no) get_field_enum(f);
        break;
      case 4: /* HISTORY */
        history_value= (enum_yes_no) get_field_enum(f);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  /* Reject illegal enum values in ENABLED */
  if ((enabled_value != ENUM_YES) && (enabled_value != ENUM_NO))
    return HA_ERR_NO_REFERENCED_ROW;

  /* Reject illegal enum values in HISTORY */
  if ((history_value != ENUM_YES) && (history_value != ENUM_NO))
    return HA_ERR_NO_REFERENCED_ROW;

  /* Reject if any of user/host/role is not provided */
  if (user->length() == 0 || host->length() == 0 || role->length() == 0)
    return HA_ERR_WRONG_COMMAND;

  enabled= (enabled_value == ENUM_YES) ? true : false;
  history= (history_value == ENUM_YES) ? true : false;

  return insert_setup_actor(user, host, role, enabled, history);
}

int table_setup_actors::delete_all_rows(void)
{
  return reset_setup_actor();
}

ha_rows table_setup_actors::get_row_count(void)
{
  return global_setup_actor_container.get_row_count();
}

table_setup_actors::table_setup_actors()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_setup_actors::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_setup_actors::rnd_next()
{
  PFS_setup_actor *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_setup_actor_iterator it= global_setup_actor_container.iterate(m_pos.m_index);
  pfs= it.scan_next(& m_pos.m_index);
  if (pfs != NULL)
  {
    make_row(pfs);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_actors::rnd_pos(const void *pos)
{
  PFS_setup_actor *pfs;

  set_position(pos);

  pfs= global_setup_actor_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    make_row(pfs);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_setup_actors::make_row(PFS_setup_actor *pfs)
{
  pfs_optimistic_state lock;

  m_row_exists= false;

  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_hostname_length= pfs->m_hostname_length;
  if (unlikely((m_row.m_hostname_length == 0) ||
               (m_row.m_hostname_length > sizeof(m_row.m_hostname))))
    return;
  memcpy(m_row.m_hostname, pfs->m_hostname, m_row.m_hostname_length);

  m_row.m_username_length= pfs->m_username_length;
  if (unlikely((m_row.m_username_length == 0) ||
               (m_row.m_username_length > sizeof(m_row.m_username))))
    return;
  memcpy(m_row.m_username, pfs->m_username, m_row.m_username_length);

  m_row.m_rolename_length= pfs->m_rolename_length;
  if (unlikely((m_row.m_rolename_length == 0) ||
               (m_row.m_rolename_length > sizeof(m_row.m_rolename))))
    return;
  memcpy(m_row.m_rolename, pfs->m_rolename, m_row.m_rolename_length);

  m_row.m_enabled_ptr= &pfs->m_enabled;
  m_row.m_history_ptr= &pfs->m_history;

  if (pfs->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

int table_setup_actors::read_row_values(TABLE *table,
                                        unsigned char *buf,
                                        Field **fields,
                                        bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* HOST */
        set_field_char_utf8(f, m_row.m_hostname, m_row.m_hostname_length);
        break;
      case 1: /* USER */
        set_field_char_utf8(f, m_row.m_username, m_row.m_username_length);
        break;
      case 2: /* ROLE */
        set_field_char_utf8(f, m_row.m_rolename, m_row.m_rolename_length);
        break;
      case 3: /* ENABLED */
        set_field_enum(f, (*m_row.m_enabled_ptr) ? ENUM_YES : ENUM_NO);
        break;
      case 4: /* HISTORY */
        set_field_enum(f, (*m_row.m_history_ptr) ? ENUM_YES : ENUM_NO);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

int table_setup_actors::update_row_values(TABLE *table,
                                          const unsigned char *old_buf,
                                          unsigned char *new_buf,
                                          Field **fields)
{
  int result;
  Field *f;
  enum_yes_no value;

  for (; (f= *fields) ; fields++)
  {
    if (bitmap_is_set(table->write_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* HOST */
      case 1: /* USER */
      case 2: /* ROLE */
        return HA_ERR_WRONG_COMMAND;
      case 3: /* ENABLED */
        value= (enum_yes_no) get_field_enum(f);
        /* Reject illegal enum values in ENABLED */
        if ((value != ENUM_YES) && (value != ENUM_NO))
          return HA_ERR_NO_REFERENCED_ROW;
        *m_row.m_enabled_ptr= (value == ENUM_YES) ? true : false;
        break;
      case 4: /* HISTORY */
        value= (enum_yes_no) get_field_enum(f);
        /* Reject illegal enum values in HISTORY */
        if ((value != ENUM_YES) && (value != ENUM_NO))
          return HA_ERR_NO_REFERENCED_ROW;
        *m_row.m_history_ptr= (value == ENUM_YES) ? true : false;
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  result= update_setup_actors_derived_flags();
  return result;
}

int table_setup_actors::delete_row_values(TABLE *table,
                                          const unsigned char *buf,
                                          Field **fields)
{
  DBUG_ASSERT(m_row_exists);

  CHARSET_INFO *cs= &my_charset_utf8_bin;
  String user(m_row.m_username, m_row.m_username_length, cs);
  String role(m_row.m_rolename, m_row.m_rolename_length, cs);
  String host(m_row.m_hostname, m_row.m_hostname_length, cs);

  return delete_setup_actor(&user, &host, &role);
}

