/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_data_lock_waits.cc
  Table DATA_LOCK_WAITS (implementation).
*/

#include "storage/perfschema/table_data_lock_waits.h"

#include <stddef.h>

#include "field.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_thread.h"
#include "pfs_buffer_container.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr.h"

THR_LOCK table_data_lock_waits::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("ENGINE") },
    { C_STRING_WITH_LEN("varchar(32)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("REQUESTING_ENGINE_LOCK_ID") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("REQUESTING_ENGINE_TRANSACTION_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("REQUESTING_THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("REQUESTING_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("REQUESTING_OBJECT_INSTANCE_BEGIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BLOCKING_ENGINE_LOCK_ID") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BLOCKING_ENGINE_TRANSACTION_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BLOCKING_THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BLOCKING_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BLOCKING_OBJECT_INSTANCE_BEGIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_data_lock_waits::m_field_def = {11, field_types};

PFS_engine_table_share table_data_lock_waits::m_share = {
  {C_STRING_WITH_LEN("data_lock_waits")},
  &pfs_readonly_acl,
  table_data_lock_waits::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_data_lock_waits::get_row_count,
  sizeof(pk_pos_t),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table *
table_data_lock_waits::create(PFS_engine_table_share *)
{
  return new table_data_lock_waits();
}

ha_rows
table_data_lock_waits::get_row_count(void)
{
  // FIXME
  return 99999;
}

table_data_lock_waits::table_data_lock_waits()
  : PFS_engine_table(&m_share, &m_pk_pos),
    m_row(NULL),
    m_pos(),
    m_next_pos(),
    m_pk_pos()
{
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++)
  {
    m_iterator[i] = NULL;
  }
}

table_data_lock_waits::~table_data_lock_waits()
{
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++)
  {
    if (m_iterator[i] != NULL)
    {
      g_data_lock_inspector[i]->destroy_data_lock_wait_iterator(m_iterator[i]);
    }
  }
}

void
table_data_lock_waits::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
  m_pk_pos.reset();
  m_container.clear();
}

int
table_data_lock_waits::rnd_next(void)
{
  row_data_lock_wait *data;

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_engine(); m_pos.next_engine())
  {
    unsigned int index = m_pos.m_index_1;

    if (m_iterator[index] == NULL)
    {
      if (g_data_lock_inspector[index] == NULL)
      {
        continue;
      }

      m_iterator[index] =
        g_data_lock_inspector[index]->create_data_lock_wait_iterator();

      if (m_iterator[index] == NULL)
      {
        continue;
      }
    }

    bool iterator_done = false;
    PSI_engine_data_lock_wait_iterator *it = m_iterator[index];

    for (;;)
    {
      data = m_container.get_row(m_pos.m_index_2);
      if (data != NULL)
      {
        m_row = data;
        m_next_pos.set_after(&m_pos);
        m_pk_pos.set(&m_row->m_hidden_pk);
        return 0;
      }

      if (iterator_done)
      {
        break;
      }

      m_container.shrink();
      iterator_done = it->scan(&m_container);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_data_lock_waits::rnd_pos(const void *pos)
{
  row_data_lock_wait *data;

  set_position(pos);

  /*
    TODO: Multiple engine support.
    Find the proper engine based on column ENGINE.
  */
  static_assert(COUNT_DATA_LOCK_ENGINES == 1,
                "We don't support multiple engines yet.");
  unsigned int index = 0;

  if (m_iterator[index] == NULL)
  {
    if (g_data_lock_inspector[index] == NULL)
    {
      return HA_ERR_RECORD_DELETED;
    }

    m_iterator[index] =
      g_data_lock_inspector[index]->create_data_lock_wait_iterator();

    if (m_iterator[index] == NULL)
    {
      return HA_ERR_RECORD_DELETED;
    }
  }

  PSI_engine_data_lock_wait_iterator *it = m_iterator[index];

  m_container.clear();
  it->fetch(&m_container,
            m_pk_pos.m_requesting_engine_lock_id,
            m_pk_pos.m_requesting_engine_lock_id_length,
            m_pk_pos.m_blocking_engine_lock_id,
            m_pk_pos.m_blocking_engine_lock_id_length);
  data = m_container.get_row(0);
  if (data != NULL)
  {
    m_row = data;
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_data_lock_waits::index_init(uint idx, bool)
{
  PFS_index_data_lock_waits *result = NULL;

  switch (idx)
  {
  case 0:
    result = PFS_NEW(PFS_index_data_lock_waits_by_requesting_lock_id);
    break;
  case 1:
    result = PFS_NEW(PFS_index_data_lock_waits_by_blocking_lock_id);
    break;
  case 2:
    result = PFS_NEW(PFS_index_data_lock_waits_by_requesting_transaction_id);
    break;
  case 3:
    result = PFS_NEW(PFS_index_data_lock_waits_by_blocking_transaction_id);
    break;
  case 4:
    result = PFS_NEW(PFS_index_data_lock_waits_by_requesting_thread_id);
    break;
  case 5:
    result = PFS_NEW(PFS_index_data_lock_waits_by_blocking_thread_id);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  m_opened_index = result;
  m_index = result;

  m_container.set_filter(m_opened_index);
  return 0;
}

int
table_data_lock_waits::index_next()
{
  return rnd_next();
}

int
table_data_lock_waits::read_row_values(TABLE *table,
                                       unsigned char *buf,
                                       Field **fields,
                                       bool read_all)
{
  Field *f;

  if (unlikely(m_row == NULL))
  {
    return HA_ERR_RECORD_DELETED;
  }

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* ENGINE */
        set_field_varchar_utf8(f, m_row->m_engine);
        break;
      case 1: /* REQUESTING_ENGINE_LOCK_ID */
        set_field_varchar_utf8(
          f,
          m_row->m_hidden_pk.m_requesting_engine_lock_id,
          m_row->m_hidden_pk.m_requesting_engine_lock_id_length);
        break;
      case 2: /* REQUESTING_ENGINE_TRANSACTION_ID */
        set_field_ulonglong(f, m_row->m_requesting_transaction_id);
        break;
      case 3: /* REQUESTING_THREAD_ID */
        set_field_ulonglong(f, m_row->m_requesting_thread_id);
        break;
      case 4: /* REQUESTING_EVENT_ID */
        set_field_ulonglong(f, m_row->m_requesting_event_id);
        break;
      case 5: /* REQUESTING_OBJECT_INSTANCE_BEGIN */
        set_field_ulonglong(f, (intptr)m_row->m_requesting_identity);
        break;
      case 6: /* BLOCKING_ENGINE_LOCK_ID */
        set_field_varchar_utf8(
          f,
          m_row->m_hidden_pk.m_blocking_engine_lock_id,
          m_row->m_hidden_pk.m_blocking_engine_lock_id_length);
        break;
      case 7: /* BLOCKING_ENGINE_TRANSACTION_ID */
        set_field_ulonglong(f, m_row->m_blocking_transaction_id);
        break;
      case 8: /* BLOCKING_THREAD_ID */
        set_field_ulonglong(f, m_row->m_blocking_thread_id);
        break;
      case 9: /* BLOCKING_EVENT_ID */
        set_field_ulonglong(f, m_row->m_blocking_event_id);
        break;
      case 10: /* BLOCKING_OBJECT_INSTANCE_BEGIN */
        set_field_ulonglong(f, (intptr)m_row->m_blocking_identity);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
