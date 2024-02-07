/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "mysql/components/services/log_builtins.h"

#include "sql-common/json_dom.h"
#include "sql/current_thd.h"
#include "sql/handler.h"
#include "sql/mysqld.h"
#include "sql/protobuf/generated/protobuf_lite/replication_asynchronous_connection_failover.pb.h"
#include "sql/rpl_async_conn_failover_configuration_propagation.h"
#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/rpl_sys_table_access.h"
#include "sql/sql_base.h"  // MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK
#include "sql/udf_service_util.h"

const MYSQL_LEX_CSTRING
    Rpl_async_conn_failover_table_operations::Primary_weight_key = {
        STRING_WITH_LEN("Primary_weight")};
const MYSQL_LEX_CSTRING
    Rpl_async_conn_failover_table_operations::Secondary_weight_key = {
        STRING_WITH_LEN("Secondary_weight")};

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
    const std::string &network_namespace, uint weight,
    const std::string &managed_name) {
  DBUG_TRACE;
  assert(network_namespace.empty());

  std::vector<uint> field_index{0, 1, 2, 3, 4, 5};
  std::vector<std::string> field_name{
      "channel", "host", "port", "network_namespace", "weight", "managed_name"};
  RPL_FAILOVER_SOURCE_TUPLE field_value{
      channel, host, port, network_namespace, weight, managed_name};
  std::string serialized_configuration;

  return execute_handler_func_send<RPL_FAILOVER_SOURCE_TUPLE>(
      m_db, m_table_failover, m_table_failover_num_field, m_lock_type,
      field_index, field_name, field_value,
      Rpl_sys_table_access::handler_write_row_func, 0, HA_WHOLE_KEY);
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::add_source_skip_send(
    const std::string &channel, const std::string &host, uint port,
    const std::string &network_namespace, uint weight,
    const std::string &managed_name, Rpl_sys_table_access &table_op) {
  DBUG_TRACE;
  assert(network_namespace.empty());

  std::vector<uint> field_index{0, 1, 2, 3, 4, 5};
  std::vector<std::string> field_name{
      "channel", "host", "port", "network_namespace", "weight", "managed_name"};
  RPL_FAILOVER_SOURCE_TUPLE field_value{
      channel, host, port, network_namespace, weight, managed_name};
  std::string serialized_configuration;

  return execute_handler_func_skip_send<RPL_FAILOVER_SOURCE_TUPLE>(
      field_index, field_name, field_value,
      Rpl_sys_table_access::handler_write_row_func, 0, HA_WHOLE_KEY, table_op);
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::add_managed(
    const std::string &channel, const std::string &host, uint port,
    const std::string &network_namespace, const std::string &managed_type,
    const std::string &managed_name, uint primary_weight,
    uint secondary_weight) {
  DBUG_TRACE;
  assert(network_namespace.empty());

  std::stringstream json_str;
  json_str << "{\"Primary_weight\": " << primary_weight
           << ", \"Secondary_weight\": " << secondary_weight << "}";

  auto res_dom = Json_dom::parse(
      json_str.str().c_str(), json_str.str().length(),
      [](const char *, size_t) {}, [] {});

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
  std::tuple<bool, std::string> ret_val{};
  std::string serialized_configuration;

  ret_val = execute_handler_func_send<RPL_FAILOVER_MANAGED_JSON_TUPLE>(
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

  ret_val = execute_handler_func_send<RPL_FAILOVER_SOURCE_TUPLE>(
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
Rpl_async_conn_failover_table_operations::add_managed_skip_send(
    const std::string &channel, const std::string &managed_type,
    const std::string &managed_name, const Json_wrapper &wrapper,
    Rpl_sys_table_access &table_op) {
  DBUG_TRACE;

  // add managed
  std::vector<uint> managed_field_index{0, 1, 2, 3};
  std::vector<std::string> managed_field_name{"channel", "managed_name",
                                              "managed_type", "configuration"};
  RPL_FAILOVER_MANAGED_JSON_TUPLE managed_field_value{channel, managed_name,
                                                      managed_type, wrapper};
  std::tuple<bool, std::string> ret_val{};
  std::string serialized_configuration;

  ret_val = execute_handler_func_skip_send<RPL_FAILOVER_MANAGED_JSON_TUPLE>(
      managed_field_index, managed_field_name, managed_field_value,
      Rpl_sys_table_access::handler_write_row_func, 0, HA_WHOLE_KEY, table_op);

  if (std::get<0>(ret_val)) {
    return ret_val;
  }

  return ret_val;
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::delete_source(
    const std::string &channel, const std::string &host, uint port,
    const std::string &network_namespace) {
  DBUG_TRACE;
  assert(network_namespace.empty());

  std::vector<uint> field_index{0, 1, 2, 3, 5};
  std::vector<std::string> field_name{"channel", "host", "port",
                                      "network_namespace", "managed_name"};
  RPL_FAILOVER_SOURCE_DEL_TUPLE field_value{channel, host, port,
                                            network_namespace, ""};

  return execute_handler_func_send<RPL_FAILOVER_SOURCE_DEL_TUPLE>(
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
      execute_handler_func_send<RPL_FAILOVER_MANAGED_DEL_TUPLE>(
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
  ret_val = execute_handler_func_send<RPL_FAILOVER_MANAGED_DEL_TUPLE>(
      m_db, m_table_failover, m_table_failover_num_field, m_lock_type,
      field_index, field_name, field_value,
      Rpl_sys_table_access::handler_delete_row_func, 1, (1L << 0) | (1L << 1));

  return ret_val;
}

bool Rpl_async_conn_failover_table_operations::reset() {
  DBUG_TRACE;
  bool error = false;

  Rpl_sys_table_access table_op_managed(m_db, m_table_managed,
                                        m_table_managed_num_field);
  Rpl_sys_table_access table_op_failover(m_db, m_table_failover,
                                         m_table_failover_num_field);

  if (table_op_managed.open(m_lock_type)) {
    return true;
  }
  error |= table_op_managed.delete_all_rows();
  error |= table_op_managed.delete_version();
  error |= table_op_managed.close(error);

  if (!error) {
    if (table_op_failover.open(m_lock_type)) {
      return true;
    }
    error |= table_op_failover.delete_all_rows();
    error |= table_op_failover.delete_version();
    error |= table_op_failover.close(error);
  }

  return error;
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

  return (key_access.deinit() || table_op.close(false));
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

  auto error = key_access.deinit() || table_op.close(false);
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

  auto error = key_access.deinit() || table_op.close(false);
  return std::make_tuple(error, source_list);
}

std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
Rpl_async_conn_failover_table_operations::read_source_all_rows_internal(
    Rpl_sys_table_access &table_op) {
  DBUG_TRACE;
  std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_list{};
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

  bool error = key_access.deinit();
  return std::make_tuple(error, source_list);
}

std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
Rpl_async_conn_failover_table_operations::read_source_all_rows() {
  bool error = false;
  std::vector<RPL_FAILOVER_SOURCE_TUPLE> source_list{};

  Rpl_sys_table_access table_op(m_db, m_table_failover,
                                m_table_failover_num_field);
  if (table_op.open(m_lock_type)) {
    return std::make_tuple(true, source_list);
  }

  std::tie(error, source_list) = read_source_all_rows_internal(table_op);

  error |= table_op.close(error);
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

  auto error = key_access.deinit() || table_op.close(false);
  return std::make_tuple(error, source_list);
}

bool Rpl_async_conn_failover_table_operations::
    read_managed_random_rows_internal(
        Rpl_sys_table_access &table_op,
        std::vector<RPL_FAILOVER_MANAGED_TUPLE> &rows) {
  DBUG_TRACE;
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

  bool error = key_access.deinit();
  return error;
}

bool Rpl_async_conn_failover_table_operations::
    read_managed_random_rows_internal(
        Rpl_sys_table_access &table_op,
        std::vector<RPL_FAILOVER_MANAGED_JSON_TUPLE> &rows) {
  DBUG_TRACE;
  TABLE *table = table_op.get_table();

  Rpl_sys_key_access key_access;
  if (!key_access.init(table, Rpl_sys_key_access::enum_key_type::RND_NEXT)) {
    do {
      /* get source detail */
      RPL_FAILOVER_MANAGED_JSON_TUPLE source_tuple{};
      get_data<RPL_FAILOVER_MANAGED_JSON_TUPLE>(table_op, source_tuple);
      rows.push_back(source_tuple);
    } while (!key_access.next());
  }

  bool error = key_access.deinit();
  return error;
}

bool Rpl_async_conn_failover_table_operations::read_managed_random_rows(
    std::vector<RPL_FAILOVER_MANAGED_TUPLE> &rows) {
  DBUG_TRACE;
  bool error = false;

  Rpl_sys_table_access table_op(m_db, m_table_managed,
                                m_table_managed_num_field);
  if (table_op.open(m_lock_type)) {
    return true;
  }

  error = read_managed_random_rows_internal(table_op, rows);

  error |= table_op.close(error);
  return error;
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
Rpl_async_conn_failover_table_operations::execute_handler_func_skip_send(
    const std::vector<uint> &field_index,
    const std::vector<std::string> &field_name, const T &field_value,
    std::function<void(Rpl_sys_table_access &, bool &, std::string &, uint &,
                       key_part_map &)>
        func,
    uint table_index, key_part_map keypart_map,
    Rpl_sys_table_access &table_op) {
  std::ostringstream str_stream;
  bool err_val{false};
  std::string err_msg{};
  std::string serialized_configuration;

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

  return std::make_tuple(err_val, err_msg);
}

template <class T>
std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::execute_handler_func_send(
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
  std::string serialized_configuration;

  Rpl_sys_table_access table_op(db_name, table_name, num_field);
  if (table_op.open(lock_type)) {
    table_op.set_error();
    str_stream << "Error opening " << db_name << "." << table_name << " table.";
    return std::make_tuple(true, str_stream.str());
  }

  std::tie(err_val, err_msg) = execute_handler_func_skip_send<T>(
      field_index, field_name, field_value, func, table_index, keypart_map,
      table_op);

  if (!err_val) {
    if (table_op.increment_version()) {
      str_stream << "Error incrementing member action configuration version"
                 << " for " << table_op.get_db_name() << "."
                 << table_op.get_table_name() << " table.";
      return std::make_tuple(true, str_stream.str());
    }

    if (rpl_acf_configuration_handler->send_table_data(table_op)) {
      str_stream << "Error sending " << db_name << "." << table_name
                 << " table data to group replication members.";
      return std::make_tuple(true, str_stream.str());
    }
  }

  return std::make_tuple(err_val, err_msg);
}
