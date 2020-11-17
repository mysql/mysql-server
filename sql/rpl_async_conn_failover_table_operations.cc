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
#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/rpl_sys_table_access.h"
#include "sql/sql_base.h"  // MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK

bool Rpl_async_conn_failover_table_operations::store_field(
    Field *field, std::string fld, CHARSET_INFO *cs = &my_charset_bin) {
  DBUG_TRACE;
  field->set_notnull();
  return field->store(fld.c_str(), fld.length(), cs);
}

bool Rpl_async_conn_failover_table_operations::store_field(Field *field,
                                                           long long fld,
                                                           bool unsigned_val) {
  DBUG_TRACE;
  field->set_notnull();
  return field->store(fld, unsigned_val);
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::insert_row(
    SENDER_CONN_TUPLE source_conn_detail) {
  DBUG_TRACE;
  TABLE *table{nullptr};
  Field **fields{nullptr};

  std::string channel{};
  std::string host{};
  uint port{0};
  std::string network_namespace{};
  uint weight{0};

  std::string err_msg{
      "Error executing asynchronous_connection_failover_add_source() UDF "
      "function."};

  Rpl_sys_table_access table_op;
  if (table_op.init(m_db, m_table, m_num_field, m_lock_type)) {
    return std::make_tuple(true, err_msg);
  }

  table = table_op.get_table();
  fields = table->field;
  std::tie(weight, channel, host, port, network_namespace) = source_conn_detail;

  /* Store channel */
  if (store_field(fields[0], channel)) {
    err_msg.assign(
        "Error saving channel field of "
        "asynchronous_connection_failover_add_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Store host */
  if (store_field(fields[1], host)) {
    err_msg.assign(
        "Error saving host field of "
        "asynchronous_connection_failover_add_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Store port */
  if (store_field(fields[2], port, true)) {
    err_msg.assign(
        "Error saving port field of "
        "asynchronous_connection_failover_add_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Store network_namespace */
  if (store_field(fields[3], network_namespace)) {
    err_msg.assign(
        "Error saving network_namespace field of "
        "asynchronous_connection_failover_add_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Store weight */
  if (store_field(fields[4], weight, true)) {
    err_msg.assign(
        "Error saving weight field of "
        "asynchronous_connection_failover_add_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Inserts a new row into the table. */
  if (table->file->ha_write_row(table->record[0]) || table_op.deinit()) {
    return std::make_tuple(true, err_msg);
  }

  err_msg.assign(
      "The UDF asynchronous_connection_failover_add_source()"
      " executed successfully.");
  return std::make_tuple(false, err_msg);
}

std::tuple<bool, std::string>
Rpl_async_conn_failover_table_operations::delete_row(
    SENDER_CONN_TUPLE source_conn_detail) {
  DBUG_TRACE;
  TABLE *table{nullptr};
  Field **fields{nullptr};

  std::string channel{};
  std::string host{};
  uint port{0};
  std::string network_namespace{};

  std::string err_msg{
      "Error executing asynchronous_connection_failover_delete_source() UDF "
      "function."};

  Rpl_sys_table_access table_op;
  if (table_op.init(m_db, m_table, m_num_field, m_lock_type)) {
    return std::make_tuple(true, err_msg);
  }

  table = table_op.get_table();
  fields = table->field;
  std::tie(std::ignore, channel, host, port, network_namespace) =
      source_conn_detail;

  /*
    create the search key on channel name, host, port and network_namespace
    (if provided).
  */
  /* Store channel */
  if (store_field(fields[0], channel)) {
    err_msg.assign(
        "Error saving channel field of "
        "asynchronous_connection_failover_delete_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Store host */
  if (store_field(fields[1], host)) {
    err_msg.assign(
        "Error saving host field of "
        "asynchronous_connection_failover_delete_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Store port */
  if (store_field(fields[2], port, true)) {
    err_msg.assign(
        "Error saving port field of "
        "asynchronous_connection_failover_delete_source().");
    return std::make_tuple(true, err_msg);
  }

  /* Store network_namespace */
  if (store_field(fields[3], network_namespace)) {
    err_msg.assign(
        "Error saving network_namespace field of "
        "asynchronous_connection_failover_delete_source().");
    return std::make_tuple(true, err_msg);
  }

  Rpl_sys_key_access key_access;
  int key_error = key_access.init(table);

  if (key_error == HA_ERR_KEY_NOT_FOUND) {
    err_msg.assign("Error no matching row was found to be deleted.");
    return std::make_tuple(true, err_msg);
  }

  if (key_error || table->file->ha_delete_row(table->record[0])) {
    return std::make_tuple(true, err_msg);
  }

  if (key_access.deinit() || table_op.deinit()) {
    return std::make_tuple(true, err_msg);
  }

  err_msg.assign(
      "The UDF asynchronous_connection_failover_delete_source() executed "
      "successfully.");
  return std::make_tuple(false, err_msg);
}

SENDER_CONN_TUPLE Rpl_async_conn_failover_table_operations::get_data(
    TABLE *table) {
  DBUG_TRACE;
  SENDER_CONN_TUPLE source_conn_detail{};

  char buff_channel[MAX_FIELD_WIDTH];
  String channel(buff_channel, sizeof(buff_channel), &my_charset_bin);

  char buff_host[MAX_FIELD_WIDTH];
  String host(buff_host, sizeof(buff_host), &my_charset_bin);

  char buff_ns[MAX_FIELD_WIDTH];
  String net_ns(buff_ns, sizeof(buff_ns), &my_charset_bin);

  table->field[0]->val_str(&channel);
  std::string channel_str(channel.c_ptr_safe(), channel.length());

  table->field[1]->val_str(&host);
  std::string host_str(host.c_ptr_safe(), host.length());

  uint port = (uint)table->field[2]->val_int();

  table->field[3]->val_str(&net_ns);
  std::string net_str(net_ns.c_ptr_safe(), net_ns.length());

  uint weight = (uint)table->field[4]->val_int();

  /* add to source credentail list */
  return std::make_tuple(weight, channel_str, host_str, port, net_str);
}

std::tuple<bool, SENDER_CONN_LIST>
Rpl_async_conn_failover_table_operations::read_rows(std::string channel_name) {
  DBUG_TRACE;
  SENDER_CONN_LIST source_conn_detail{};

  Rpl_sys_table_access table_op;
  if (table_op.init(m_db, m_table, m_num_field, m_lock_type)) {
    return std::make_tuple(true, source_conn_detail);
  }

  TABLE *table = table_op.get_table();

  /* Store channel */
  if (store_field(table->field[0], channel_name)) {
    return std::make_tuple(true, source_conn_detail);
  }

  Rpl_sys_key_access key_access;
  if (!key_access.init(table)) {
    do {
      /* get source connection detail */
      source_conn_detail.push_back(get_data(table));
    } while (!key_access.next());
  }

  auto error = key_access.deinit() || table_op.deinit();
  if (!error) {
    std::sort(source_conn_detail.begin(), source_conn_detail.end(),
              [](auto const &t1, auto const &t2) { return t1 > t2; });
  }

  return std::make_tuple(error, source_conn_detail);
}

std::tuple<bool, SENDER_CONN_LIST>
Rpl_async_conn_failover_table_operations::read_all_rows() {
  DBUG_TRACE;
  SENDER_CONN_LIST source_conn_detail{};

  Rpl_sys_table_access table_op;
  if (table_op.init(m_db, m_table, m_num_field, m_lock_type)) {
    return std::make_tuple(true, source_conn_detail);
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  if (!key_access.init(table, Rpl_sys_key_access::enum_key_type::INDEX_NEXT)) {
    do {
      /* get source connection detail */
      source_conn_detail.push_back(get_data(table));
    } while (!key_access.next());
  }

  auto error = key_access.deinit() || table_op.deinit();
  return std::make_tuple(error, source_conn_detail);
}

std::tuple<bool, SENDER_CONN_LIST>
Rpl_async_conn_failover_table_operations::read_random_rows() {
  DBUG_TRACE;
  SENDER_CONN_LIST source_conn_detail{};

  Rpl_sys_table_access table_op;
  if (table_op.init(m_db, m_table, m_num_field, m_lock_type)) {
    return std::make_tuple(true, source_conn_detail);
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  if (!key_access.init(table, Rpl_sys_key_access::enum_key_type::RND_NEXT)) {
    do {
      /* get source connection detail */
      source_conn_detail.push_back(get_data(table));
    } while (!key_access.next());
  }

  auto error = key_access.deinit() || table_op.deinit();
  return std::make_tuple(error, source_conn_detail);
}

std::tuple<bool, SENDER_CONN_TUPLE>
Rpl_async_conn_failover_table_operations::read_random_rows_pos(
    std::string pos) {
  DBUG_TRACE;
  SENDER_CONN_TUPLE source_conn_detail{};

  Rpl_sys_table_access table_op;
  if (table_op.init(m_db, m_table, m_num_field, m_lock_type)) {
    return std::make_tuple(true, source_conn_detail);
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;

  if (!key_access.init(table, pos)) {
    if (!key_access.next()) {
      /* get source connection detail */
      source_conn_detail = get_data(table);
    }
  }

  auto error = key_access.deinit() || table_op.deinit();
  return std::make_tuple(error, source_conn_detail);
}
