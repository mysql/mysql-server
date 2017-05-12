/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/table_table_handles.cc
  Table TABLE_TABLE_HANDLES (implementation).
*/

#include "storage/perfschema/table_table_handles.h"

#include <stddef.h>

#include "field.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "pfs_buffer_container.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_stat.h"

THR_LOCK table_table_handles::m_table_lock;

/* clang-format off */
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
    { C_STRING_WITH_LEN("OWNER_THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OWNER_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("INTERNAL_LOCK") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EXTERNAL_LOCK") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_table_handles::m_field_def = {8, field_types};

PFS_engine_table_share table_table_handles::m_share = {
  {C_STRING_WITH_LEN("table_handles")},
  &pfs_readonly_acl,
  table_table_handles::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_table_handles::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_table_handles_by_object::match(PFS_table *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key_1.match(OBJECT_TYPE_TABLE))
    {
      return false;
    }
  }

  PFS_table_share *share = sanitize_table_share(pfs->m_share);

  if (m_fields >= 2)
  {
    if (!m_key_2.match(share))
    {
      return false;
    }
  }

  if (m_fields >= 3)
  {
    if (!m_key_3.match(share))
    {
      return false;
    }
  }

  return true;
}

bool
PFS_index_table_handles_by_instance::match(PFS_table *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }

  return true;
}

bool
PFS_index_table_handles_by_owner::match(PFS_table *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key_1.match_owner(pfs))
    {
      return false;
    }
  }

  if (m_fields >= 2)
  {
    if (!m_key_2.match_owner(pfs))
    {
      return false;
    }
  }

  return true;
}

PFS_engine_table *
table_table_handles::create(void)
{
  return new table_table_handles();
}

ha_rows
table_table_handles::get_row_count(void)
{
  return global_table_container.get_row_count();
}

table_table_handles::table_table_handles()
  : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0)
{
}

void
table_table_handles::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_table_handles::rnd_init(bool)
{
  return 0;
}

int
table_table_handles::rnd_next(void)
{
  PFS_table *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_table_iterator it = global_table_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL)
  {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int
table_table_handles::rnd_pos(const void *pos)
{
  PFS_table *pfs;

  set_position(pos);

  pfs = global_table_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_table_handles::index_init(uint idx, bool)
{
  PFS_index_table_handles *result = NULL;

  switch (idx)
  {
  case 0:
    result = PFS_NEW(PFS_index_table_handles_by_instance);
    break;
  case 1:
    result = PFS_NEW(PFS_index_table_handles_by_object);
    break;
  case 2:
    result = PFS_NEW(PFS_index_table_handles_by_owner);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_table_handles::index_next(void)
{
  PFS_table *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_table_iterator it = global_table_container.iterate(m_pos.m_index);

  do
  {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL)
    {
      if (m_opened_index->match(pfs))
      {
        if (!make_row(pfs))
        {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int
table_table_handles::make_row(PFS_table *table)
{
  pfs_optimistic_state lock;
  PFS_table_share *share;
  PFS_thread *thread;

  table->m_lock.begin_optimistic_lock(&lock);

  share = sanitize_table_share(table->m_share);
  if (share == NULL)
  {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_object.make_row(share))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_identity = table->m_identity;

  thread = sanitize_thread(table->m_thread_owner);
  if (thread != NULL)
  {
    m_row.m_owner_thread_id = thread->m_thread_internal_id;
    m_row.m_owner_event_id = table->m_owner_event_id;
  }
  else
  {
    m_row.m_owner_thread_id = 0;
    m_row.m_owner_event_id = 0;
  }

  m_row.m_internal_lock = table->m_internal_lock;
  m_row.m_external_lock = table->m_external_lock;

  if (!table->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int
table_table_handles::read_row_values(TABLE *table,
                                     unsigned char *buf,
                                     Field **fields,
                                     bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* OBJECT_TYPE */
      case 1: /* OBJECT_SCHEMA */
      case 2: /* OBJECT_NAME */
        m_row.m_object.set_field(f->field_index, f);
        break;
      case 3: /* OBJECT_INSTANCE_BEGIN */
        set_field_ulonglong(f, (intptr)m_row.m_identity);
        break;
      case 4: /* OWNER_THREAD_ID */
        set_field_ulonglong(f, m_row.m_owner_thread_id);
        break;
      case 5: /* OWNER_EVENT_ID */
        set_field_ulonglong(f, m_row.m_owner_event_id);
        break;
      case 6: /* INTERNAL_LOCK */
        set_field_lock_type(f, m_row.m_internal_lock);
        break;
      case 7: /* EXTERNAL_LOCK */
        set_field_lock_type(f, m_row.m_external_lock);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
