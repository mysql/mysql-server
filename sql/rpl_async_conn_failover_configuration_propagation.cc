/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "sql/rpl_async_conn_failover_configuration_propagation.h"
#include <mutex_lock.h>
#include <my_dbug.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_message_service.h>
#include <mysql/components/services/group_replication_status_service.h>
#include "mysql/components/services/log_builtins.h"
#include "sql-common/json_dom.h"
#include "sql/log.h"
#include "sql/mysqld.h"  // srv_registry
#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_channel_service_interface.h"
#include "sql/rpl_group_replication.h"
#include "sql/rpl_msr.h"

/*
  receive function
*/
DEFINE_BOOL_METHOD(receive_acf_configuration,
                   (const char *tag, const unsigned char *data,
                    size_t data_length)) {
  return rpl_acf_configuration_handler->receive(tag, data, data_length);
}

BEGIN_SERVICE_IMPLEMENTATION(group_replication,
                             group_replication_message_service_recv)
receive_acf_configuration, END_SERVICE_IMPLEMENTATION();

/*
  Must match the keys on Rpl_acf_status_configuration::enum_key
*/
const std::vector<std::string> Rpl_acf_status_configuration::m_key_names{
    "SOURCE_CONNECTION_AUTO_FAILOVER"};

Rpl_acf_status_configuration::Rpl_acf_status_configuration() {
  mysql_mutex_init(0, &m_lock, MY_MUTEX_INIT_FAST);
}

Rpl_acf_status_configuration::~Rpl_acf_status_configuration() {
  mysql_mutex_destroy(&m_lock);
}

std::string Rpl_acf_status_configuration::get_key_name(
    Rpl_acf_status_configuration::enum_key key) {
  assert(key < m_key_names.size());
  return m_key_names[key];
}

bool Rpl_acf_status_configuration::reset() {
  DBUG_TRACE;
  /*
    On operations that touch both channels info objects and this
    object, the lock order must be:
      1) channel_map.wrlock()
      2) Rpl_acf_status_configuration::m_lock
    thence the caller must acquire channel_map.wrlock() before calling
    this method.
  */
  channel_map.assert_some_wrlock();
  MUTEX_LOCK(guard, &m_lock);

  m_version = 0;
  m_status.clear();

  return unset_source_connection_auto_failover_on_all_channels();
}

void Rpl_acf_status_configuration::reload() {
  DBUG_TRACE;
  /*
    On operations that touch both channels info objects and this
    object, the lock order must be:
      1) channel_map.rdlock()
      2) Rpl_acf_status_configuration::m_lock
    thence the caller must acquire channel_map.rdlock() before calling
    this method.
  */
  channel_map.assert_some_lock();
  MUTEX_LOCK(guard, &m_lock);

  m_version = 0;
  m_status.clear();

  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    Master_info *mi = it->second;
    if (Master_info::is_configured(mi) &&
        mi->is_source_connection_auto_failover()) {
      m_version = 1;

      std::pair<std::string, std::string> key_pair = std::make_pair(
          mi->get_channel(), get_key_name(SOURCE_CONNECTION_AUTO_FAILOVER));
      m_status.insert(std::make_pair(key_pair, 1));
    }
  }
}

void Rpl_acf_status_configuration::delete_channel_status(
    const std::string &channel, Rpl_acf_status_configuration::enum_key key) {
  DBUG_TRACE;
  MUTEX_LOCK(guard, &m_lock);

  std::string key_name = get_key_name(key);
  std::pair<std::string, std::string> key_pair =
      std::make_pair(channel, key_name);

  auto it = m_status.find(key_pair);
  if (it != m_status.end()) {
    m_status.erase(it);
    m_version++;
  }
}

void Rpl_acf_status_configuration::set_value_and_increment_version(
    const std::string &channel, Rpl_acf_status_configuration::enum_key key,
    int value,
    protobuf_replication_asynchronous_connection_failover::VariableStatusList
        &configuration) {
  DBUG_TRACE;
  MUTEX_LOCK(guard, &m_lock);

  m_version++;

  std::string key_name = get_key_name(key);
  std::pair<std::string, std::string> key_pair =
      std::make_pair(channel, key_name);
  m_status[key_pair] = value;

  // Add full content to output configuration.
  configuration.set_origin(server_uuid);
  configuration.set_version(m_version);

  for (auto it = m_status.begin(); it != m_status.end(); ++it) {
    protobuf_replication_asynchronous_connection_failover::VariableStatus
        *status = configuration.add_status();
    status->set_channel(it->first.first);
    status->set_key(it->first.second);
    status->set_status(it->second);
  }
}

bool Rpl_acf_status_configuration::set(
    const protobuf_replication_asynchronous_connection_failover::
        VariableStatusList &configuration) {
  DBUG_TRACE;
  /*
    On operations that touch both channels info objects and this
    object, the lock order must be:
      1) channel_map.wrlock()
      2) Rpl_acf_status_configuration::m_lock
    thence the caller must acquire channel_map.wrlock() before calling
    this method.
  */
  channel_map.assert_some_wrlock();
  MUTEX_LOCK(guard, &m_lock);

  if (configuration.version() > m_version) {
    if (unset_source_connection_auto_failover_on_all_channels()) {
      return true;
    }

    m_version = configuration.version();
    m_status.clear();

    for (const protobuf_replication_asynchronous_connection_failover::
             VariableStatus &status : configuration.status()) {
      std::pair<std::string, std::string> key_pair =
          std::make_pair(status.channel(), status.key());
      m_status.insert(std::make_pair(key_pair, status.status()));

      // Update `SOURCE_CONNECTION_AUTO_FAILOVER` on channel configuration.
      if (!get_key_name(
               Rpl_acf_status_configuration::SOURCE_CONNECTION_AUTO_FAILOVER)
               .compare(status.key())) {
        bool error = channel_change_source_connection_auto_failover(
            status.channel().c_str(), status.status());
        if (error) {
          return true;
        }
      }
    }
  }

  return false;
}

bool Rpl_acf_status_configuration::set(
    const protobuf_replication_asynchronous_connection_failover::
        SourceAndManagedAndStatusList &configuration) {
  DBUG_TRACE;
  /*
    On operations that touch both channels info objects and this
    object, the lock order must be:
      1) channel_map.wrlock()
      2) Rpl_acf_status_configuration::m_lock
    thence the caller must acquire channel_map.wrlock() before calling
    this method.
  */
  channel_map.assert_some_wrlock();
  MUTEX_LOCK(guard, &m_lock);

  if (unset_source_connection_auto_failover_on_all_channels()) {
    return true;
  }

  m_version = configuration.status_version();
  m_status.clear();

  for (const protobuf_replication_asynchronous_connection_failover::
           VariableStatus &status : configuration.status()) {
    std::pair<std::string, std::string> key_pair =
        std::make_pair(status.channel(), status.key());
    m_status.insert(std::make_pair(key_pair, status.status()));

    // Update `SOURCE_CONNECTION_AUTO_FAILOVER` on channel configuration.
    if (!get_key_name(
             Rpl_acf_status_configuration::SOURCE_CONNECTION_AUTO_FAILOVER)
             .compare(status.key())) {
      bool error = channel_change_source_connection_auto_failover(
          status.channel().c_str(), status.status());
      if (error) {
        return true;
      }
    }
  }

  return false;
}

void Rpl_acf_status_configuration::get(
    protobuf_replication_asynchronous_connection_failover::
        SourceAndManagedAndStatusList &configuration) {
  DBUG_TRACE;
  MUTEX_LOCK(guard, &m_lock);

  configuration.set_status_version(m_version);
  configuration.clear_status();

  for (std::map<std::pair<std::string, std::string>, int>::iterator it =
           m_status.begin();
       it != m_status.end(); ++it) {
    protobuf_replication_asynchronous_connection_failover::VariableStatus
        *status = configuration.add_status();
    status->set_channel(it->first.first);
    status->set_key(it->first.second);
    status->set_status(it->second);
  }
}

Rpl_acf_configuration_handler::Rpl_acf_configuration_handler() {}

Rpl_acf_configuration_handler::~Rpl_acf_configuration_handler() { deinit(); }

bool Rpl_acf_configuration_handler::init() {
  DBUG_TRACE;

  my_service<SERVICE_TYPE(registry_registration)> registrator(
      "registry_registration", srv_registry);
  using group_replication_message_service_recv_t =
      SERVICE_TYPE_NO_CONST(group_replication_message_service_recv);
  bool result = registrator->register_service(
      "group_replication_message_service_recv.replication_asynchronous_"
      "connection_failover_configuration",
      reinterpret_cast<my_h_service>(
          const_cast<group_replication_message_service_recv_t *>(
              &SERVICE_IMPLEMENTATION(
                  group_replication, group_replication_message_service_recv))));

  if (result) {
    LogErr(ERROR_LEVEL, ER_GRP_RPL_FAILOVER_REGISTER_MESSAGE_LISTENER_SERVICE);
  }

  return result;
}

bool Rpl_acf_configuration_handler::deinit() {
  DBUG_TRACE;

  my_service<SERVICE_TYPE(registry_registration)> registrator(
      "registry_registration", srv_registry);
  return registrator->unregister(
      "group_replication_message_service_recv.replication_asynchronous_"
      "connection_failover_configuration");
}

bool Rpl_acf_configuration_handler::receive(const char *tag,
                                            const unsigned char *data,
                                            size_t data_length) {
  DBUG_TRACE;

  if (!m_message_tag[0].compare(tag)) {
    return receive_failover(data, data_length);
  } else if (!m_message_tag[1].compare(tag)) {
    return receive_managed(data, data_length);
  } else if (!m_message_tag[2].compare(tag)) {
    return receive_channel_status(data, data_length);
  } else if (!m_message_tag[3].compare(tag)) {
    return receive_failover_and_managed_and_status(data, data_length);
  }

  return false;
}

bool Rpl_acf_configuration_handler::receive_failover(const unsigned char *data,
                                                     size_t data_length) {
  DBUG_TRACE;
  bool err_val{false};
  std::string err_msg{};

  protobuf_replication_asynchronous_connection_failover::SourceList
      configuration;
  if (!configuration.ParseFromArray(data, data_length)) {
    return true;
  }

  if (configuration.origin().compare(server_uuid)) {
    /* Received replication_asynchronous_connection_failover table data */
    Rpl_sys_table_access table_op(m_db, m_table_failover,
                                  m_table_failover_num_field);
    if (table_op.open(TL_WRITE)) {
      table_op.set_error();
      return true;
    }

    /* Ignore update if stored version is greater than received version */
    if (table_op.get_version() > configuration.version()) return false;

    /* Update received version. */
    if (table_op.update_version(configuration.version())) return true;

    google::protobuf::RepeatedPtrField<
        protobuf_replication_asynchronous_connection_failover::Source> *fld =
        configuration.mutable_source();
    google::protobuf::internal::RepeatedPtrIterator<
        protobuf_replication_asynchronous_connection_failover::Source>
        it = fld->begin();

    /*
      Delete all rows from table than add received rows.
      The table will become empty if no rows are received.
    */
    if (table_op.delete_all_rows()) {
      return true;
    }

    /* Add received rows to table. */
    for (; it != fld->end(); ++it) {
      std::tie(err_val, err_msg) =
          Rpl_async_conn_failover_table_operations::add_source_skip_send(
              it->channel(), it->host(), it->port(), it->network_namespace(),
              it->weight(), it->managed_name(), table_op);
      if (err_val) {
        return true;
      }
    }

    if (table_op.close(false)) {
      return true;
    }
  }

  return err_val;
}

bool Rpl_acf_configuration_handler::receive_managed(const unsigned char *data,
                                                    size_t data_length) {
  DBUG_TRACE;
  bool err_val{false};
  std::string err_msg{};

  protobuf_replication_asynchronous_connection_failover::ManagedList
      configuration;
  if (!configuration.ParseFromArray(data, data_length)) {
    return true;
  }

  if (configuration.origin().compare(server_uuid)) {
    /*
      Received replication_asynchronous_connection_failover_managed table
      data.
    */
    Rpl_sys_table_access table_op(m_db, m_table_managed,
                                  m_table_managed_num_field);
    if (table_op.open(TL_WRITE)) {
      table_op.set_error();
      return true;
    }

    /* Ignore update if stored version is greater than received version. */
    if (table_op.get_version() > configuration.version()) return false;

    /* Update received version. */
    if (table_op.update_version(configuration.version())) return true;

    google::protobuf::RepeatedPtrField<
        protobuf_replication_asynchronous_connection_failover::Managed> *fld =
        configuration.mutable_managed();
    google::protobuf::internal::RepeatedPtrIterator<
        protobuf_replication_asynchronous_connection_failover::Managed>
        it = fld->begin();

    /*
      Delete all rows from table than add received rows.
      The table will become empty if no rows are received.
    */
    if (table_op.delete_all_rows()) {
      return true;
    }

    /* Add received rows to table. */
    for (; it != fld->end(); ++it) {
      json_binary::Value json_val = json_binary::parse_binary(
          it->configuration().c_str(), it->configuration().length());
      if (json_val.type() == json_binary::Value::ERROR) {
        return true;
      }

      Json_wrapper wrapper(json_val);
      std::tie(err_val, err_msg) =
          Rpl_async_conn_failover_table_operations::add_managed_skip_send(
              it->channel(), it->managed_type(), it->managed_name(), wrapper,
              table_op);
      if (err_val) {
        return true;
      }
    }

    if (table_op.close(false)) {
      return true;
    }
  }

  return err_val;
}

bool Rpl_acf_configuration_handler::receive_channel_status(
    const unsigned char *data, size_t data_length) {
  DBUG_TRACE;

  /* Received replica status data. */
  protobuf_replication_asynchronous_connection_failover::VariableStatusList
      configuration;
  if (!configuration.ParseFromArray(data, data_length)) {
    return true;
  }

  if (configuration.origin().compare(server_uuid)) {
    channel_map.wrlock();
    bool set_status_error = m_rpl_failover_channels_status.set(configuration);
    channel_map.unlock();
    if (set_status_error) {
      return true;
    }
  }

  return false;
}

bool Rpl_acf_configuration_handler::send_failover(const char *data,
                                                  size_t data_length) {
  DBUG_TRACE;
  return send(m_message_tag[0].c_str(), data, data_length);
}

bool Rpl_acf_configuration_handler::send_managed(const char *data,
                                                 size_t data_length) {
  DBUG_TRACE;
  return send(m_message_tag[1].c_str(), data, data_length);
}

bool Rpl_acf_configuration_handler::send_channel_status(const char *data,
                                                        size_t data_length) {
  DBUG_TRACE;
  return send(m_message_tag[2].c_str(), data, data_length);
}

bool Rpl_acf_configuration_handler::send(const char *tag, const char *data,
                                         size_t data_length) {
  DBUG_TRACE;
  bool error = false;

  if (tag == nullptr) return true;

  my_h_service gr_status_service_handler = nullptr;
  SERVICE_TYPE(group_replication_status_service_v1) *gr_status_service =
      nullptr;
  my_h_service gr_send_service_handler = nullptr;
  SERVICE_TYPE(group_replication_message_service_send) *gr_send_service =
      nullptr;

  srv_registry->acquire("group_replication_status_service_v1",
                        &gr_status_service_handler);
  if (nullptr == gr_status_service_handler) {
    // GR plugin is not loaded.
    goto end;
  }

  srv_registry->acquire("group_replication_message_service_send",
                        &gr_send_service_handler);
  if (nullptr == gr_send_service_handler) {
    // GR plugin is not loaded.
    goto end;
  }

  gr_status_service =
      reinterpret_cast<SERVICE_TYPE(group_replication_status_service_v1) *>(
          gr_status_service_handler);
  if (!gr_status_service
           ->is_group_in_single_primary_mode_and_im_the_primary()) {
    goto end;
  }

  gr_send_service =
      reinterpret_cast<SERVICE_TYPE(group_replication_message_service_send) *>(
          gr_send_service_handler);
  error = gr_send_service->send(tag, pointer_cast<const unsigned char *>(data),
                                data_length);

end:
  srv_registry->release(gr_send_service_handler);
  srv_registry->release(gr_status_service_handler);

  return error;
}

bool Rpl_acf_configuration_handler::send_channel_status_and_version_data(
    const std::string &channel, Rpl_acf_status_configuration::enum_key key,
    int status) {
  DBUG_TRACE;

  if (channel_map.is_group_replication_channel_name(channel.c_str())) {
    return false;
  }

  if (is_group_replication_member_secondary()) {
    return false;
  }

  protobuf_replication_asynchronous_connection_failover::VariableStatusList
      configuration;
  m_rpl_failover_channels_status.set_value_and_increment_version(
      channel, key, status, configuration);

  std::string serialized_configuration{};
  if (!configuration.SerializeToString(&serialized_configuration)) {
    return true;
  }

  if (rpl_acf_configuration_handler->send_channel_status(
          serialized_configuration.c_str(),
          serialized_configuration.length())) {
    return true;
  }

  return false;
}

void Rpl_acf_configuration_handler::delete_channel_status(
    const std::string &channel, Rpl_acf_status_configuration::enum_key key) {
  DBUG_TRACE;
  m_rpl_failover_channels_status.delete_channel_status(channel, key);
}

bool Rpl_acf_configuration_handler::send_managed_data(
    Rpl_sys_table_access &table_op) {
  std::vector<RPL_FAILOVER_MANAGED_JSON_TUPLE> managed_list;
  protobuf_replication_asynchronous_connection_failover::ManagedList
      configuration;
  std::string serialized_configuration{};

  if (Rpl_async_conn_failover_table_operations::
          read_managed_random_rows_internal(table_op, managed_list)) {
    return true;
  }

  for (auto managed_detail : managed_list) {
    auto managed = configuration.add_managed();
    managed->set_channel(std::get<0>(managed_detail));
    managed->set_managed_name(std::get<1>(managed_detail));
    managed->set_managed_type(std::get<2>(managed_detail));

    // Convert Json_wrapper to binary format
    String buffer;
    if (std::get<3>(managed_detail).to_binary(current_thd, &buffer)) {
      return true;
    }

    std::string str = std::string(buffer.ptr(), buffer.length());
    managed->set_configuration(str);
  }

  configuration.set_origin(server_uuid);
  configuration.set_version(table_op.get_version());

  if (!configuration.SerializeToString(&serialized_configuration)) {
    return true;
  }

  if (table_op.close(false)) {
    return true;
  }

  if (rpl_acf_configuration_handler->send_managed(
          serialized_configuration.c_str(),
          serialized_configuration.length())) {
    return true;
  }

  return false;
}

bool Rpl_acf_configuration_handler::send_failover_data(
    Rpl_sys_table_access &table_op) {
  bool error{false};
  RPL_FAILOVER_SOURCE_LIST source_list;
  protobuf_replication_asynchronous_connection_failover::SourceList
      configuration;
  std::string serialized_configuration{};

  std::tie(error, source_list) =
      Rpl_async_conn_failover_table_operations::read_source_all_rows_internal(
          table_op);
  if (error) return error;

  for (auto source_detail : source_list) {
    auto source = configuration.add_source();
    source->set_channel(std::get<0>(source_detail));
    source->set_host(std::get<1>(source_detail));
    source->set_port(std::get<2>(source_detail));
    source->set_network_namespace(std::get<3>(source_detail));
    source->set_weight(std::get<4>(source_detail));
    source->set_managed_name(std::get<5>(source_detail));
  }

  configuration.set_origin(server_uuid);
  configuration.set_version(table_op.get_version());

  if (!configuration.SerializeToString(&serialized_configuration)) {
    return true;
  }

  if (table_op.close(false)) {
    return true;
  }

  if (rpl_acf_configuration_handler->send_failover(
          serialized_configuration.c_str(),
          serialized_configuration.length())) {
    return true;
  }

  return false;
}

bool Rpl_acf_configuration_handler::send_table_data(
    Rpl_sys_table_access &table_op) {
  if (!table_op.get_table_name().compare(m_table_failover)) {
    return rpl_acf_configuration_handler->send_failover_data(table_op);
  } else if (!table_op.get_table_name().compare(m_table_managed)) {
    return rpl_acf_configuration_handler->send_managed_data(table_op);
  }

  return true;
}

void Rpl_acf_configuration_handler::reload_failover_channels_status() {
  DBUG_TRACE;
  m_rpl_failover_channels_status.reload();
}

bool Rpl_acf_configuration_handler::get_configuration(
    std::string &serialized_configuration) {
  DBUG_TRACE;
  bool error = false;

  protobuf_replication_asynchronous_connection_failover::
      SourceAndManagedAndStatusList configuration;
  configuration.set_origin(server_uuid);

  /* failover sources table */
  RPL_FAILOVER_SOURCE_LIST source_list;
  Rpl_sys_table_access table_sources(m_db, m_table_failover,
                                     m_table_failover_num_field);
  if (table_sources.open(TL_READ)) {
    return true;
  }
  std::tie(error, source_list) =
      Rpl_async_conn_failover_table_operations::read_source_all_rows_internal(
          table_sources);
  if (error) {
    return error;
  }

  configuration.set_source_version(table_sources.get_version());
  for (RPL_FAILOVER_SOURCE_TUPLE source_tuple : source_list) {
    protobuf_replication_asynchronous_connection_failover::Source *source =
        configuration.add_source();
    source->set_channel(std::get<0>(source_tuple));
    source->set_host(std::get<1>(source_tuple));
    source->set_port(std::get<2>(source_tuple));
    source->set_network_namespace(std::get<3>(source_tuple));
    source->set_weight(std::get<4>(source_tuple));
    source->set_managed_name(std::get<5>(source_tuple));
  }

  if (table_sources.close(error)) {
    return true;
  }

  /* failover managed table */
  std::vector<RPL_FAILOVER_MANAGED_JSON_TUPLE> managed_list;
  Rpl_sys_table_access table_managed(m_db, m_table_managed,
                                     m_table_managed_num_field);
  if (table_managed.open(TL_READ)) {
    return true;
  }
  error = Rpl_async_conn_failover_table_operations::
      read_managed_random_rows_internal(table_managed, managed_list);
  if (error) {
    return error;
  }

  configuration.set_managed_version(table_managed.get_version());
  for (RPL_FAILOVER_MANAGED_JSON_TUPLE managed_tuple : managed_list) {
    protobuf_replication_asynchronous_connection_failover::Managed *managed =
        configuration.add_managed();
    managed->set_channel(std::get<0>(managed_tuple));
    managed->set_managed_name(std::get<1>(managed_tuple));
    managed->set_managed_type(std::get<2>(managed_tuple));

    // Convert Json_wrapper to binary format
    String buffer;
    if (std::get<3>(managed_tuple).to_binary(current_thd, &buffer)) {
      return true;
    }

    std::string str = std::string(buffer.ptr(), buffer.length());
    managed->set_configuration(str);
  }

  if (table_managed.close(error)) {
    return true;
  }

  /* status */
  m_rpl_failover_channels_status.get(configuration);

  if (!configuration.SerializeToString(&serialized_configuration)) {
    return true;
  }

  return error;
}

bool Rpl_acf_configuration_handler::set_configuration(
    const std::vector<std::string>
        &exchanged_replication_failover_channels_serialized_configuration) {
  DBUG_TRACE;

  if (exchanged_replication_failover_channels_serialized_configuration.size() <
      1) {
    /*
      This member joined a group on which all members do not support
      WL#14020, as such this member needs to reset its replication
      failover channels configuration to the default one.
    */
    LogErr(WARNING_LEVEL, ER_GRP_RPL_FAILOVER_CONF_DEFAULT_CONFIGURATION);
    Rpl_async_conn_failover_table_operations sql_operations(TL_WRITE);
    if (sql_operations.reset()) {
      LogErr(ERROR_LEVEL,
             ER_GRP_RPL_FAILOVER_CONF_UNABLE_TO_SET_DEFAULT_CONFIGURATION);
      return true;
    }

    if (m_rpl_failover_channels_status.reset()) {
      LogErr(ERROR_LEVEL,
             ER_GRP_RPL_FAILOVER_CONF_UNABLE_TO_SET_DEFAULT_CONFIGURATION);
      return true;
    }

    return false;
  }

  /*
    Since we receive the replication failover channels configuration from
    all non-joining members, and its changes may be being propagated
    concurrently with membership changes, we need to choose the configuration
    with higher version.
  */
  protobuf_replication_asynchronous_connection_failover::
      SourceAndManagedAndStatusList configuration_sources_with_higher_version;
  configuration_sources_with_higher_version.set_source_version(0);

  protobuf_replication_asynchronous_connection_failover::
      SourceAndManagedAndStatusList configuration_managed_with_higher_version;
  configuration_managed_with_higher_version.set_managed_version(0);

  protobuf_replication_asynchronous_connection_failover::
      SourceAndManagedAndStatusList configuration_status_with_higher_version;
  configuration_status_with_higher_version.set_status_version(0);

  for (std::string replication_failover_channels_serialized_configuration :
       exchanged_replication_failover_channels_serialized_configuration) {
    protobuf_replication_asynchronous_connection_failover::
        SourceAndManagedAndStatusList configuration;

    if (!configuration.ParseFromString(
            replication_failover_channels_serialized_configuration)) {
      LogErr(ERROR_LEVEL, ER_GRP_RPL_FAILOVER_CONF_PARSE_ON_MEMBER_JOIN);
      continue;
    }

    if (configuration.source_version() >
        configuration_sources_with_higher_version.source_version()) {
      configuration_sources_with_higher_version.CopyFrom(configuration);
    }

    if (configuration.managed_version() >
        configuration_managed_with_higher_version.managed_version()) {
      configuration_managed_with_higher_version.CopyFrom(configuration);
    }

    if (configuration.status_version() >
        configuration_status_with_higher_version.status_version()) {
      configuration_status_with_higher_version.CopyFrom(configuration);
    }
  }

  /* failover sources table */
  if (set_failover_sources_internal(
          configuration_sources_with_higher_version)) {
    return true;
  }

  /* failover managed table */
  if (set_failover_managed_internal(
          configuration_managed_with_higher_version)) {
    return true;
  }

  /* status */
  if (m_rpl_failover_channels_status.set(
          configuration_status_with_higher_version)) {
    return true;
  }

  return false;
}

bool Rpl_acf_configuration_handler::
    force_my_replication_failover_channels_configuration_on_all_members() {
  DBUG_TRACE;

  std::string serialized_configuration;
  bool error = get_configuration(serialized_configuration);
  if (error) {
    return true;
  }

  return send(m_message_tag[3].c_str(), serialized_configuration.c_str(),
              serialized_configuration.length());
}

bool Rpl_acf_configuration_handler::receive_failover_and_managed_and_status(
    const unsigned char *data, size_t data_length) {
  DBUG_TRACE;

  protobuf_replication_asynchronous_connection_failover::
      SourceAndManagedAndStatusList configuration;
  if (!configuration.ParseFromArray(data, data_length)) {
    return true;
  }

  if (configuration.origin().compare(server_uuid)) {
    /* failover sources table */
    if (set_failover_sources_internal(configuration)) {
      return true;
    }

    /* failover managed table */
    if (set_failover_managed_internal(configuration)) {
      return true;
    }

    /* status */
    channel_map.wrlock();
    bool set_status_error = m_rpl_failover_channels_status.set(configuration);
    channel_map.unlock();
    if (set_status_error) {
      return true;
    }
  }

  return false;
}

bool Rpl_acf_configuration_handler::set_failover_sources_internal(
    const protobuf_replication_asynchronous_connection_failover::
        SourceAndManagedAndStatusList &configuration) {
  DBUG_TRACE;

  /* failover sources table */
  Rpl_sys_table_access table_sources(m_db, m_table_failover,
                                     m_table_failover_num_field);
  if (table_sources.open(TL_WRITE)) {
    return true;
  }

  /*
    Older MySQL versions may have configuration without version,
    which means its value will be 0, though we only store version
    greater than 1 on the versions table.
  */
  if (configuration.source_version() > 0) {
    if (table_sources.update_version(configuration.source_version())) {
      return true;
    }
  } else {
    if (table_sources.delete_version()) {
      return true;
    }
  }

  if (table_sources.delete_all_rows()) {
    return true;
  }

  for (const protobuf_replication_asynchronous_connection_failover::Source
           &source : configuration.source()) {
    bool err_val{false};
    std::string err_msg{};
    std::tie(err_val, err_msg) =
        Rpl_async_conn_failover_table_operations::add_source_skip_send(
            source.channel(), source.host(), source.port(),
            source.network_namespace(), source.weight(), source.managed_name(),
            table_sources);
    if (err_val) {
      return true;
    }
  }

  if (table_sources.close(false, true)) {
    return true;
  }

  return false;
}

bool Rpl_acf_configuration_handler::set_failover_managed_internal(
    const protobuf_replication_asynchronous_connection_failover::
        SourceAndManagedAndStatusList &configuration) {
  DBUG_TRACE;

  Rpl_sys_table_access table_managed(m_db, m_table_managed,
                                     m_table_managed_num_field);
  if (table_managed.open(TL_WRITE)) {
    return true;
  }

  /*
    Older MySQL versions may have configuration without version,
    which means its value will be 0, though we only store version
    greater than 1 on the versions table.
  */
  if (configuration.managed_version() > 0) {
    if (table_managed.update_version(configuration.managed_version())) {
      return true;
    }
  } else {
    if (table_managed.delete_version()) {
      return true;
    }
  }

  if (table_managed.delete_all_rows()) {
    return true;
  }

  for (const protobuf_replication_asynchronous_connection_failover::Managed
           &managed : configuration.managed()) {
    bool err_val{false};
    std::string err_msg{};

    json_binary::Value json_val = json_binary::parse_binary(
        managed.configuration().c_str(), managed.configuration().length());
    if (json_val.type() == json_binary::Value::ERROR) {
      return true;
    }

    Json_wrapper wrapper(json_val);
    std::tie(err_val, err_msg) =
        Rpl_async_conn_failover_table_operations::add_managed_skip_send(
            managed.channel(), managed.managed_type(), managed.managed_name(),
            wrapper, table_managed);
    if (err_val) {
      return true;
    }
  }

  if (table_managed.close(false, true)) {
    return true;
  }

  return false;
}
