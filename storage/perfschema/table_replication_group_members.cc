/*
  Copyright (c) 2013, 2019, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
  @file storage/perfschema/table_replication_group_members.cc
  Table replication_group_members (implementation).
*/

#include "storage/perfschema/table_replication_group_members.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "mysql/plugin_group_replication.h"
#include "sql/field.h"
#include "sql/log.h"
#include "sql/plugin_table.h"
#include "sql/rpl_group_replication.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"
#include "thr_lock.h"

/*
  Callbacks implementation for GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS.
*/
static void set_channel_name(void *const context, const char &value,
                             size_t length) {
  struct st_row_group_members *row =
      static_cast<struct st_row_group_members *>(context);
  const size_t max = CHANNEL_NAME_LENGTH;
  length = std::min(length, max);

  row->channel_name_length = length;
  memcpy(row->channel_name, &value, length);
}

static void set_member_id(void *const context, const char &value,
                          size_t length) {
  struct st_row_group_members *row =
      static_cast<struct st_row_group_members *>(context);
  const size_t max = UUID_LENGTH;
  length = std::min(length, max);

  row->member_id_length = length;
  memcpy(row->member_id, &value, length);
}

static void set_member_host(void *const context, const char &value,
                            size_t length) {
  struct st_row_group_members *row =
      static_cast<struct st_row_group_members *>(context);
  const size_t max = HOSTNAME_LENGTH;
  length = std::min(length, max);

  row->member_host_length = length;
  memcpy(row->member_host, &value, length);
}

static void set_member_port(void *const context, unsigned int value) {
  struct st_row_group_members *row =
      static_cast<struct st_row_group_members *>(context);
  row->member_port = value;
}

static void set_member_state(void *const context, const char &value,
                             size_t length) {
  struct st_row_group_members *row =
      static_cast<struct st_row_group_members *>(context);
  const size_t max = NAME_LEN;
  length = std::min(length, max);

  row->member_state_length = length;
  memcpy(row->member_state, &value, length);
}

static void set_member_version(void *const context, const char &value,
                               size_t length) {
  struct st_row_group_members *row =
      static_cast<struct st_row_group_members *>(context);
  const size_t max = NAME_LEN;
  length = std::min(length, max);

  row->member_version_length = length;
  memcpy(row->member_version, &value, length);
}

static void set_member_role(void *const context, const char &value,
                            size_t length) {
  struct st_row_group_members *row =
      static_cast<struct st_row_group_members *>(context);
  const size_t max = NAME_LEN;
  length = std::min(length, max);

  row->member_role_length = length;
  memcpy(row->member_role, &value, length);
}

THR_LOCK table_replication_group_members::m_table_lock;

Plugin_table table_replication_group_members::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_group_members",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) not null,\n"
    "  MEMBER_ID CHAR(36) collate utf8mb4_bin not null,\n"
    "  MEMBER_HOST CHAR(60) collate utf8mb4_bin not null,\n"
    "  MEMBER_PORT INTEGER,\n"
    "  MEMBER_STATE CHAR(64) collate utf8mb4_bin not null,\n"
    "  MEMBER_ROLE CHAR(64) collate utf8mb4_bin not null,\n"
    "  MEMBER_VERSION CHAR(64) collate utf8mb4_bin not null\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_replication_group_members::m_share = {
    &pfs_readonly_acl,
    &table_replication_group_members::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_replication_group_members::get_row_count,
    sizeof(pos_t), /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_replication_group_members::create(
    PFS_engine_table_share *) {
  return new table_replication_group_members();
}

table_replication_group_members::table_replication_group_members()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_replication_group_members::~table_replication_group_members() {}

void table_replication_group_members::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_replication_group_members::get_row_count() {
  return get_group_replication_members_number_info();
}

int table_replication_group_members::rnd_next(void) {
  if (!is_group_replication_plugin_loaded()) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < get_row_count();
       m_pos.next()) {
    m_next_pos.set_after(&m_pos);
    return make_row(m_pos.m_index);
  }

  return HA_ERR_END_OF_FILE;
}

int table_replication_group_members::rnd_pos(const void *pos) {
  if (!is_group_replication_plugin_loaded()) {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < get_row_count());
  return make_row(m_pos.m_index);
}

int table_replication_group_members::make_row(uint index) {
  DBUG_ENTER("table_replication_group_members::make_row");
  // Set default values.
  m_row.channel_name_length = 0;
  m_row.member_id_length = 0;
  m_row.member_host_length = 0;
  m_row.member_port = 0;
  m_row.member_state_length = 0;
  m_row.member_version_length = 0;
  m_row.member_role_length = 0;

  // Set callbacks on GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS.
  const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS callbacks = {
      &m_row,           &set_channel_name,   &set_member_id,
      &set_member_host, &set_member_port,    &set_member_state,
      &set_member_role, &set_member_version,
  };

  // Query plugin and let callbacks do their job.
  if (get_group_replication_group_members_info(index, callbacks)) {
    DBUG_PRINT("info", ("Group Replication stats not available!"));
  } else {
  }

  DBUG_RETURN(0);
}

int table_replication_group_members::read_row_values(TABLE *table,
                                                     unsigned char *buf,
                                                     Field **fields,
                                                     bool read_all) {
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /** channel_name */
          set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
          break;
        case 1: /** member_id */
          set_field_char_utf8(f, m_row.member_id, m_row.member_id_length);
          break;
        case 2: /** member_host */
          set_field_char_utf8(f, m_row.member_host, m_row.member_host_length);
          break;
        case 3: /** member_port */
          if (m_row.member_port > 0) {
            set_field_ulong(f, m_row.member_port);
          } else {
            f->set_null();
          }
          break;
        case 4: /** member_state */
          set_field_char_utf8(f, m_row.member_state, m_row.member_state_length);
          break;
        case 5: /** member_role */
          set_field_char_utf8(f, m_row.member_role, m_row.member_role_length);
          break;
        case 6: /** member_version */
          set_field_char_utf8(f, m_row.member_version,
                              m_row.member_version_length);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
