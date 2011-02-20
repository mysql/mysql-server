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
  @file storage/perfschema/table_file_summary.cc
  Table FILE_SUMMARY_BY_xxx (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_file_summary.h"
#include "pfs_global.h"

THR_LOCK table_file_summary_by_event_name::m_table_lock;

static const TABLE_FIELD_TYPE fs_by_event_name_field_types[]=
{
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_file_summary_by_event_name::m_field_def=
{ 5, fs_by_event_name_field_types };

PFS_engine_table_share
table_file_summary_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("file_summary_by_event_name") },
  &pfs_truncatable_acl,
  &table_file_summary_by_event_name::create,
  NULL, /* write_row */
  table_file_summary_by_event_name::delete_all_rows,
  1000, /* records */
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_file_summary_by_event_name::create(void)
{
  return new table_file_summary_by_event_name();
}

int table_file_summary_by_event_name::delete_all_rows(void)
{
  reset_file_class_io();
  return 0;
}

table_file_summary_by_event_name::table_file_summary_by_event_name()
  : PFS_engine_table(&m_share, &m_pos),
  m_pos(1), m_next_pos(1)
{}

void table_file_summary_by_event_name::reset_position(void)
{
  m_pos.m_index= 1;
  m_next_pos.m_index= 1;
}

int table_file_summary_by_event_name::rnd_next(void)
{
  PFS_file_class *file_class;

  m_pos.set_at(&m_next_pos);

  file_class= find_file_class(m_pos.m_index);
  if (file_class)
  {
    make_row(file_class);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_file_summary_by_event_name::rnd_pos(const void *pos)
{
  PFS_file_class *file_class;

  set_position(pos);

  file_class= find_file_class(m_pos.m_index);
  if (file_class)
  {
    make_row(file_class);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

/**
  Build a row.
  @param klass            the file class the cursor is reading
*/
void table_file_summary_by_event_name::make_row(PFS_file_class *klass)
{
  m_row.m_name= &klass->m_name[0];
  m_row.m_name_length= klass->m_name_length;
  m_row.m_file_stat= klass->m_file_stat;
}

int table_file_summary_by_event_name::read_row_values(TABLE *table,
                                                      unsigned char *,
                                                      Field **fields,
                                                      bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  /* The row always exists for classes */

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 1: /* COUNT_READ */
        set_field_ulonglong(f, m_row.m_file_stat.m_count_read);
        break;
      case 2: /* COUNT_WRITE */
        set_field_ulonglong(f, m_row.m_file_stat.m_count_write);
        break;
      case 3: /* READ_BYTES */
        set_field_ulonglong(f, m_row.m_file_stat.m_read_bytes);
        break;
      case 4: /* WRITE_BYTES */
        set_field_ulonglong(f, m_row.m_file_stat.m_write_bytes);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

THR_LOCK table_file_summary_by_instance::m_table_lock;

static const TABLE_FIELD_TYPE fs_by_instance_field_types[]=
{
  {
    { C_STRING_WITH_LEN("FILE_NAME") },
    { C_STRING_WITH_LEN("varchar(512)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_file_summary_by_instance::m_field_def=
{ 6, fs_by_instance_field_types };

PFS_engine_table_share
table_file_summary_by_instance::m_share=
{
  { C_STRING_WITH_LEN("file_summary_by_instance") },
  &pfs_truncatable_acl,
  &table_file_summary_by_instance::create,
  NULL, /* write_row */
  table_file_summary_by_instance::delete_all_rows,
  1000, /* records */
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_file_summary_by_instance::create(void)
{
  return new table_file_summary_by_instance();
}

int table_file_summary_by_instance::delete_all_rows(void)
{
  reset_file_instance_io();
  return 0;
}

table_file_summary_by_instance::table_file_summary_by_instance()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_file_summary_by_instance::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_file_summary_by_instance::rnd_next(void)
{
  PFS_file *pfs;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < file_max;
       m_pos.next())
  {
    pfs= &file_array[m_pos.m_index];
    if (pfs->m_lock.is_populated())
    {
      make_row(pfs);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_file_summary_by_instance::rnd_pos(const void *pos)
{
  PFS_file *pfs;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < file_max);
  pfs= &file_array[m_pos.m_index];

  if (! pfs->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  make_row(pfs);
  return 0;
}

/**
  Build a row.
  @param pfs              the file the cursor is reading
*/
void table_file_summary_by_instance::make_row(PFS_file *pfs)
{
  pfs_lock lock;
  PFS_file_class *safe_class;

  m_row_exists= false;

  /* Protect this reader against a file delete */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class= sanitize_file_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  m_row.m_filename= pfs->m_filename;
  m_row.m_filename_length= pfs->m_filename_length;
  m_row.m_name= safe_class->m_name;
  m_row.m_name_length= safe_class->m_name_length;
  m_row.m_file_stat= pfs->m_file_stat;

  if (pfs->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

int table_file_summary_by_instance::read_row_values(TABLE *table,
                                                          unsigned char *,
                                                          Field **fields,
                                                          bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* FILENAME */
        set_field_varchar_utf8(f, m_row.m_filename, m_row.m_filename_length);
        break;
      case 1: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 2: /* COUNT_READ */
        set_field_ulonglong(f, m_row.m_file_stat.m_count_read);
        break;
      case 3: /* COUNT_WRITE */
        set_field_ulonglong(f, m_row.m_file_stat.m_count_write);
        break;
      case 4: /* READ_BYTES */
        set_field_ulonglong(f, m_row.m_file_stat.m_read_bytes);
        break;
      case 5: /* WRITE_BYTES */
        set_field_ulonglong(f, m_row.m_file_stat.m_write_bytes);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

