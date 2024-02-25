/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_setup_threads.cc
  Table SETUP_THREADS (implementation).
*/

#include "storage/perfschema/table_setup_threads.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_setup_threads::m_table_lock;

Plugin_table table_setup_threads::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "setup_threads",
    /* Definition */
    "  NAME VARCHAR(128) not null,\n"
    "  ENABLED ENUM ('YES', 'NO') not null,\n"
    "  HISTORY ENUM ('YES', 'NO') not null,\n"
    "  PROPERTIES SET('singleton', 'user') not null,\n"
    "  VOLATILITY int not null,\n"
    "  DOCUMENTATION LONGTEXT,\n"
    "  PRIMARY KEY (NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_setup_threads::m_share = {
    &pfs_updatable_acl,
    table_setup_threads::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_setup_threads::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_setup_threads::match(PFS_instr_class *klass) {
  if (m_fields >= 1) {
    return m_key.match(klass);
  }

  return true;
}

PFS_engine_table *table_setup_threads::create(PFS_engine_table_share *) {
  return new table_setup_threads();
}

ha_rows table_setup_threads::get_row_count() { return thread_class_max; }

table_setup_threads::table_setup_threads()
    : PFS_engine_table(&m_share, &m_pos), m_pos(1), m_next_pos(1) {}

void table_setup_threads::reset_position() {
  m_pos.m_index = 1;
  m_next_pos.m_index = 1;
}

int table_setup_threads::rnd_next() {
  PFS_thread_class *instr_class = nullptr;

  /* Do not advertise threads when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos);; m_pos.next()) {
    instr_class = find_thread_class(m_pos.m_index);

    if (instr_class) {
      m_next_pos.set_after(&m_pos);
      return make_row(instr_class);
    }

    return HA_ERR_END_OF_FILE;
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_threads::rnd_pos(const void *pos) {
  PFS_thread_class *instr_class = nullptr;

  /* Do not advertise threads when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);

  instr_class = find_thread_class(m_pos.m_index);

  if (instr_class) {
    return make_row(instr_class);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_setup_threads::index_init(uint idx [[maybe_unused]], bool) {
  PFS_index_setup_threads *result;

  assert(idx == 0);
  result = PFS_NEW(PFS_index_setup_threads);

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_setup_threads::index_next() {
  PFS_thread_class *instr_class = nullptr;

  /* Do not advertise threads when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos);; m_pos.next()) {
    instr_class = find_thread_class(m_pos.m_index);

    if (instr_class) {
      if (m_opened_index->match(instr_class)) {
        if (!make_row(instr_class)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    } else {
      return HA_ERR_END_OF_FILE;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_threads::make_row(PFS_thread_class *klass) {
  m_row.m_instr_class = klass;

  return 0;
}

int table_setup_threads::read_row_values(TABLE *table, unsigned char *buf,
                                         Field **fields, bool read_all) {
  Field *f;
  const char *doc;
  int properties;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  /*
    The row always exist, the instrument classes
    are static and never disappear.
  */

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* NAME */
          set_field_varchar_utf8mb4(f, m_row.m_instr_class->m_name.str(),
                                    m_row.m_instr_class->m_name.length());
          break;
        case 1: /* ENABLED */
          set_field_enum(f,
                         m_row.m_instr_class->m_enabled ? ENUM_YES : ENUM_NO);
          break;
        case 2: /* HISTORY */
          set_field_enum(f,
                         m_row.m_instr_class->m_history ? ENUM_YES : ENUM_NO);
          break;
        case 3: /* PROPERTIES */
          properties = 0;
          if (m_row.m_instr_class->is_singleton()) {
            properties |= THREAD_PROPERTIES_SET_SINGLETON;
          }
          if (m_row.m_instr_class->is_user()) {
            properties |= THREAD_PROPERTIES_SET_USER;
          }
          set_field_set(f, properties);
          break;
        case 4: /* VOLATILITY */
          set_field_ulong(f, m_row.m_instr_class->m_volatility);
          break;
        case 5: /* DOCUMENTATION */
          doc = m_row.m_instr_class->m_documentation;
          if (doc != nullptr) {
            set_field_blob(f, doc, strlen(doc));
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

int table_setup_threads::update_row_values(TABLE *table, const unsigned char *,
                                           unsigned char *, Field **fields) {
  Field *f;
  enum_yes_no value;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index())) {
      switch (f->field_index()) {
        case 1: /* ENABLED */
          value = (enum_yes_no)get_field_enum(f);
          m_row.m_instr_class->m_enabled = (value == ENUM_YES);
          break;
        case 2: /* HISTORY */
          value = (enum_yes_no)get_field_enum(f);
          m_row.m_instr_class->m_history = (value == ENUM_YES);
          break;
        default:
          return HA_ERR_WRONG_COMMAND;
      }
    }
  }

  /* No derived flag to update. */

  return 0;
}
