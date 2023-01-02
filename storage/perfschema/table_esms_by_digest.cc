/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

/**
  @file storage/perfschema/table_esms_by_digest.cc
  Table EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_DIGEST (implementation).
*/

#include "storage/perfschema/table_esms_by_digest.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_digest.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_esms_by_digest::m_table_lock;

Plugin_table table_esms_by_digest::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_statements_summary_by_digest",
    /* Definition */
    "  SCHEMA_NAME VARCHAR(64),\n"
    "  DIGEST VARCHAR(64),\n"
    "  DIGEST_TEXT LONGTEXT,\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  SUM_LOCK_TIME BIGINT unsigned not null,\n"
    "  SUM_ERRORS BIGINT unsigned not null,\n"
    "  SUM_WARNINGS BIGINT unsigned not null,\n"
    "  SUM_ROWS_AFFECTED BIGINT unsigned not null,\n"
    "  SUM_ROWS_SENT BIGINT unsigned not null,\n"
    "  SUM_ROWS_EXAMINED BIGINT unsigned not null,\n"
    "  SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,\n"
    "  SUM_CREATED_TMP_TABLES BIGINT unsigned not null,\n"
    "  SUM_SELECT_FULL_JOIN BIGINT unsigned not null,\n"
    "  SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,\n"
    "  SUM_SELECT_RANGE BIGINT unsigned not null,\n"
    "  SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,\n"
    "  SUM_SELECT_SCAN BIGINT unsigned not null,\n"
    "  SUM_SORT_MERGE_PASSES BIGINT unsigned not null,\n"
    "  SUM_SORT_RANGE BIGINT unsigned not null,\n"
    "  SUM_SORT_ROWS BIGINT unsigned not null,\n"
    "  SUM_SORT_SCAN BIGINT unsigned not null,\n"
    "  SUM_NO_INDEX_USED BIGINT unsigned not null,\n"
    "  SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,\n"
    "  SUM_CPU_TIME BIGINT unsigned not null,\n"
    "  MAX_CONTROLLED_MEMORY BIGINT unsigned not null,\n"
    "  MAX_TOTAL_MEMORY BIGINT unsigned not null,\n"
    "  COUNT_SECONDARY BIGINT unsigned not null,\n"
    "  FIRST_SEEN TIMESTAMP(6) not null,\n"
    "  LAST_SEEN TIMESTAMP(6) not null,\n"
    "  QUANTILE_95 BIGINT unsigned not null,\n"
    "  QUANTILE_99 BIGINT unsigned not null,\n"
    "  QUANTILE_999 BIGINT unsigned not null,\n"
    "  QUERY_SAMPLE_TEXT LONGTEXT,\n"
    "  QUERY_SAMPLE_SEEN TIMESTAMP(6) not null,\n"
    "  QUERY_SAMPLE_TIMER_WAIT BIGINT unsigned not null,\n"
    "  UNIQUE KEY (SCHEMA_NAME, DIGEST) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_esms_by_digest::m_share = {
    &pfs_truncatable_acl,
    table_esms_by_digest::create,
    nullptr, /* write_row */
    table_esms_by_digest::delete_all_rows,
    table_esms_by_digest::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_esms_by_digest::match(PFS_statements_digest_stat *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    return m_key_2.match(pfs);
  }
  return true;
}

PFS_engine_table *table_esms_by_digest::create(PFS_engine_table_share *) {
  return new table_esms_by_digest();
}

int table_esms_by_digest::delete_all_rows() {
  reset_esms_by_digest();
  return 0;
}

ha_rows table_esms_by_digest::get_row_count() { return digest_max; }

table_esms_by_digest::table_esms_by_digest()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {
  m_normalizer = time_normalizer::get_statement();
}

void table_esms_by_digest::reset_position() {
  m_pos = 0;
  m_next_pos = 0;
}

int table_esms_by_digest::rnd_next() {
  PFS_statements_digest_stat *digest_stat;

  if (statements_digest_stat_array == nullptr) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < digest_max; m_pos.next()) {
    digest_stat = &statements_digest_stat_array[m_pos.m_index];
    if (digest_stat->m_lock.is_populated()) {
      if (digest_stat->m_first_seen != 0) {
        m_next_pos.set_after(&m_pos);
        return make_row(digest_stat);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_esms_by_digest::rnd_pos(const void *pos) {
  PFS_statements_digest_stat *digest_stat;

  if (statements_digest_stat_array == nullptr) {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);
  digest_stat = &statements_digest_stat_array[m_pos.m_index];

  if (digest_stat->m_lock.is_populated()) {
    if (digest_stat->m_first_seen != 0) {
      return make_row(digest_stat);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_esms_by_digest::index_init(uint idx [[maybe_unused]], bool) {
  PFS_index_esms_by_digest *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_esms_by_digest);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_esms_by_digest::index_next() {
  PFS_statements_digest_stat *digest_stat;

  if (statements_digest_stat_array == nullptr) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < digest_max; m_pos.next()) {
    digest_stat = &statements_digest_stat_array[m_pos.m_index];
    if (digest_stat->m_first_seen != 0) {
      if (m_opened_index->match(digest_stat)) {
        if (!make_row(digest_stat)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_esms_by_digest::make_row(PFS_statements_digest_stat *digest_stat) {
  m_row.m_first_seen = digest_stat->m_first_seen;
  m_row.m_last_seen = digest_stat->m_last_seen;
  m_row.m_digest.make_row(digest_stat);

  /*
    Get statements stats.
  */
  m_row.m_stat.set(m_normalizer, &digest_stat->m_stat);

  PFS_histogram *histogram = &digest_stat->m_histogram;

  ulong index;
  ulonglong count_star = 0;

  for (index = 0; index < NUMBER_OF_BUCKETS; index++) {
    count_star += histogram->read_bucket(index);
  }

  if (count_star == 0) {
    m_row.m_p95 = 0;
    m_row.m_p99 = 0;
    m_row.m_p999 = 0;
  } else {
    const ulonglong count_95 = ((count_star * 95) + 99) / 100;
    const ulonglong count_99 = ((count_star * 99) + 99) / 100;
    const ulonglong count_999 = ((count_star * 999) + 999) / 1000;

    assert(count_95 != 0);
    assert(count_95 <= count_star);
    assert(count_99 != 0);
    assert(count_99 <= count_star);
    assert(count_999 != 0);
    assert(count_999 <= count_star);

    ulong index_95 = 0;
    ulong index_99 = 0;
    ulong index_999 = 0;
    bool index_95_set = false;
    bool index_99_set = false;
    bool index_999_set = false;
    ulonglong count = 0;

    for (index = 0; index < NUMBER_OF_BUCKETS; index++) {
      count += histogram->read_bucket(index);

      if ((count >= count_95) && !index_95_set) {
        index_95 = index;
        index_95_set = true;
      }

      if ((count >= count_99) && !index_99_set) {
        index_99 = index;
        index_99_set = true;
      }

      if ((count >= count_999) && !index_999_set) {
        index_999 = index;
        index_999_set = true;
      }
    }

    m_row.m_p95 = g_histogram_pico_timers.m_bucket_timer[index_95 + 1];
    m_row.m_p99 = g_histogram_pico_timers.m_bucket_timer[index_99 + 1];
    m_row.m_p999 = g_histogram_pico_timers.m_bucket_timer[index_999 + 1];
  }

  /* Format the query sample sqltext string for output. */
  format_sqltext(digest_stat->m_query_sample,
                 digest_stat->m_query_sample_length,
                 get_charset(digest_stat->m_query_sample_cs_number, MYF(0)),
                 digest_stat->m_query_sample_truncated, m_row.m_query_sample);

  m_row.m_query_sample_seen = digest_stat->m_query_sample_seen;
  m_row.m_query_sample_timer_wait =
      m_normalizer->wait_to_pico(digest_stat->m_query_sample_timer_wait);
  return 0;
}

int table_esms_by_digest::read_row_values(TABLE *table, unsigned char *buf,
                                          Field **fields, bool read_all) {
  Field *f;

  /*
    Set the null bits. It indicates how many fields could be null
    in the table.
  */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* SCHEMA_NAME */
        case 1: /* DIGEST */
        case 2: /* DIGEST_TEXT */
          m_row.m_digest.set_field(f->field_index(), f);
          break;
        case 31: /* FIRST_SEEN */
          set_field_timestamp(f, m_row.m_first_seen);
          break;
        case 32: /* LAST_SEEN */
          set_field_timestamp(f, m_row.m_last_seen);
          break;
        case 33: /* QUANTILE_95 */
          set_field_ulonglong(f, m_row.m_p95);
          break;
        case 34: /* QUANTILE_99 */
          set_field_ulonglong(f, m_row.m_p99);
          break;
        case 35: /* QUANTILE_999 */
          set_field_ulonglong(f, m_row.m_p999);
          break;
        case 36: /* QUERY_SAMPLE_TEXT */
          if (m_row.m_query_sample.length())
            set_field_text(f, m_row.m_query_sample.ptr(),
                           m_row.m_query_sample.length(),
                           m_row.m_query_sample.charset());
          else {
            f->set_null();
          }
          break;
        case 37: /* QUERY_SAMPLE_SEEN */
          set_field_timestamp(f, m_row.m_query_sample_seen);
          break;
        case 38: /* QUERY_SAMPLE_TIMER_WAIT */
          set_field_ulonglong(f, m_row.m_query_sample_timer_wait);
          break;
        default: /* 3, ... COUNT/SUM/MIN/AVG/MAX */
          m_row.m_stat.set_field(f->field_index() - 3, f);
          break;
      }
    }
  }

  return 0;
}
