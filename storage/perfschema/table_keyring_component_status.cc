/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_keyring_component_status.cc
  TABLE KEYRING_KEYS.
*/

#include "storage/perfschema/table_keyring_component_status.h"

#include <stddef.h>
#include <algorithm>
#include <memory>

#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/keyring_metadata_query.h>
#include <mysql/components/services/registry.h>
#include <mysql/service_plugin_registry.h>

THR_LOCK table_keyring_component_status::m_table_lock;

namespace {
/** symbolic names for field offsets, keep in sync with field_types */
enum keyring_keys_field_offsets { FO_STATUS_KEY, FO_STATUS_VALUE };
}  // namespace

constexpr size_t STATUS_KEY_LENGTH = 256;
constexpr size_t STATUS_VALUE_LENGTH = 1024;

Plugin_table table_keyring_component_status::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "keyring_component_status",
    /* Definition */
    " STATUS_KEY VARCHAR(256) NOT NULL,\n"
    " STATUS_VALUE VARCHAR(1024) NOT NULL\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_keyring_component_status::m_share = {
    &pfs_readonly_acl,
    table_keyring_component_status::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_keyring_component_status::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* Perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_keyring_component_status::create(
    PFS_engine_table_share *) {
  return new table_keyring_component_status();
}

table_keyring_component_status::table_keyring_component_status()
    : PFS_engine_table(&m_share, &m_pos),
      m_row(nullptr),
      m_pos(0),
      m_next_pos(0) {
  /* Make a copy of keyring component status */
  materialize();
}

void table_keyring_component_status::materialize() {
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  const my_service<SERVICE_TYPE(keyring_component_metadata_query)>
      metadata_query_service("keyring_component_metadata_query",
                             plugin_registry);
  if (!metadata_query_service) {
    my_h_keyring_component_metadata_iterator iterator = nullptr;

    if (!metadata_query_service->init(&iterator)) {
      for (; metadata_query_service->is_valid(iterator);
           (void)metadata_query_service->next(iterator)) {
        size_t key_buffer_length = 0;
        size_t value_buffer_length = 0;
        if (metadata_query_service->get_length(iterator, &key_buffer_length,
                                               &value_buffer_length) != 0)
          break;
        const std::unique_ptr<char[]> key_buffer(new char[key_buffer_length]);
        const std::unique_ptr<char[]> value_buffer(
            new char[value_buffer_length]);

        if (key_buffer == nullptr || value_buffer == nullptr) break;

        memset(key_buffer.get(), 0, key_buffer_length);
        memset(value_buffer.get(), 0, value_buffer_length);

        if (metadata_query_service->get(iterator, key_buffer.get(),
                                        key_buffer_length, value_buffer.get(),
                                        value_buffer_length) != 0)
          break;

        m_row_keyring_component_status.push_back(
            {{key_buffer.get(), std::min(STATUS_KEY_LENGTH, key_buffer_length)},
             {value_buffer.get(),
              std::min(STATUS_VALUE_LENGTH, value_buffer_length)}});
      }
      (void)metadata_query_service->deinit(iterator);
    }
    iterator = nullptr;
  }
  mysql_plugin_registry_release(plugin_registry);
}

ha_rows table_keyring_component_status::get_row_count() {
  /* A hint for optimizer - number bytes not number entries */
  return sizeof(row_keyring_component_status);
}

void table_keyring_component_status::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_keyring_component_status::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_row_keyring_component_status.size());
  m_row = &m_row_keyring_component_status[m_pos.m_index];
  return 0;
}

int table_keyring_component_status::rnd_next() {
  m_pos.set_at(&m_next_pos);
  if (m_pos.m_index < m_row_keyring_component_status.size()) {
    m_row = &m_row_keyring_component_status[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    return 0;
  }
  m_row = nullptr;
  return HA_ERR_END_OF_FILE;
}

int table_keyring_component_status::read_row_values(TABLE *table,
                                                    unsigned char *buf,
                                                    Field **fields,
                                                    bool read_all) {
  Field *f;
  assert(m_row);
  /* Set the null bits */
  assert(table->s->null_bytes == 0);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case keyring_keys_field_offsets::FO_STATUS_KEY:
          set_field_varchar_utf8mb4(f, m_row->m_status_key.c_str());
          break;
        case keyring_keys_field_offsets::FO_STATUS_VALUE:
          set_field_varchar_utf8mb4(f, m_row->m_status_value.c_str());
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
