/*
  Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_rpl_async_connection_failover_managed.cc
  Table replication_asynchronous_connection_failover_managed (implementation).
*/

#include "storage/perfschema/table_rpl_async_connection_failover_managed.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/field.h"
#include "sql/json_dom.h"
#include "sql/plugin_table.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /* Multisource replciation */
#include "sql/rpl_rli.h"
#include "sql/rpl_slave.h"
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
    " CHANNEL_NAME CHAR(64) CHARACTER SET utf8 COLLATE utf8_general_ci "
    "NOT NULL COMMENT 'The replication channel name that connects source and "
    "replica.',\n"
    " MANAGED_NAME CHAR(64) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL"
    " DEFAULT '' COMMENT 'The name of the source which needs to be managed.',\n"
    " MANAGED_TYPE CHAR(64) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL"
    " DEFAULT '' COMMENT 'Determines the managed type.',\n"
    " CONFIGURATION JSON DEFAULT NULL COMMENT 'The data to help manage group. "
    "For Managed_type = GroupReplication, Configuration value should contain "
    "{\"Primary_weight\": 80, \"Secondary_weight\": 60}, so that it assigns "
    "weight=80 to PRIMARY of the group, and weight=60 for rest of the members "
    "in mysql.replication_asynchronous_connection_failover table.',\n"
    "  PRIMARY KEY(CHANNEL_NAME, MANAGED_NAME) \n",
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

bool PFS_index_rpl_async_conn_failover_managed::match(
    RPL_FAILOVER_MANAGED_JSON_TUPLE source_managed_tuple) {
  DBUG_TRACE;
  st_row_rpl_async_conn_failover_managed row;
  std::string channel_name;
  std::string managed_name;

  std::tie(channel_name, managed_name, std::ignore, std::ignore) =
      source_managed_tuple;

  row.channel_name_length = channel_name.length();
  memcpy(row.channel_name, channel_name.c_str(), row.channel_name_length);
  if (!m_key_1.match_not_null(row.channel_name, row.channel_name_length)) {
    return false;
  }

  row.managed_name_length = managed_name.length();
  memcpy(row.managed_name, managed_name.c_str(), row.managed_name_length);
  if (!m_key_2.match_not_null(row.managed_name, row.managed_name_length)) {
    return false;
  }

  return true;
}

PFS_engine_table *table_rpl_async_connection_failover_managed::create(
    PFS_engine_table_share *) {
  return new table_rpl_async_connection_failover_managed();
}

table_rpl_async_connection_failover_managed::
    table_rpl_async_connection_failover_managed()
    : PFS_engine_table(&m_share, &m_pos),
      m_pos(0),
      m_next_pos(0),
      read_error{false} {}

table_rpl_async_connection_failover_managed::
    ~table_rpl_async_connection_failover_managed() {}

void table_rpl_async_connection_failover_managed::reset_position(void) {
  DBUG_TRACE;
  m_pos = 0;
  m_next_pos = 0;
}

ha_rows table_rpl_async_connection_failover_managed::get_row_count() {
  DBUG_TRACE;
  return (ha_rows)table_rpl_async_connection_failover_managed::num_rows;
}

int table_rpl_async_connection_failover_managed::rnd_init(bool) {
  DBUG_TRACE;
  Rpl_async_conn_failover_table_operations table_op(TL_READ);
  std::vector<RPL_FAILOVER_MANAGED_TUPLE> source_list;
  read_error = table_op.read_managed_random_rows(source_list);

  for (auto source_detail : source_list) {
    std::stringstream json_str;
    json_str << "{\"Primary_weight\": " << std::get<3>(source_detail)
             << ", \"Secondary_weight\": " << std::get<4>(source_detail) << "}";

    auto res_dom = Json_dom::parse(json_str.str().c_str(),
                                   json_str.str().length(), nullptr, nullptr);

    if (res_dom == nullptr ||
        res_dom->json_type() != enum_json_type::J_OBJECT) {
      my_error(ER_INVALID_USER_ATTRIBUTE_JSON, MYF(0));
      return 1;
    }

    Json_object_ptr res_obj_ptr(down_cast<Json_object *>(res_dom.release()));
    Json_dom_ptr json_dom = create_dom_ptr<Json_object>();
    Json_object *json_ob = down_cast<Json_object *>(json_dom.get());
    json_ob->merge_patch(std::move(res_obj_ptr));
    Json_wrapper wrapper(json_ob->clone());

    auto source_mng_detail =
        std::make_tuple(std::get<0>(source_detail), std::get<1>(source_detail),
                        std::get<2>(source_detail), wrapper);

    source_managed_list.push_back(source_mng_detail);
  }
  return 0;
}

int table_rpl_async_connection_failover_managed::rnd_next(void) {
  DBUG_TRACE;
  if (read_error) return 1;

  table_rpl_async_connection_failover_managed::num_rows =
      source_managed_list.size();

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < source_managed_list.size();
       m_pos.next()) {
    auto source_managed_tuple = source_managed_list[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    return make_row(source_managed_tuple);
  }

  return HA_ERR_END_OF_FILE;
}

int table_rpl_async_connection_failover_managed::rnd_pos(const void *pos) {
  DBUG_TRACE;
  RPL_FAILOVER_MANAGED_JSON_TUPLE source_managed_tuple;
  bool error{false};
  int res{HA_ERR_RECORD_DELETED};

  set_position(pos);
  auto upos = std::to_string(m_pos.m_index);
  Rpl_async_conn_failover_table_operations table_op(TL_READ);
  std::tie(error, source_managed_tuple) =
      table_op.read_managed_random_rows_pos(upos);

  table_rpl_async_connection_failover_managed::num_rows = 1;
  if (error) return res;

  res = make_row(source_managed_tuple);
  return res;
}

int table_rpl_async_connection_failover_managed::index_init(
    uint idx MY_ATTRIBUTE((unused)), bool) {
  DBUG_TRACE;
  PFS_index_rpl_async_conn_failover_managed *result = nullptr;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_rpl_async_conn_failover_managed);
  m_opened_index = result;
  m_index = result;

  Rpl_async_conn_failover_table_operations table_op(TL_READ);

  std::vector<RPL_FAILOVER_MANAGED_TUPLE> source_list;
  read_error = table_op.read_managed_random_rows(source_list);

  for (auto source_detail : source_list) {
    std::stringstream json_str;
    json_str << "{\"Primary_weight\": " << std::get<3>(source_detail)
             << ", \"Secondary_weight\": " << std::get<4>(source_detail) << "}";

    auto res_dom = Json_dom::parse(json_str.str().c_str(),
                                   json_str.str().length(), nullptr, nullptr);

    if (res_dom == nullptr ||
        res_dom->json_type() != enum_json_type::J_OBJECT) {
      // my_error(ER_INVALID_USER_ATTRIBUTE_JSON, MYF(0));
      return 1;
    }

    Json_object_ptr res_obj_ptr(down_cast<Json_object *>(res_dom.release()));
    Json_dom_ptr json_dom = create_dom_ptr<Json_object>();
    Json_object *json_ob = down_cast<Json_object *>(json_dom.get());
    json_ob->merge_patch(std::move(res_obj_ptr));
    Json_wrapper wrapper(json_ob->clone());

    auto source_mng_detail =
        std::make_tuple(std::get<0>(source_detail), std::get<1>(source_detail),
                        std::get<2>(source_detail), wrapper);

    source_managed_list.push_back(source_mng_detail);
  }
  return 0;
}

int table_rpl_async_connection_failover_managed::index_next(void) {
  DBUG_TRACE;
  if (read_error) return 1;

  table_rpl_async_connection_failover_managed::num_rows =
      source_managed_list.size();

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < source_managed_list.size();
       m_pos.next()) {
    auto source_managed_tuple = source_managed_list[m_pos.m_index];
    if (m_opened_index->match(source_managed_tuple)) {
      m_next_pos.set_after(&m_pos);
      return make_row(source_managed_tuple);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_rpl_async_connection_failover_managed::make_row(
    RPL_FAILOVER_MANAGED_JSON_TUPLE source_tuple) {
  DBUG_TRACE;
  std::string channel{};
  std::string managed_name{};
  std::string managed_type{};
  Json_wrapper configuration;
  Json_object json_config;

  std::tie(channel, managed_name, managed_type, configuration) = source_tuple;

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
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (Field *f = nullptr; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /** channel_name */
          set_field_char_utf8(f, m_row.channel_name, m_row.channel_name_length);
          break;
        case 1: /** managed_name */
          set_field_char_utf8(f, m_row.managed_name, m_row.managed_name_length);
          break;
        case 2: /** managed_type */
          set_field_char_utf8(f, m_row.managed_type, m_row.managed_type_length);
          break;
        case 3: /** configuration */
          set_field_json(f, &m_row.configuration);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
