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
  @file storage/perfschema/table_os_global_by_type.cc
  Table OBJECTS_SUMMARY_GLOBAL_BY_TYPE (implementation).
*/

#include "storage/perfschema/table_os_global_by_type.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_os_global_by_type::m_table_lock;

Plugin_table table_os_global_by_type::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "objects_summary_global_by_type",
    /* Definition */
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(64),\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA,\n"
    "                       OBJECT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_os_global_by_type::m_share = {
    &pfs_truncatable_acl,
    table_os_global_by_type::create,
    nullptr, /* write_row */
    table_os_global_by_type::delete_all_rows,
    table_os_global_by_type::get_row_count,
    sizeof(pos_os_global_by_type),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_os_global_by_type::match(PFS_table_share *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(OBJECT_TYPE_TABLE)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }

  if (m_fields >= 3) {
    if (!m_key_3.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_os_global_by_type::match(PFS_program *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }

  if (m_fields >= 3) {
    if (!m_key_3.match(pfs)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_os_global_by_type::create(PFS_engine_table_share *) {
  return new table_os_global_by_type();
}

int table_os_global_by_type::delete_all_rows() {
  reset_table_waits_by_table_handle();
  reset_table_waits_by_table();
  return 0;
}

ha_rows table_os_global_by_type::get_row_count() {
  return global_table_share_container.get_row_count() +
         global_program_container.get_row_count();
}

table_os_global_by_type::table_os_global_by_type()
    : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos() {
  // FIXME, verify
  m_normalizer = time_normalizer::get_wait();
}

void table_os_global_by_type::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

int table_os_global_by_type::rnd_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    switch (m_pos.m_index_1) {
      case pos_os_global_by_type::VIEW_TABLE: {
        PFS_table_share *table_share;
        bool has_more_share = true;

        for (; has_more_share; m_pos.m_index_2++) {
          table_share = global_table_share_container.get(m_pos.m_index_2,
                                                         &has_more_share);
          if (table_share != nullptr) {
            make_table_row(table_share);
            m_next_pos.set_after(&m_pos);
            return 0;
          }
        }
      } break;
      case pos_os_global_by_type::VIEW_PROGRAM: {
        PFS_program *pfs_program;
        bool has_more_program = true;

        for (; has_more_program; m_pos.m_index_2++) {
          pfs_program =
              global_program_container.get(m_pos.m_index_2, &has_more_program);
          if (pfs_program != nullptr) {
            make_program_row(pfs_program);
            m_next_pos.set_after(&m_pos);
            return 0;
          }
        }
      } break;
      default:
        break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_os_global_by_type::rnd_pos(const void *pos) {
  set_position(pos);

  switch (m_pos.m_index_1) {
    case pos_os_global_by_type::VIEW_TABLE: {
      PFS_table_share *table_share;
      table_share = global_table_share_container.get(m_pos.m_index_2);
      if (table_share != nullptr) {
        make_table_row(table_share);
        return 0;
      }
    } break;
    case pos_os_global_by_type::VIEW_PROGRAM: {
      PFS_program *pfs_program;
      pfs_program = global_program_container.get(m_pos.m_index_2);
      if (pfs_program != nullptr) {
        make_program_row(pfs_program);
        return 0;
      }
    } break;
    default:
      break;
  }

  return HA_ERR_RECORD_DELETED;
}

int table_os_global_by_type::index_init(uint idx [[maybe_unused]], bool) {
  PFS_index_os_global_by_type *result;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_os_global_by_type);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_os_global_by_type::index_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    switch (m_pos.m_index_1) {
      case pos_os_global_by_type::VIEW_TABLE: {
        PFS_table_share *table_share;
        bool has_more_share = true;

        for (; has_more_share; m_pos.m_index_2++) {
          table_share = global_table_share_container.get(m_pos.m_index_2,
                                                         &has_more_share);
          if (table_share != nullptr) {
            if (m_opened_index->match(table_share)) {
              make_table_row(table_share);
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
        }
      } break;
      case pos_os_global_by_type::VIEW_PROGRAM: {
        PFS_program *pfs_program;
        bool has_more_program = true;

        for (; has_more_program; m_pos.m_index_2++) {
          pfs_program =
              global_program_container.get(m_pos.m_index_2, &has_more_program);
          if (pfs_program != nullptr) {
            if (m_opened_index->match(pfs_program)) {
              make_program_row(pfs_program);
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
        }
      } break;
      default:
        break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_os_global_by_type::make_program_row(PFS_program *pfs_program) {
  pfs_optimistic_state lock;

  pfs_program->m_lock.begin_optimistic_lock(&lock);

  m_row.m_object.make_row(pfs_program);

  m_row.m_stat.set(m_normalizer, &pfs_program->m_sp_stat.m_timer1_stat);

  if (!pfs_program->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_os_global_by_type::make_table_row(PFS_table_share *share) {
  pfs_optimistic_state lock;
  PFS_single_stat cumulated_stat;
  uint safe_key_count;

  share->m_lock.begin_optimistic_lock(&lock);

  m_row.m_object.make_row(share);

  /* This is a dirty read, some thread can write data while we are reading it */
  safe_key_count = sanitize_index_count(share->m_key_count);

  share->sum(&cumulated_stat, safe_key_count);

  if (!share->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (share->get_refcount() > 0) {
    /* For all the table handles still opened ... */
    PFS_table_iterator it = global_table_container.iterate();
    PFS_table *table = it.scan_next();

    while (table != nullptr) {
      if (table->m_share == share) {
        /*
          If the opened table handle is for this table share,
          aggregate the table handle statistics.
        */
        table->m_table_stat.sum(&cumulated_stat, safe_key_count);
      }
      table = it.scan_next();
    }
  }

  m_row.m_stat.set(m_normalizer, &cumulated_stat);

  return 0;
}

int table_os_global_by_type::read_row_values(TABLE *table, unsigned char *buf,
                                             Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* OBJECT_TYPE */
          set_field_object_type(f, m_row.m_object.m_object_type);
          break;
        case 1: /* SCHEMA_NAME */
          set_nullable_field_schema_name(f, &m_row.m_object.m_schema_name);
          break;
        case 2: /* OBJECT_NAME */
          set_nullable_field_object_name(f, &m_row.m_object.m_object_name);
          break;
        case 3: /* COUNT */
          set_field_ulonglong(f, m_row.m_stat.m_count);
          break;
        case 4: /* SUM */
          set_field_ulonglong(f, m_row.m_stat.m_sum);
          break;
        case 5: /* MIN */
          set_field_ulonglong(f, m_row.m_stat.m_min);
          break;
        case 6: /* AVG */
          set_field_ulonglong(f, m_row.m_stat.m_avg);
          break;
        case 7: /* MAX */
          set_field_ulonglong(f, m_row.m_stat.m_max);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
