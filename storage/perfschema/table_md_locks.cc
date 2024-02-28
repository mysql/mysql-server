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
  @file storage/perfschema/table_md_locks.cc
  Table METADATA_LOCKS (implementation).
*/

#include "storage/perfschema/table_md_locks.h"

#include <assert.h>
#include <stddef.h>

#include "my_inttypes.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"

THR_LOCK table_metadata_locks::m_table_lock;

Plugin_table table_metadata_locks::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "metadata_locks",
    /* Definition */
    "  OBJECT_TYPE VARCHAR(64) not null,\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(64),\n"
    "  COLUMN_NAME VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  LOCK_TYPE VARCHAR(32) not null,\n"
    "  LOCK_DURATION VARCHAR(32) not null,\n"
    "  LOCK_STATUS VARCHAR(32) not null,\n"
    "  SOURCE VARCHAR(64),\n"
    "  OWNER_THREAD_ID BIGINT unsigned,\n"
    "  OWNER_EVENT_ID BIGINT unsigned,\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  KEY (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, COLUMN_NAME) USING HASH,\n"
    "  KEY (OWNER_THREAD_ID, OWNER_EVENT_ID) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_metadata_locks::m_share = {
    &pfs_readonly_acl,
    table_metadata_locks::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_metadata_locks::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_metadata_locks_by_instance::match(const PFS_metadata_lock *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_metadata_locks_by_object::match(const PFS_metadata_lock *pfs) {
  PFS_column_row object_row;

  if (object_row.make_row(&pfs->m_mdl_key)) {
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

  if (m_fields >= 4) {
    if (!m_key_4.match(&object_row)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_metadata_locks_by_owner::match(const PFS_metadata_lock *pfs) {
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

PFS_engine_table *table_metadata_locks::create(PFS_engine_table_share *) {
  return new table_metadata_locks();
}

ha_rows table_metadata_locks::get_row_count() {
  return global_mdl_container.get_row_count();
}

table_metadata_locks::table_metadata_locks()
    : PFS_engine_table(&m_share, &m_pos),
      m_pos(0),
      m_next_pos(0),
      m_opened_index(nullptr) {}

void table_metadata_locks::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_metadata_locks::rnd_next() {
  PFS_metadata_lock *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_mdl_iterator it = global_mdl_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_metadata_locks::rnd_pos(const void *pos) {
  PFS_metadata_lock *pfs;

  set_position(pos);

  pfs = global_mdl_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_metadata_locks::index_init(uint idx, bool) {
  PFS_index_metadata_locks *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_metadata_locks_by_instance);
      break;
    case 1:
      result = PFS_NEW(PFS_index_metadata_locks_by_object);
      break;
    case 2:
      result = PFS_NEW(PFS_index_metadata_locks_by_owner);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_metadata_locks::index_next() {
  PFS_metadata_lock *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_mdl_iterator it = global_mdl_container.iterate(m_pos.m_index);

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

int table_metadata_locks::make_row(PFS_metadata_lock *pfs) {
  pfs_optimistic_state lock;

  /* Protect this reader against a metadata lock destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_identity = pfs->m_identity;
  m_row.m_mdl_type = pfs->m_mdl_type;
  m_row.m_mdl_duration = pfs->m_mdl_duration;
  m_row.m_mdl_status = pfs->m_mdl_status;

  make_source_column(pfs->m_src_file, pfs->m_src_line, m_row.m_source,
                     sizeof(m_row.m_source), m_row.m_source_length);

  m_row.m_owner_thread_id = static_cast<ulong>(pfs->m_owner_thread_id);
  m_row.m_owner_event_id = static_cast<ulong>(pfs->m_owner_event_id);

  if (m_row.m_object.make_row(&pfs->m_mdl_key)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_metadata_locks::read_row_values(TABLE *table, unsigned char *buf,
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
        case 3: /* COLUMN_NAME */
          m_row.m_object.set_nullable_field(f->field_index(), f);
          break;
        case 4: /* OBJECT_INSTANCE */
          set_field_ulonglong(f, (intptr)m_row.m_identity);
          break;
        case 5: /* LOCK_TYPE */
          set_field_mdl_type(f, m_row.m_mdl_type);
          break;
        case 6: /* LOCK_DURATION */
          set_field_mdl_duration(f, m_row.m_mdl_duration);
          break;
        case 7: /* LOCK_STATUS */
          set_field_mdl_status(f, m_row.m_mdl_status);
          break;
        case 8: /* SOURCE */
          set_field_varchar_utf8mb4(f, m_row.m_source, m_row.m_source_length);
          break;
        case 9: /* OWNER_THREAD_ID */
          if (m_row.m_owner_thread_id != 0) {
            set_field_ulonglong(f, m_row.m_owner_thread_id);
          } else {
            f->set_null();
          }
          break;
        case 10: /* OWNER_EVENT_ID */
          if (m_row.m_owner_event_id != 0) {
            set_field_ulonglong(f, m_row.m_owner_event_id);
          } else {
            f->set_null();
          }
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
