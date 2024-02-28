/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_user_defined_functions.cc
  Table USER_DEFINED_FUNCTIONS (implementation).
*/

#include "storage/perfschema/table_user_defined_functions.h"

#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/sql_class.h"
#include "sql/sql_udf.h"
#include "sql/table.h"

THR_LOCK table_user_defined_functions::m_table_lock;

Plugin_table table_user_defined_functions::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "user_defined_functions",
    /* Definition */
    "  UDF_NAME VARCHAR(64) not null,\n"
    "  UDF_RETURN_TYPE VARCHAR(20) not null,\n"
    "  UDF_TYPE VARCHAR(20) not null,\n"
    "  UDF_LIBRARY VARCHAR(1024),\n"
    "  UDF_USAGE_COUNT BIGINT,\n"
    "  PRIMARY KEY (UDF_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_user_defined_functions::m_share = {
    &pfs_readonly_acl,
    table_user_defined_functions::create,
    nullptr, /* write_row */
    nullptr, /* delete all rows */
    table_user_defined_functions::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_user_defined_functions_by_name::match(
    const row_user_defined_functions *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row->m_name, row->m_name_length)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_user_defined_functions::create(
    PFS_engine_table_share *) {
  auto *t = new table_user_defined_functions();
  if (t != nullptr) {
    THD *thd = current_thd;
    assert(thd != nullptr);
    t->materialize(thd);
  }
  return t;
}

ha_rows table_user_defined_functions::get_row_count() {
  ha_rows count;
  udf_hash_rlock();
  count = udf_hash_size();
  udf_hash_unlock();
  return count;
}

table_user_defined_functions::table_user_defined_functions()
    : PFS_engine_table(&m_share, &m_pos),
      m_all_rows(nullptr),
      m_row_count(0),
      m_row(nullptr),
      m_pos(0),
      m_next_pos(0),
      m_opened_index(nullptr) {}

struct udf_materialize_state_s {
  row_user_defined_functions *rows;
  row_user_defined_functions *row;
};

void table_user_defined_functions::materialize_udf_funcs(udf_func *udf,
                                                         void *arg) {
  auto *s = (struct udf_materialize_state_s *)arg;

  make_row(udf, s->row);
  s->row++;
}

void table_user_defined_functions::materialize(THD *thd) {
  uint size;
  struct udf_materialize_state_s state;

  assert(m_all_rows == nullptr);
  assert(m_row_count == 0);

  udf_hash_rlock();

  size = udf_hash_size();
  if (size == 0) {
    goto end;
  }

  state.rows = (row_user_defined_functions *)thd->alloc(
      size * sizeof(row_user_defined_functions));
  if (state.rows == nullptr) {
    /* Out of memory, this thread will error out. */
    goto end;
  }

  state.row = state.rows;

  udf_hash_for_each(materialize_udf_funcs, &state);

  m_all_rows = state.rows;
  m_row_count = size;

end:
  udf_hash_unlock();
}

int table_user_defined_functions::make_row(const udf_func *entry,
                                           row_user_defined_functions *row) {
  /* keep in sync with Item_result */
  static const char *const return_types[] = {
      "char", "double", "integer", "row", /** not valid for UDFs */
      "decimal" /** char *, to be converted to/from a decimal */
  };
  static uint constexpr return_type_lengths[] = {
      sizeof("char") - 1, sizeof("double") - 1, sizeof("integer") - 1,
      sizeof("row") - 1, sizeof("decimal") - 1};

  /* keep in sync with Item_udftype */
  static const char *const udf_types[] = {nullptr,  // invalid value
                                          "function", "aggregate"};
  static uint constexpr udf_type_lengths[] = {
      0,  // invalid value
      sizeof("function") - 1,
      sizeof("aggregate") - 1,
  };

  row->m_name_length =
      (uint)std::min(sizeof(row->m_name) - 1, entry->name.length);
  memcpy(row->m_name, entry->name.str, row->m_name_length);

  assert(entry->returns >= 0);
  assert(entry->returns < 5);
  row->m_return_type = return_types[entry->returns];
  row->m_return_type_length = return_type_lengths[entry->returns];

  assert(entry->type > 0);
  assert(entry->type < 3);
  row->m_type = udf_types[entry->type];
  row->m_type_length = udf_type_lengths[entry->type];

  row->m_library_length = (uint)std::min(sizeof(row->m_library) - 1,
                                         entry->dl ? strlen(entry->dl) : 0);
  if (entry->dl) {
    memcpy(row->m_library, entry->dl, row->m_library_length);
  } else {
    row->m_library_length = 0;
  }
  row->m_library[row->m_library_length] = 0;

  row->m_usage_count = entry->usage_count;
  return 0;
}

void table_user_defined_functions::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_user_defined_functions::rnd_next() {
  int result;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < m_row_count) {
    m_row = &m_all_rows[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    result = 0;
  } else {
    m_row = nullptr;
    result = HA_ERR_END_OF_FILE;
  }

  return result;
}

int table_user_defined_functions::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_row_count);
  m_row = &m_all_rows[m_pos.m_index];
  return 0;
}

int table_user_defined_functions::index_init(uint idx, bool) {
  PFS_index_user_defined_functions *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_user_defined_functions_by_name);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_user_defined_functions::index_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_row_count; m_pos.next()) {
    m_row = &m_all_rows[m_pos.m_index];

    if (m_opened_index->match(m_row)) {
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  m_row = nullptr;

  return HA_ERR_END_OF_FILE;
}

int table_user_defined_functions::read_row_values(TABLE *table,
                                                  unsigned char *buf,
                                                  Field **fields,
                                                  bool read_all) {
  Field *f;

  assert(m_row);

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* UDF_NAME */
          set_field_varchar_utf8mb4(f, m_row->m_name, m_row->m_name_length);
          break;
        case 1: /* UDF_RETURN_TYPE */
          set_field_varchar_utf8mb4(f, m_row->m_return_type,
                                    m_row->m_return_type_length);
          break;
        case 2: /* UDF_TYPE */
          set_field_varchar_utf8mb4(f, m_row->m_type, m_row->m_type_length);
          break;
        case 3: /* UDF_LIBRARY */
          if (m_row->m_library_length)
            set_field_varchar_utf8mb4(f, m_row->m_library,
                                      m_row->m_library_length);
          else
            f->set_null();
          break;
        case 4: /* UDF_USAGE_COUNT */
          set_field_ulonglong(f, m_row->m_usage_count);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
