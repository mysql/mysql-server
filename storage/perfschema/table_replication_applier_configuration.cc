/*
  Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_replication_applier_configuration.cc
  Table replication_applier_configuration (implementation).
*/

#include "storage/perfschema/table_replication_applier_configuration.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /* Multisource replication */
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_replication_applier_configuration::m_table_lock;

Plugin_table table_replication_applier_configuration::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_applier_configuration",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) not null,\n"
    "  DESIRED_DELAY INTEGER not null,\n"
    "  PRIVILEGE_CHECKS_USER TEXT CHARACTER SET utf8mb3 COLLATE utf8mb3_bin "
    "null"
    "    COMMENT 'User name for the security context of the applier.',\n"
    "  REQUIRE_ROW_FORMAT ENUM('YES', 'NO') not null COMMENT "
    "    'Indicates whether the channel shall only accept row based events.',\n"
    "  REQUIRE_TABLE_PRIMARY_KEY_CHECK ENUM('STREAM','ON','OFF','GENERATE')"
    " not null"
    " COMMENT 'Indicates what is the channel policy regarding tables without"
    " primary keys on create and alter table queries',\n"
    "  ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_TYPE "
    "ENUM('OFF','LOCAL','UUID')  not null "
    "    COMMENT 'Indicates whether the channel will generate a new GTID for"
    " anonymous transactions. OFF means that anonymous transactions will remain"
    " anonymous. LOCAL means that anonymous transactions will be assigned a"
    " newly generated GTID based on server_uuid. UUID indicates that"
    " anonymous transactions will be assigned a newly generated GTID based on"
    " Assign_gtids_to_anonymous_transactions_value',\n"
    "  ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_VALUE TEXT CHARACTER SET utf8mb3 "
    "COLLATE utf8mb3_bin null "
    "    COMMENT 'Indicates the UUID used while generating GTIDs for anonymous"
    " transactions',\n"
    "  PRIMARY KEY (CHANNEL_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_replication_applier_configuration::m_share = {
    &pfs_readonly_acl,
    table_replication_applier_configuration::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_replication_applier_configuration::get_row_count,
    sizeof(pos_t), /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_rpl_applier_config::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_applier_config row;

    /* Mutex locks not necessary for channel name. */
    row.channel_name_length =
        mi->get_channel() ? (uint)strlen(mi->get_channel()) : 0;
    memcpy(row.channel_name, mi->get_channel(), row.channel_name_length);

    if (!m_key.match_not_null(row.channel_name, row.channel_name_length)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_replication_applier_configuration::create(
    PFS_engine_table_share *) {
  return new table_replication_applier_configuration();
}

table_replication_applier_configuration::
    table_replication_applier_configuration()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_replication_applier_configuration::
    ~table_replication_applier_configuration() = default;

void table_replication_applier_configuration::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_replication_applier_configuration::get_row_count() {
  return channel_map.get_max_channels();
}

int table_replication_applier_configuration::rnd_next(void) {
  Master_info *mi;
  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels(); m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);
    if (mi && mi->host[0]) {
      make_row(mi);
      m_next_pos.set_after(&m_pos);
      channel_map.unlock();
      return 0;
    }
  }

  channel_map.unlock();
  return HA_ERR_END_OF_FILE;
}

int table_replication_applier_configuration::rnd_pos(const void *pos) {
  int res = HA_ERR_RECORD_DELETED;

  Master_info *mi;

  set_position(pos);

  channel_map.rdlock();

  if ((mi = channel_map.get_mi_at_pos(m_pos.m_index))) {
    res = make_row(mi);
  }

  channel_map.unlock();

  return res;
}

int table_replication_applier_configuration::index_init(uint idx
                                                        [[maybe_unused]],
                                                        bool) {
  PFS_index_rpl_applier_config *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_rpl_applier_config);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_replication_applier_configuration::index_next(void) {
  int res = HA_ERR_END_OF_FILE;

  Master_info *mi;

  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels() && res != 0;
       m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    if (mi && mi->host[0]) {
      if (m_opened_index->match(mi)) {
        res = make_row(mi);
        m_next_pos.set_after(&m_pos);
      }
    }
  }

  channel_map.unlock();

  return res;
}

int table_replication_applier_configuration::make_row(Master_info *mi) {
  DBUG_TRACE;
  assert(mi != nullptr);
  assert(mi->rli != nullptr);

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  m_row.channel_name_length = mi->get_channel() ? strlen(mi->get_channel()) : 0;
  memcpy(m_row.channel_name, mi->get_channel(), m_row.channel_name_length);
  m_row.desired_delay = mi->rli->get_sql_delay();

  std::ostringstream oss;

  if (mi->rli->is_privilege_checks_user_corrupted())
    oss << "<INVALID>" << std::flush;
  else if (mi->rli->get_privilege_checks_username().length() != 0) {
    std::string username{replace_all_in_str(
        mi->rli->get_privilege_checks_username(), "'", "\\'")};

    oss << "'" << username << "'@";

    if (mi->rli->get_privilege_checks_hostname().length() != 0)
      oss << "'" << mi->rli->get_privilege_checks_hostname() << "'";
    else
      oss << "%";

    oss << std::flush;
  }

  m_row.privilege_checks_user.assign(oss.str());

  m_row.requires_row_format =
      mi->rli->is_row_format_required() ? PS_RPL_YES : PS_RPL_NO;

  m_row.require_table_primary_key_check =
      mi->rli->get_require_table_primary_key_check();

  m_row.assign_gtids_to_anonymous_transactions_type =
      mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type();

  m_row.assign_gtids_to_anonymous_transactions_value.assign(
      mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_value());

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  return 0;
}

int table_replication_applier_configuration::read_row_values(TABLE *table,
                                                             unsigned char *buf,
                                                             Field **fields,
                                                             bool read_all) {
  DBUG_TRACE;
  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (Field *f = nullptr; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /**channel_name*/
          set_field_char_utf8mb4(f, m_row.channel_name,
                                 m_row.channel_name_length);
          break;
        case 1: /** desired_delay */
          set_field_ulong(f, static_cast<ulong>(m_row.desired_delay));
          break;
        case 2: /**privilege_checks_user*/
          if (m_row.privilege_checks_user.length() != 0)
            set_field_text(f, m_row.privilege_checks_user.data(),
                           m_row.privilege_checks_user.length(),
                           &my_charset_utf8mb4_bin);
          else
            f->set_null();
          break;
        case 3: /** require_row_format */
          set_field_enum(f, m_row.requires_row_format);
          break;
        case 4: /** require_table_primary_key_check */
          set_field_enum(f, m_row.require_table_primary_key_check);
          break;
        case 5: /** assign_gtids_to_anonymous_transactions_type */
          set_field_enum(
              f, static_cast<ulong>(
                     m_row.assign_gtids_to_anonymous_transactions_type));
          break;
        case 6: /** assign_gtids_to_anonymous_transactions_value */
          if (m_row.assign_gtids_to_anonymous_transactions_value.length() != 0)
            set_field_text(
                f, m_row.assign_gtids_to_anonymous_transactions_value.data(),
                m_row.assign_gtids_to_anonymous_transactions_value.length(),
                &my_charset_utf8mb4_bin);
          else
            f->set_null();
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
