/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_mems_by_thread_by_event_name.cc
  Table MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_mems_by_thread_by_event_name.h"

#include <stddef.h>

#include "field.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "pfs_buffer_container.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_memory.h"
#include "pfs_visitor.h"

THR_LOCK table_mems_by_thread_by_event_name::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_ALLOC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_FREE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_ALLOC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_FREE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOW_COUNT_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CURRENT_COUNT_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("HIGH_COUNT_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOW_NUMBER_OF_BYTES_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CURRENT_NUMBER_OF_BYTES_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("HIGH_NUMBER_OF_BYTES_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_mems_by_thread_by_event_name::m_field_def = {12, field_types};

PFS_engine_table_share table_mems_by_thread_by_event_name::m_share = {
  {C_STRING_WITH_LEN("memory_summary_by_thread_by_event_name")},
  &pfs_readonly_acl,
  table_mems_by_thread_by_event_name::create,
  NULL, /* write_row */
  table_mems_by_thread_by_event_name::delete_all_rows,
  table_mems_by_thread_by_event_name::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_mems_by_thread_by_event_name::match(PFS_thread *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key_1.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_mems_by_thread_by_event_name::match(PFS_memory_class *klass)
{
  if (m_fields >= 2)
  {
    if (!m_key_2.match(klass))
    {
      return false;
    }
  }
  return true;
}

PFS_engine_table *
table_mems_by_thread_by_event_name::create(void)
{
  return new table_mems_by_thread_by_event_name();
}

int
table_mems_by_thread_by_event_name::delete_all_rows(void)
{
  reset_memory_by_thread();
  return 0;
}

ha_rows
table_mems_by_thread_by_event_name::get_row_count(void)
{
  return global_thread_container.get_row_count() * memory_class_max;
}

table_mems_by_thread_by_event_name::table_mems_by_thread_by_event_name()
  : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos()
{
}

void
table_mems_by_thread_by_event_name::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int
table_mems_by_thread_by_event_name::rnd_next(void)
{
  PFS_thread *thread;
  PFS_memory_class *memory_class;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread())
  {
    thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (thread != NULL)
    {
      do
      {
        memory_class = find_memory_class(m_pos.m_index_2);
        if (memory_class != NULL)
        {
          if (!memory_class->is_global())
          {
            m_next_pos.set_after(&m_pos);
            return make_row(thread, memory_class);
          }
          m_pos.next_class();
        }
      } while (memory_class != NULL);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_mems_by_thread_by_event_name::rnd_pos(const void *pos)
{
  PFS_thread *thread;
  PFS_memory_class *memory_class;

  set_position(pos);

  thread = global_thread_container.get(m_pos.m_index_1);
  if (thread != NULL)
  {
    memory_class = find_memory_class(m_pos.m_index_2);
    if (memory_class != NULL)
    {
      if (!memory_class->is_global())
      {
        return make_row(thread, memory_class);
      }
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_mems_by_thread_by_event_name::index_init(uint idx, bool)
{
  DBUG_ASSERT(idx == 0);
  m_opened_index = PFS_NEW(PFS_index_mems_by_thread_by_event_name);
  m_index = m_opened_index;
  return 0;
}

int
table_mems_by_thread_by_event_name::index_next(void)
{
  PFS_thread *thread;
  PFS_memory_class *memory_class;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread())
  {
    thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (thread != NULL)
    {
      if (m_opened_index->match(thread))
      {
        do
        {
          memory_class = find_memory_class(m_pos.m_index_2);
          if (memory_class != NULL)
          {
            if (!memory_class->is_global())
            {
              if (m_opened_index->match(memory_class))
              {
                if (!make_row(thread, memory_class))
                {
                  m_next_pos.set_after(&m_pos);
                  return 0;
                }
              }
            }
            m_pos.next_class();
          }
        } while (memory_class != NULL);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_mems_by_thread_by_event_name::make_row(PFS_thread *thread,
                                             PFS_memory_class *klass)
{
  pfs_optimistic_state lock;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id = thread->m_thread_internal_id;

  m_row.m_event_name.make_row(klass);

  PFS_connection_memory_visitor visitor(klass);
  PFS_connection_iterator::visit_thread(thread, &visitor);

  if (!thread->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_stat.set(&visitor.m_stat);

  return 0;
}

int
table_mems_by_thread_by_event_name::read_row_values(TABLE *table,
                                                    unsigned char *,
                                                    Field **fields,
                                                    bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* THREAD_ID */
        set_field_ulonglong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* EVENT_NAME */
        m_row.m_event_name.set_field(f);
        break;
      default: /* 2, ... HIGH_NUMBER_OF_BYTES_USED */
        m_row.m_stat.set_field(f->field_index - 2, f);
        break;
      }
    }
  }

  return 0;
}
