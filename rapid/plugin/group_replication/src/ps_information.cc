/* Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <string>

#include "plugin.h"
#include "ps_information.h"

using std::string;

bool get_group_members_info(uint index,
                            const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks,
                            Group_member_info_manager_interface
                                *group_member_manager,
                            char *group_name_pointer,
                            char *channel_name)
{
  if (channel_name != NULL)
  {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  /*
   This case means that the plugin has never been initialized...
   and one would not be able to extract information
   */
  if (group_member_manager == NULL)
  {
    const char* member_state= Group_member_info::get_member_status_string(
                                  Group_member_info::MEMBER_OFFLINE);
    callbacks.set_member_state(callbacks.context, *member_state,
                               strlen(member_state));
    return false;
  }

  size_t number_of_members= group_member_manager->get_number_of_members();
  if (index >= number_of_members) {
    /* purecov: begin inspected */
    if (index != 0) {
      // No members on view.
      return true;
    }
    /* purecov: end */
  }

  Group_member_info* member_info=
      group_member_manager->get_group_member_info_by_index(index);

  if(member_info == NULL) // The requested member is not managed...
  {
    return true; /* purecov: inspected */
  }

  // Get info from view.
  callbacks.set_member_id(callbacks.context,
                          *member_info->get_uuid().c_str(),
                          member_info->get_uuid().length());

  callbacks.set_member_host(callbacks.context,
                            *member_info->get_hostname().c_str(),
                            member_info->get_hostname().length());

  callbacks.set_member_port(callbacks.context, member_info->get_port());

  const char* member_state;

  // override the state if we think it is unreachable
  if (!member_info->is_unreachable())
    member_state= Group_member_info::get_member_status_string(
                                member_info->get_recovery_status());
  else
    member_state= Group_member_info::get_member_status_string(
                                Group_member_info::MEMBER_UNREACHABLE);

  callbacks.set_member_state(callbacks.context, *member_state,
                             strlen(member_state));

  delete member_info;

  return false;
}

bool get_group_member_stats(const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks,
                            Group_member_info_manager_interface
                                *group_member_manager,
                            Applier_module *applier_module,
                            Gcs_operations *gcs_module,
                            char *group_name_pointer,
                            char *channel_name)
{
  if (group_member_manager != NULL)
  {
    char *hostname, *uuid;
    uint port;
    unsigned int server_version;
    st_server_ssl_variables server_ssl_variables=
      {false,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

    get_server_parameters(&hostname, &port, &uuid, &server_version,
                          &server_ssl_variables);

    callbacks.set_member_id(callbacks.context, *uuid, strlen(uuid));
  }

  if(channel_name != NULL)
  {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  //Retrieve view information
  Gcs_view *view= gcs_module->get_current_view();
  if (view != NULL)
  {
    const char* view_id_representation=
        view->get_view_id().get_representation().c_str();
    callbacks.set_view_id(callbacks.context, *view_id_representation,
                          strlen(view_id_representation));
    delete view;
  }

  DBUG_EXECUTE_IF("group_replication_get_group_member_stats",
  {
    const char act[] =
        "now signal signal.reached_get_group_member_stats "
        "wait_for signal.resume_get_group_member_stats";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  //Check if the group replication has started and a valid certifier exists
  if(!get_plugin_is_stopping() && applier_module != NULL)
  {
    Pipeline_member_stats *pipeline_stats= applier_module->get_local_pipeline_stats();
    if (pipeline_stats != NULL)
    {
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

      /* clean-up */
      delete pipeline_stats;
    }
  }

  return false;
}

bool get_connection_status(const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks,
                           char *group_name_pointer,
                           char *channel_name,
                           bool is_group_replication_running)
{
  if (channel_name != NULL)
  {
    callbacks.set_channel_name(callbacks.context, *channel_name,
                               strlen(channel_name));
  }

  if (group_name_pointer != NULL)
  {
    callbacks.set_group_name(callbacks.context, *group_name_pointer,
                             strlen(group_name_pointer));
    callbacks.set_source_uuid(callbacks.context, *group_name_pointer,
                              strlen(group_name_pointer));
  }

  callbacks.set_service_state(callbacks.context,
                              is_group_replication_running);

  return false;
}

