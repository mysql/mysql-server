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

#ifndef RPL_ASYNC_CONN_FAILOVER_TABLE_OPERATIONS_H
#define RPL_ASYNC_CONN_FAILOVER_TABLE_OPERATIONS_H

#include "sql/field.h"
#include "sql/rpl_sys_table_access.h"

/* Connection detail tuple */
using SENDER_CONN_TUPLE =
    std::tuple<uint, std::string, std::string, uint, std::string>;

/* Connection detail list */
using SENDER_CONN_LIST = std::vector<SENDER_CONN_TUPLE>;

/*
  Class provides read, write and delete function to
  replication_asynchronous_connection_failover table.
*/
class Rpl_async_conn_failover_table_operations {
 public:
  /**
    Construction.

    @param[in]  lock_type     How to lock the table
    @param[in]  max_num_field Maximum number of fields
  */
  Rpl_async_conn_failover_table_operations(
      enum thr_lock_type lock_type = TL_WRITE, uint max_num_field = 5)
      : m_lock_type(lock_type), m_num_field(max_num_field) {}

  ~Rpl_async_conn_failover_table_operations() {}

  /**
    Insert row provided by
    asynchronous_connection_failover_add_source() UDF to
    replication_asynchronous_connection_failover table.

    @param[in] source_conn_detail  std::tuple containing <channel, host, port,
                                   network_namespace, weight>

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> insert_row(
      SENDER_CONN_TUPLE source_conn_detail);

  /**
    Delete row provided by
    asynchronous_connection_failover_delete_source() UDF in
    replication_asynchronous_connection_failover table.

    @param[in] source_conn_detail  std::tuple containing <channel, host, port,
                            network_namespace, weight>

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> delete_row(
      SENDER_CONN_TUPLE source_conn_detail);

  /**
    Get source network configuration details (<hostname, port,
    network_namespace>) from replication_asynchronous_connection_failover table
    for the channel. It uses index scan (ha_index_read_idx_map) to fetch rows
    for the channel name.

    @param[in]  channel_name      The channel name

    @returns std::tuple<bool, SENDER_CONN_LIST> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of source network
             configuration details (<hostname, port, network_namespace>).
  */
  std::tuple<bool, SENDER_CONN_LIST> read_rows(std::string channel_name);

  /**
    Get all the rows from the replication_asynchronous_connection_failover
    table. It uses index scan (ha_index_first) to fetch all the rows.

    @returns std::tuple<bool, SENDER_CONN_LIST> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of source network
             configuration details (<hostname, port, network_namespace>).
  */
  std::tuple<bool, SENDER_CONN_LIST> read_all_rows();

  /**
    Get all the rows from the replication_asynchronous_connection_failover
    table using random scan (ha_rnd_next) to fetch all the rows.

    @returns std::tuple<bool, SENDER_CONN_LIST> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of source network
             configuration details (<hostname, port, network_namespace>).
  */
  std::tuple<bool, SENDER_CONN_LIST> read_random_rows();

  /**
    Get the row at the position from
    replication_asynchronous_connection_failover table using random scan
    (ha_rnd_pos) to fetch the row.

    @returns std::tuple<bool, SENDER_CONN_LIST> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple contains source network configuration
             details (<hostname, port, network_namespace>).
  */
  std::tuple<bool, SENDER_CONN_TUPLE> read_random_rows_pos(std::string pos);

  /**
    Stores provided string to table's field.

    @param[in]  field        Field class object
    @param[in]  fld          The std::string value to be saved.
    @param[in]  cs           Charset info

    @retval true   Error
    @retval false  Success
  */
  bool store_field(Field *field, std::string fld, CHARSET_INFO *cs);

  /**
    Stores provided string to table's field.

    @param[in]  field        Field class object
    @param[in]  fld          The long long value to be saved.
    @param[in]  unsigned_val If value is unsigned.

    @retval true   Error
    @retval false  Success
  */
  bool store_field(Field *field, long long fld, bool unsigned_val);

 private:
  enum thr_lock_type m_lock_type;  // table lock type
  uint m_num_field;                // number of fields in table
  /* the table which needs to be opened for read/delete/insert operations */
  const std::string m_table{"replication_asynchronous_connection_failover"};
  const std::string m_db{"mysql"};  // the database table belongs to

  /**
    Get stored data in table.

    @returns SENDER_CONN_TUPLE where tuple is list of source network
             configuration details (<hostname, port, network_namespace>).
  */
  SENDER_CONN_TUPLE get_data(TABLE *table);
};

#endif /* RPL_ASYNC_CONN_FAILOVER_TABLE_OPERATIONS_H */
