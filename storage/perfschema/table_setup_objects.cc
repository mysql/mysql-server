/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_setup_objects.cc
  Table SETUP_OBJECTS (implementation).
*/

#include "storage/perfschema/table_setup_objects.h"

#include <assert.h>
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
#include "storage/perfschema/pfs_setup_object.h"
#include "storage/perfschema/table_helper.h"

THR_LOCK table_setup_objects::m_table_lock;

Plugin_table table_setup_objects::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "setup_objects",
    /* Definition */
    "  OBJECT_TYPE ENUM ('EVENT', 'FUNCTION', 'PROCEDURE', 'TABLE',\n"
    "                    'TRIGGER') not null default 'TABLE',\n"
    "  OBJECT_SCHEMA VARCHAR(64) default '%',\n"
    "  OBJECT_NAME VARCHAR(64) NOT null default '%',\n"
    "  ENABLED ENUM ('YES', 'NO') not null default 'YES',\n"
    "  TIMED ENUM ('YES', 'NO') not null default 'YES',\n"
    "  UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA,\n"
    "                       OBJECT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_setup_objects::m_share = {
    &pfs_editable_acl,
    table_setup_objects::create,
    table_setup_objects::write_row,
    table_setup_objects::delete_all_rows,
    table_setup_objects::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

static int update_derived_flags() {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  update_table_share_derived_flags(thread);
  update_program_share_derived_flags(thread);
  update_table_derived_flags();
  return 0;
}

bool PFS_index_setup_objects::match(PFS_setup_object *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs->m_key.m_object_type)) {
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

PFS_engine_table *table_setup_objects::create(PFS_engine_table_share *) {
  return new table_setup_objects();
}

int table_setup_objects::write_row(PFS_engine_table *, TABLE *table,
                                   unsigned char *, Field **fields) {
  int result;
  Field *f;
  enum_object_type object_type = OBJECT_TYPE_TABLE;
  String object_schema_data("%", 1, &my_charset_utf8mb4_bin);
  String object_name_data("%", 1, &my_charset_utf8mb4_bin);
  String *object_schema = &object_schema_data;
  String *object_name = &object_name_data;
  enum_yes_no enabled_value = ENUM_YES;
  enum_yes_no timed_value = ENUM_YES;
  bool enabled = true;
  bool timed = true;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* OBJECT_TYPE */
          object_type = (enum_object_type)get_field_enum(f);
          break;
        case 1: /* OBJECT_SCHEMA */
          object_schema = get_field_varchar_utf8mb4(f, &object_schema_data);
          break;
        case 2: /* OBJECT_NAME */
          object_name = get_field_varchar_utf8mb4(f, &object_name_data);
          break;
        case 3: /* ENABLED */
          enabled_value = (enum_yes_no)get_field_enum(f);
          break;
        case 4: /* TIMED */
          timed_value = (enum_yes_no)get_field_enum(f);
          break;
        default:
          assert(false);
      }
    }
  }

  /* Reject illegal enum values in OBJECT_TYPE */
  if (object_type < FIRST_OBJECT_TYPE || object_type > LAST_OBJECT_TYPE ||
      object_type == OBJECT_TYPE_TEMPORARY_TABLE) {
    return HA_ERR_NO_REFERENCED_ROW;
  }

  /* Reject illegal enum values in ENABLED */
  if ((enabled_value != ENUM_YES) && (enabled_value != ENUM_NO)) {
    return HA_ERR_NO_REFERENCED_ROW;
  }

  /* Reject illegal enum values in TIMED */
  if ((timed_value != ENUM_YES) && (timed_value != ENUM_NO)) {
    return HA_ERR_NO_REFERENCED_ROW;
  }

  enabled = (enabled_value == ENUM_YES) ? true : false;
  timed = (timed_value == ENUM_YES) ? true : false;

  PFS_schema_name schema_value;
  PFS_object_name object_value;
  schema_value.set(object_schema->ptr(), object_schema->length());

  /*
    Collation rules for PFS_object_name depends on
    what the object actually is.
  */
  if (object_type == OBJECT_TYPE_TABLE) {
    object_value.set_as_table(object_name->ptr(), object_name->length());
  } else {
    object_value.set_as_routine(object_name->ptr(), object_name->length());
  }

  result = insert_setup_object(object_type, &schema_value, &object_value,
                               enabled, timed);
  if (result == 0) {
    result = update_derived_flags();
  }
  return result;
}

int table_setup_objects::delete_all_rows(void) {
  int result = reset_setup_object();
  if (result == 0) {
    result = update_derived_flags();
  }
  return result;
}

ha_rows table_setup_objects::get_row_count(void) {
  return global_setup_object_container.get_row_count();
}

table_setup_objects::table_setup_objects()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_setup_objects::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_setup_objects::rnd_next(void) {
  PFS_setup_object *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_setup_object_iterator it =
      global_setup_object_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_objects::rnd_pos(const void *pos) {
  PFS_setup_object *pfs;

  set_position(pos);

  pfs = global_setup_object_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_setup_objects::index_init(uint idx [[maybe_unused]], bool) {
  PFS_index_setup_objects *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_setup_objects);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_setup_objects::index_next(void) {
  PFS_setup_object *pfs;
  bool has_more = true;

  for (m_pos.set_at(&m_next_pos); has_more; m_pos.next()) {
    pfs = global_setup_object_container.get(m_pos.m_index, &has_more);

    if (pfs != nullptr) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_objects::make_row(PFS_setup_object *pfs) {
  pfs_optimistic_state lock;
  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_object_type = pfs->m_key.m_object_type;
  m_row.m_schema_name = pfs->m_key.m_schema_name;
  m_row.m_object_name = pfs->m_key.m_object_name;
  m_row.m_enabled_ptr = &pfs->m_enabled;
  m_row.m_timed_ptr = &pfs->m_timed;

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_setup_objects::read_row_values(TABLE *table, unsigned char *buf,
                                         Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* OBJECT_TYPE */
          set_field_enum(f, m_row.m_object_type);
          break;
        case 1: /* OBJECT_SCHEMA */
          if (m_row.m_schema_name.length())
            set_field_varchar_utf8mb4(f, m_row.m_schema_name.ptr(),
                                      m_row.m_schema_name.length());
          else {
            f->set_null();
          }
          break;
        case 2: /* OBJECT_NAME */
          if (m_row.m_object_name.length())
            set_field_varchar_utf8mb4(f, m_row.m_object_name.ptr(),
                                      m_row.m_object_name.length());
          else {
            f->set_null();
          }
          break;
        case 3: /* ENABLED */
          set_field_enum(f, (*m_row.m_enabled_ptr) ? ENUM_YES : ENUM_NO);
          break;
        case 4: /* TIMED */
          set_field_enum(f, (*m_row.m_timed_ptr) ? ENUM_YES : ENUM_NO);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}

int table_setup_objects::update_row_values(TABLE *table, const unsigned char *,
                                           unsigned char *, Field **fields) {
  int result;
  Field *f;
  enum_yes_no value;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index())) {
      switch (f->field_index()) {
        case 3: /* ENABLED */
          value = (enum_yes_no)get_field_enum(f);
          /* Reject illegal enum values in ENABLED */
          if ((value != ENUM_YES) && (value != ENUM_NO)) {
            return HA_ERR_NO_REFERENCED_ROW;
          }
          *m_row.m_enabled_ptr = (value == ENUM_YES) ? true : false;
          break;
        case 4: /* TIMED */
          value = (enum_yes_no)get_field_enum(f);
          /* Reject illegal enum values in TIMED */
          if ((value != ENUM_YES) && (value != ENUM_NO)) {
            return HA_ERR_NO_REFERENCED_ROW;
          }
          *m_row.m_timed_ptr = (value == ENUM_YES) ? true : false;
          break;
        default:
          return HA_ERR_WRONG_COMMAND;
      }
    }
  }

  result = update_derived_flags();
  return result;
}

int table_setup_objects::delete_row_values(TABLE *, const unsigned char *,
                                           Field **) {
  int result = delete_setup_object(m_row.m_object_type, &m_row.m_schema_name,
                                   &m_row.m_object_name);

  if (result == 0) {
    result = update_derived_flags();
  }
  return result;
}
