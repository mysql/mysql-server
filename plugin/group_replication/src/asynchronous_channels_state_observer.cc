/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/asynchronous_channels_state_observer.h"

#include <mysql/components/services/log_builtins.h>
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin.h"

Asynchronous_channels_state_observer::Asynchronous_channels_state_observer() =
    default;

int Asynchronous_channels_state_observer::thread_start(
    Binlog_relay_IO_param *param) {
  /*
    If server is auto starting on non bootstrap member,
    then block all slave threads till member comes ONLINE.
  */
  if (is_plugin_auto_starting_on_non_bootstrap_member() &&
      strcmp(param->channel_name, "group_replication_recovery") != 0 &&
      strcmp(param->channel_name, "group_replication_applier") != 0) {
    const enum_wait_on_start_process_result abort =
        initiate_wait_on_start_process();
    switch (abort) {
      case WAIT_ON_START_PROCESS_SUCCESS:
        break;
      case WAIT_ON_START_PROCESS_ABORT_ON_CLONE:
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_REPLICA_THREAD_ERROR_ON_CLONE,
                     "replica IO", param->channel_name);
        return 1;
      case WAIT_ON_START_PROCESS_ABORT_SECONDARY_MEMBER:
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_REPLICA_THREAD_ERROR_ON_SECONDARY_MEMBER,
                     "replica IO", param->channel_name);
        return 1;
    }

    if (group_member_mgr && local_member_info->get_recovery_status() ==
                                Group_member_info::MEMBER_ONLINE) {
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_REPLICA_IO_THREAD_UNBLOCKED,
                   param->channel_name);
    } else if (group_member_mgr && (local_member_info->get_recovery_status() ==
                                        Group_member_info::MEMBER_ERROR ||
                                    local_member_info->get_recovery_status() ==
                                        Group_member_info::MEMBER_OFFLINE)) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_REPLICA_IO_THREAD_ERROR_OUT,
                   param->channel_name);
      return 1;
    }
  }

  /* Can't start replica relay io thread when group replication is running on
   single-primary mode on secondary */
  if (is_plugin_configured_and_starting() &&
      strcmp(param->channel_name, "group_replication_recovery") != 0 &&
      strcmp(param->channel_name, "group_replication_applier") != 0 &&
      group_member_mgr && local_member_info->in_primary_mode()) {
    std::string m_uuid;
    group_member_mgr->get_primary_member_uuid(m_uuid);

    if (m_uuid == "UNDEFINED") {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_REPLICA_IO_THD_PRIMARY_UNKNOWN,
                   param->channel_name);
      return 1;
    }

    if (m_uuid != local_member_info->get_uuid()) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SALVE_IO_THD_ON_SECONDARY_MEMBER,
                   param->channel_name);
      return 1;
    }
  }

  if (plugin_is_group_replication_running() &&
      !param->source_connection_auto_failover &&
      !primary_election_handler->is_an_election_running()) {
    std::pair<std::string, std::string> action_initiator_and_description;
    if (group_action_coordinator->is_group_action_running(
            action_initiator_and_description)) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_CHANNEL_THREAD_WHEN_GROUP_ACTION_RUNNING,
                   "IO THREAD", param->channel_name,
                   action_initiator_and_description.second.c_str(),
                   action_initiator_and_description.first.c_str());
      return 1;
    }
  }

  return 0;
}

int Asynchronous_channels_state_observer::thread_stop(Binlog_relay_IO_param *) {
  return 0;
}

int Asynchronous_channels_state_observer::applier_start(
    Binlog_relay_IO_param *param) {
  /*
    If server is auto starting on non bootstrap member,
    then block all slave threads till member comes ONLINE.
  */
  if (is_plugin_auto_starting_on_non_bootstrap_member() &&
      strcmp(param->channel_name, "group_replication_recovery") != 0 &&
      strcmp(param->channel_name, "group_replication_applier") != 0) {
    const enum_wait_on_start_process_result abort =
        initiate_wait_on_start_process();
    switch (abort) {
      case WAIT_ON_START_PROCESS_SUCCESS:
        break;
      case WAIT_ON_START_PROCESS_ABORT_ON_CLONE:
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_REPLICA_THREAD_ERROR_ON_CLONE,
                     "replica applier", param->channel_name);
        return 1;
      case WAIT_ON_START_PROCESS_ABORT_SECONDARY_MEMBER:
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_REPLICA_THREAD_ERROR_ON_SECONDARY_MEMBER,
                     "replica applier", param->channel_name);
        return 1;
    }

    if (group_member_mgr && local_member_info->get_recovery_status() ==
                                Group_member_info::MEMBER_ONLINE) {
      LogPluginErr(INFORMATION_LEVEL,
                   ER_GRP_RPL_REPLICA_APPLIER_THREAD_UNBLOCKED,
                   param->channel_name);
    } else if (group_member_mgr && (local_member_info->get_recovery_status() ==
                                        Group_member_info::MEMBER_ERROR ||
                                    local_member_info->get_recovery_status() ==
                                        Group_member_info::MEMBER_OFFLINE)) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_REPLICA_APPLIER_THREAD_ERROR_OUT,
                   param->channel_name);
      return 1;
    }
  }

  /* Can't start replica relay sql thread when group replication is running on
     single-primary mode on secondary */
  if (is_plugin_configured_and_starting() &&
      strcmp(param->channel_name, "group_replication_recovery") != 0 &&
      strcmp(param->channel_name, "group_replication_applier") != 0 &&
      group_member_mgr && local_member_info->in_primary_mode()) {
    std::string m_uuid;
    group_member_mgr->get_primary_member_uuid(m_uuid);

    if (m_uuid == "UNDEFINED") {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_REPLICA_SQL_THD_PRIMARY_UNKNOWN,
                   param->channel_name);
      return 1;
    }

    if (m_uuid != local_member_info->get_uuid()) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_REPLICA_SQL_THD_ON_SECONDARY_MEMBER,
                   param->channel_name);
      return 1;
    }
  }

  if (plugin_is_group_replication_running() &&
      !param->source_connection_auto_failover &&
      !primary_election_handler->is_an_election_running()) {
    std::pair<std::string, std::string> action_initiator_and_description;
    if (group_action_coordinator->is_group_action_running(
            action_initiator_and_description)) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_CHANNEL_THREAD_WHEN_GROUP_ACTION_RUNNING,
                   "SQL THREAD", param->channel_name,
                   action_initiator_and_description.second.c_str(),
                   action_initiator_and_description.first.c_str());
      return 1;
    }
  }

  return 0;
}

int Asynchronous_channels_state_observer::applier_stop(Binlog_relay_IO_param *,
                                                       bool) {
  return 0;
}

int Asynchronous_channels_state_observer::before_request_transmit(
    Binlog_relay_IO_param *, uint32) {
  return 0;
}

int Asynchronous_channels_state_observer::after_read_event(
    Binlog_relay_IO_param *, const char *, unsigned long, const char **,
    unsigned long *) {
  return 0;
}

int Asynchronous_channels_state_observer::after_queue_event(
    Binlog_relay_IO_param *, const char *, unsigned long, uint32) {
  return 0;
}

int Asynchronous_channels_state_observer::after_reset_slave(
    Binlog_relay_IO_param *) {
  return 0;
}

int Asynchronous_channels_state_observer::applier_log_event(
    Binlog_relay_IO_param *param, Trans_param *trans_param, int &out) {
  out = 0;

  if (is_plugin_configured_and_starting() ||
      (group_member_mgr && local_member_info->get_recovery_status() ==
                               Group_member_info::MEMBER_ONLINE)) {
    Replication_thread_api channel_interface;

    /*
      Cycle through all involved tables to assess if they all
      comply with the plugin runtime requirements. For now:
      - The table must be from a transactional engine
      - It must contain at least one primary key
      - It should not contain 'ON DELETE/UPDATE CASCADE' referential action
    */
    for (uint table = 0; table < trans_param->number_of_tables; table++) {
      if (trans_param->tables_info[table].db_type != DB_TYPE_INNODB) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NEEDS_INNODB_TABLE,
                     trans_param->tables_info[table].table_name);
        out++;
      }

      if (trans_param->tables_info[table].number_of_primary_keys == 0) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_PRIMARY_KEY_NOT_DEFINED,
                     trans_param->tables_info[table].table_name);
        out++;
      }

      if (is_plugin_configured_and_starting() &&
          local_member_info->has_enforces_update_everywhere_checks() &&
          trans_param->tables_info[table].has_cascade_foreign_key &&
          !channel_interface.is_own_event_applier(
              param->thread_id, "group_replication_applier")) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FK_WITH_CASCADE_UNSUPPORTED,
                     trans_param->tables_info[table].table_name);
        out++;
      }
    }
  }

  return 0;
}
