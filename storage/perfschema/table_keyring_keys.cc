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
  @file storage/perfschema/table_keyring_keys.cc
  TABLE KEYRING_KEYS.
*/

#include "storage/perfschema/table_keyring_keys.h"

#include <assert.h>
#include <stddef.h>

#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/keyring_keys_metadata_iterator.h>
#include <mysql/components/services/mysql_keyring_native_key_id.h>
#include <mysql/components/services/registry.h>
#include <mysql/service_plugin_registry.h>

constexpr auto KEYRING_ITEM_BUFFER_SIZE = 256;
constexpr auto MAX_FIELD_LENGTH = 64;

THR_LOCK table_keyring_keys::s_table_lock;

namespace {
/** symbolic names for field offsets, keep in sync with field_types */
enum keyring_keys_field_offsets { FO_KEY_ID, FO_KEY_OWNER, FO_BACKEND_KEY_ID };
}  // namespace

Plugin_table table_keyring_keys::s_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "keyring_keys",
    /* Definition */
    "  KEY_ID VARCHAR(255) NOT NULL,\n"
    "  KEY_OWNER VARCHAR(255),\n"
    "  BACKEND_KEY_ID VARCHAR(255)\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA CHARACTER SET utf8mb4 COLLATE utf8mb4_bin",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_keyring_keys::s_share = {
    &pfs_readonly_acl,
    table_keyring_keys::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_keyring_keys::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &s_table_lock,
    &s_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_keyring_keys::create(PFS_engine_table_share *) {
  return new table_keyring_keys();
}

table_keyring_keys::table_keyring_keys()
    : PFS_engine_table(&s_share, &m_pos),
      m_row(nullptr),
      m_pos(0),
      m_next_pos(0) {
  /*
    Make a safe copy of the keys from the keyring
    that will not change while parsing it
  */
  copy_keys_from_keyring();
}

ha_rows table_keyring_keys::get_row_count() {
  /*
  The real number of keys in the keyring does not matter,
  we only need to hint the optimizer,
  (which is a number of bytes, not keys)
  */
  return sizeof(row_keyring_keys);
}

void table_keyring_keys::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_keyring_keys::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_copy_keyring_keys.size());
  m_row = &m_copy_keyring_keys[m_pos.m_index];
  return 0;
}

int table_keyring_keys::rnd_next() {
  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < m_copy_keyring_keys.size()) {
    m_row = &m_copy_keyring_keys[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    return 0;
  }
  m_row = nullptr;
  return HA_ERR_END_OF_FILE;
}

int table_keyring_keys::read_row_values(TABLE *table, unsigned char *buf,
                                        Field **fields, bool read_all) {
  Field *f;

  assert(m_row);

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case FO_KEY_ID:
          set_field_varchar_utf8mb4(
              f, m_row->m_key_id.c_str(),
              static_cast<uint>(m_row->m_key_id.length()));
          break;
        case FO_KEY_OWNER:
          set_field_varchar_utf8mb4(
              f, m_row->m_key_owner.c_str(),
              static_cast<uint>(m_row->m_key_owner.length()));
          break;
        case FO_BACKEND_KEY_ID:
          set_field_varchar_utf8mb4(
              f, m_row->m_backend_key_id.c_str(),
              static_cast<uint>(m_row->m_backend_key_id.length()));
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}

/**
  Query the keyring for the list of the keys

  @return status
    @retval true    keys are copied in keyring_keys
    @retval false   error. Either because of parsing or keyring empty.
*/
static bool fetch_keys(std::vector<row_keyring_keys> &keyring_keys) {
  char key_id[KEYRING_ITEM_BUFFER_SIZE] = "\0";
  char user_id[KEYRING_ITEM_BUFFER_SIZE] = "\0";
  char backend_key_id[KEYRING_ITEM_BUFFER_SIZE] = "\0";
  bool next_ok = true;

  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  const my_service<SERVICE_TYPE(keyring_keys_metadata_iterator)>
      keyring_keys_metadata_iterator("keyring_keys_metadata_iterator",
                                     plugin_registry);
  // check if the keyring plugin supports the backend key ID
  const my_service<SERVICE_TYPE(mysql_keyring_native_key_id)> svc(
      "mysql_keyring_native_key_id", plugin_registry);

  my_h_keyring_keys_metadata_iterator forward_iterator = nullptr;
  if (keyring_keys_metadata_iterator->init(&forward_iterator) != 0) {
    mysql_plugin_registry_release(plugin_registry);
    return false;
  }

  for (; keyring_keys_metadata_iterator->is_valid(forward_iterator) && next_ok;
       next_ok = !keyring_keys_metadata_iterator->next(forward_iterator)) {
    memset(key_id, 0, KEYRING_ITEM_BUFFER_SIZE);
    memset(user_id, 0, KEYRING_ITEM_BUFFER_SIZE);
    if (keyring_keys_metadata_iterator->get(forward_iterator, key_id,
                                            KEYRING_ITEM_BUFFER_SIZE, user_id,
                                            KEYRING_ITEM_BUFFER_SIZE) != 0) {
      break;
    }

    // truncate longer strings
    static_assert(KEYRING_ITEM_BUFFER_SIZE > MAX_FIELD_LENGTH);
    key_id[MAX_FIELD_LENGTH] = '\0';
    user_id[MAX_FIELD_LENGTH] = '\0';

    // get the backend ID for the key
    if (svc.is_valid()) {
      svc->get_backend_key_id(key_id, user_id, backend_key_id,
                              MAX_FIELD_LENGTH);
      backend_key_id[MAX_FIELD_LENGTH] = '\0';
    } else {
      backend_key_id[0] = '\0';
    }

    // got a key, add it to the list of the keys
    keyring_keys.push_back(row_keyring_keys{key_id, user_id, backend_key_id});
  }
  keyring_keys_metadata_iterator->deinit(forward_iterator);
  forward_iterator = nullptr;
  mysql_plugin_registry_release(plugin_registry);
  return true;
}

/**
  Copy the keys from the keyring vault

  Create a copy of the keys in the keyring, under lock,
  ensuring no simultaneous operation will modify the keys

  @return status
    @retval true    keys are copied in m_copy_keyring_keys
    @retval false   error. Either because of parsing or keyring empty.
*/
bool table_keyring_keys::copy_keys_from_keyring() {
  m_copy_keyring_keys.clear();
  return fetch_keys(m_copy_keyring_keys);
}
