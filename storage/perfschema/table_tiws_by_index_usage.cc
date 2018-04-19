/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_tiws_by_index_usage.cc
  Table TABLE_IO_WAITS_SUMMARY_BY_INDEX_USAGE (implementation).
*/

#include "storage/perfschema/table_tiws_by_index_usage.h"

#include "my_config.h"

#include <stddef.h>

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_tiws_by_index_usage::m_table_lock;

Plugin_table table_tiws_by_index_usage::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "table_io_waits_summary_by_index_usage",
    /* Definition */
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(64),\n"
    "  INDEX_NAME VARCHAR(64),\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  COUNT_READ BIGINT unsigned not null,\n"
    "  SUM_TIMER_READ BIGINT unsigned not null,\n"
    "  MIN_TIMER_READ BIGINT unsigned not null,\n"
    "  AVG_TIMER_READ BIGINT unsigned not null,\n"
    "  MAX_TIMER_READ BIGINT unsigned not null,\n"
    "  COUNT_WRITE BIGINT unsigned not null,\n"
    "  SUM_TIMER_WRITE BIGINT unsigned not null,\n"
    "  MIN_TIMER_WRITE BIGINT unsigned not null,\n"
    "  AVG_TIMER_WRITE BIGINT unsigned not null,\n"
    "  MAX_TIMER_WRITE BIGINT unsigned not null,\n"
    "  COUNT_FETCH BIGINT unsigned not null,\n"
    "  SUM_TIMER_FETCH BIGINT unsigned not null,\n"
    "  MIN_TIMER_FETCH BIGINT unsigned not null,\n"
    "  AVG_TIMER_FETCH BIGINT unsigned not null,\n"
    "  MAX_TIMER_FETCH BIGINT unsigned not null,\n"
    "  COUNT_INSERT BIGINT unsigned not null,\n"
    "  SUM_TIMER_INSERT BIGINT unsigned not null,\n"
    "  MIN_TIMER_INSERT BIGINT unsigned not null,\n"
    "  AVG_TIMER_INSERT BIGINT unsigned not null,\n"
    "  MAX_TIMER_INSERT BIGINT unsigned not null,\n"
    "  COUNT_UPDATE BIGINT unsigned not null,\n"
    "  SUM_TIMER_UPDATE BIGINT unsigned not null,\n"
    "  MIN_TIMER_UPDATE BIGINT unsigned not null,\n"
    "  AVG_TIMER_UPDATE BIGINT unsigned not null,\n"
    "  MAX_TIMER_UPDATE BIGINT unsigned not null,\n"
    "  COUNT_DELETE BIGINT unsigned not null,\n"
    "  SUM_TIMER_DELETE BIGINT unsigned not null,\n"
    "  MIN_TIMER_DELETE BIGINT unsigned not null,\n"
    "  AVG_TIMER_DELETE BIGINT unsigned not null,\n"
    "  MAX_TIMER_DELETE BIGINT unsigned not null,\n"
    "  UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
    "                       INDEX_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_tiws_by_index_usage::m_share = {
    &pfs_truncatable_acl,
    table_tiws_by_index_usage::create,
    NULL, /* write_row */
    table_tiws_by_index_usage::delete_all_rows,
    table_tiws_by_index_usage::get_row_count,
    sizeof(pos_tiws_by_index_usage),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_tiws_by_index_usage::match(PFS_table_share *pfs) {
  PFS_object_row object_row;

  if (object_row.make_row(pfs)) {
    return false;
  }

  if (m_fields >= 1) {
    if (!m_key_1.match(&object_row)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(&object_row)) {
      return false;
    }
  }

  if (m_fields >= 3) {
    if (!m_key_3.match(&object_row)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_tiws_by_index_usage::match(PFS_table_share *share, uint index) {
  PFS_index_row index_row;
  PFS_table_share_index *pfs_index = share->find_index_stat(index);

  if (index_row.make_index_name(pfs_index,
                                index)) /* andles pfs_index == NULL */
  {
    return false;
  }

  if (m_fields >= 4) {
    if (!m_key_4.match(&index_row)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_tiws_by_index_usage::create(PFS_engine_table_share *) {
  return new table_tiws_by_index_usage();
}

int table_tiws_by_index_usage::delete_all_rows(void) {
  reset_table_io_waits_by_table_handle();
  reset_table_io_waits_by_table();
  return 0;
}

ha_rows table_tiws_by_index_usage::get_row_count(void) {
  return global_table_share_index_container.get_row_count();
}

table_tiws_by_index_usage::table_tiws_by_index_usage()
    : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos() {
  m_normalizer = time_normalizer::get_wait();
}

void table_tiws_by_index_usage::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_tiws_by_index_usage::rnd_init(bool) { return 0; }

int table_tiws_by_index_usage::rnd_next(void) {
  PFS_table_share *table_share;
  bool has_more_table = true;

  for (m_pos.set_at(&m_next_pos); has_more_table; m_pos.next_table()) {
    table_share =
        global_table_share_container.get(m_pos.m_index_1, &has_more_table);
    if (table_share != NULL) {
      uint safe_key_count = sanitize_index_count(table_share->m_key_count);
      if (m_pos.m_index_2 < safe_key_count) {
        m_next_pos.set_after(&m_pos);
        return make_row(table_share, m_pos.m_index_2);
      }

      if (m_pos.m_index_2 <= MAX_INDEXES) {
        m_pos.m_index_2 = MAX_INDEXES;
        m_next_pos.set_after(&m_pos);
        return make_row(table_share, m_pos.m_index_2);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_tiws_by_index_usage::rnd_pos(const void *pos) {
  PFS_table_share *table_share;

  set_position(pos);

  table_share = global_table_share_container.get(m_pos.m_index_1);
  if (table_share != NULL) {
    uint safe_key_count = sanitize_index_count(table_share->m_key_count);
    if (m_pos.m_index_2 < safe_key_count) {
      return make_row(table_share, m_pos.m_index_2);
    }
    if (m_pos.m_index_2 == MAX_INDEXES) {
      return make_row(table_share, m_pos.m_index_2);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_tiws_by_index_usage::index_init(uint idx MY_ATTRIBUTE((unused)),
                                          bool) {
  PFS_index_tiws_by_index_usage *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_tiws_by_index_usage);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_tiws_by_index_usage::index_next(void) {
  PFS_table_share *table_share;
  bool has_more_table = true;

  for (m_pos.set_at(&m_next_pos); has_more_table; m_pos.next_table()) {
    table_share =
        global_table_share_container.get(m_pos.m_index_1, &has_more_table);
    if (table_share != NULL) {
      if (m_opened_index->match(table_share)) {
        uint safe_key_count = sanitize_index_count(table_share->m_key_count);
        for (; m_pos.m_index_2 <= MAX_INDEXES; m_pos.m_index_2++) {
          if (m_opened_index->match(table_share, m_pos.m_index_2)) {
            if (m_pos.m_index_2 < safe_key_count) {
              if (!make_row(table_share, m_pos.m_index_2)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            } else {
              if (m_pos.m_index_2 <= MAX_INDEXES) {
                m_pos.m_index_2 = MAX_INDEXES;
                if (!make_row(table_share, m_pos.m_index_2)) {
                  m_next_pos.set_after(&m_pos);
                  return 0;
                }
              }
            }
          }
        } /* next index */
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_tiws_by_index_usage::make_row(PFS_table_share *pfs_share,
                                        uint index) {
  PFS_table_share_index *pfs_index;
  pfs_optimistic_state lock;

  DBUG_ASSERT(index <= MAX_INDEXES);

  pfs_share->m_lock.begin_optimistic_lock(&lock);

  PFS_index_io_stat_visitor visitor;
  PFS_object_iterator::visit_table_indexes(pfs_share, index, &visitor);

  if (!visitor.m_stat.m_has_data) {
    pfs_index = pfs_share->find_index_stat(index);
    if (pfs_index == NULL) {
      return HA_ERR_RECORD_DELETED;
    }
  } else {
    pfs_index = pfs_share->find_index_stat(index);
  }

  if (m_row.m_index.make_row(pfs_share, pfs_index, index)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!pfs_share->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);

  return 0;
}

int table_tiws_by_index_usage::read_row_values(TABLE *table, unsigned char *buf,
                                               Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* OBJECT_TYPE */
        case 1: /* SCHEMA_NAME */
        case 2: /* OBJECT_NAME */
        case 3: /* INDEX_NAME */
          m_row.m_index.set_field(f->field_index, f);
          break;
        case 4: /* COUNT_STAR */
          set_field_ulonglong(f, m_row.m_stat.m_all.m_count);
          break;
        case 5: /* SUM */
          set_field_ulonglong(f, m_row.m_stat.m_all.m_sum);
          break;
        case 6: /* MIN */
          set_field_ulonglong(f, m_row.m_stat.m_all.m_min);
          break;
        case 7: /* AVG */
          set_field_ulonglong(f, m_row.m_stat.m_all.m_avg);
          break;
        case 8: /* MAX */
          set_field_ulonglong(f, m_row.m_stat.m_all.m_max);
          break;
        case 9: /* COUNT_READ */
          set_field_ulonglong(f, m_row.m_stat.m_all_read.m_count);
          break;
        case 10: /* SUM_READ */
          set_field_ulonglong(f, m_row.m_stat.m_all_read.m_sum);
          break;
        case 11: /* MIN_READ */
          set_field_ulonglong(f, m_row.m_stat.m_all_read.m_min);
          break;
        case 12: /* AVG_READ */
          set_field_ulonglong(f, m_row.m_stat.m_all_read.m_avg);
          break;
        case 13: /* MAX_READ */
          set_field_ulonglong(f, m_row.m_stat.m_all_read.m_max);
          break;
        case 14: /* COUNT_WRITE */
          set_field_ulonglong(f, m_row.m_stat.m_all_write.m_count);
          break;
        case 15: /* SUM_WRITE */
          set_field_ulonglong(f, m_row.m_stat.m_all_write.m_sum);
          break;
        case 16: /* MIN_WRITE */
          set_field_ulonglong(f, m_row.m_stat.m_all_write.m_min);
          break;
        case 17: /* AVG_WRITE */
          set_field_ulonglong(f, m_row.m_stat.m_all_write.m_avg);
          break;
        case 18: /* MAX_WRITE */
          set_field_ulonglong(f, m_row.m_stat.m_all_write.m_max);
          break;
        case 19: /* COUNT_FETCH */
          set_field_ulonglong(f, m_row.m_stat.m_fetch.m_count);
          break;
        case 20: /* SUM_FETCH */
          set_field_ulonglong(f, m_row.m_stat.m_fetch.m_sum);
          break;
        case 21: /* MIN_FETCH */
          set_field_ulonglong(f, m_row.m_stat.m_fetch.m_min);
          break;
        case 22: /* AVG_FETCH */
          set_field_ulonglong(f, m_row.m_stat.m_fetch.m_avg);
          break;
        case 23: /* MAX_FETCH */
          set_field_ulonglong(f, m_row.m_stat.m_fetch.m_max);
          break;
        case 24: /* COUNT_INSERT */
          set_field_ulonglong(f, m_row.m_stat.m_insert.m_count);
          break;
        case 25: /* SUM_INSERT */
          set_field_ulonglong(f, m_row.m_stat.m_insert.m_sum);
          break;
        case 26: /* MIN_INSERT */
          set_field_ulonglong(f, m_row.m_stat.m_insert.m_min);
          break;
        case 27: /* AVG_INSERT */
          set_field_ulonglong(f, m_row.m_stat.m_insert.m_avg);
          break;
        case 28: /* MAX_INSERT */
          set_field_ulonglong(f, m_row.m_stat.m_insert.m_max);
          break;
        case 29: /* COUNT_UPDATE */
          set_field_ulonglong(f, m_row.m_stat.m_update.m_count);
          break;
        case 30: /* SUM_UPDATE */
          set_field_ulonglong(f, m_row.m_stat.m_update.m_sum);
          break;
        case 31: /* MIN_UPDATE */
          set_field_ulonglong(f, m_row.m_stat.m_update.m_min);
          break;
        case 32: /* AVG_UPDATE */
          set_field_ulonglong(f, m_row.m_stat.m_update.m_avg);
          break;
        case 33: /* MAX_UPDATE */
          set_field_ulonglong(f, m_row.m_stat.m_update.m_max);
          break;
        case 34: /* COUNT_DELETE */
          set_field_ulonglong(f, m_row.m_stat.m_delete.m_count);
          break;
        case 35: /* SUM_DELETE */
          set_field_ulonglong(f, m_row.m_stat.m_delete.m_sum);
          break;
        case 36: /* MIN_DELETE */
          set_field_ulonglong(f, m_row.m_stat.m_delete.m_min);
          break;
        case 37: /* AVG_DELETE */
          set_field_ulonglong(f, m_row.m_stat.m_delete.m_avg);
          break;
        case 38: /* MAX_DELETE */
          set_field_ulonglong(f, m_row.m_stat.m_delete.m_max);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
