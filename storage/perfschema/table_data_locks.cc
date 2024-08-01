/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_data_locks.cc
  Table DATA_LOCKS (implementation).
*/

#include "storage/perfschema/table_data_locks.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
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

THR_LOCK table_data_locks::m_table_lock;

Plugin_table table_data_locks::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "data_locks",
    /* Definition */
    "  ENGINE VARCHAR(32) not null,\n"
    "  ENGINE_LOCK_ID VARCHAR(128) not null,\n"
    "  ENGINE_TRANSACTION_ID BIGINT unsigned,\n"
    "  THREAD_ID BIGINT unsigned,\n"
    "  EVENT_ID BIGINT unsigned,\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(64),\n"
    "  PARTITION_NAME VARCHAR(64),\n"
    "  SUBPARTITION_NAME VARCHAR(64),\n"
    "  INDEX_NAME VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  LOCK_TYPE VARCHAR(32) not null,\n"
    "  LOCK_MODE VARCHAR(32) not null,\n"
    "  LOCK_STATUS VARCHAR(32) not null,\n"
    "  LOCK_DATA VARCHAR(8192) CHARACTER SET utf8mb4,\n"
    "  PRIMARY KEY (ENGINE_LOCK_ID, ENGINE) USING HASH,\n"
    "  KEY (ENGINE_TRANSACTION_ID, ENGINE) USING HASH,\n"
    "  KEY (THREAD_ID, EVENT_ID) USING HASH,\n"
    "  KEY (OBJECT_SCHEMA, OBJECT_NAME, PARTITION_NAME,\n"
    "       SUBPARTITION_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_data_locks::m_share = {
    &pfs_readonly_acl,
    table_data_locks::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_data_locks::get_row_count,
    sizeof(pk_pos_t),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_data_locks::create(PFS_engine_table_share *) {
  return new table_data_locks();
}

ha_rows table_data_locks::get_row_count() {
  // FIXME
  return 99999;
}

table_data_locks::table_data_locks()
    : PFS_engine_table(&m_share, &m_pk_pos),
      m_row(nullptr),
      m_opened_pk(nullptr),
      m_opened_index(nullptr) {
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++) {
    m_iterator[i] = nullptr;
  }
}

void table_data_locks::destroy_iterators() {
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++) {
    if (m_iterator[i] != nullptr) {
      g_data_lock_inspector[i]->destroy_data_lock_iterator(m_iterator[i]);
      m_iterator[i] = nullptr;
    }
  }
}

table_data_locks::~table_data_locks() { destroy_iterators(); }

void table_data_locks::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
  m_pk_pos.reset();
  m_container.clear();
  destroy_iterators();
}

int table_data_locks::rnd_next() {
  row_data_lock *data;

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_engine();
       m_pos.next_engine()) {
    const unsigned int index = m_pos.m_index_1;

    if (m_iterator[index] == nullptr) {
      if (g_data_lock_inspector[index] == nullptr) {
        continue;
      }

      m_iterator[index] =
          g_data_lock_inspector[index]->create_data_lock_iterator();

      if (m_iterator[index] == nullptr) {
        continue;
      }
    }

    bool iterator_done = false;
    PSI_engine_data_lock_iterator *it = m_iterator[index];

    for (;;) {
      data = m_container.get_row(m_pos.m_index_2);
      if (data != nullptr) {
        m_row = data;
        m_next_pos.set_after(&m_pos);
        m_pk_pos.set(&m_row->m_hidden_pk);
        return 0;
      }

      if (iterator_done) {
        break;
      }

      m_container.shrink();
      /*
        TODO: avoid requesting column LOCK_DATA if not used.
      */

      /*
        The implementation of PSI_engine_data_lock_iterator::scan(),
        inside a storage engine, is expected to:
        - (1) not report all the data at once,
        - (2) implement re-startable scans internally,
        - (3) report a bounded number of rows per scan (1).

        This is to allow allocating only a bounded amount of memory
        in the data container, to cap the peak memory consumption
        of the container.

        TODO: Innodb_data_lock_iterator::scan()
        does not satisfy (3) currently.
      */

      iterator_done = it->scan(&m_container, true);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_data_locks::rnd_pos(const void *pos) {
  row_data_lock *data;

  set_position(pos);

  /*
    TODO: Multiple engine support.
    Find the proper engine based on column ENGINE.
  */
  static_assert(COUNT_DATA_LOCK_ENGINES == 1,
                "We don't support multiple engines yet.");
  constexpr unsigned int index = 0;

  if (m_iterator[index] == nullptr) {
    if (g_data_lock_inspector[index] == nullptr) {
      return HA_ERR_RECORD_DELETED;
    }

    m_iterator[index] =
        g_data_lock_inspector[index]->create_data_lock_iterator();

    if (m_iterator[index] == nullptr) {
      return HA_ERR_RECORD_DELETED;
    }
  }

  PSI_engine_data_lock_iterator *it = m_iterator[index];

  m_container.clear();

  /*
    TODO: avoid requesting column LOCK_DATA if not used.
  */
  it->fetch(&m_container, m_pk_pos.str(), m_pk_pos.length(), true);
  data = m_container.get_row(0);
  if (data != nullptr) {
    m_row = data;
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

int table_data_locks::index_init(uint idx, bool) {
  PFS_pk_data_locks *pk = nullptr;
  PFS_index_data_locks *index = nullptr;

  switch (idx) {
    case 0:
      pk = PFS_NEW(PFS_pk_data_locks);
      index = pk;
      break;
    case 1:
      index = PFS_NEW(PFS_index_data_locks_by_transaction_id);
      break;
    case 2:
      index = PFS_NEW(PFS_index_data_locks_by_thread_id);
      break;
    case 3:
      index = PFS_NEW(PFS_index_data_locks_by_object);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_pk = pk;
  m_opened_index = index;
  m_index = index;

  m_container.set_filter(m_opened_index);
  return 0;
}

int table_data_locks::index_next() {
  int status;

  if (m_opened_pk != nullptr) {
    pk_pos_data_lock *position = m_opened_pk->get_pk();
    /*
     * In the ideal case when:
     * - the opened index is the PRIMARY KEY
     * - the keypart field ENGINE_LOCK_ID is provided
     * - the index fetch is an exact match HA_READ_KEY_EXACT
     * then we can inspect the ENGINE_LOCK_ID value,
     * and perform a PSI_engine_data_lock_iterator::fetch()
     * in the underlying storage engine.
     *
     * Evaluating the condition in the second part
     * of the primary key, ENGINE, will be done as
     * an index condition pushdown when adding rows
     * to the container, filtered by
     * PFS_pk_data_locks::match_engine().
     */
    if (position != nullptr) {
      if (m_opened_pk->m_key_fetch_count == 0) {
        status = rnd_pos(position);
        if (status != 0) {
          status = HA_ERR_KEY_NOT_FOUND;
        }
      } else {
        status = HA_ERR_KEY_NOT_FOUND;
      }

      m_opened_pk->m_key_fetch_count++;
      return status;
    }
  }

  /*
   * For every other cases:
   * - index is the PRIMARY KEY, but ENGINE_LOCK_ID is not available
   *   (not possible in practice, the HASH index will not be used then)
   * - index is not the PRIMARY KEY
   * we execute a scan, with filtering done as an index condition pushdown,
   * attached to the data container.
   */
  status = rnd_next();

  return status;
}

int table_data_locks::read_row_values(TABLE *table, unsigned char *buf,
                                      Field **fields, bool read_all) {
  Field *f;

  if (unlikely(m_row == nullptr)) {
    return HA_ERR_RECORD_DELETED;
  }

  /* Set the null bits */
  assert(table->s->null_bytes == 2);
  buf[0] = 0;
  buf[1] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* ENGINE */
          set_field_varchar_utf8mb4(f, m_row->m_engine);
          break;
        case 1: /* ENGINE_LOCK_ID */
          set_field_varchar_utf8mb4(f, m_row->m_hidden_pk.str(),
                                    m_row->m_hidden_pk.length());
          break;
        case 2: /* ENGINE_TRANSACTION_ID */
          if (m_row->m_transaction_id != 0) {
            set_field_ulonglong(f, m_row->m_transaction_id);
          } else {
            f->set_null();
          }
          break;
        case 3: /* THREAD_ID */
          if (m_row->m_thread_id != 0) {
            set_field_ulonglong(f, m_row->m_thread_id);
          } else {
            f->set_null();
          }
          break;
        case 4: /* EVENT_ID */
          if (m_row->m_event_id != 0) {
            set_field_ulonglong(f, m_row->m_event_id);
          } else {
            f->set_null();
          }
          break;
        case 5: /* OBJECT_SCHEMA */
          m_row->m_index_row.set_nullable_field(1, f);
          break;
        case 6: /* OBJECT_NAME  */
          m_row->m_index_row.set_nullable_field(2, f);
          break;
        case 7: /* PARTITION_NAME */
          if (m_row->m_partition_name_length > 0) {
            set_field_varchar_utf8mb4(f, m_row->m_partition_name,
                                      m_row->m_partition_name_length);
          } else {
            f->set_null();
          }
          break;
        case 8: /* SUBPARTITION_NAME */
          if (m_row->m_sub_partition_name_length > 0) {
            set_field_varchar_utf8mb4(f, m_row->m_sub_partition_name,
                                      m_row->m_sub_partition_name_length);
          } else {
            f->set_null();
          }
          break;
        case 9: /* INDEX_NAME */
          m_row->m_index_row.set_nullable_field(3, f);
          break;
        case 10: /* OBJECT_INSTANCE_BEGIN */
          set_field_ulonglong(f, (intptr)m_row->m_identity);
          break;
        case 11: /* LOCK_TYPE */
          set_field_varchar_utf8mb4(f, m_row->m_lock_type);
          break;
        case 12: /* LOCK_MODE */
          set_field_varchar_utf8mb4(f, m_row->m_lock_mode);
          break;
        case 13: /* LOCK_STATUS */
          set_field_varchar_utf8mb4(f, m_row->m_lock_status);
          break;
        case 14: /* LOCK_DATA */
          if (m_row->m_lock_data != nullptr) {
            set_field_varchar_utf8mb4(f, m_row->m_lock_data);
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
