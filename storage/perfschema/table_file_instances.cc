/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_file_instances.cc
  Table FILE_INSTANCES (implementation).
*/

#include "storage/perfschema/table_file_instances.h"

#include <assert.h>
#include <stddef.h>

#include "my_compiler.h"

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"

THR_LOCK table_file_instances::m_table_lock;

Plugin_table table_file_instances::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "file_instances",
    /* Definition */
    "  FILE_NAME VARCHAR(512) not null,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  OPEN_COUNT INTEGER unsigned not null,\n"
    "  PRIMARY KEY (FILE_NAME) USING HASH,\n"
    "  KEY (EVENT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_file_instances::m_share = {
    &pfs_readonly_acl,
    table_file_instances::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_file_instances::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_file_instances_by_file_name::match(const PFS_file *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_file_instances_by_event_name::match(const PFS_file *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_file_instances::create(PFS_engine_table_share *) {
  return new table_file_instances();
}

ha_rows table_file_instances::get_row_count() {
  return global_file_container.get_row_count();
}

table_file_instances::table_file_instances()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_file_instances::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_file_instances::rnd_next() {
  PFS_file *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_file_iterator it = global_file_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_file_instances::rnd_pos(const void *pos) {
  PFS_file *pfs;

  set_position(pos);

  pfs = global_file_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_file_instances::index_init(uint idx, bool) {
  PFS_index_file_instances *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_file_instances_by_file_name);
      break;
    case 1:
      result = PFS_NEW(PFS_index_file_instances_by_event_name);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_file_instances::index_next() {
  PFS_file *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_file_iterator it = global_file_container.iterate(m_pos.m_index);

  do {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != nullptr) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  } while (pfs != nullptr);

  return HA_ERR_END_OF_FILE;
}

int table_file_instances::make_row(PFS_file *pfs) {
  pfs_optimistic_state lock;
  PFS_file_class *safe_class;

  /* Protect this reader against a file delete */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_file_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_file_name = pfs->m_file_name;
  m_row.m_event_name = safe_class->m_name.str();
  m_row.m_event_name_length = safe_class->m_name.length();
  m_row.m_open_count = pfs->m_file_stat.m_open_count;

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_file_instances::read_row_values(TABLE *table, unsigned char *,
                                          Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* FILENAME */
          set_field_varchar_utf8mb4(f, m_row.m_file_name.ptr(),
                                    m_row.m_file_name.length());
          break;
        case 1: /* EVENT_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_event_name,
                                    m_row.m_event_name_length);
          break;
        case 2: /* OPEN_COUNT */
          set_field_ulong(f, m_row.m_open_count);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
