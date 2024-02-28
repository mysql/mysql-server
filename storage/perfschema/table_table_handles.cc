/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_table_handles.cc
  Table TABLE_TABLE_HANDLES (implementation).
*/

#include "storage/perfschema/table_table_handles.h"

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
#include "storage/perfschema/pfs_stat.h"

THR_LOCK table_table_handles::m_table_lock;

Plugin_table table_table_handles::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "table_handles",
    /* Definition */
    "  OBJECT_TYPE VARCHAR(64) not null,\n"
    "  OBJECT_SCHEMA VARCHAR(64) not null,\n"
    "  OBJECT_NAME VARCHAR(64) not null,\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  OWNER_THREAD_ID BIGINT unsigned,\n"
    "  OWNER_EVENT_ID BIGINT unsigned,\n"
    "  INTERNAL_LOCK VARCHAR(64),\n"
    "  EXTERNAL_LOCK VARCHAR(64),\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  KEY (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH,\n"
    "  KEY (OWNER_THREAD_ID, OWNER_EVENT_ID) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_table_handles::m_share = {
    &pfs_readonly_acl,
    table_table_handles::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_table_handles::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_table_handles_by_object::match(PFS_table *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(OBJECT_TYPE_TABLE)) {
      return false;
    }
  }

  const PFS_table_share *share = sanitize_table_share(pfs->m_share);
  if (share == nullptr) {
    return false;
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(share)) {
      return false;
    }
  }

  if (m_fields >= 3) {
    if (!m_key_3.match(share)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_table_handles_by_instance::match(PFS_table *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_table_handles_by_owner::match(PFS_table *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match_owner(pfs)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match_owner(pfs)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_table_handles::create(PFS_engine_table_share *) {
  return new table_table_handles();
}

ha_rows table_table_handles::get_row_count() {
  return global_table_container.get_row_count();
}

table_table_handles::table_table_handles()
    : PFS_engine_table(&m_share, &m_pos),
      m_pos(0),
      m_next_pos(0),
      m_opened_index(nullptr) {}

void table_table_handles::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_table_handles::rnd_init(bool) { return 0; }

int table_table_handles::rnd_next() {
  PFS_table *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_table_iterator it = global_table_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_table_handles::rnd_pos(const void *pos) {
  PFS_table *pfs;

  set_position(pos);

  pfs = global_table_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_table_handles::index_init(uint idx, bool) {
  PFS_index_table_handles *result = nullptr;

  switch (idx) {
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
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_table_handles::index_next() {
  PFS_table *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_table_iterator it = global_table_container.iterate(m_pos.m_index);

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

int table_table_handles::make_row(PFS_table *table) {
  pfs_optimistic_state lock;
  PFS_table_share *share;
  PFS_thread *thread;

  table->m_lock.begin_optimistic_lock(&lock);

  share = sanitize_table_share(table->m_share);
  if (share == nullptr) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_object.make_row(share)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_identity = table->m_identity;

  thread = sanitize_thread(table->m_thread_owner);
  if (thread != nullptr) {
    m_row.m_owner_thread_id = thread->m_thread_internal_id;
    m_row.m_owner_event_id = table->m_owner_event_id;
  } else {
    m_row.m_owner_thread_id = 0;
    m_row.m_owner_event_id = 0;
  }

  m_row.m_internal_lock = table->m_internal_lock;
  m_row.m_external_lock = table->m_external_lock;

  if (!table->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_table_handles::read_row_values(TABLE *table, unsigned char *buf,
                                         Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* OBJECT_TYPE */
        case 1: /* OBJECT_SCHEMA */
        case 2: /* OBJECT_NAME */
          m_row.m_object.set_field(f->field_index(), f);
          break;
        case 3: /* OBJECT_INSTANCE_BEGIN */
          set_field_ulonglong(f, (intptr)m_row.m_identity);
          break;
        case 4: /* OWNER_THREAD_ID */
          if (m_row.m_owner_thread_id != 0) {
            set_field_ulonglong(f, m_row.m_owner_thread_id);
          } else {
            f->set_null();
          }
          break;
        case 5: /* OWNER_EVENT_ID */
          if (m_row.m_owner_event_id != 0) {
            set_field_ulonglong(f, m_row.m_owner_event_id);
          } else {
            f->set_null();
          }
          break;
        case 6: /* INTERNAL_LOCK */
          set_field_lock_type(f, m_row.m_internal_lock);
          break;
        case 7: /* EXTERNAL_LOCK */
          set_field_lock_type(f, m_row.m_external_lock);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
