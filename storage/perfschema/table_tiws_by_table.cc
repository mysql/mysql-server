/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
  */

/**
  @file storage/perfschema/table_tiws_by_table.cc
  Table TABLE_IO_WAITS_SUMMARY_BY_TABLE (implementation).
*/

#include "storage/perfschema/table_tiws_by_table.h"

#include <stddef.h>

#include "field.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "pfs_buffer_container.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_visitor.h"

THR_LOCK table_tiws_by_table::m_table_lock;

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
    { C_STRING_WITH_LEN("COUNT_STAR") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_FETCH") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_FETCH") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_FETCH") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_FETCH") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_FETCH") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_UPDATE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_UPDATE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_UPDATE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_UPDATE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_UPDATE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_DELETE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_DELETE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_DELETE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_DELETE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_DELETE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_tiws_by_table::m_field_def = {38, field_types};

PFS_engine_table_share table_tiws_by_table::m_share = {
  {C_STRING_WITH_LEN("table_io_waits_summary_by_table")},
  &pfs_truncatable_acl,
  table_tiws_by_table::create,
  NULL, /* write_row */
  table_tiws_by_table::delete_all_rows,
  table_tiws_by_table::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_tiws_by_table::match(const PFS_table_share *share)
{
  if (m_fields >= 1)
  {
    if (!m_key_1.match(OBJECT_TYPE_TABLE))
    {
      return false;
    }
  }

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

PFS_engine_table *
table_tiws_by_table::create(void)
{
  return new table_tiws_by_table();
}

int
table_tiws_by_table::delete_all_rows(void)
{
  reset_table_io_waits_by_table_handle();
  reset_table_io_waits_by_table();
  return 0;
}

ha_rows
table_tiws_by_table::get_row_count(void)
{
  return global_table_share_container.get_row_count();
}

table_tiws_by_table::table_tiws_by_table()
  : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0)
{
}

void
table_tiws_by_table::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_tiws_by_table::rnd_init(bool)
{
  m_normalizer = time_normalizer::get(wait_timer);
  return 0;
}

int
table_tiws_by_table::rnd_next(void)
{
  PFS_table_share *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_table_share_iterator it =
    global_table_share_container.iterate(m_pos.m_index);
  do
  {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL)
    {
      if (pfs->m_enabled)
      {
        m_next_pos.set_after(&m_pos);
        return make_row(pfs);
      }
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int
table_tiws_by_table::rnd_pos(const void *pos)
{
  PFS_table_share *pfs;

  set_position(pos);

  pfs = global_table_share_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    if (pfs->m_enabled)
    {
      return make_row(pfs);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_tiws_by_table::index_init(uint idx, bool)
{
  m_normalizer = time_normalizer::get(wait_timer);

  PFS_index_tiws_by_table *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_tiws_by_table);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_tiws_by_table::index_next(void)
{
  PFS_table_share *share;
  bool has_more_share = true;

  for (m_pos.set_at(&m_next_pos); has_more_share; m_pos.next())
  {
    share = global_table_share_container.get(m_pos.m_index, &has_more_share);

    if (share != NULL)
    {
      if (share->m_enabled)
      {
        if (m_opened_index->match(share))
        {
          if (!make_row(share))
          {
            m_next_pos.set_after(&m_pos);
            return 0;
          }
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_tiws_by_table::make_row(PFS_table_share *share)
{
  pfs_optimistic_state lock;

  share->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_object.make_row(share))
  {
    return HA_ERR_RECORD_DELETED;
  }

  PFS_table_io_stat_visitor visitor;
  PFS_object_iterator::visit_tables(share, &visitor);

  if (!share->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);

  return 0;
}

int
table_tiws_by_table::read_row_values(TABLE *table,
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
      case 1: /* SCHEMA_NAME */
      case 2: /* OBJECT_NAME */
        m_row.m_object.set_field(f->field_index, f);
        break;
      case 3: /* COUNT_STAR */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_count);
        break;
      case 4: /* SUM */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_sum);
        break;
      case 5: /* MIN */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_min);
        break;
      case 6: /* AVG */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_avg);
        break;
      case 7: /* MAX */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_max);
        break;
      case 8: /* COUNT_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_count);
        break;
      case 9: /* SUM_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_sum);
        break;
      case 10: /* MIN_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_min);
        break;
      case 11: /* AVG_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_avg);
        break;
      case 12: /* MAX_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_max);
        break;
      case 13: /* COUNT_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_count);
        break;
      case 14: /* SUM_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_sum);
        break;
      case 15: /* MIN_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_min);
        break;
      case 16: /* AVG_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_avg);
        break;
      case 17: /* MAX_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_max);
        break;
      case 18: /* COUNT_FETCH */
        set_field_ulonglong(f, m_row.m_stat.m_fetch.m_count);
        break;
      case 19: /* SUM_FETCH */
        set_field_ulonglong(f, m_row.m_stat.m_fetch.m_sum);
        break;
      case 20: /* MIN_FETCH */
        set_field_ulonglong(f, m_row.m_stat.m_fetch.m_min);
        break;
      case 21: /* AVG_FETCH */
        set_field_ulonglong(f, m_row.m_stat.m_fetch.m_avg);
        break;
      case 22: /* MAX_FETCH */
        set_field_ulonglong(f, m_row.m_stat.m_fetch.m_max);
        break;
      case 23: /* COUNT_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_insert.m_count);
        break;
      case 24: /* SUM_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_insert.m_sum);
        break;
      case 25: /* MIN_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_insert.m_min);
        break;
      case 26: /* AVG_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_insert.m_avg);
        break;
      case 27: /* MAX_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_insert.m_max);
        break;
      case 28: /* COUNT_UPDATE */
        set_field_ulonglong(f, m_row.m_stat.m_update.m_count);
        break;
      case 29: /* SUM_UPDATE */
        set_field_ulonglong(f, m_row.m_stat.m_update.m_sum);
        break;
      case 30: /* MIN_UPDATE */
        set_field_ulonglong(f, m_row.m_stat.m_update.m_min);
        break;
      case 31: /* AVG_UPDATE */
        set_field_ulonglong(f, m_row.m_stat.m_update.m_avg);
        break;
      case 32: /* MAX_UPDATE */
        set_field_ulonglong(f, m_row.m_stat.m_update.m_max);
        break;
      case 33: /* COUNT_DELETE */
        set_field_ulonglong(f, m_row.m_stat.m_delete.m_count);
        break;
      case 34: /* SUM_DELETE */
        set_field_ulonglong(f, m_row.m_stat.m_delete.m_sum);
        break;
      case 35: /* MIN_DELETE */
        set_field_ulonglong(f, m_row.m_stat.m_delete.m_min);
        break;
      case 36: /* AVG_DELETE */
        set_field_ulonglong(f, m_row.m_stat.m_delete.m_avg);
        break;
      case 37: /* MAX_DELETE */
        set_field_ulonglong(f, m_row.m_stat.m_delete.m_max);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
