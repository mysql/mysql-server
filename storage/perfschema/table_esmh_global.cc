/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_esmh_global.cc
  Table EVENTS_STATEMENTS_HISTOGRAM_GLOBAL (implementation).
*/

#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_esmh_global.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "pfs_timer.h"
#include "pfs_visitor.h"
#include "pfs_digest.h"
#include "field.h"

THR_LOCK table_esmh_global::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("BUCKET_NUMBER") },
    { C_STRING_WITH_LEN("int(10)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BUCKET_TIMER_LOW") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BUCKET_TIMER_HIGH") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_BUCKET") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_BUCKET_AND_LOWER") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("BUCKET_QUANTILE") },
    { C_STRING_WITH_LEN("double(7,6)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_esmh_global::m_field_def = {6, field_types};

PFS_engine_table_share table_esmh_global::m_share = {
  {C_STRING_WITH_LEN("events_statements_histogram_global")},
  &pfs_truncatable_acl,
  table_esmh_global::create,
  NULL,
  table_esmh_global::delete_all_rows,
  table_esmh_global::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_field_def,
  false,
  false};

bool
PFS_index_esmh_global::match_bucket(ulong bucket_index)
{
  if (m_fields >= 1)
  {
    return m_key_1.match(bucket_index);
  }

  return true;
}

PFS_engine_table *
table_esmh_global::create(void)
{
  table_esmh_global *table = new table_esmh_global();
  table->materialize();
  return table;
}

int
table_esmh_global::delete_all_rows()
{
  reset_histogram_global();
  return 0;
}

ha_rows
table_esmh_global::get_row_count(void)
{
  return NUMBER_OF_BUCKETS;
}

table_esmh_global::table_esmh_global()
  : PFS_engine_table(&m_share, &m_pos),
    m_pos(0),
    m_next_pos(0),
    m_materialized(false)
{
}

void
table_esmh_global::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_esmh_global::rnd_next(void)
{
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < NUMBER_OF_BUCKETS;
       m_pos.next())
  {
    make_row(m_pos.m_index);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int
table_esmh_global::rnd_pos(const void *pos)
{
  set_position(pos);

  if (m_pos.m_index < NUMBER_OF_BUCKETS)
  {
    return make_row(m_pos.m_index);
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_esmh_global::index_init(uint idx, bool)
{
  PFS_index_esmh_global *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_esmh_global);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_esmh_global::index_next(void)
{
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < NUMBER_OF_BUCKETS;
       m_pos.next())
  {
    if (m_opened_index->match_bucket(m_pos.m_index))
    {
      make_row(m_pos.m_index);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

void
table_esmh_global::materialize()
{
  if (!m_materialized)
  {
    PFS_histogram *histogram = &global_statements_histogram;

    ulong index;
    ulonglong count = 0;
    ulonglong count_and_lower = 0;

    for (index = 0; index < NUMBER_OF_BUCKETS; index++)
    {
      count = histogram->read_bucket(index);
      count_and_lower += count;

      PFS_esmh_global_bucket &b = m_materialized_histogram.m_buckets[index];

      b.m_count_bucket = count;
      b.m_count_bucket_and_lower = count_and_lower;
    }

    m_materialized = true;
  }
}

int
table_esmh_global::make_row(ulong bucket_index)
{
  DBUG_ASSERT(m_materialized);
  DBUG_ASSERT(bucket_index < NUMBER_OF_BUCKETS);

  m_row.m_bucket_number = bucket_index;
  m_row.m_bucket_timer_low =
    g_histogram_pico_timers.m_bucket_timer[bucket_index];
  m_row.m_bucket_timer_high =
    g_histogram_pico_timers.m_bucket_timer[bucket_index + 1];

  m_row.m_count_bucket =
    m_materialized_histogram.m_buckets[bucket_index].m_count_bucket;
  m_row.m_count_bucket_and_lower =
    m_materialized_histogram.m_buckets[bucket_index].m_count_bucket_and_lower;

  ulonglong count_star =
    m_materialized_histogram.m_buckets[NUMBER_OF_BUCKETS - 1]
      .m_count_bucket_and_lower;

  if (count_star > 0)
  {
    double dividend = m_row.m_count_bucket_and_lower;
    double divisor = count_star;
    m_row.m_percentile = dividend / divisor; /* computed with double, not int */
  }
  else
  {
    m_row.m_percentile = 0.0;
  }

  return 0;
}

int
table_esmh_global::read_row_values(TABLE *table,
                                   unsigned char *buf,
                                   Field **fields,
                                   bool read_all)
{
  Field *f;

  /*
    Set the null bits. It indicates how many fields could be null
    in the table.
  */
  DBUG_ASSERT(table->s->null_bytes == 1);  // TODO: WHY ?
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* BUCKET_NUMBER */
        set_field_ulong(f, m_row.m_bucket_number);
        break;
      case 1: /* BUCKET_TIMER_LOW */
        set_field_ulonglong(f, m_row.m_bucket_timer_low);
        break;
      case 2: /* BUCKET_TIMER_HIGH */
        set_field_ulonglong(f, m_row.m_bucket_timer_high);
        break;
      case 3: /* COUNT_BUCKET */
        set_field_ulonglong(f, m_row.m_count_bucket);
        break;
      case 4: /* COUNT_BUCKET_AND_LOWER */
        set_field_ulonglong(f, m_row.m_count_bucket_and_lower);
        break;
      case 5: /* BUCKET_QUANTILE */
        set_field_double(f, m_row.m_percentile);
        break;
      default:
        DBUG_ASSERT(false);
        break;
      }
    }
  }

  return 0;
}
