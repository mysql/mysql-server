/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_setup_objects.cc
  Table SETUP_OBJECTS (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_setup_object.h"
#include "table_setup_objects.h"
#include "table_helper.h"
#include "pfs_global.h"

THR_LOCK table_setup_objects::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("OBJECT_TYPE") },
    { C_STRING_WITH_LEN("enum(\'TABLE\')") },
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
#if 0
  {
    { C_STRING_WITH_LEN("ENABLED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  },
#endif
  {
    { C_STRING_WITH_LEN("TIMED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_setup_objects::m_field_def=
{ 4, field_types };

PFS_engine_table_share
table_setup_objects::m_share=
{
  { C_STRING_WITH_LEN("setup_objects") },
  &pfs_editable_acl,
  table_setup_objects::create,
  table_setup_objects::write_row,
  table_setup_objects::delete_all_rows,
  table_setup_objects::get_row_count,
  1000, /* records */
  sizeof(PFS_simple_index),
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
  Field *f;
  enum_object_type object_type= OBJECT_TYPE_TABLE;
  String object_schema_data("%", 1, &my_charset_utf8_bin);
  String object_name_data("%", 1, &my_charset_utf8_bin);
  String *object_schema= &object_schema_data;
  String *object_name= &object_name_data;
  enum_yes_no yes_no;
  bool enabled= true;
  bool timed= true;

  for (; (f= *fields) ; fields++)
  {
    if (bitmap_is_set(table->write_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* OBJECT_TYPE */
        object_type= (enum_object_type) get_field_enum(f);
        break;
      case 1: /* OBJECT_SCHEMA */
        object_schema= get_field_varchar_utf8(f, &object_schema_data);
        break;
      case 2: /* OBJECT_NAME */
        object_name= get_field_varchar_utf8(f, &object_name_data);
        break;
#if 0
      case 3: /* ENABLED */
        yes_no= (enum_yes_no) get_field_enum(f);
        enabled= (yes_no == ENUM_YES) ? true : false;
        break;
#endif
      case 3: /* TIMED */
        yes_no= (enum_yes_no) get_field_enum(f);
        timed= (yes_no == ENUM_YES) ? true : false;
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  /* Reject illegal enum values in OBJECT_TYPE */
  if (object_type != OBJECT_TYPE_TABLE)
    return HA_ERR_NO_REFERENCED_ROW;

  return insert_setup_object(object_type, object_schema, object_name,
                             enabled, timed);
}

int table_setup_objects::delete_all_rows(void)
{
  return reset_setup_object();
}

ha_rows table_setup_objects::get_row_count(void)
{
  return setup_object_count();
}

table_setup_objects::table_setup_objects()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_setup_objects::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_setup_objects::rnd_next(void)
{
  PFS_setup_object *pfs;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < setup_object_max;
       m_pos.next())
  {
    pfs= &setup_object_array[m_pos.m_index];
    if (pfs->m_lock.is_populated())
    {
      make_row(pfs);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_objects::rnd_pos(const void *pos)
{
  PFS_setup_object *pfs;

  set_position(pos);

  DBUG_ASSERT(m_pos.m_index < setup_object_max);
  pfs= &setup_object_array[m_pos.m_index];
  if (pfs->m_lock.is_populated())
  {
    make_row(pfs);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_setup_objects::make_row(PFS_setup_object *pfs)
{
  pfs_lock lock;

  m_row_exists= false;

  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_object_type= pfs->get_object_type();
  memcpy(m_row.m_schema_name, pfs->m_schema_name, pfs->m_schema_name_length);
  m_row.m_schema_name_length= pfs->m_schema_name_length;
  memcpy(m_row.m_object_name, pfs->m_object_name, pfs->m_object_name_length);
  m_row.m_object_name_length= pfs->m_object_name_length;
#if 0
  m_row.m_enabled_ptr= &pfs->m_enabled;
#else
  /*
    The ENABLED column does not exist,
    so disabled records are be displayed.
  */
  if (! pfs->m_enabled)
    return;
#endif

  m_row.m_timed_ptr= &pfs->m_timed;

  if (pfs->m_lock.end_optimistic_lock(&lock))
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
        set_field_enum(f, m_row.m_object_type);
        break;
      case 1: /* OBJECT_SCHEMA */
        if (m_row.m_schema_name_length)
          set_field_varchar_utf8(f, m_row.m_schema_name,
                                 m_row.m_schema_name_length);
        else
          f->set_null();
        break;
      case 2: /* OBJECT_NAME */
        if (m_row.m_object_name_length)
          set_field_varchar_utf8(f, m_row.m_object_name,
                                 m_row.m_object_name_length);
        else
          f->set_null();
        break;
#if 0
      case 3: /* ENABLED */
        set_field_enum(f, (*m_row.m_enabled_ptr) ? ENUM_YES : ENUM_NO);
        break;
#endif
      case 3: /* TIMED */
        set_field_enum(f, (*m_row.m_timed_ptr) ? ENUM_YES : ENUM_NO);
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
        return HA_ERR_WRONG_COMMAND;
#if 0
      case 3: /* ENABLED */
        value= (enum_yes_no) get_field_enum(f);
        *m_row.m_enabled_ptr= (value == ENUM_YES) ? true : false;
        break;
#endif
      case 3: /* TIMED */
        value= (enum_yes_no) get_field_enum(f);
        *m_row.m_timed_ptr= (value == ENUM_YES) ? true : false;
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  setup_objects_version++;
  return 0;
}

int table_setup_objects::delete_row_values(TABLE *table,
                                           const unsigned char *buf,
                                           Field **fields)
{
  Field *f;
  enum_object_type object_type= OBJECT_TYPE_TABLE;
  String object_schema_data("", 0, &my_charset_utf8_bin);
  String object_name_data("", 0, &my_charset_utf8_bin);
  String *object_schema= NULL;
  String *object_name= NULL;

  for (; (f= *fields) ; fields++)
  {
    if (bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* OBJECT_TYPE */
        object_type= (enum_object_type) get_field_enum(f);
        break;
      case 1: /* OBJECT_SCHEMA */
        object_schema= get_field_varchar_utf8(f, &object_schema_data);
        break;
      case 2: /* OBJECT_NAME */
        object_name= get_field_varchar_utf8(f, &object_name_data);
        break;
      case 3: /* ENABLED */
      case 4: /* TIMED */
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  DBUG_ASSERT(object_schema != NULL);
  DBUG_ASSERT(object_name != NULL);

  return delete_setup_object(object_type, object_schema, object_name);
}

