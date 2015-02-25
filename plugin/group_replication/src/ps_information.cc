/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <string>
#include <sstream>

#include "ps_information.h"

using std::string;

enum enum_member_state
map_protocol_node_state_to_server_member_state
                   (Cluster_member_info::Cluster_member_status protocol_status)
{
  switch(protocol_status)
  {
    case 1:  // MEMBER_ONLINE
      return MEMBER_STATE_ONLINE;
    case 2:  // MEMBER_OFFLINE
      return MEMBER_STATE_OFFLINE;
    case 3:  // MEMBER_IN_RECOVERY
      return MEMBER_STATE_RECOVERING;
    default:
      return MEMBER_STATE_OFFLINE;
  }
}

char* get_last_certified_transaction(char* buf, rpl_gno last_seq_num,
                                     char* gcs_group_pointer)
{
  if(last_seq_num > 0)
  {
    char seq_num[MAX_GNO_TEXT_LENGTH+1];
    char *last_cert_seq_num= my_safe_itoa(10, last_seq_num,
                                          &seq_num[sizeof(seq_num)- 1]);

    string group(gcs_group_pointer);
    group.append(":");
    group.append(last_cert_seq_num);
    buf= (char*)my_malloc(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                               PSI_NOT_INSTRUMENTED,
#endif
                               Gtid::MAX_TEXT_LENGTH+1, MYF(0));

    memcpy(buf, group.c_str(), Gtid::MAX_TEXT_LENGTH+1);
  }
  return buf;
}

bool get_gcs_group_members_info(uint index,
                                GROUP_REPLICATION_GROUP_MEMBERS_INFO *info,
                                Cluster_member_info_manager_interface
                                                        *cluster_member_manager,
                                Gcs_interface *gcs_module,
                                char* gcs_group_pointer,
                                char *channel_name)
{
  //Initialize all values
  memset(info, 0, sizeof(GROUP_REPLICATION_GROUP_MEMBERS_INFO));
  info->channel_name= NULL;
  info->member_id= NULL;
  info->member_host= NULL;
  info->member_port= 0;
  info->member_state= MEMBER_STATE_OFFLINE;

  info->channel_name= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                 PSI_NOT_INSTRUMENTED,
#endif
                                 channel_name,
                                 strlen(channel_name),
                                 MYF(0));

  /*
   This case means that the plugin has never been initialized...
   and one would not be able to extract information
   */
  if(cluster_member_manager == NULL)
  {
    info->member_id= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                             PSI_NOT_INSTRUMENTED,
#endif
                             "",
                             0,
                             MYF(0));

    info->member_host= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                             PSI_NOT_INSTRUMENTED,
#endif
                             "",
                             0,
                             MYF(0));

    return false;
  }

  Gcs_control_interface* ctrl_if= NULL;
  if(gcs_group_pointer != NULL)
  {
    string gcs_group_name(gcs_group_pointer);
    Gcs_group_identifier group_id(gcs_group_name);

    ctrl_if= gcs_module->get_control_session(group_id);
  }

  uint number_of_members= 0;
  if(ctrl_if != NULL)
  {
    Gcs_view* view= ctrl_if->get_current_view();
    if(view != NULL)
    {
      number_of_members= view->get_members()->size();
    }
  }
  else
  {
    number_of_members= 1;
  }

  if (index >= number_of_members) {
    if (index != 0) {
      // No members on view.
      return true;
    }
  }

  Cluster_member_info* member_info
              = cluster_member_manager->get_cluster_member_info_by_index(index);

  if(member_info == NULL) // The requested member is not managed...
  {
    return true;
  }

  // Get info from view.
  info->member_id= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                              PSI_NOT_INSTRUMENTED,
#endif
                              member_info->get_uuid()->c_str(),
                              member_info->get_uuid()->length(),
                              MYF(0));

  info->member_host= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                   PSI_NOT_INSTRUMENTED,
#endif
                                   member_info->get_hostname()->c_str(),
                                   member_info->get_hostname()->length(),
                                   MYF(0));

  info->member_port= member_info->get_port();

  info->member_state=
      map_protocol_node_state_to_server_member_state(
          member_info->get_recovery_status());

  delete member_info;

  return false;
}

bool get_gcs_group_member_stats(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO *info,
                                Cluster_member_info_manager_interface
                                                        *cluster_member_manager,
                                Applier_module *applier_module,
                                Gcs_interface *gcs_module,
                                char* gcs_group_pointer,
                                char *channel_name)
{
  //Initialize all fields
  memset(info, 0, sizeof(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO));
  info->channel_name= NULL;
  info->committed_transactions= NULL;
  info->last_conflict_free_transaction= NULL;
  info->member_id= NULL;
  info->view_id= NULL;
  info->transaction_conflicts_detected= 0;
  info->transaction_certified= 0;
  info->transactions_in_validation= 0;
  info->transaction_in_queue= 0;

  //This means that the plugin never started
  if(cluster_member_manager == NULL)
  {
    info->member_id= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                PSI_NOT_INSTRUMENTED,
#endif
                                "",
                                0,
                                MYF(0));
  }
  else
  {
    char *hostname, *uuid;
    uint port;
    get_server_host_port_uuid(&hostname, &port, &uuid);

    info->member_id= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                PSI_NOT_INSTRUMENTED,
#endif
                                uuid,
                                strlen(uuid),
                                MYF(0));
  }

  if(channel_name != NULL)
  {
    info->channel_name= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                PSI_NOT_INSTRUMENTED,
#endif
                                channel_name,
                                strlen(channel_name),
                                MYF(0));
  }

  //Retrieve view information
  Gcs_view* view= NULL;
  if(gcs_group_pointer != NULL)
  {
    string gcs_group_name(gcs_group_pointer);
    Gcs_group_identifier group_id(gcs_group_name);

    if(gcs_module != NULL)
    {
      Gcs_control_interface* ctrl_if= gcs_module->get_control_session(group_id);
      if(ctrl_if != NULL)
      {
        view= ctrl_if->get_current_view();
      }
    }
  }

  if(view != NULL)
  {
    char* view_id_representation= view->get_view_id()->get_representation();
    info->view_id= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                              PSI_NOT_INSTRUMENTED,
#endif
                              view_id_representation,
                              strlen(view_id_representation),
                              MYF(0));
  }

  Certification_handler *cert= NULL;

  //Check if the gcs replication has started and a valid certifier exists
  if(applier_module != NULL &&
     (cert = applier_module->get_certification_handler()) != NULL)
  {
    Certifier_interface *cert_module= cert->get_certifier();

    info->transaction_conflicts_detected= cert_module->get_negative_certified();
    info->transaction_certified= cert_module->get_positive_certified() +
                                 info->transaction_conflicts_detected;
    info->transactions_in_validation= cert_module->get_certification_info_size();
    info->transaction_in_queue= applier_module->get_message_queue_size();

    Gtid_set* stable_gtid_set= cert_module->get_group_stable_transactions_set();

    if(stable_gtid_set)
      info->committed_transactions= stable_gtid_set->to_string();

    rpl_gno temp_seq_num= cert_module->get_last_sequence_number();
    char* buf= NULL;
    info->last_conflict_free_transaction=
        get_last_certified_transaction(buf, temp_seq_num, gcs_group_pointer);
  }

  return false;
}

bool get_gcs_connection_status(GROUP_REPLICATION_CONNECTION_STATUS_INFO *info,
                               Gcs_interface *gcs_module,
                               char* gcs_group_pointer,
                               char *channel_name,
                               bool is_gcs_running)
{
  //Initialize all fields
  memset(info, 0, sizeof(GROUP_REPLICATION_CONNECTION_STATUS_INFO));
  info->channel_name= NULL;
  info->group_name= NULL;

  if(channel_name != NULL)
  {
    info->channel_name= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                   PSI_NOT_INSTRUMENTED,
#endif
                                   channel_name,
                                   strlen(channel_name),
                                   MYF(0));
  }

  if(gcs_group_pointer != NULL)
  {
    info->group_name= my_strndup(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                PSI_NOT_INSTRUMENTED,
#endif
                                gcs_group_pointer,
                                strlen(gcs_group_pointer),
                                MYF(0));
  }

  info->service_state= is_gcs_running;

  return false;
}

