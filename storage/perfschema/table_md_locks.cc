/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_md_locks.cc
  Table METADATA_LOCKS (implementation).
*/

#include "my_global.h"
#include "my_thread.h"
#include "pfs_instr.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_md_locks.h"
#include "pfs_global.h"
#include "pfs_buffer_container.h"
#include "field.h"

THR_LOCK table_metadata_locks::m_table_lock;

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
    { C_STRING_WITH_LEN("OBJECT_INSTANCE_BEGIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOCK_TYPE") },
    { C_STRING_WITH_LEN("varchar(32)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOCK_DURATION") },
    { C_STRING_WITH_LEN("varchar(32)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOCK_STATUS") },
    { C_STRING_WITH_LEN("varchar(32)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SOURCE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OWNER_THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OWNER_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_metadata_locks::m_field_def=
{ 10, field_types };

PFS_engine_table_share
table_metadata_locks::m_share=
{
  { C_STRING_WITH_LEN("metadata_locks") },
  &pfs_readonly_acl,
  table_metadata_locks::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_metadata_locks::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table* table_metadata_locks::create(void)
{
  return new table_metadata_locks();
}

ha_rows
table_metadata_locks::get_row_count(void)
{
  return global_mdl_container.get_row_count();
}

table_metadata_locks::table_metadata_locks()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_metadata_locks::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_metadata_locks::rnd_next(void)
{
  PFS_metadata_lock *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_mdl_iterator it= global_mdl_container.iterate(m_pos.m_index);
  pfs= it.scan_next(& m_pos.m_index);
  if (pfs != NULL)
  {
    make_row(pfs);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_metadata_locks::rnd_pos(const void *pos)
{
  PFS_metadata_lock *pfs;

  set_position(pos);

  pfs= global_mdl_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    make_row(pfs);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_metadata_locks::make_row(PFS_metadata_lock *pfs)
{
  pfs_optimistic_state lock;

  m_row_exists= false;

  /* Protect this reader against a metadata lock destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_identity= pfs->m_identity;
  m_row.m_mdl_type= pfs->m_mdl_type;
  m_row.m_mdl_duration= pfs->m_mdl_duration;
  m_row.m_mdl_status= pfs->m_mdl_status;

  /* Disable source file and line to avoid stale __FILE__ pointers. */
  m_row.m_source_length= 0;

  m_row.m_owner_thread_id= static_cast<ulong>(pfs->m_owner_thread_id);
  m_row.m_owner_event_id= static_cast<ulong>(pfs->m_owner_event_id);

  if (m_row.m_object.make_row(& pfs->m_mdl_key))
    return;

  if (pfs->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

int table_metadata_locks::read_row_values(TABLE *table,
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
      case 1: /* OBJECT_SCHEMA */
      case 2: /* OBJECT_NAME */
        m_row.m_object.set_nullable_field(f->field_index, f);
        break;
      case 3: /* OBJECT_INSTANCE */
        set_field_ulonglong(f, (intptr) m_row.m_identity);
        break;
      case 4: /* LOCK_TYPE */
        set_field_mdl_type(f, m_row.m_mdl_type);
        break;
      case 5: /* LOCK_DURATION */
        set_field_mdl_duration(f, m_row.m_mdl_duration);
        break;
      case 6: /* LOCK_STATUS */
        set_field_mdl_status(f, m_row.m_mdl_status);
        break;
      case 7: /* SOURCE */
        set_field_varchar_utf8(f, m_row.m_source, m_row.m_source_length);
        break;
      case 8: /* OWNER_THREAD_ID */
        if (m_row.m_owner_thread_id != 0)
          set_field_ulonglong(f, m_row.m_owner_thread_id);
        else
          f->set_null();
        break;
      case 9: /* OWNER_EVENT_ID */
        if (m_row.m_owner_event_id != 0)
          set_field_ulonglong(f, m_row.m_owner_event_id);
        else
          f->set_null();
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

