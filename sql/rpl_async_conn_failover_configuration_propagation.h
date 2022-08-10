/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef RPL_ASYNC_CONN_FAILOVER_CONFIGURATION_PROPAGATION_INCLUDED
#define RPL_ASYNC_CONN_FAILOVER_CONFIGURATION_PROPAGATION_INCLUDED

#include <map>
#include <string>
#include <vector>
#include "sql/protobuf/generated/protobuf_lite/replication_asynchronous_connection_failover.pb.h"
#include "sql/rpl_sys_table_access.h"

/**
  Memory storage of the replication failover channel status
  configuration propagated to Group Replication members.
*/
class Rpl_acf_status_configuration {
 public:
  Rpl_acf_status_configuration();

  virtual ~Rpl_acf_status_configuration();

  /**
    Status keys propagated with the group.
  */
  enum enum_key { SOURCE_CONNECTION_AUTO_FAILOVER = 0 };

  /**
    Clears the status configuration.

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool reset();

  /**
    Reloads the status configuration from runtime information.
  */
  void reload();

  /**
    Delete the status configuration value.

    @param[in] channel  the channel name.
    @param[in] key      the variable whose status to set and increment
                        version.
  */
  void delete_channel_status(const std::string &channel,
                             Rpl_acf_status_configuration::enum_key key);

  /**
    Sets the status configuration value and increment version value.

    @param[in] channel  the channel name.
    @param[in] key      the variable whose status to set and increment
                        version.
    @param[in] value    the variable status value to set.
    @param[in] configuration  the configuration in protobuf
  */
  void set_value_and_increment_version(
      const std::string &channel, Rpl_acf_status_configuration::enum_key key,
      int value,
      protobuf_replication_asynchronous_connection_failover::VariableStatusList
          &configuration);

  /**
    Sets the status configuration with the one received from the
    group.

    @param[in] configuration  the configuration in protobuf

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool set(const protobuf_replication_asynchronous_connection_failover::
               VariableStatusList &configuration);

  /**
    Sets the status configuration with the one received from the
    group.

    @param[in] configuration  the configuration in protobuf

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool set(const protobuf_replication_asynchronous_connection_failover::
               SourceAndManagedAndStatusList &configuration);

  /**
    Gets the status configuration to send to the group.

    @param[out] configuration  the configuration in protobuf
  */
  void get(protobuf_replication_asynchronous_connection_failover::
               SourceAndManagedAndStatusList &configuration);

 private:
  mysql_mutex_t m_lock;

  ulonglong m_version{0};

  /*
    Stores replication failover channel status propagated to Group
    Replication members.
    map(<channel,key>, value)
  */
  std::map<std::pair<std::string, std::string>, int> m_status;

  static const std::vector<std::string> m_key_names;

  static std::string get_key_name(Rpl_acf_status_configuration::enum_key key);
};

/*
  Class provides functions to send and receive
  replication_asynchronous_connection_failover and
  replication_asynchronous_connection_failover_managed table data, and
  SOURCE_CONNECTION_AUTO_FAILOVER value of CHANGE REPLICATION SOURCE command.
*/
class Rpl_acf_configuration_handler {
 public:
  /**
    Construction.
  */
  Rpl_acf_configuration_handler();

  /**
    Destruction.
  */
  virtual ~Rpl_acf_configuration_handler();

  /**
    Initialize and configure group_replication_message_service_recv service
    so member can receive and process data from group members.

    @return function return value which determines if was:
            false  Successful
            true   Error
  */
  bool init();

  /**
    Receive data sent by group replication group member.

    @param[in] tag    identifier which determine type of data received.
    @param[in] data   data received.
    @param[in] data_length   size of data received.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool receive(const char *tag, const unsigned char *data, size_t data_length);

  /**
    Send variable status to its group replication group members.

    @param[in] channel  the channel name.
    @param[in] key  the variable whose status to be send.
    @param[in] status  the variable status to be send.

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool send_channel_status_and_version_data(
      const std::string &channel, Rpl_acf_status_configuration::enum_key key,
      int status);

  /**
    Delete channel status.

    @param[in] channel  the channel name.
    @param[in] key  the variable whose status to be send.
  */
  void delete_channel_status(const std::string &channel,
                             Rpl_acf_status_configuration::enum_key key);

  /**
    Get stored data in
    mysql.replication_asynchronous_connection_failover or
    mysql.replication_asynchronous_connection_failover_managed table and send
    to its group replication group members.

    @param[in]  table_op  Rpl_sys_table_access class object.

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool send_table_data(Rpl_sys_table_access &table_op);

  /**
    Get stored data in
    mysql.replication_asynchronous_connection_failover table and send
    to its group replication group members.

    @param[in]  table_op  Rpl_sys_table_access class object.

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool send_failover_data(Rpl_sys_table_access &table_op);

  /**
    Reload the failover channel status from runtime information.
  */
  void reload_failover_channels_status();

  /**
    Get data stored in
    mysql.replication_asynchronous_connection_failover or
    mysql.replication_asynchronous_connection_failover_managed table
    in protobuf serialized string format.

    @param[out]  serialized_configuration  save in protobuf serialized string
                                           format.

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool get_configuration(std::string &serialized_configuration);

  /**
    Save data in
    mysql.replication_asynchronous_connection_failover or
    mysql.replication_asynchronous_connection_failover_managed table.

    @param[in] exchanged_replication_failover_channels_serialized_configuration
                 save data from serialized string format.

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool set_configuration(
      const std::vector<std::string>
          &exchanged_replication_failover_channels_serialized_configuration);

  /**
    Collect and broadcast the replication failover channels configuration
    in a serialized
    protobuf_replication_asynchronous_connection_failover::SourceAndManagedAndStatusList
    message, that will override the configuration on all group members.

    @return the operation status
      @retval false  OK
      @retval true   Error
   */
  bool force_my_replication_failover_channels_configuration_on_all_members();

 private:
  /* The list of tag identfiers which type of data sent. */
  const std::vector<std::string> m_message_tag{
      "mysql_replication_asynchronous_connection_failover",
      "mysql_replication_asynchronous_connection_managed",
      "mysql_replication_asynchronous_connection_variable_status",
      "mysql_replication_asynchronous_connection_failover_and_managed_and_"
      "status"};

  /*
    The database replication_asynchronous_connection_failover and
    replication_asynchronous_connection_failover_managed table belongs to.
  */
  const std::string m_db{"mysql"};

  /* replication_asynchronous_connection_failover table name */
  const std::string m_table_failover{
      "replication_asynchronous_connection_failover"};

  /* number of fields in replication_asynchronous_connection_failover table */
  const uint m_table_failover_num_field{6};

  /* replication_asynchronous_connection_failover_managed table name */
  const std::string m_table_managed{
      "replication_asynchronous_connection_failover_managed"};
  /*
    number of fields in replication_asynchronous_connection_failover_managed
    table
  */
  const uint m_table_managed_num_field{4};

  /*
    Stores replication failover channel status propagated to Group
    Replication members.
  */
  Rpl_acf_status_configuration m_rpl_failover_channels_status;

  /**
    Unregister group_replication_message_service_recv service.

    @return function return value which determines if was:
            false  Successful
            true   Error
  */
  bool deinit();

  /**
    Receive mysql.replication_asynchronous_connection_failover table data sent
    by group replication group member.

    @param[in] data   data received.
    @param[in] data_length   size of data received.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool receive_failover(const unsigned char *data, size_t data_length);

  /**
    Receive mysql.replication_asynchronous_connection_failover_managed table
    data sent by group replication group member.

    @param[in] data   data received.
    @param[in] data_length   size of data received.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool receive_managed(const unsigned char *data, size_t data_length);

  /**
    Receive SOURCE_CONNECTION_AUTO_FAILOVER value of CHANGE REPLICATION SOURCE
    command data sent by group replication group member.

    @param[in] data   data received.
    @param[in] data_length   size of data received.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool receive_channel_status(const unsigned char *data, size_t data_length);

  /**
    Receive mysql.replication_asynchronous_connection_failover and
    mysql.replication_asynchronous_connection_failover_managed table
    data sent by group replication group member.

    @param[in] data   data received.
    @param[in] data_length   size of data received.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool receive_failover_and_managed_and_status(const unsigned char *data,
                                               size_t data_length);

  /**
    Send data to all group replication group members.

    @param[in] tag    identifier which determine type of data.
    @param[in] data   data to be send
    @param[in] data_length   size of data to be send.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool send(const char *tag, const char *data, size_t data_length);

  /**
    Send mysql.replication_asynchronous_connection_failover table data
    to group replication group members.

    @param[in] data   data to be send
    @param[in] data_length   size of data to be send.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool send_failover(const char *data, size_t data_length);

  /**
    Send mysql.replication_asynchronous_connection_failover_managed table data
    to group replication group members.

    @param[in] data   data to be send
    @param[in] data_length   size of data to be send.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool send_managed(const char *data, size_t data_length);

  /**
    Get stored data in
    mysql.replication_asynchronous_connection_failover_managed table and send
    to its group replication group members.

    @param[in]  table_op  Rpl_sys_table_access class object.

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool send_managed_data(Rpl_sys_table_access &table_op);

  /**
    Send SOURCE_CONNECTION_AUTO_FAILOVER value of CHANGE REPLICATION SOURCE
    command data to group replication group members.

    @param[in] data   data to be send
    @param[in] data_length   size of data to be send.

    @return function return value which determines if read was:
            false  Successful
            true   Error
  */
  bool send_channel_status(const char *data, size_t data_length);

  /**
    Save data in
    mysql.replication_asynchronous_connection_failover table.

    @param[in] configuration  the configuration in protobuf

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool set_failover_sources_internal(
      const protobuf_replication_asynchronous_connection_failover::
          SourceAndManagedAndStatusList &configuration);

  /**
    Save data in
    mysql.replication_asynchronous_connection_failover_managed table.

    @param[in] configuration  the configuration in protobuf

    @return function return value which determines:
            false  Successful
            true   Error
  */
  bool set_failover_managed_internal(
      const protobuf_replication_asynchronous_connection_failover::
          SourceAndManagedAndStatusList &configuration);
};

#endif /* RPL_ASYNC_CONN_FAILOVER_CONFIGURATION_PROPAGATION_INCLUDED */
