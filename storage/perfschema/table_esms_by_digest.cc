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
  @file storage/perfschema/table_esms_by_digest.cc
  Table EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_DIGEST (implementation).
*/

#include "storage/perfschema/table_esms_by_digest.h"

#include <stddef.h>

#include "field.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_digest.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "pfs_instr_class.h"
#include "pfs_timer.h"
#include "pfs_visitor.h"

THR_LOCK table_esms_by_digest::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("SCHEMA_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("DIGEST") },
    { C_STRING_WITH_LEN("varchar(32)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("DIGEST_TEXT") },
    { C_STRING_WITH_LEN("longtext") },
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
    { C_STRING_WITH_LEN("SUM_LOCK_TIME") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ERRORS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_WARNINGS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ROWS_AFFECTED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ROWS_SENT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ROWS_EXAMINED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_CREATED_TMP_DISK_TABLES") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_CREATED_TMP_TABLES") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SELECT_FULL_JOIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SELECT_FULL_RANGE_JOIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SELECT_RANGE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SELECT_RANGE_CHECK") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SELECT_SCAN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SORT_MERGE_PASSES") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SORT_RANGE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SORT_ROWS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_SORT_SCAN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NO_INDEX_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NO_GOOD_INDEX_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("FIRST_SEEN") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LAST_SEEN") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("QUANTILE_95") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("QUANTILE_99") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("QUANTILE_999") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_esms_by_digest::m_field_def = {32, field_types};

PFS_engine_table_share table_esms_by_digest::m_share = {
  {C_STRING_WITH_LEN("events_statements_summary_by_digest")},
  &pfs_truncatable_acl,
  table_esms_by_digest::create,
  NULL, /* write_row */
  table_esms_by_digest::delete_all_rows,
  table_esms_by_digest::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_esms_by_digest::match(PFS_statements_digest_stat *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key_1.match(pfs))
    {
      return false;
    }
  }

  if (m_fields >= 2)
  {
    return m_key_2.match(pfs);
  }
  return true;
}

PFS_engine_table *
table_esms_by_digest::create(void)
{
  return new table_esms_by_digest();
}

int
table_esms_by_digest::delete_all_rows(void)
{
  reset_esms_by_digest();
  return 0;
}

ha_rows
table_esms_by_digest::get_row_count(void)
{
  return digest_max;
}

table_esms_by_digest::table_esms_by_digest()
  : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0)
{
}

void
table_esms_by_digest::reset_position(void)
{
  m_pos = 0;
  m_next_pos = 0;
}

int
table_esms_by_digest::rnd_next(void)
{
  PFS_statements_digest_stat *digest_stat;

  if (statements_digest_stat_array == NULL)
  {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < digest_max; m_pos.next())
  {
    digest_stat = &statements_digest_stat_array[m_pos.m_index];
    if (digest_stat->m_lock.is_populated())
    {
      if (digest_stat->m_first_seen != 0)
      {
        m_next_pos.set_after(&m_pos);
        return make_row(digest_stat);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_esms_by_digest::rnd_pos(const void *pos)
{
  PFS_statements_digest_stat *digest_stat;

  if (statements_digest_stat_array == NULL)
  {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);
  digest_stat = &statements_digest_stat_array[m_pos.m_index];

  if (digest_stat->m_lock.is_populated())
  {
    if (digest_stat->m_first_seen != 0)
    {
      return make_row(digest_stat);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_esms_by_digest::index_init(uint idx, bool)
{
  PFS_index_esms_by_digest *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_esms_by_digest);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_esms_by_digest::index_next(void)
{
  PFS_statements_digest_stat *digest_stat;

  if (statements_digest_stat_array == NULL)
  {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < digest_max; m_pos.next())
  {
    digest_stat = &statements_digest_stat_array[m_pos.m_index];
    if (digest_stat->m_first_seen != 0)
    {
      if (m_opened_index->match(digest_stat))
      {
        if (!make_row(digest_stat))
        {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_esms_by_digest::make_row(PFS_statements_digest_stat *digest_stat)
{
  m_row.m_first_seen = digest_stat->m_first_seen;
  m_row.m_last_seen = digest_stat->m_last_seen;
  m_row.m_digest.make_row(digest_stat);

  /*
    Get statements stats.
  */
  time_normalizer *normalizer = time_normalizer::get(statement_timer);
  m_row.m_stat.set(normalizer, &digest_stat->m_stat);

  PFS_histogram *histogram = &digest_stat->m_histogram;

  ulong index;
  ulonglong count_star = 0;

  for (index = 0; index < NUMBER_OF_BUCKETS; index++)
  {
    count_star += histogram->read_bucket(index);
  }

  if (count_star == 0)
  {
    m_row.m_p95 = 0;
    m_row.m_p99 = 0;
    m_row.m_p999 = 0;
  }
  else
  {
    ulonglong count_95 = ((count_star * 95) + 99) / 100;
    ulonglong count_99 = ((count_star * 99) + 99) / 100;
    ulonglong count_999 = ((count_star * 999) + 999) / 1000;

    DBUG_ASSERT(count_95 != 0);
    DBUG_ASSERT(count_95 <= count_star);
    DBUG_ASSERT(count_99 != 0);
    DBUG_ASSERT(count_99 <= count_star);
    DBUG_ASSERT(count_999 != 0);
    DBUG_ASSERT(count_999 <= count_star);

    ulong index_95 = 0;
    ulong index_99 = 0;
    ulong index_999 = 0;
    bool index_95_set = false;
    bool index_99_set = false;
    bool index_999_set = false;
    ulonglong count = 0;

    for (index = 0; index < NUMBER_OF_BUCKETS; index++)
    {
      count += histogram->read_bucket(index);

      if ((count >= count_95) && !index_95_set)
      {
        index_95 = index;
        index_95_set = true;
      }

      if ((count >= count_99) && !index_99_set)
      {
        index_99 = index;
        index_99_set = true;
      }

      if ((count >= count_999) && !index_999_set)
      {
        index_999 = index;
        index_999_set = true;
      }
    }

    m_row.m_p95 = g_histogram_pico_timers.m_bucket_timer[index_95 + 1];
    m_row.m_p99 = g_histogram_pico_timers.m_bucket_timer[index_99 + 1];
    m_row.m_p999 = g_histogram_pico_timers.m_bucket_timer[index_999 + 1];
  }

  return 0;
}

int
table_esms_by_digest::read_row_values(TABLE *table,
                                      unsigned char *buf,
                                      Field **fields,
                                      bool read_all)
{
  Field *f;

  /*
    Set the null bits. It indicates how many fields could be null
    in the table.
  */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* SCHEMA_NAME */
      case 1: /* DIGEST */
      case 2: /* DIGEST_TEXT */
        m_row.m_digest.set_field(f->field_index, f);
        break;
      case 27: /* FIRST_SEEN */
        set_field_timestamp(f, m_row.m_first_seen);
        break;
      case 28: /* LAST_SEEN */
        set_field_timestamp(f, m_row.m_last_seen);
        break;
      case 29: /* QUANTILE_95 */
        set_field_ulonglong(f, m_row.m_p95);
        break;
      case 30: /* QUANTILE_99 */
        set_field_ulonglong(f, m_row.m_p99);
        break;
      case 31: /* QUANTILE_999 */
        set_field_ulonglong(f, m_row.m_p999);
        break;
      default: /* 3, ... COUNT/SUM/MIN/AVG/MAX */
        m_row.m_stat.set_field(f->field_index - 3, f);
        break;
      }
    }
  }

  return 0;
}
