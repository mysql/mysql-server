/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
  @file storage/perfschema/table_replication_asynchronous_connection_failover.cc
  Table replication_asynchronous_connection_failover (implementation).
*/

#include "storage/perfschema/table_replication_asynchronous_connection_failover.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /* Multisource replciation */
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_replication_asynchronous_connection_failover::m_table_lock;

ha_rows table_replication_asynchronous_connection_failover::num_rows = 0;

Plugin_table table_replication_asynchronous_connection_failover::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_asynchronous_connection_failover",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci "
    "NOT NULL COMMENT 'The replication channel name that connects source and "
    "replica.',\n"
    "  HOST CHAR(255) CHARACTER SET ASCII NOT NULL COMMENT 'The source "
    "hostname that the replica will attempt to switch over the replication "
    "connection to in case of a failure.',\n"
    "  PORT INTEGER NOT NULL COMMENT 'The source port that the replica "
    "will attempt to switch over the replication connection to in case of a "
    "failure.',\n"
    "  NETWORK_NAMESPACE CHAR(64) COMMENT 'The source network namespace that "
    "the replica will attempt to switch over the replication connection to "
    "in case of a failure. If its value is empty, connections use the default "
    "(global) namespace.',\n"
    "  WEIGHT INTEGER UNSIGNED NOT NULL COMMENT 'The order in which the "
    "replica shall try to switch the connection over to when there are "
    "failures. Weight can be set to a number between 1 and 100, where 100 is "
    "the highest weight and 1 the lowest.',\n"
    " MANAGED_NAME CHAR(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci "
    "NOT NULL DEFAULT '' COMMENT 'The name of the group which this server "
    "belongs to.'\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share
    table_replication_asynchronous_connection_failover::m_share{
        &pfs_readonly_acl,
        /** Open table function. */
        table_replication_asynchronous_connection_failover::create,
        nullptr, /* write_row */
        nullptr, /* delete_all_rows */
        /* records */
        table_replication_asynchronous_connection_failover::get_row_count,
        sizeof(pos_t), /* ref length */
        &m_table_lock,
        &m_table_def,
        true, /* perpetual */
        PFS_engine_table_proxy(),
        {0},
        false /* m_in_purgatory */
    };

PFS_engine_table *table_replication_asynchronous_connection_failover::create(
    PFS_engine_table_share *) {
  return new table_replication_asynchronous_connection_failover();
}

table_replication_asynchronous_connection_failover::
    table_replication_asynchronous_connection_failover()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_replication_asynchronous_connection_failover::
    ~table_replication_asynchronous_connection_failover() = default;

void table_replication_asynchronous_connection_failover::reset_position() {
  DBUG_TRACE;
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
  m_source_conn_detail.clear();
  table_replication_asynchronous_connection_failover::num_rows = 0;
}

ha_rows table_replication_asynchronous_connection_failover::get_row_count() {
  DBUG_TRACE;
  return table_replication_asynchronous_connection_failover::num_rows;
}

int table_replication_asynchronous_connection_failover::rnd_init(bool) {
  DBUG_TRACE;
  bool error{true};

  Rpl_async_conn_failover_table_operations table_op(TL_READ);
  std::tie(error, m_source_conn_detail) = table_op.read_source_random_rows();
  if (error) {
    table_replication_asynchronous_connection_failover::num_rows = 0;
    m_source_conn_detail.clear();
    return HA_ERR_INTERNAL_ERROR;
  }

  table_replication_asynchronous_connection_failover::num_rows =
      m_source_conn_detail.size();
  return 0;
}

int table_replication_asynchronous_connection_failover::rnd_next() {
  DBUG_TRACE;

  m_pos.set_at(&m_next_pos);
  if (m_pos.m_index < m_source_conn_detail.size()) {
    m_next_pos.set_after(&m_pos);
    return make_row(m_pos.m_index);
  }

  return HA_ERR_END_OF_FILE;
}

int table_replication_asynchronous_connection_failover::rnd_pos(
    const void *pos) {
  DBUG_TRACE;

  set_position(pos);
  assert(m_pos.m_index < m_source_conn_detail.size());
  if (m_pos.m_index < m_source_conn_detail.size()) {
    return make_row(m_pos.m_index);
  }

  return HA_ERR_END_OF_FILE;
}

int table_replication_asynchronous_connection_failover::make_row(uint index) {
  DBUG_TRACE;

  m_row.channel_name_length = 0;
  m_row.host_length = 0;
  m_row.port = 0;
  m_row.network_namespace_length = 0;
  m_row.weight = 0;
  m_row.managed_name_length = 0;

  if (index >= m_source_conn_detail.size()) {
    return HA_ERR_END_OF_FILE;
  }

  std::string channel{};
  std::string host{};
  std::string network_namespace{};
  uint port;
  uint weight;
  std::string managed_name{};

  auto source_tuple = m_source_conn_detail[index];
  std::tie(channel, host, port, std::ignore, weight, managed_name) =
      source_tuple;

  channel_map.rdlock();
  Master_info *mi = channel_map.get_mi(channel.c_str());
  if (nullptr != mi) {
    network_namespace.assign(mi->network_namespace_str());
  }
  channel_map.unlock();

  m_row.channel_name_length = channel.length();
  memcpy(m_row.channel_name, channel.c_str(), channel.length());

  m_row.host_length = host.length();
  memcpy(m_row.host, host.c_str(), host.length());

  m_row.port = port;

  m_row.network_namespace_length = network_namespace.length();
  memcpy(m_row.network_namespace, network_namespace.c_str(),
         network_namespace.length());

  m_row.weight = weight;

  m_row.managed_name_length = managed_name.length();
  memcpy(m_row.managed_name, managed_name.c_str(), managed_name.length());

  return 0;
}

int table_replication_asynchronous_connection_failover::read_row_values(
    TABLE *table, unsigned char *buf, Field **fields, bool read_all) {
  DBUG_TRACE;
  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  if (m_pos.m_index >= m_source_conn_detail.size()) {
    return HA_ERR_END_OF_FILE;
  }

  for (Field *f = nullptr; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /** channel_name */
          set_field_char_utf8mb4(f, m_row.channel_name,
                                 m_row.channel_name_length);
          break;
        case 1: /** host */
          set_field_char_utf8mb4(f, m_row.host, m_row.host_length);
          break;
        case 2: /** port */
          set_field_ulong(f, m_row.port);
          break;
        case 3: /** network_namespace */
          set_field_char_utf8mb4(f, m_row.network_namespace,
                                 m_row.network_namespace_length);
          break;
        case 4: /** weight */
          set_field_ulong(f, m_row.weight);
          break;
        case 5: /** managed_name */
          set_field_char_utf8mb4(f, m_row.managed_name,
                                 m_row.managed_name_length);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
