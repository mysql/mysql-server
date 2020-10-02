/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/components/services/log_builtins.h"

#include "sql/current_thd.h"
#include "sql/handler.h"
#include "sql/json_dom.h"
#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/rpl_sys_table_access.h"
#include "sql/sql_base.h"  // MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK
#include "sql/udf_service_util.h"

/*
 Only used on this file.
*/
/* <channel, host, port, network_namespace, managed_name> */
using RPL_FAILOVER_SOURCE_DEL_TUPLE =
    std::tuple<std::string, std::string, uint, std::string, std::string>;

/* <channel, managed_name> */
using RPL_FAILOVER_MANAGED_DEL_TUPLE = std::tuple<std::string, std::string>;

template void Rpl_async_conn_failover_table_operations::get_data(
    Rpl_sys_table_access &table_op, RPL_FAILOVER_SOURCE_TUPLE &rows);

template void Rpl_async_conn_failover_table_operations::get_data(
    Rpl_sys_table_access &table_op, RPL_FAILOVER_MANAGED_JSON_TUPLE &rows);

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::add_source(
    const std::string &channel, const std::string &host, uint port,
    const std::string &network_namespace, uint weight) {
  DBUG_TRACE;

  std::vector<uint> field_index{0, 1, 2, 3, 4, 5};
  std::vector<std::string> field_name{
      "channel", "host", "port", "network_namespace", "weight", "managed_name"};
  RPL_FAILOVER_SOURCE_TUPLE field_value{channel,           host,   port,
                                        network_namespace, weight, ""};

  return execute_handler_func<RPL_FAILOVER_SOURCE_TUPLE>(
      m_db, m_table_failover, m_table_failover_num_field, m_lock_type,
      field_index, field_name, field_value,
      Rpl_sys_table_access::handler_write_row_func, 0, HA_WHOLE_KEY);
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::add_managed(
    const std::string &channel, const std::string &host, uint port,
    const std::string &network_namespace, const std::string &managed_type,
    const std::string &managed_name, uint primary_weight,
    uint secondary_weight) {
  DBUG_TRACE;

  std::stringstream json_str;
  json_str << "{\"Primary_weight\": " << primary_weight
           << ", \"Secondary_weight\": " << secondary_weight << "}";

  auto res_dom = Json_dom::parse(json_str.str().c_str(),
                                 json_str.str().length(), nullptr, nullptr);

  if (res_dom == nullptr || res_dom->json_type() != enum_json_type::J_OBJECT) {
    return std::make_tuple(true, "Error parsing Json value.");
  }

  Json_object_ptr res_obj_ptr(down_cast<Json_object *>(res_dom.release()));
  Json_dom_ptr json_dom = create_dom_ptr<Json_object>();
  Json_object *json_ob = down_cast<Json_object *>(json_dom.get());
  json_ob->merge_patch(std::move(res_obj_ptr));
  Json_wrapper wrapper(json_ob->clone());

  // add managed
  std::vector<uint> managed_field_index{0, 1, 2, 3};
  std::vector<std::string> managed_field_name{"channel", "managed_name",
                                              "managed_type", "configuration"};
  RPL_FAILOVER_MANAGED_JSON_TUPLE managed_field_value{channel, managed_name,
                                                      managed_type, wrapper};

  std::tuple<bool, std::string> ret_val =
      execute_handler_func<RPL_FAILOVER_MANAGED_JSON_TUPLE>(
          m_db, m_table_managed, m_table_managed_num_field, m_lock_type,
          managed_field_index, managed_field_name, managed_field_value,
          Rpl_sys_table_access::handler_write_row_func, 0, HA_WHOLE_KEY);
  if (std::get<0>(ret_val)) {
    return ret_val;
  }

  LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_MANAGED_NAME_ADDED, managed_name.c_str(),
         channel.c_str());

  // add source
  std::vector<uint> field_index{0, 1, 2, 3, 4, 5};
  std::vector<std::string> field_name{
      "channel", "host", "port", "network_namespace", "weight", "managed_name"};
  RPL_FAILOVER_SOURCE_TUPLE field_value{
      channel, host, port, network_namespace, secondary_weight, managed_name};

  ret_val = execute_handler_func<RPL_FAILOVER_SOURCE_TUPLE>(
      m_db, m_table_failover, m_table_failover_num_field, m_lock_type,
      field_index, field_name, field_value,
      Rpl_sys_table_access::handler_write_row_func, 0, HA_WHOLE_KEY);
  if (!std::get<0>(ret_val)) {
    LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_SENDER_ADDED, host.c_str(), port,
           network_namespace.c_str(), channel.c_str(), managed_name.c_str());
  }

  return ret_val;
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::delete_source(
    const std::string &channel, const std::string &host, uint port,
    const std::string &network_namespace) {
  DBUG_TRACE;

  std::vector<uint> field_index{0, 1, 2, 3, 5};
  std::vector<std::string> field_name{"channel", "host", "port",
                                      "network_namespace", "managed_name"};
  RPL_FAILOVER_SOURCE_DEL_TUPLE field_value{channel, host, port,
                                            network_namespace, ""};

  return execute_handler_func<RPL_FAILOVER_SOURCE_DEL_TUPLE>(
      m_db, m_table_failover, m_table_failover_num_field, m_lock_type,
      field_index, field_name, field_value,
      Rpl_sys_table_access::handler_delete_row_func, 0, HA_WHOLE_KEY);
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::delete_managed(
    const std::string &channel, const std::string &managed_name) {
  DBUG_TRACE;

  // delete managed
  std::vector<uint> managed_field_index{0, 1};
  std::vector<std::string> field_name{"channel", "managed_name"};
  RPL_FAILOVER_MANAGED_DEL_TUPLE field_value{channel, managed_name};

  std::tuple<bool, std::string> ret_val =
      execute_handler_func<RPL_FAILOVER_MANAGED_DEL_TUPLE>(
          m_db, m_table_managed, m_table_managed_num_field, m_lock_type,
          managed_field_index, field_name, field_value,
          Rpl_sys_table_access::handler_delete_row_func, 0, HA_WHOLE_KEY);
  if (std::get<0>(ret_val)) {
    return ret_val;
  }

  LogErr(SYSTEM_LEVEL, ER_RPL_ASYNC_MANAGED_NAME_REMOVED, managed_name.c_str(),
         channel.c_str());

  // delete source
  std::vector<uint> field_index{0, 5};
  ret_val = execute_handler_func<RPL_FAILOVER_MANAGED_DEL_TUPLE>(
      m_db, m_table_failover, m_table_failover_num_field, m_lock_type,
      field_index, field_name, field_value,
      Rpl_sys_table_access::handler_delete_row_func, 1, (1L << 0) | (1L << 1));

  return ret_val;
}

bool Rpl_async_conn_failover_table_operations::read_managed_rows_for_channel(
    std::string channel_name, std::vector<RPL_FAILOVER_MANAGED_TUPLE> &rows) {
  DBUG_TRACE;

  Rpl_sys_table_access table_op(m_db, m_table_managed,
                                m_table_managed_num_field);
  if (table_op.open(m_lock_type)) {
    return true;
  }

  TABLE *table = table_op.get_table();

  /* Store channel */
  if (table_op.store_field(table->field[0], channel_name)) {
    return true;
  }

  Rpl_sys_key_access key_access;
  if (!key_access.init(table)) {
    do {
      /* get source detail */
      RPL_FAILOVER_MANAGED_JSON_TUPLE source_tuple{};
      get_data<RPL_FAILOVER_MANAGED_JSON_TUPLE>(table_op, source_tuple);

      auto primary_weight{0}, secondary_weight{0};
      Json_wrapper json_val =
          std::get<3>(source_tuple).lookup(Primary_weight_key);
      if ((!json_val.empty() || json_val.type() != enum_json_type::J_ERROR) &&
          json_val.type() == enum_json_type::J_INT) {
        primary_weight = json_val.get_int();
      }

      json_val = std::get<3>(source_tuple).lookup(Secondary_weight_key);
      if ((!json_val.empty() || json_val.type() != enum_json_type::J_ERROR) &&
          json_val.type() == enum_json_type::J_INT) {
        secondary_weight = json_val.get_int();
      }

      rows.push_back(std::make_tuple(
          std::get<0>(source_tuple), std::get<1>(source_tuple),
          std::get<2>(source_tuple), primary_weight, secondary_weight));
    } while (!key_access.next());
  }

  return (key_access.deinit() || table_op.close(true));
}

std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
Rpl_async_conn_failover_table_operations::read_source_rows_for_channel(
    std::string channel_name) {
  DBUG_TRACE;
  std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_list{};

  Rpl_sys_table_access table_op(m_db, m_table_failover,
                                m_table_failover_num_field);
  if (table_op.open(m_lock_type)) {
    return std::make_tuple(true, source_list);
  }

  TABLE *table = table_op.get_table();

  /* Store channel */
  if (table_op.store_field(table->field[0], channel_name)) {
    return std::make_tuple(true, source_list);
  }

  Rpl_sys_key_access key_access;
  if (!key_access.init(table)) {
    do {
      /* get source detail */
      RPL_FAILOVER_SOURCE_TUPLE source_tuple{};
      get_data<RPL_FAILOVER_SOURCE_TUPLE>(table_op, source_tuple);
      source_list.push_back(source_tuple);
    } while (!key_access.next());
  }

  auto error = key_access.deinit() || table_op.close(true);
  return std::make_tuple(error, source_list);
}

std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
Rpl_async_conn_failover_table_operations::
    read_source_rows_for_channel_and_managed_name(std::string channel_name,
                                                  std::string managed_name) {
  DBUG_TRACE;
  std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_list{};

  Rpl_sys_table_access table_op(m_db, m_table_failover,
                                m_table_failover_num_field);
  if (table_op.open(m_lock_type)) {
    return std::make_tuple(true, source_list);
  }

  TABLE *table = table_op.get_table();

  /* Store channel */
  if (table_op.store_field(table->field[0], channel_name)) {
    return std::make_tuple(true, source_list);
  }

  /* Store managed_name */
  if (table_op.store_field(table->field[5], managed_name)) {
    return std::make_tuple(true, source_list);
  }

  Rpl_sys_key_access key_access;
  if (!key_access.init(table, 1, true, (key_part_map)((1L << 0) | (1L << 1)),
                       HA_READ_KEY_EXACT)) {
    do {
      /* get source detail */
      RPL_FAILOVER_SOURCE_TUPLE source_tuple{};
      get_data<RPL_FAILOVER_SOURCE_TUPLE>(table_op, source_tuple);
      source_list.push_back(source_tuple);
    } while (!key_access.next());
  }

  auto error = key_access.deinit() || table_op.close(true);
  return std::make_tuple(error, source_list);
}

std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
Rpl_async_conn_failover_table_operations::read_source_all_rows() {
  DBUG_TRACE;
  std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_list{};

  Rpl_sys_table_access table_op(m_db, m_table_failover,
                                m_table_failover_num_field);
  if (table_op.open(m_lock_type)) {
    return std::make_tuple(true, source_list);
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  if (!key_access.init(table, Rpl_sys_key_access::enum_key_type::INDEX_NEXT)) {
    do {
      /* get source detail */
      RPL_FAILOVER_SOURCE_TUPLE source_tuple{};
      get_data<RPL_FAILOVER_SOURCE_TUPLE>(table_op, source_tuple);
      source_list.push_back(source_tuple);
    } while (!key_access.next());
  }

  auto error = key_access.deinit() || table_op.close(true);
  return std::make_tuple(error, source_list);
}

std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
Rpl_async_conn_failover_table_operations::read_source_random_rows() {
  DBUG_TRACE;
  std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_list{};

  Rpl_sys_table_access table_op(m_db, m_table_failover,
                                m_table_failover_num_field);
  if (table_op.open(m_lock_type)) {
    return std::make_tuple(true, source_list);
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  if (!key_access.init(table, Rpl_sys_key_access::enum_key_type::RND_NEXT)) {
    do {
      /* get source detail */
      RPL_FAILOVER_SOURCE_TUPLE source_tuple{};
      get_data<RPL_FAILOVER_SOURCE_TUPLE>(table_op, source_tuple);
      source_list.push_back(source_tuple);
    } while (!key_access.next());
  }

  auto error = key_access.deinit() || table_op.close(true);
  return std::make_tuple(error, source_list);
}

bool Rpl_async_conn_failover_table_operations::read_managed_random_rows(
    std::vector<RPL_FAILOVER_MANAGED_TUPLE> &rows) {
  DBUG_TRACE;

  Rpl_sys_table_access table_op(m_db, m_table_managed,
                                m_table_managed_num_field);
  if (table_op.open(m_lock_type)) {
    return true;
  }

  TABLE *table = table_op.get_table();

  Rpl_sys_key_access key_access;
  if (!key_access.init(table, Rpl_sys_key_access::enum_key_type::RND_NEXT)) {
    do {
      /* get source detail */
      RPL_FAILOVER_MANAGED_JSON_TUPLE source_tuple{};
      get_data<RPL_FAILOVER_MANAGED_JSON_TUPLE>(table_op, source_tuple);

      auto primary_weight{0}, secondary_weight{0};
      Json_wrapper json_val =
          std::get<3>(source_tuple).lookup(Primary_weight_key);
      if ((!json_val.empty() || json_val.type() != enum_json_type::J_ERROR) &&
          json_val.type() == enum_json_type::J_INT) {
        primary_weight = json_val.get_int();
      }

      json_val = std::get<3>(source_tuple).lookup(Secondary_weight_key);
      if ((!json_val.empty() || json_val.type() != enum_json_type::J_ERROR) &&
          json_val.type() == enum_json_type::J_INT) {
        secondary_weight = json_val.get_int();
      }

      rows.push_back(std::make_tuple(
          std::get<0>(source_tuple), std::get<1>(source_tuple),
          std::get<2>(source_tuple), primary_weight, secondary_weight));
    } while (!key_access.next());
  }

  return (key_access.deinit() || table_op.close(true));
}

std::tuple<bool, RPL_FAILOVER_SOURCE_TUPLE>
Rpl_async_conn_failover_table_operations::read_source_random_rows_pos(
    std::string pos) {
  DBUG_TRACE;
  RPL_FAILOVER_SOURCE_TUPLE source_detail{};

  Rpl_sys_table_access table_op(m_db, m_table_failover,
                                m_table_failover_num_field);
  if (table_op.open(m_lock_type)) {
    return std::make_tuple(true, source_detail);
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;

  if (!key_access.init(table, pos)) {
    if (!key_access.next()) {
      /* get source connection detail */
      get_data<RPL_FAILOVER_SOURCE_TUPLE>(table_op, source_detail);
    }
  }

  auto error = key_access.deinit() || table_op.close(true);
  return std::make_tuple(error, source_detail);
}

std::tuple<bool, RPL_FAILOVER_MANAGED_JSON_TUPLE>
Rpl_async_conn_failover_table_operations::read_managed_random_rows_pos(
    std::string pos) {
  DBUG_TRACE;
  RPL_FAILOVER_MANAGED_JSON_TUPLE source_detail{};

  Rpl_sys_table_access table_op(m_db, m_table_managed,
                                m_table_managed_num_field);
  if (table_op.open(m_lock_type)) {
    return std::make_tuple(true, source_detail);
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;

  if (!key_access.init(table, pos)) {
    if (!key_access.next()) {
      /* get source connection detail */
      get_data<RPL_FAILOVER_MANAGED_JSON_TUPLE>(table_op, source_detail);
    }
  }

  auto error = key_access.deinit() || table_op.close(true);
  return std::make_tuple(error, source_detail);
}

template <class TUP>
void Rpl_async_conn_failover_table_operations::get_data(
    Rpl_sys_table_access &table_op, TUP &rows) {
  DBUG_TRACE;
  TABLE *table = table_op.get_table();
  Field **fields{table->field};

  Rpl_sys_table_access::for_each_in_tuple(
      rows, [&](const auto &n, auto &x) { table_op.get_field(fields[n], x); });
}

template <class T>
std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::execute_handler_func(
    const std::string &db_name, const std::string &table_name, uint num_field,
    enum thr_lock_type lock_type, const std::vector<uint> &field_index,
    const std::vector<std::string> &field_name, const T &field_value,
    std::function<void(Rpl_sys_table_access &, bool &, std::string &, uint &,
                       key_part_map &)>
        func,
    uint table_index, key_part_map keypart_map) {
  std::ostringstream str_stream;
  bool err_val{false};
  std::string err_msg{};

  Rpl_sys_table_access table_op(db_name, table_name, num_field);
  if (table_op.open(lock_type)) {
    table_op.set_error();
    str_stream << "Error opening " << db_name << "." << table_name << " table.";
    return std::make_tuple(true, str_stream.str());
  }
  TABLE *table = table_op.get_table();

  Rpl_sys_table_access::for_each_in_tuple(
      field_value, [&](const auto &n, const auto &x) {
        if (table_op.store_field(table->field[field_index[n]], x)) {
          err_msg.assign(table_op.get_field_error_msg(field_name[n]));
          err_val = true;
        }
      });

  if (err_val) {
    table_op.set_error();
    return std::make_tuple(err_val, err_msg);
  }

  /* Call handler function to write/delete... into the table. */
  func(table_op, err_val, err_msg, table_index, keypart_map);
  if (err_val) {
    table_op.set_error();
    return std::make_tuple(err_val, err_msg);
  }

  if (table_op.close(err_val)) {
    str_stream << "Error closing " << db_name << "." << table_name << " table.";
    return std::make_tuple(true, str_stream.str());
  }

  return std::make_tuple(err_val, err_msg);
}
