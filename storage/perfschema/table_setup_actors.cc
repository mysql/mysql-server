/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_setup_actors.cc
  Table SETUP_ACTORS (implementation).
*/

#include "storage/perfschema/table_setup_actors.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_setup_actor.h"

THR_LOCK table_setup_actors::m_table_lock;

Plugin_table table_setup_actors::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "setup_actors",
    /* Definition */
    "  HOST CHAR(60) COLLATE utf8mb4_bin default '%' not null,\n"
    "  USER CHAR(32) COLLATE utf8mb4_bin default '%' not null,\n"
    "  `ROLE` CHAR(32) COLLATE utf8mb4_bin default '%' not null,\n"
    "  ENABLED ENUM ('YES', 'NO') not null default 'YES',\n"
    "  HISTORY ENUM ('YES', 'NO') not null default 'YES',\n"
    "  PRIMARY KEY (HOST, USER, `ROLE`) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_setup_actors::m_share = {
    &pfs_editable_acl,
    table_setup_actors::create,
    table_setup_actors::write_row,
    table_setup_actors::delete_all_rows,
    table_setup_actors::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_setup_actors::match(PFS_setup_actor *pfs) {
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

PFS_engine_table *table_setup_actors::create(PFS_engine_table_share *) {
  return new table_setup_actors();
}

int table_setup_actors::write_row(PFS_engine_table *, TABLE *table,
                                  unsigned char *, Field **fields) {
  Field *f;
  String user_data("%", 1, &my_charset_utf8mb4_bin);
  String host_data("%", 1, &my_charset_utf8mb4_bin);
  String role_data("%", 1, &my_charset_utf8mb4_bin);
  String *user = &user_data;
  String *host = &host_data;
  String *role = &role_data;
  enum_yes_no enabled_value = ENUM_YES;
  enum_yes_no history_value = ENUM_YES;
  bool enabled;
  bool history;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* HOST */
          host = get_field_char_utf8(f, &host_data);
          break;
        case 1: /* USER */
          user = get_field_char_utf8(f, &user_data);
          break;
        case 2: /* ROLE */
          role = get_field_char_utf8(f, &role_data);
          break;
        case 3: /* ENABLED */
          enabled_value = (enum_yes_no)get_field_enum(f);
          break;
        case 4: /* HISTORY */
          history_value = (enum_yes_no)get_field_enum(f);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  /* Reject illegal enum values in ENABLED */
  if ((enabled_value != ENUM_YES) && (enabled_value != ENUM_NO)) {
    return HA_ERR_NO_REFERENCED_ROW;
  }

  /* Reject illegal enum values in HISTORY */
  if ((history_value != ENUM_YES) && (history_value != ENUM_NO)) {
    return HA_ERR_NO_REFERENCED_ROW;
  }

  /* Reject if any of user/host/role is not provided */
  if (user->length() == 0 || host->length() == 0 || role->length() == 0) {
    return HA_ERR_WRONG_COMMAND;
  }

  enabled = (enabled_value == ENUM_YES) ? true : false;
  history = (history_value == ENUM_YES) ? true : false;

  return insert_setup_actor(user, host, role, enabled, history);
}

int table_setup_actors::delete_all_rows(void) { return reset_setup_actor(); }

ha_rows table_setup_actors::get_row_count(void) {
  return global_setup_actor_container.get_row_count();
}

table_setup_actors::table_setup_actors()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_setup_actors::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_setup_actors::rnd_next() {
  PFS_setup_actor *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_setup_actor_iterator it =
      global_setup_actor_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_actors::rnd_pos(const void *pos) {
  PFS_setup_actor *pfs;

  set_position(pos);

  pfs = global_setup_actor_container.get(m_pos.m_index);
  if (pfs != NULL) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_setup_actors::index_init(uint idx MY_ATTRIBUTE((unused)), bool) {
  PFS_index_setup_actors *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_setup_actors);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_setup_actors::index_next() {
  PFS_setup_actor *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_setup_actor_iterator it =
      global_setup_actor_container.iterate(m_pos.m_index);

  do {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int table_setup_actors::make_row(PFS_setup_actor *pfs) {
  pfs_optimistic_state lock;
  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_hostname_length = pfs->m_hostname_length;

  if (unlikely((m_row.m_hostname_length == 0) ||
               (m_row.m_hostname_length > sizeof(m_row.m_hostname)))) {
    return HA_ERR_RECORD_DELETED;
  }

  memcpy(m_row.m_hostname, pfs->m_hostname, m_row.m_hostname_length);
  m_row.m_username_length = pfs->m_username_length;

  if (unlikely((m_row.m_username_length == 0) ||
               (m_row.m_username_length > sizeof(m_row.m_username)))) {
    return HA_ERR_RECORD_DELETED;
  }

  memcpy(m_row.m_username, pfs->m_username, m_row.m_username_length);
  m_row.m_rolename_length = pfs->m_rolename_length;

  if (unlikely((m_row.m_rolename_length == 0) ||
               (m_row.m_rolename_length > sizeof(m_row.m_rolename)))) {
    return HA_ERR_RECORD_DELETED;
  }

  memcpy(m_row.m_rolename, pfs->m_rolename, m_row.m_rolename_length);
  m_row.m_enabled_ptr = &pfs->m_enabled;
  m_row.m_history_ptr = &pfs->m_history;

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_setup_actors::read_row_values(TABLE *table, unsigned char *,
                                        Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* HOST */
          set_field_char_utf8(f, m_row.m_hostname, m_row.m_hostname_length);
          break;
        case 1: /* USER */
          set_field_char_utf8(f, m_row.m_username, m_row.m_username_length);
          break;
        case 2: /* ROLE */
          set_field_char_utf8(f, m_row.m_rolename, m_row.m_rolename_length);
          break;
        case 3: /* ENABLED */
          set_field_enum(f, (*m_row.m_enabled_ptr) ? ENUM_YES : ENUM_NO);
          break;
        case 4: /* HISTORY */
          set_field_enum(f, (*m_row.m_history_ptr) ? ENUM_YES : ENUM_NO);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

int table_setup_actors::update_row_values(TABLE *table, const unsigned char *,
                                          unsigned char *, Field **fields) {
  int result;
  Field *f;
  enum_yes_no value;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* HOST */
        case 1: /* USER */
        case 2: /* ROLE */
          return HA_ERR_WRONG_COMMAND;
        case 3: /* ENABLED */
          value = (enum_yes_no)get_field_enum(f);
          /* Reject illegal enum values in ENABLED */
          if ((value != ENUM_YES) && (value != ENUM_NO)) {
            return HA_ERR_NO_REFERENCED_ROW;
          }
          *m_row.m_enabled_ptr = (value == ENUM_YES) ? true : false;
          break;
        case 4: /* HISTORY */
          value = (enum_yes_no)get_field_enum(f);
          /* Reject illegal enum values in HISTORY */
          if ((value != ENUM_YES) && (value != ENUM_NO)) {
            return HA_ERR_NO_REFERENCED_ROW;
          }
          *m_row.m_history_ptr = (value == ENUM_YES) ? true : false;
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  result = update_setup_actors_derived_flags();
  return result;
}

int table_setup_actors::delete_row_values(TABLE *, const unsigned char *,
                                          Field **) {
  CHARSET_INFO *cs = &my_charset_utf8mb4_bin;
  String user(m_row.m_username, m_row.m_username_length, cs);
  String role(m_row.m_rolename, m_row.m_rolename_length, cs);
  String host(m_row.m_hostname, m_row.m_hostname_length, cs);

  return delete_setup_actor(&user, &host, &role);
}
