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
  @file storage/perfschema/table_threads.cc
  Table THREADS (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "table_threads.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"

THR_LOCK table_threads::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("PROCESSLIST_ID") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_threads::m_field_def=
{ 3, field_types };

PFS_engine_table_share
table_threads::m_share=
{
  { C_STRING_WITH_LEN("threads") },
  &pfs_readonly_acl,
  &table_threads::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  1000, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_threads::create(void)
{
  return new table_threads();
}

table_threads::table_threads()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_threads::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_threads::rnd_next(void)
{
  PFS_thread *pfs;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < thread_max;
       m_pos.next())
  {
    pfs= &thread_array[m_pos.m_index];
    if (pfs->m_lock.is_populated())
    {
      make_row(pfs);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_threads::rnd_pos(const void *pos)
{
  PFS_thread *pfs;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < thread_max);
  pfs= &thread_array[m_pos.m_index];
  if (pfs->m_lock.is_populated())
  {
    make_row(pfs);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_threads::make_row(PFS_thread *pfs)
{
  pfs_lock lock;
  PFS_thread_class *safe_class;

  m_row_exists= false;

  /* Protect this reader against thread termination */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class= sanitize_thread_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  m_row.m_thread_internal_id= pfs->m_thread_internal_id;
  m_row.m_thread_id= pfs->m_thread_id;
  m_row.m_name= safe_class->m_name;
  m_row.m_name_length= safe_class->m_name_length;

  if (pfs->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

int table_threads::read_row_values(TABLE *table,
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
      case 0: /* THREAD_ID */
        set_field_ulong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* PROCESSLIST_ID */
        set_field_ulong(f, m_row.m_thread_id);
        break;
      case 2: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

