/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <string>

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/ps_information.h"

using std::string;

bool get_group_members_info(
    uint index, const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS &callbacks,
    Group_member_info_manager_interface *group_member_manager,
    char *channel_name) {
  if (channel_name != NULL) {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  /*
   This case means that the plugin has never been initialized...
   and one would not be able to extract information
   */
  if (group_member_manager == NULL) {
    const char *member_state = Group_member_info::get_member_status_string(
        Group_member_info::MEMBER_OFFLINE);
    callbacks.set_member_state(callbacks.context, *member_state,
                               strlen(member_state));
    return false;
  }

  size_t number_of_members = group_member_manager->get_number_of_members();
  if (index >= number_of_members) {
    /* purecov: begin inspected */
    if (index != 0) {
      // No members on view.
      return true;
    }
    /* purecov: end */
  }

  Group_member_info *member_info = NULL;
  /*
    If the local member is already OFFLINE but still has the previous
    membership because is waiting for the leave view, do not report
    the other members.
  */
  if (local_member_info != NULL && local_member_info->get_recovery_status() ==
                                       Group_member_info::MEMBER_OFFLINE) {
    member_info = group_member_manager->get_group_member_info(
        local_member_info->get_uuid());
  } else {
    member_info = group_member_manager->get_group_member_info_by_index(index);
  }

  if (member_info == NULL)  // The requested member is not managed...
  {
    return true; /* purecov: inspected */
  }

  // Get info from view.
  std::string uuid(member_info->get_uuid());
  callbacks.set_member_id(callbacks.context, *uuid.c_str(), uuid.length());

  std::string hostname(member_info->get_hostname());
  callbacks.set_member_host(callbacks.context, *hostname.c_str(),
                            hostname.length());

  callbacks.set_member_port(callbacks.context, member_info->get_port());

  const char *member_state;
  const char *member_role = member_info->get_member_role_string();
  std::string member_version =
      (member_info->get_recovery_status() != Group_member_info::MEMBER_OFFLINE)
          ? member_info->get_member_version().get_version_string()
          : "";

  // override the state if we think it is unreachable
  if (!member_info->is_unreachable())
    member_state = Group_member_info::get_member_status_string(
        member_info->get_recovery_status());
  else
    member_state = Group_member_info::get_member_status_string(
        Group_member_info::MEMBER_UNREACHABLE);

  callbacks.set_member_state(callbacks.context, *member_state,
                             strlen(member_state));

  callbacks.set_member_role(callbacks.context, *member_role,
                            strlen(member_role));

  callbacks.set_member_version(callbacks.context, *member_version.c_str(),
                               member_version.length());

  delete member_info;

  return false;
}

bool get_group_member_stats(
    uint index, const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS &callbacks,
    Group_member_info_manager_interface *group_member_manager,
    Applier_module *applier_module, Gcs_operations *gcs_module,
    char *channel_name) {
  if (channel_name != NULL) {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  /*
   This case means that the plugin has never been initialized...
   and one would not be able to extract information
   */
  if (group_member_manager == NULL) {
    return false;
  }

  Group_member_info *member_info = NULL;
  /*
    If the local member is already OFFLINE but still has the previous
    membership because is waiting for the leave view, do not report
    the other members.
  */
  if (local_member_info != NULL && local_member_info->get_recovery_status() ==
                                       Group_member_info::MEMBER_OFFLINE) {
    member_info = group_member_manager->get_group_member_info(
        local_member_info->get_uuid());
  } else {
    member_info = group_member_manager->get_group_member_info_by_index(index);
  }

  if (member_info == NULL)  // The requested member is not managed...
  {
    return true; /* purecov: inspected */
  }

  std::string uuid(member_info->get_uuid());
  callbacks.set_member_id(callbacks.context, *uuid.c_str(), uuid.length());

  // Retrieve view information
  Gcs_view *view = gcs_module->get_current_view();
  if (view != NULL) {
    const char *view_id_representation =
        view->get_view_id().get_representation().c_str();
    callbacks.set_view_id(callbacks.context, *view_id_representation,
                          strlen(view_id_representation));
    delete view;
  }

  // Check if the group replication has started and a valid certifier exists
  if (applier_module != NULL) {
    // For local member fetch information locally
    Certification_handler *cert = applier_module->get_certification_handler();
    Certifier_interface *cert_module = (cert ? cert->get_certifier() : NULL);
    if (local_member_info && !local_member_info->get_uuid().compare(uuid) &&
        cert_module) {
      /* certification related data */
      callbacks.set_transactions_conflicts_detected(
          callbacks.context, cert_module->get_negative_certified());
      callbacks.set_transactions_certified(
          callbacks.context,
          applier_module->get_pipeline_stats_member_collector()
              ->get_transactions_certified());
      callbacks.set_transactions_rows_in_validation(
          callbacks.context, cert_module->get_certification_info_size());
      callbacks.set_transactions_in_queue(
          callbacks.context, applier_module->get_message_queue_size());

      /* applier information */
      callbacks.set_transactions_remote_applier_queue(
          callbacks.context,
          applier_module->get_pipeline_stats_member_collector()
              ->get_transactions_waiting_apply());
      callbacks.set_transactions_remote_applied(
          callbacks.context,
          applier_module->get_pipeline_stats_member_collector()
              ->get_transactions_applied());

      /* local member information */
      callbacks.set_transactions_local_proposed(
          callbacks.context,
          applier_module->get_pipeline_stats_member_collector()
              ->get_transactions_local());
      callbacks.set_transactions_local_rollback(
          callbacks.context,
          applier_module->get_pipeline_stats_member_collector()
              ->get_transactions_local_rollback());

      /* transactions data */
      char *committed_transactions_buf = NULL;
      size_t committed_transactions_buf_length = 0;
      int get_group_stable_transactions_set_string_outcome =
          cert_module->get_group_stable_transactions_set_string(
              &committed_transactions_buf, &committed_transactions_buf_length);
      if (!get_group_stable_transactions_set_string_outcome &&
          committed_transactions_buf_length > 0) {
        callbacks.set_transactions_committed(callbacks.context,
                                             *committed_transactions_buf,
                                             committed_transactions_buf_length);
      }
      my_free(committed_transactions_buf);

      std::string last_conflict_free_transaction;
      cert_module->get_last_conflict_free_transaction(
          &last_conflict_free_transaction);
      callbacks.set_last_conflict_free_transaction(
          callbacks.context, *last_conflict_free_transaction.c_str(),
          last_conflict_free_transaction.length());
    } else  // Fetch network received information for remote members
    {
      Pipeline_member_stats *pipeline_stats = NULL;
      if ((pipeline_stats =
               applier_module->get_flow_control_module()->get_pipeline_stats(
                   member_info->get_gcs_member_id().get_member_id())) != NULL) {
        callbacks.set_last_conflict_free_transaction(
            callbacks.context,
            *pipeline_stats->get_transaction_last_conflict_free().c_str(),
            pipeline_stats->get_transaction_last_conflict_free().length());

        callbacks.set_transactions_committed(
            callbacks.context,
            *pipeline_stats->get_transaction_committed_all_members().c_str(),
            pipeline_stats->get_transaction_committed_all_members().length());

        /* certification related data */
        callbacks.set_transactions_conflicts_detected(
            callbacks.context,
            pipeline_stats->get_transactions_negative_certified());
        callbacks.set_transactions_certified(
            callbacks.context, pipeline_stats->get_transactions_certified());
        callbacks.set_transactions_rows_in_validation(
            callbacks.context,
            pipeline_stats->get_transactions_rows_validating());
        callbacks.set_transactions_in_queue(
            callbacks.context,
            pipeline_stats->get_transactions_waiting_certification());

        /* applier information */
        callbacks.set_transactions_remote_applier_queue(
            callbacks.context,
            pipeline_stats->get_transactions_waiting_apply());
        callbacks.set_transactions_remote_applied(
            callbacks.context, pipeline_stats->get_transactions_applied());

        /* local member information */
        callbacks.set_transactions_local_proposed(
            callbacks.context, pipeline_stats->get_transactions_local());
        callbacks.set_transactions_local_rollback(
            callbacks.context,
            pipeline_stats->get_transactions_local_rollback());

        /* clean-up */
        delete pipeline_stats;
      }
    }
  }

  delete member_info;

  return false;
}

bool get_connection_status(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS &callbacks,
    char *group_name_pointer, char *channel_name,
    bool is_group_replication_running) {
  if (channel_name != NULL) {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  if (group_name_pointer != NULL) {
    callbacks.set_group_name(callbacks.context, *group_name_pointer,
                             strlen(group_name_pointer));
    callbacks.set_source_uuid(callbacks.context, *group_name_pointer,
                              strlen(group_name_pointer));
  }

  callbacks.set_service_state(callbacks.context, is_group_replication_running);

  return false;
}
