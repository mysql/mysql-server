/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include <string>

#include "mutex_lock.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/ps_information.h"
#include "string_with_len.h"

using std::string;

bool get_group_members_info(
    uint index, const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS &callbacks,
    char *channel_name) {
  if (channel_name != nullptr) {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  /*
   This case means that the plugin has never been initialized...
   and one would not be able to extract information
   */
  if (group_member_mgr == nullptr) {
    const char *member_state = Group_member_info::get_member_status_string(
        Group_member_info::MEMBER_OFFLINE);
    callbacks.set_member_state(callbacks.context, *member_state,
                               strlen(member_state));
    char *hostname = nullptr;
    char *uuid = nullptr;
    uint port = 0U;
    uint server_version = 0U;
    uint admin_port = 0U;
    get_server_parameters(&hostname, &port, &uuid, &server_version,
                          &admin_port);
    callbacks.set_member_id(callbacks.context, *uuid, strlen(uuid));
    callbacks.set_member_host(callbacks.context, *hostname, strlen(hostname));
    callbacks.set_member_port(callbacks.context, port);

    const Member_version member_version(server_version);
    const std::string member_version_string =
        member_version.get_version_string();
    callbacks.set_member_version(callbacks.context,
                                 *member_version_string.c_str(),
                                 member_version_string.length());

    const char *incoming_connection_protocol =
        Communication_stack_to_string::to_string(
            static_cast<enum_transport_protocol>(
                get_communication_stack_var()));
    callbacks.set_member_incoming_communication_protocol(
        callbacks.context, *incoming_connection_protocol,
        strlen(incoming_connection_protocol));

    return false;
  }

  size_t number_of_members = group_member_mgr->get_number_of_members();
  if (index >= number_of_members) {
    /* purecov: begin inspected */
    if (index != 0) {
      // No members on view.
      return true;
    }
    /* purecov: end */
  }

  Group_member_info member_info;
  bool member_info_not_found = true;
  /*
    If the local member is already OFFLINE but still has the previous
    membership because is waiting for the leave view, do not report
    the other members.
  */
  if (local_member_info != nullptr &&
      local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_OFFLINE) {
    member_info_not_found = group_member_mgr->get_group_member_info(
        local_member_info->get_uuid(), member_info);
  } else {
    member_info_not_found =
        group_member_mgr->get_group_member_info_by_index(index, member_info);
  }

  if (member_info_not_found)  // The requested member is not managed...
  {
    return true; /* purecov: inspected */
  }

  // Get info from view.
  std::string uuid(member_info.get_uuid());
  callbacks.set_member_id(callbacks.context, *uuid.c_str(), uuid.length());

  std::string hostname(member_info.get_hostname());
  callbacks.set_member_host(callbacks.context, *hostname.c_str(),
                            hostname.length());

  callbacks.set_member_port(callbacks.context, member_info.get_port());

  const char *member_state;
  const char *member_role = member_info.get_member_role_string();
  const std::string member_version =
      member_info.get_member_version().get_version_string();

  // override the state if we think it is unreachable
  if (!member_info.is_unreachable())
    member_state = Group_member_info::get_member_status_string(
        member_info.get_recovery_status());
  else
    member_state = Group_member_info::get_member_status_string(
        Group_member_info::MEMBER_UNREACHABLE);

  callbacks.set_member_state(callbacks.context, *member_state,
                             strlen(member_state));

  callbacks.set_member_role(callbacks.context, *member_role,
                            strlen(member_role));

  callbacks.set_member_version(callbacks.context, *member_version.c_str(),
                               member_version.length());

  enum_transport_protocol incoming_connection_protocol_value = INVALID_PROTOCOL;
  if (gcs_module == nullptr || (local_member_info->get_recovery_status() ==
                                Group_member_info::MEMBER_OFFLINE)) {
    // use the value that is present in the variable
    incoming_connection_protocol_value =
        static_cast<enum_transport_protocol>(get_communication_stack_var());
  } else {
    incoming_connection_protocol_value =
        gcs_module->get_current_incoming_connections_protocol();
  }

  const char *incoming_connection_protocol =
      Communication_stack_to_string::to_string(
          incoming_connection_protocol_value);
  callbacks.set_member_incoming_communication_protocol(
      callbacks.context, *incoming_connection_protocol,
      strlen(incoming_connection_protocol));

  return false;
}

bool get_group_member_stats(
    uint index, const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS &callbacks,
    char *channel_name) {
  if (channel_name != nullptr) {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  /*
   This case means that the plugin has never been initialized...
   and one would not be able to extract information
   */
  if (group_member_mgr == nullptr) {
    return false;
  }

  Group_member_info member_info;
  bool member_info_not_found = true;
  /*
    If the local member is already OFFLINE but still has the previous
    membership because is waiting for the leave view, do not report
    the other members.
  */
  if (local_member_info != nullptr &&
      local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_OFFLINE) {
    member_info_not_found = group_member_mgr->get_group_member_info(
        local_member_info->get_uuid(), member_info);
  } else {
    member_info_not_found =
        group_member_mgr->get_group_member_info_by_index(index, member_info);
  }

  if (member_info_not_found)  // The requested member is not managed...
  {
    return true; /* purecov: inspected */
  }

  std::string uuid(member_info.get_uuid());
  callbacks.set_member_id(callbacks.context, *uuid.c_str(), uuid.length());

  if (nullptr == local_member_info ||
      local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_OFFLINE) {
    return false;
  }

  // Retrieve view information
  Gcs_view *view = gcs_module->get_current_view();
  if (view != nullptr) {
    const char *view_id_representation =
        view->get_view_id().get_representation().c_str();
    callbacks.set_view_id(callbacks.context, *view_id_representation,
                          strlen(view_id_representation));
    delete view;
  }

  DBUG_EXECUTE_IF("group_replication_get_group_member_stats", {
    const char act[] =
        "now signal signal.reached_get_group_member_stats "
        "wait_for signal.resume_get_group_member_stats";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  Checkable_rwlock::Guard g(*get_plugin_running_lock(),
                            Checkable_rwlock::READ_LOCK);

  DBUG_EXECUTE_IF(
      "group_replication_get_group_member_stats_plugin_running_lock_acquired", {
        const char act[] =
            "now signal "
            "signal.reached_get_group_member_stats_plugin_running_lock_"
            "acquired "
            "wait_for "
            "signal.resume_get_group_member_stats_plugin_running_lock_acquired";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });

  /*
    Protect the access to `applier_module` against concurrent auto-rejoin
    attempts which may destroy `applier_module` while is it being used to
    fetch the pipeline stats.
  */
  MUTEX_LOCK(lock, get_plugin_applier_module_initialize_terminate_lock());

  /*
    Check if the group replication is running and a valid certifier exists,
    while plugin_running_lock is acquired.
  */
  Pipeline_member_stats *pipeline_stats = nullptr;
  if (!get_plugin_is_stopping() && applier_module != nullptr &&
      (pipeline_stats =
           ((local_member_info && !local_member_info->get_uuid().compare(uuid))
                ? applier_module->get_local_pipeline_stats()
                : applier_module->get_flow_control_module()->get_pipeline_stats(
                      member_info.get_gcs_member_id().get_member_id()))) !=
          nullptr) {
    std::string last_conflict_free_transaction;
    pipeline_stats->get_transaction_last_conflict_free(
        last_conflict_free_transaction);
    callbacks.set_last_conflict_free_transaction(
        callbacks.context, *last_conflict_free_transaction.c_str(),
        last_conflict_free_transaction.length());

    std::string transaction_committed_all_members;
    pipeline_stats->get_transaction_committed_all_members(
        transaction_committed_all_members);
    callbacks.set_transactions_committed(
        callbacks.context, *transaction_committed_all_members.c_str(),
        transaction_committed_all_members.length());

    /* certification related data */
    callbacks.set_transactions_conflicts_detected(
        callbacks.context,
        pipeline_stats->get_transactions_negative_certified());
    callbacks.set_transactions_certified(
        callbacks.context, pipeline_stats->get_transactions_certified());
    callbacks.set_transactions_rows_in_validation(
        callbacks.context, pipeline_stats->get_transactions_rows_validating());
    callbacks.set_transactions_in_queue(
        callbacks.context,
        pipeline_stats->get_transactions_waiting_certification());

    /* applier information */
    callbacks.set_transactions_remote_applier_queue(
        callbacks.context, pipeline_stats->get_transactions_waiting_apply());
    callbacks.set_transactions_remote_applied(
        callbacks.context, pipeline_stats->get_transactions_applied());

    /* local member information */
    callbacks.set_transactions_local_proposed(
        callbacks.context, pipeline_stats->get_transactions_local());
    callbacks.set_transactions_local_rollback(
        callbacks.context, pipeline_stats->get_transactions_local_rollback());

    /* clean-up */
    delete pipeline_stats;
  }

  return false;
}

bool get_connection_status(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS &callbacks,
    char *group_name_pointer, char *channel_name,
    bool is_group_replication_running) {
  if (channel_name != nullptr) {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  if (group_name_pointer != nullptr) {
    callbacks.set_group_name(callbacks.context, *group_name_pointer,
                             strlen(group_name_pointer));
    callbacks.set_source_uuid(callbacks.context, *group_name_pointer,
                              strlen(group_name_pointer));
  }

  callbacks.set_service_state(callbacks.context, is_group_replication_running);

  return false;
}
