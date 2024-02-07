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
  @file storage/perfschema/table_rpl_async_connection_failover_managed.cc
  Table replication_asynchronous_connection_failover_managed (implementation).
*/

#include "storage/perfschema/table_rpl_async_connection_failover_managed.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql-common/json_dom.h"
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

THR_LOCK table_rpl_async_connection_failover_managed::m_table_lock;

ha_rows table_rpl_async_connection_failover_managed::num_rows = 0;

Plugin_table table_rpl_async_connection_failover_managed::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_asynchronous_connection_failover_managed",
    /* Definition */
    " CHANNEL_NAME CHAR(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci "
    "NOT NULL COMMENT 'The replication channel name that connects source and "
    "replica.',\n"
    " MANAGED_NAME CHAR(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci "
    "NOT "
    "NULL"
    " DEFAULT '' COMMENT 'The name of the source which needs to be managed.',\n"
    " MANAGED_TYPE CHAR(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci "
    "NOT "
    "NULL"
    " DEFAULT '' COMMENT 'Determines the managed type.',\n"
    " CONFIGURATION JSON DEFAULT NULL COMMENT 'The data to help manage group. "
    "For Managed_type = GroupReplication, Configuration value should contain "
    "{\"Primary_weight\": 80, \"Secondary_weight\": 60}, so that it assigns "
    "weight=80 to PRIMARY of the group, and weight=60 for rest of the members "
    "in mysql.replication_asynchronous_connection_failover table.'\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_rpl_async_connection_failover_managed::m_share{
    &pfs_readonly_acl,
    /** Open table function. */
    table_rpl_async_connection_failover_managed::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    /* records */
    table_rpl_async_connection_failover_managed::get_row_count,
    sizeof(pos_t), /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_rpl_async_connection_failover_managed::create(
    PFS_engine_table_share *) {
  return new table_rpl_async_connection_failover_managed();
}

table_rpl_async_connection_failover_managed::
    table_rpl_async_connection_failover_managed()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_rpl_async_connection_failover_managed::
    ~table_rpl_async_connection_failover_managed() = default;

void table_rpl_async_connection_failover_managed::reset_position() {
  DBUG_TRACE;
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
  m_source_managed_list.clear();
  table_rpl_async_connection_failover_managed::num_rows = 0;
}

ha_rows table_rpl_async_connection_failover_managed::get_row_count() {
  DBUG_TRACE;
  return table_rpl_async_connection_failover_managed::num_rows;
}

int table_rpl_async_connection_failover_managed::rnd_init(bool) {
  DBUG_TRACE;

  Rpl_async_conn_failover_table_operations table_op(TL_READ);
  std::vector<RPL_FAILOVER_MANAGED_TUPLE> source_list;
  auto error = table_op.read_managed_random_rows(source_list);
  if (error) {
    m_source_managed_list.clear();
    table_rpl_async_connection_failover_managed::num_rows = 0;
    return HA_ERR_INTERNAL_ERROR;
  }

  for (auto source_detail : source_list) {
    std::stringstream json_str;
    json_str << "{\"Primary_weight\": " << std::get<3>(source_detail)
             << ", \"Secondary_weight\": " << std::get<4>(source_detail) << "}";

    auto res_dom = Json_dom::parse(
        json_str.str().c_str(), json_str.str().length(),
        [](const char *, size_t) {},
        [] { my_error(ER_JSON_DOCUMENT_TOO_DEEP, MYF(0)); });

    if (res_dom == nullptr ||
        res_dom->json_type() != enum_json_type::J_OBJECT) {
      my_error(ER_INVALID_USER_ATTRIBUTE_JSON, MYF(0));
      m_source_managed_list.clear();
      table_rpl_async_connection_failover_managed::num_rows = 0;
      return HA_ERR_INTERNAL_ERROR;
    }

    Json_object_ptr res_obj_ptr(down_cast<Json_object *>(res_dom.release()));
    const Json_dom_ptr json_dom = create_dom_ptr<Json_object>();
    auto *json_ob = down_cast<Json_object *>(json_dom.get());
    json_ob->merge_patch(std::move(res_obj_ptr));
    const Json_wrapper wrapper(json_ob->clone());

    auto source_mng_detail =
        std::make_tuple(std::get<0>(source_detail), std::get<1>(source_detail),
                        std::get<2>(source_detail), wrapper);

    m_source_managed_list.push_back(source_mng_detail);
  }

  table_rpl_async_connection_failover_managed::num_rows =
      m_source_managed_list.size();

  return 0;
}

int table_rpl_async_connection_failover_managed::rnd_next() {
  DBUG_TRACE;

  m_pos.set_at(&m_next_pos);
  if (m_pos.m_index < m_source_managed_list.size()) {
    m_next_pos.set_after(&m_pos);
    return make_row(m_pos.m_index);
  }

  return HA_ERR_END_OF_FILE;
}

int table_rpl_async_connection_failover_managed::rnd_pos(const void *pos) {
  DBUG_TRACE;

  set_position(pos);
  assert(m_pos.m_index < m_source_managed_list.size());
  if (m_pos.m_index < m_source_managed_list.size()) {
    return make_row(m_pos.m_index);
  }

  return HA_ERR_END_OF_FILE;
}

int table_rpl_async_connection_failover_managed::make_row(uint index) {
  DBUG_TRACE;

  m_row.channel_name_length = 0;
  m_row.managed_name_length = 0;
  m_row.managed_type_length = 0;

  if (index >= m_source_managed_list.size()) {
    return HA_ERR_END_OF_FILE;
  }

  std::string channel{};
  std::string managed_name{};
  std::string managed_type{};
  Json_wrapper configuration;

  auto managed_tuple = m_source_managed_list[index];
  std::tie(channel, managed_name, managed_type, configuration) = managed_tuple;

  m_row.channel_name_length = channel.length();
  memcpy(m_row.channel_name, channel.c_str(), channel.length());

  m_row.managed_name_length = managed_name.length();
  memcpy(m_row.managed_name, managed_name.c_str(), managed_name.length());

  m_row.managed_type_length = managed_type.length();
  memcpy(m_row.managed_type, managed_type.c_str(), managed_type.length());

  m_row.configuration = Json_wrapper(configuration);

  return 0;
}

int table_rpl_async_connection_failover_managed::read_row_values(
    TABLE *table, unsigned char *buf, Field **fields, bool read_all) {
  DBUG_TRACE;
  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  if (m_pos.m_index >= m_source_managed_list.size()) {
    return HA_ERR_END_OF_FILE;
  }

  for (Field *f = nullptr; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /** channel_name */
          set_field_char_utf8mb4(f, m_row.channel_name,
                                 m_row.channel_name_length);
          break;
        case 1: /** managed_name */
          set_field_char_utf8mb4(f, m_row.managed_name,
                                 m_row.managed_name_length);
          break;
        case 2: /** managed_type */
          set_field_char_utf8mb4(f, m_row.managed_type,
                                 m_row.managed_type_length);
          break;
        case 3: /** configuration */
          set_field_json(f, &m_row.configuration);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
