/* Copyright (C) 2008-2009 Sun Microsystems, Inc

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file storage/perfschema/table_setup_objects.cc
  Table SETUP_OBJECTS (implementation).
*/

#include "sql_priv.h"
#include "unireg.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_setup_objects.h"
#include "pfs_global.h"

THR_LOCK table_setup_objects::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("OBJECT_TYPE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_SCHEMA") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ENABLED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AGGREGATED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_setup_objects::m_field_def=
{ 6, field_types };

PFS_engine_table_share
table_setup_objects::m_share=
{
  { C_STRING_WITH_LEN("SETUP_OBJECTS") },
  &pfs_editable_acl,
  &table_setup_objects::create,
  table_setup_objects::write_row,
  table_setup_objects::delete_all_rows,
  1000, /* records */
  sizeof(pos_setup_objects),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_setup_objects::create(void)
{
  return new table_setup_objects();
}

int table_setup_objects::write_row(TABLE *table, unsigned char *buf,
                                   Field **fields)
{
  /* Not implemented */
  return HA_ERR_WRONG_COMMAND;
}

int table_setup_objects::delete_all_rows(void)
{
  /* Not implemented */
  return HA_ERR_WRONG_COMMAND;
}

table_setup_objects::table_setup_objects()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(), m_next_pos()
{}

void table_setup_objects::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_setup_objects::rnd_next(void)
{
  PFS_table_share *table_share;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    switch (m_pos.m_index_1) {
    case pos_setup_objects::VIEW_TABLE:
      for ( ; m_pos.m_index_2 < table_share_max; m_pos.m_index_2++)
      {
        table_share= &table_share_array[m_pos.m_index_2];
        if (table_share->m_lock.is_populated())
        {
          make_row(table_share);
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      break;
    case pos_setup_objects::VIEW_EVENT:
    case pos_setup_objects::VIEW_PROCEDURE:
    case pos_setup_objects::VIEW_FUNCTION:
    default:
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_objects::rnd_pos(const void *pos)
{
  PFS_table_share *share;

  set_position(pos);

  switch (m_pos.m_index_1) {
  case pos_setup_objects::VIEW_TABLE:
    DBUG_ASSERT(m_pos.m_index_2 < table_share_max);
    share= &table_share_array[m_pos.m_index_2];
    if (share->m_lock.is_populated())
    {
      make_row(share);
      return 0;
    }
    break;
  case pos_setup_objects::VIEW_EVENT:
  case pos_setup_objects::VIEW_PROCEDURE:
  case pos_setup_objects::VIEW_FUNCTION:
  default:
    break;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_setup_objects::make_row(PFS_table_share *share)
{
  pfs_lock lock;

  m_row_exists= false;
  if (share == NULL)
    return;

  share->m_lock.begin_optimistic_lock(&lock);

  m_row.m_schema_name= &share->m_schema_name[0];
  m_row.m_schema_name_length= share->m_schema_name_length;
  m_row.m_object_name= &share->m_table_name[0];
  m_row.m_object_name_length= share->m_table_name_length;
  m_row.m_enabled_ptr= &share->m_enabled;
  m_row.m_timed_ptr= &share->m_timed;
  m_row.m_aggregated_ptr= &share->m_aggregated;

  if (share->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

int table_setup_objects::read_row_values(TABLE *table,
                                         unsigned char *buf,
                                         Field **fields,
                                         bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* OBJECT_TYPE */
        set_field_varchar_utf8(f, "TABLE", 5);
        break;
      case 1: /* OBJECT_SCHEMA */
        set_field_varchar_utf8(f, m_row.m_schema_name,
                               m_row.m_schema_name_length);
        break;
      case 2: /* OBJECT_NAME */
        set_field_varchar_utf8(f, m_row.m_object_name,
                               m_row.m_object_name_length);
        break;
      case 3: /* ENABLED */
        set_field_enum(f, (*m_row.m_enabled_ptr) ? ENUM_YES : ENUM_NO);
        break;
      case 4: /* TIMED */
        set_field_enum(f, (*m_row.m_timed_ptr) ? ENUM_YES : ENUM_NO);
        break;
      case 5: /* AGGREGATED */
        set_field_enum(f, (*m_row.m_aggregated_ptr) ? ENUM_YES : ENUM_NO);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

int table_setup_objects::update_row_values(TABLE *table,
                                           const unsigned char *,
                                           unsigned char *,
                                           Field **fields)
{
  Field *f;
  enum_yes_no value;

  for (; (f= *fields) ; fields++)
  {
    if (bitmap_is_set(table->write_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* OBJECT_TYPE */
      case 1: /* OBJECT_SCHEMA */
      case 2: /* OBJECT_NAME */
        my_error(ER_WRONG_PERFSCHEMA_USAGE, MYF(0));
        return HA_ERR_WRONG_COMMAND;
      case 3: /* ENABLED */
        value= (enum_yes_no) get_field_enum(f);
        *m_row.m_enabled_ptr= (value == ENUM_YES) ? true : false;
        break;
      case 4: /* TIMED */
        value= (enum_yes_no) get_field_enum(f);
        *m_row.m_timed_ptr= (value == ENUM_YES) ? true : false;
        break;
      case 5: /* AGGREGATED */
        value= (enum_yes_no) get_field_enum(f);
        *m_row.m_aggregated_ptr= (value == ENUM_YES) ? true : false;
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

