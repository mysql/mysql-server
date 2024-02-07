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

#ifndef RPL_ASYNC_CONN_FAILOVER_TABLE_OPERATIONS_H
#define RPL_ASYNC_CONN_FAILOVER_TABLE_OPERATIONS_H

#include <functional>
#include <tuple>
#include "sql/field.h"
#include "sql/table.h"
#include "sql/udf_service_util.h"
#include "string_with_len.h"

class Rpl_sys_table_access;
class Json_wrapper;

namespace protobuf_replication_asynchronous_connection_failover {
class SourceAndManagedList;
}

/* <channel, host, port, network_namespace, weight, managed_name> */
using RPL_FAILOVER_SOURCE_TUPLE =
    std::tuple<std::string, std::string, uint, std::string, uint, std::string>;

/* <channel, managed_name, managed_type, configuration> */
using RPL_FAILOVER_MANAGED_JSON_TUPLE =
    std::tuple<std::string, std::string, std::string, Json_wrapper>;

/* <channel, managed_name, managed_type, primary_weight, secondary_weight> */
using RPL_FAILOVER_MANAGED_TUPLE =
    std::tuple<std::string, std::string, std::string, uint, uint>;

using RPL_FAILOVER_SOURCE_LIST = std::vector<RPL_FAILOVER_SOURCE_TUPLE>;

/*
  Class provides read, write and delete function to
  replication_asynchronous_connection_failover and
  replication_asynchronous_connection_failover_managed
  tables.
*/
class Rpl_async_conn_failover_table_operations {
 public:
  /**
    Construction.

    @param[in]  lock_type     How to lock the table
  */
  Rpl_async_conn_failover_table_operations(
      enum thr_lock_type lock_type = TL_WRITE)
      : m_lock_type(lock_type) {}

  virtual ~Rpl_async_conn_failover_table_operations() = default;

  /**
    Insert row for a unmanaged sender on
    replication_asynchronous_connection_failover table, and
    send stored table data to its group replication group members.

    @param[in] channel            channel
    @param[in] host               sender host
    @param[in] port               sender port
    @param[in] network_namespace  sender network_namespace
    @param[in] weight             sender weight
    @param[in] managed_name       The name of the group which this server
                                  belongs to.

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> add_source(const std::string &channel,
                                           const std::string &host, uint port,
                                           const std::string &network_namespace,
                                           uint weight,
                                           const std::string &managed_name);

  /**
    Insert row for a unmanaged sender on
    replication_asynchronous_connection_failover table.

    @param[in] channel            channel
    @param[in] host               sender host
    @param[in] port               sender port
    @param[in] network_namespace  sender network_namespace
    @param[in] weight             sender weight
    @param[in] managed_name       The name of the group which this server
                                  belongs to.
    @param[in] table_op           Rpl_sys_table_access class object.

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  static std::tuple<bool, std::string> add_source_skip_send(
      const std::string &channel, const std::string &host, uint port,
      const std::string &network_namespace, uint weight,
      const std::string &managed_name, Rpl_sys_table_access &table_op);

  /**
    Insert row on replication_asynchronous_connection_failover_managed
    and replication_asynchronous_connection_failover tables, and
    send stored table data to its group replication group members.

    @param[in] channel            channel
    @param[in] host               sender host
    @param[in] port               sender port
    @param[in] network_namespace  sender network_namespace
    @param[in] managed_type       Determines the manged group type.
    @param[in] managed_name       The name of the group which this server
                                  belongs to
    @param[in] primary_weight     weight assigned to the primary
    @param[in] secondary_weight   weight assigned to the secondary

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> add_managed(
      const std::string &channel, const std::string &host, uint port,
      const std::string &network_namespace, const std::string &managed_type,
      const std::string &managed_name, uint primary_weight,
      uint secondary_weight);

  /**
    Insert row on replication_asynchronous_connection_failover_managed
    table.

    @param[in] channel            channel
    @param[in] managed_type       Determines the manged group type.
    @param[in] managed_name       The name of the group which this server
                                  belongs to.
    @param[in] wrapper            contains weight assigned to the primary
                                  and secondary member in Json format.
    @param[in] table_op           Rpl_sys_table_access class object.

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  static std::tuple<bool, std::string> add_managed_skip_send(
      const std::string &channel, const std::string &managed_type,
      const std::string &managed_name, const Json_wrapper &wrapper,
      Rpl_sys_table_access &table_op);

  /**
    Delete row for a unmanaged sender on
    replication_asynchronous_connection_failover table.

    @param[in] channel            channel
    @param[in] host               sender host
    @param[in] port               sender port
    @param[in] network_namespace  sender network_namespace

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> delete_source(
      const std::string &channel, const std::string &host, uint port,
      const std::string &network_namespace);

  /**
    Delete row on replication_asynchronous_connection_failover_managed table
    and all its sources on replication_asynchronous_connection_failover table.

    @param[in] channel           The asynchronous replication channel name
    @param[in] managed_name      The name of the group which this server
                                 belongs to.

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> delete_managed(const std::string &channel,
                                               const std::string &managed_name);

  /**
    Delete all rows on replication_asynchronous_connection_failover_managed
    and replication_asynchronous_connection_failover tables, and delete
    its respective rows on replication_group_configuration_version table.

    @return operation status:
            false  Successful
            true   Error
  */
  bool reset();

  /**
    Read all sources for a channel.
    It uses index scan (ha_index_read_idx_map) to fetch rows for the channel
    name.

    @param[in]  channel_name      The channel name

    @returns std::tuple<bool, List_of_Tuple> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of return details based on
             open table and template provided.
  */
  std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
  read_source_rows_for_channel(std::string channel_name);

  /**
    Real all sources for a channel and a managed name.
    It uses index scan (ha_index_read_idx_map) to fetch rows for the channel
    name and manged name.

    @param[in]  channel_name      The channel name
    @param[in]  managed_name      The name of the group which this server
                                  belongs to.

    @returns std::tuple<bool, List_of_Tuple> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of return details based on
             open table and template provided.
  */
  std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
  read_source_rows_for_channel_and_managed_name(std::string channel_name,
                                                std::string managed_name);

  /**
    Read rows and fields from
    replication_asynchronous_connection_failover_managed table and returns its
    details in provided RPL_FAILOVER_MANAGED_TUPLE tuple. It uses index scan
    (ha_index_read_idx_map) to fetch rows for the channel name.

    @param[in]  channel_name  The channel name
    @param[out] rows     return rows read from
                         replication_asynchronous_connection_failover_managed

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool read_managed_rows_for_channel(
      std::string channel_name, std::vector<RPL_FAILOVER_MANAGED_TUPLE> &rows);

  /**
    Read all sources.
    It uses index scan (ha_index_first) to fetch all the rows.

    @param[in] table_op           Rpl_sys_table_access class object.

    @returns std::tuple<bool, List_of_Tuple> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of return details based on
             open table and template provided.
  */
  static std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
  read_source_all_rows_internal(Rpl_sys_table_access &table_op);

  /**
    Read all sources.
    It uses index scan (ha_index_first) to fetch all the rows.

    @returns std::tuple<bool, List_of_Tuple> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of return details based on
             open table and template provided.
  */
  std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
  read_source_all_rows();

  /**
    Get all sources using random scan (ha_rnd_next) to fetch all the rows.

    @returns std::tuple<bool, List_of_Tuple> where each element has
             following meaning:

             first element of tuple is function return value and determines:
               false  Successful
               true   Error

             second element of the tuple is list of return details based on
             open table and template provided.
  */
  std::tuple<bool, std::vector<RPL_FAILOVER_SOURCE_TUPLE>>
  read_source_random_rows();

  /**
    Read rows and fields from
    replication_asynchronous_connection_failover_managed table and returns its
    details in provided RPL_FAILOVER_MANAGED_JSON_TUPLE tuple. It uses random
    scan     (ha_rnd_next) to fetch all the rows.

    @param[in] table_op           Rpl_sys_table_access class object.
    @param[out] rows   return rows read from
                       replication_asynchronous_connection_failover_managed

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  static bool read_managed_random_rows_internal(
      Rpl_sys_table_access &table_op,
      std::vector<RPL_FAILOVER_MANAGED_JSON_TUPLE> &rows);

  /**
    Read rows and fields from
    replication_asynchronous_connection_failover_managed table and returns its
    details in provided RPL_FAILOVER_MANAGED_TUPLE tuple. It uses random
    scan     (ha_rnd_next) to fetch all the rows.

    @param[in] table_op           Rpl_sys_table_access class object.
    @param[out] rows   return rows read from
                       replication_asynchronous_connection_failover_managed

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  static bool read_managed_random_rows_internal(
      Rpl_sys_table_access &table_op,
      std::vector<RPL_FAILOVER_MANAGED_TUPLE> &rows);

  /**
    Read rows and fields from
    replication_asynchronous_connection_failover_managed table and returns its
    details in provided RPL_FAILOVER_MANAGED_TUPLE tuple. It uses random scan
    (ha_rnd_next) to fetch all the rows.

    @param[out] rows   return rows read from
                       replication_asynchronous_connection_failover_managed

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool read_managed_random_rows(std::vector<RPL_FAILOVER_MANAGED_TUPLE> &rows);

  /**
    Get stored data in table.

    @param[in]  table_op  Rpl_sys_table_access class object.
    @param[out] rows      Fetch and store read rows in the tuple.
  */
  template <class TUP>
  static void get_data(Rpl_sys_table_access &table_op, TUP &rows);

  /* Configuration column primary key name */
  static const MYSQL_LEX_CSTRING Primary_weight_key;

  /* Configuration column secondary key name */
  static const MYSQL_LEX_CSTRING Secondary_weight_key;

 private:
  enum thr_lock_type m_lock_type;   // table lock type
  const std::string m_db{"mysql"};  // the database table belongs to

  const std::string m_table_failover{
      "replication_asynchronous_connection_failover"};
  const uint m_table_failover_num_field{6};

  const std::string m_table_managed{
      "replication_asynchronous_connection_failover_managed"};
  const uint m_table_managed_num_field{4};

  /**
    A wrapper template function to save/delete data to given table.

    @param[in] field_index The list of field's position to be written or match
                           while querying for delete operations.
    @param[in] field_name  The list of field names of the table.
    @param[in] field_value The field values to be written or match while
                           querying for delete operations.
    @param[in] func        The handler class function to write/delete data.
    @param[in] table_index The table index/key position (by default i.e. on
                           position 0, if primary key present is used).
    @param[in] keypart_map Which part of key to use.
    @param[in] table_op    Rpl_sys_table_access class object.

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  template <class T>
  static std::tuple<bool, std::string> execute_handler_func_skip_send(
      const std::vector<uint> &field_index,
      const std::vector<std::string> &field_name, const T &field_value,
      std::function<void(Rpl_sys_table_access &, bool &, std::string &, uint &,
                         key_part_map &)>
          func,
      uint table_index, key_part_map keypart_map,
      Rpl_sys_table_access &table_op);

  /**
    A wrapper template function to save/delete data to given table,
    and send stored table data to its group replication group members.


    @param[in] db_name  The database whose table will be used to
                         write/delete data.
    @param[in] table_name  The table to which data will be written or deleted.
    @param[in] num_field   The number of fields to be written or match while
                           querying for delete operations.
    @param[in] lock_type   How to lock the table
    @param[in] field_index The list of field's position to be written or match
                           while querying for delete operations.
    @param[in] field_name  The list of field names of the table.
    @param[in] field_value The field values to be written or match while
                           querying for delete operations.
    @param[in] func        The handler class function to write/delete data.
    @param[in] table_index The table index/key position (by default i.e. on
                           position 0, if primary key present is used).
    @param[in] keypart_map Which part of key to use.

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  template <class T>
  static std::tuple<bool, std::string> execute_handler_func_send(
      const std::string &db_name, const std::string &table_name, uint num_field,
      enum thr_lock_type lock_type, const std::vector<uint> &field_index,
      const std::vector<std::string> &field_name, const T &field_value,
      std::function<void(Rpl_sys_table_access &, bool &, std::string &, uint &,
                         key_part_map &)>
          func,
      uint table_index, key_part_map keypart_map);
};

#endif /* RPL_ASYNC_CONN_FAILOVER_TABLE_OPERATIONS_H */
