/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "my_dbug.h"
#include "plugin/group_replication/include/gcs_event_handlers.h"
#include "plugin/group_replication/include/pipeline_stats.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/single_primary_message.h"

using std::vector;

Plugin_gcs_events_handler::
Plugin_gcs_events_handler(Applier_module_interface* applier_module,
                          Recovery_module* recovery_module,
                          Plugin_gcs_view_modification_notifier* vc_notifier,
                          Compatibility_module* compatibility_module,
                          ulong components_stop_timeout)
: applier_module(applier_module), recovery_module(recovery_module),
  view_change_notifier(vc_notifier),
  compatibility_manager(compatibility_module),
  stop_wait_timeout(components_stop_timeout)
{
  this->temporary_states= new std::set<Group_member_info*,
                                       Group_member_info_pointer_comparator>();
  this->joiner_compatibility_status= new st_compatibility_types(INCOMPATIBLE);

#ifndef DBUG_OFF
    set_number_of_members_on_view_changed_to_10= false;
    DBUG_EXECUTE_IF("group_replication_set_number_of_members_on_view_changed_to_10",
                    { set_number_of_members_on_view_changed_to_10= true; };);
#endif
}

Plugin_gcs_events_handler::~Plugin_gcs_events_handler()
{
  delete temporary_states;
  delete joiner_compatibility_status;
}

void
Plugin_gcs_events_handler::on_message_received(const Gcs_message& message) const
{
  Plugin_gcs_message::enum_cargo_type message_type=
      Plugin_gcs_message::get_cargo_type(
          message.get_message_data().get_payload());

  switch (message_type)
  {
  case Plugin_gcs_message::CT_TRANSACTION_MESSAGE:
    handle_transactional_message(message);
    break;

  case Plugin_gcs_message::CT_CERTIFICATION_MESSAGE:
    handle_certifier_message(message);
    break;

  case Plugin_gcs_message::CT_RECOVERY_MESSAGE:
    handle_recovery_message(message);
    break;

  case Plugin_gcs_message::CT_PIPELINE_STATS_MEMBER_MESSAGE:
    handle_stats_message(message);
    break;

  case Plugin_gcs_message::CT_SINGLE_PRIMARY_MESSAGE:
    handle_single_primary_message(message);
    break;

  default:
    break; /* purecov: inspected */
  }

  /*
   We need to see if a notification should be sent at this
   point in time because we may have received a recovery
   message that has updated our state.
  */
  notify_and_reset_ctx(m_notification_ctx);
}

void
Plugin_gcs_events_handler::
handle_transactional_message(const Gcs_message& message) const
{
  if ( (local_member_info->get_recovery_status() == Group_member_info::MEMBER_IN_RECOVERY ||
        local_member_info->get_recovery_status() == Group_member_info::MEMBER_ONLINE) &&
        this->applier_module)
  {
    const unsigned char* payload_data= NULL;
    size_t payload_size= 0;
    Plugin_gcs_message::get_first_payload_item_raw_data(
        message.get_message_data().get_payload(),
        &payload_data, &payload_size);

    this->applier_module->handle(payload_data, static_cast<ulong>(payload_size));
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Message received while the plugin is not ready,"
                " message discarded"); /* purecov: inspected */
  }
}

void
Plugin_gcs_events_handler::handle_certifier_message(const Gcs_message& message) const
{
  if (this->applier_module == NULL)
  {
    log_message(MY_ERROR_LEVEL,
                "Message received without a proper group replication applier"); /* purecov: inspected */
    return; /* purecov: inspected */
  }

  Certifier_interface *certifier=
      this->applier_module->get_certification_handler()->get_certifier();

  const unsigned char* payload_data= NULL;
  size_t payload_size= 0;
  Plugin_gcs_message::get_first_payload_item_raw_data(
      message.get_message_data().get_payload(),
      &payload_data, &payload_size);

  if (certifier->handle_certifier_data(payload_data,
                                       static_cast<ulong>(payload_size),
                                       message.get_origin()))
  {
    log_message(MY_ERROR_LEVEL, "Error processing message in Certifier"); /* purecov: inspected */
  }
}

void
Plugin_gcs_events_handler::handle_recovery_message(const Gcs_message& message) const
{
  Recovery_message recovery_message(message.get_message_data().get_payload(),
                                    message.get_message_data().get_payload_length());

  std::string member_uuid= recovery_message.get_member_uuid();

  bool is_local= !member_uuid.compare(local_member_info->get_uuid());
  if(is_local)
  {
    // Only change member status if member is still on recovery.
    Group_member_info::Group_member_status member_status=
        local_member_info->get_recovery_status();
    if (member_status != Group_member_info::MEMBER_IN_RECOVERY)
    {
      log_message(MY_INFORMATION_LEVEL,
                  "This server was not declared online since it is on status %s",
                  Group_member_info::get_member_status_string(member_status)); /* purecov: inspected */
      return; /* purecov: inspected */
    }

    log_message(MY_INFORMATION_LEVEL,
                "This server was declared online within the replication group");

    /*
     The member is declared as online upon receiving this message

     A notification may be flagged and eventually triggered when
     the on_message handle is finished.
    */
    group_member_mgr->update_member_status(
      member_uuid,
      Group_member_info::MEMBER_ONLINE,
      m_notification_ctx);

    /**
      Disable the read mode in the server if the member is:
      - joining
      - doesn't have a higher possible incompatible version
      - We are not on Primary mode.
    */
    if (*joiner_compatibility_status != READ_COMPATIBLE &&
        (local_member_info->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY ||
         !local_member_info->in_primary_mode()))
    {
      if (disable_server_read_mode(PSESSION_DEDICATED_THREAD))
      {
        log_message(MY_WARNING_LEVEL,
                    "When declaring the plugin online it was not possible to "
                      "disable the server read mode settings. "
                      "Try to disable it manually."); /* purecov: inspected */
      }
    }

  }
  else
  {
    Group_member_info* member_info= group_member_mgr->get_group_member_info(member_uuid);
    if (member_info != NULL)
    {
      log_message(MY_INFORMATION_LEVEL,
                  "The member with address %s:%u was declared online within the "
                  "replication group",
                  member_info->get_hostname().c_str(), member_info->get_port());
      delete member_info;

      /*
       The member is declared as online upon receiving this message
       We need to run this before running update_recovery_process

       A notification may be flagged and eventually triggered when
       the on_message handle is finished.
      */
      group_member_mgr->update_member_status(
        member_uuid,
        Group_member_info::MEMBER_ONLINE,
        m_notification_ctx);

      if (local_member_info->get_recovery_status() ==
              Group_member_info::MEMBER_IN_RECOVERY)
      {
        /*
          Inform recovery of a possible new donor
        */
        recovery_module->update_recovery_process(false, false);
      }
    }
  }

  /*
   Check if we were waiting for some server to recover to
   elect a new leader.

   Following line protects against servers joining the group
   while the bootstrapped node has not yet finished recovery.
   Therefore, it is going to become primary when it finishes recovery.
   */
  this->handle_leader_election_if_needed();
}

void
Plugin_gcs_events_handler::handle_stats_message(const Gcs_message& message) const
{
  if (this->applier_module == NULL)
  {
    log_message(MY_ERROR_LEVEL,
                "Message received without a proper group replication applier"); /* purecov: inspected */
    return; /* purecov: inspected */
  }

  this->applier_module->get_flow_control_module()
      ->handle_stats_data(message.get_message_data().get_payload(),
                          message.get_message_data().get_payload_length(),
                          message.get_origin().get_member_id());
}

void
Plugin_gcs_events_handler::handle_single_primary_message(const Gcs_message& message) const
{
  if (this->applier_module == NULL)
  {
    log_message(MY_ERROR_LEVEL,
                "Message received without a proper group replication applier"); /* purecov: inspected */
    return; /* purecov: inspected */
  }

  Single_primary_message
      single_primary_message(message.get_message_data().get_payload(),
                             message.get_message_data().get_payload_length());

  if (single_primary_message.get_single_primary_message_type() ==
      Single_primary_message::SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE)
  {
    Single_primary_action_packet *single_primary_action=
        new Single_primary_action_packet(Single_primary_action_packet::QUEUE_APPLIED);
    this->applier_module->add_single_primary_action_packet(single_primary_action);
  }
}

void
Plugin_gcs_events_handler::on_suspicions(const std::vector<Gcs_member_identifier>& members,
                                         const std::vector<Gcs_member_identifier>& unreachable) const
{
  if (members.empty() && unreachable.empty()) // nothing to do
    return; /* purecov: inspected */

  DBUG_ASSERT(members.size() >= unreachable.size());

  std::vector<Gcs_member_identifier> tmp_unreachable(unreachable);
  std::vector<Gcs_member_identifier>::const_iterator mit;
  std::vector<Gcs_member_identifier>::iterator uit;

  if (!members.empty())
  {
    for (mit= members.begin(); mit != members.end(); mit ++)
    {
      Gcs_member_identifier member= *mit;
      Group_member_info* member_info=
        group_member_mgr->get_group_member_info_by_member_id(member);

      if (member_info == NULL) //Trying to update a non-existing member
        continue; /* purecov: inspected */

      uit= std::find(tmp_unreachable.begin(), tmp_unreachable.end(), member);
      if (uit != tmp_unreachable.end())
      {
        if (!member_info->is_unreachable())
        {
          log_message(MY_WARNING_LEVEL,
                      "Member with address %s:%u has become unreachable.",
                      member_info->get_hostname().c_str(), member_info->get_port());
          // flag as a member having changed state
          m_notification_ctx.set_member_state_changed();
          member_info->set_unreachable();
        }
        // remove to not check again against this one
        tmp_unreachable.erase(uit);
      }
      else
      {
        if (member_info->is_unreachable())
        {
          log_message(MY_WARNING_LEVEL,
                      "Member with address %s:%u is reachable again.",
                      member_info->get_hostname().c_str(), member_info->get_port());
          /* purecov: begin inspected */
          // flag as a member having changed state
          m_notification_ctx.set_member_state_changed();
          member_info->set_reachable();
          /* purecov: end */
        }
      }
    }
  }

  if ((members.size() - unreachable.size()) <= (members.size() / 2))
  {
    if (!group_partition_handler->get_timeout_on_unreachable())
      log_message(MY_ERROR_LEVEL,
                  "This server is not able to reach a majority of members "
                  "in the group. This server will now block all updates. "
                  "The server will remain blocked until contact with the "
                  "majority is restored. "
                  "It is possible to use group_replication_force_members "
                  "to force a new group membership.");
    else
      log_message(MY_ERROR_LEVEL,
                  "This server is not able to reach a majority of members "
                  "in the group. This server will now block all updates. "
                  "The server will remain blocked for the next %lu seconds. "
                  "Unless contact with the majority is restored, after this "
                  "time the member will error out and leave the group. "
                  "It is possible to use group_replication_force_members "
                  "to force a new group membership.",
                  group_partition_handler->get_timeout_on_unreachable());

    if (!group_partition_handler->is_partition_handler_running() &&
        !group_partition_handler->is_partition_handling_terminated())
      group_partition_handler->launch_partition_handler_thread();

    // flag as having lost quorum
    m_notification_ctx.set_quorum_lost();
  }
  else
  {
    /*
      This code is present on on_view_changed and on_suspicions as no assumption
      can be made about the order in which these methods are invoked.
    */
    if (group_partition_handler->is_member_on_partition())
    {
      if (group_partition_handler->abort_partition_handler_if_running())
      {
        log_message(MY_WARNING_LEVEL,
                    "A group membership change was received but the plugin is "
                    "already leaving due to the configured timeout on "
                    "group_replication_unreachable_majority_timeout option.");
      }
      else
      {
        /* If it was not running or we canceled it in time */
        log_message(MY_WARNING_LEVEL,
                    "The member has resumed contact with a majority of the "
                    "members in the group. Regular operation is restored and "
                    "transactions are unblocked.");
      }
    }
  }
  notify_and_reset_ctx(m_notification_ctx);
}

void
Plugin_gcs_events_handler::log_members_leaving_message(const Gcs_view& new_view) const
{
  std::string members_leaving;
  std::string primary_member_host;

  get_hosts_from_view(new_view.get_leaving_members(), members_leaving, primary_member_host);

  log_message(MY_WARNING_LEVEL,
              "Members removed from the group: %s",
              members_leaving.c_str());

  if (!primary_member_host.empty())
    log_message(MY_INFORMATION_LEVEL,
                "Primary server with address %s left the group. "
                "Electing new Primary.",
                primary_member_host.c_str());
}

void
Plugin_gcs_events_handler::log_members_joining_message(const Gcs_view& new_view) const
{
  std::string members_joining;
  std::string primary_member_host;

  get_hosts_from_view(new_view.get_joined_members(), members_joining, primary_member_host);

  log_message(MY_INFORMATION_LEVEL,
              "Members joined the group: %s",
              members_joining.c_str());
}

void
Plugin_gcs_events_handler::get_hosts_from_view(const std::vector<Gcs_member_identifier> &members,
                         std::string& all_hosts, std::string& primary_host) const
{
  std::stringstream hosts_string;
  std::stringstream primary_string;
  std::vector<Gcs_member_identifier>::const_iterator all_members_it= members.begin();

  while (all_members_it != members.end())
  {
    Group_member_info* member_info= group_member_mgr->
                                     get_group_member_info_by_member_id((*all_members_it));
    all_members_it++;

    if (member_info == NULL)
      continue;

    hosts_string << member_info->get_hostname() << ":" << member_info->get_port();

    /**
     Check in_primary_mode has been added for safety.
     Since primary role is in single-primary mode.
    */
    if (member_info->in_primary_mode() &&
        member_info->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY)
    {
      if (primary_string.rdbuf()->in_avail() != 0)
        primary_string << ", ";
      primary_string << member_info->get_hostname() << ":" << member_info->get_port();
    }

    if (all_members_it != members.end())
    {
      hosts_string << ", ";
    }
  }
  all_hosts.assign (hosts_string.str());
  primary_host.assign (primary_string.str());
}

void
Plugin_gcs_events_handler::on_view_changed(const Gcs_view& new_view,
                                           const Exchanged_data &exchanged_data)
                                           const
{
  bool is_leaving= is_member_on_vector(new_view.get_leaving_members(),
                                       local_member_info->get_gcs_member_id());

  bool is_primary= (local_member_info->in_primary_mode() &&
                    local_member_info->get_role() ==
                      Group_member_info::MEMBER_ROLE_PRIMARY);

  bool is_joining= is_member_on_vector(new_view.get_joined_members(),
                                       local_member_info->get_gcs_member_id());

  // Was member expelled from the group due to network failures?
  if (this->was_member_expelled_from_group(new_view))
  {
    DBUG_ASSERT(is_leaving);
    goto end;
  }

  //An early error on the applier can render the join invalid
  if (is_joining &&
      local_member_info->get_recovery_status() == Group_member_info::MEMBER_ERROR)
  {
    log_message(MY_ERROR_LEVEL,
                "There was a previous plugin error while the member joined the group. "
                "The member will now exit the group.");
    view_change_notifier->cancel_view_modification(GROUP_REPLICATION_CONFIGURATION_ERROR);
  }
  else
  {
    /*
      This code is present on on_view_changed and on_suspicions as no assumption
      can be made about the order in which these methods are invoked.
    */
    if (!is_leaving && group_partition_handler->is_member_on_partition())
    {
      if (group_partition_handler->abort_partition_handler_if_running())
      {
        log_message(MY_WARNING_LEVEL,
                    "A group membership change was received but the plugin is "
                    "already leaving due to the configured timeout on "
                    "group_replication_unreachable_majority_timeout option.");
        goto end;
      }
      else
      {
        /* If it was not running or we canceled it in time */
        log_message(MY_WARNING_LEVEL,
                    "The member has resumed contact with a majority of the "
                    "members in the group. Regular operation is restored and "
                    "transactions are unblocked.");
      }
    }

    /*
      Maybe on_suspicions we already executed the above block but it was too late.
      No point in repeating the message, but we need to break the view install.
    */
    if (!is_leaving &&
        group_partition_handler->is_partition_handling_terminated())
      goto end;

    if (!is_leaving && new_view.get_leaving_members().size() > 0)
      log_members_leaving_message(new_view);

    //update the Group Manager with all the received states
    if (update_group_info_manager(new_view, exchanged_data, is_joining, is_leaving) &&
        is_joining)
    {
      view_change_notifier->cancel_view_modification();
      return;
    }

    if (!is_joining && new_view.get_joined_members().size() > 0)
      log_members_joining_message(new_view);

    //enable conflict detection if someone on group have it enabled
    if (local_member_info->in_primary_mode() &&
        group_member_mgr->is_conflict_detection_enabled())
    {
      Certifier_interface *certifier=
          this->applier_module->get_certification_handler()->get_certifier();
      certifier->enable_conflict_detection();
    }

    //Inform any interested handler that the view changed
    View_change_pipeline_action *vc_action=
      new View_change_pipeline_action(is_leaving);

    applier_module->handle_pipeline_action(vc_action);
    delete vc_action;

    //Update any recovery running process and handle state changes
    this->handle_leaving_members(new_view, is_joining, is_leaving);

    //Handle joining members
    this->handle_joining_members(new_view, is_joining, is_leaving);

    if (is_leaving)
      gcs_module->leave_coordination_member_left();

    // Handle leader election if needed
    this->handle_leader_election_if_needed();

    //Signal that the injected view was delivered
    if (view_change_notifier->is_injected_view_modification())
      view_change_notifier->end_view_modification();
  }

  if (!is_leaving)
  {
    std::string view_id_representation= "";
    Gcs_view *view= gcs_module->get_current_view();
    if (view != NULL)
    {
      view_id_representation= view->get_view_id().get_representation();
      delete view;
    }

    log_message(MY_INFORMATION_LEVEL,
                "Group membership changed to %s on view %s.",
                group_member_mgr->get_string_current_view_active_hosts().c_str(),
                view_id_representation.c_str());
  }
  else
  {
    log_message(MY_INFORMATION_LEVEL,
                "Group membership changed: This member has left the group.");
  }

end:
  /* if I am the primary and I am leaving, notify about role change */
  if (is_leaving && is_primary)
  {
    group_member_mgr->update_member_role(
      local_member_info->get_uuid(),
      Group_member_info::MEMBER_ROLE_SECONDARY,
      m_notification_ctx);
  }

  /* flag view change */
  m_notification_ctx.set_view_changed();
  if (is_leaving)
    /*
      The leave view is an optimistic and local view.
      Therefore its ID is not meaningful, since it is not
      a global one.
     */
    m_notification_ctx.set_view_id("");
  else
    m_notification_ctx.set_view_id(new_view.get_view_id().get_representation());

  /* trigger notification */
  notify_and_reset_ctx(m_notification_ctx);
}

bool
Plugin_gcs_events_handler::was_member_expelled_from_group(const Gcs_view& view) const
{
  DBUG_ENTER("Plugin_gcs_events_handler::was_member_expelled_from_group");
  bool result= false;

  if (view.get_error_code() == Gcs_view::MEMBER_EXPELLED)
  {
    result= true;
    log_message(MY_ERROR_LEVEL,
                "Member was expelled from the group due to network failures, "
                "changing member status to ERROR.");
    /*
      Delete all members from group info except the local one.

      Regarding the notifications, these are not triggered here, but
      rather at the end of the handle function that calls this one:
      on_view_changed.
     */
    std::vector<Group_member_info*> to_update;
    group_member_mgr->update(&to_update);
    group_member_mgr->update_member_status(
      local_member_info->get_uuid(),
      Group_member_info::MEMBER_ERROR,
      m_notification_ctx);

    group_member_mgr->update_member_role(
      local_member_info->get_uuid(),
      Group_member_info::MEMBER_ROLE_SECONDARY,
      m_notification_ctx);

    bool aborted= false;
    applier_module->add_suspension_packet();
    int error= applier_module->wait_for_applier_complete_suspension(&aborted, false);
    /*
      We do not need to kill ongoing transactions when the applier
      is already stopping.
    */
    if (!error)
      applier_module->kill_pending_transactions(true, true);
  }

  DBUG_RETURN(result);
}

std::vector<Group_member_info*>::iterator
Plugin_gcs_events_handler::sort_and_get_lowest_version_member_position(
  std::vector<Group_member_info*>* all_members_info) const
{
  std::vector<Group_member_info*>::iterator it;

  // sort in ascending order of lower member version
  std::sort(all_members_info->begin(), all_members_info->end(),
            Group_member_info::comparator_group_member_version);

  /* if vector contains only single version then leader should be picked from
     all members
   */
  std::vector<Group_member_info*>::iterator lowest_version_end=
    all_members_info->end();

  /* first member will have lowest version as members are already
     sorted above using member_version.
   */
  it= all_members_info->begin();
  Group_member_info* first_member= *it;
  uint32 lowest_major_version=
    first_member->get_member_version().get_major_version();

  /* to avoid read compatibility issue leader should be picked only from lowest
     version members so save position where member version differs.

     set lowest_version_end when major version changes

     eg: for a list: 5.7.18, 5.7.18, 5.7.19, 5.7.20, 5.7.21, 8.0.2
         the members to be considered for election will be:
            5.7.18, 5.7.18, 5.7.19, 5.7.20, 5.7.21
         and server_uuid based algorithm will be used to elect primary

     eg: for a list: 5.7.20, 5.7.21, 8.0.2, 8.0.2
         the members to be considered for election will be:
            5.7.20, 5.7.21
         and member weight based algorithm will be used to elect primary
  */
  for(it= all_members_info->begin() + 1; it != all_members_info->end(); it++)
  {
    if (lowest_major_version != (*it)->get_member_version().get_major_version())
    {
      lowest_version_end= it;
      break;
    }
  }

  return lowest_version_end;
}

void Plugin_gcs_events_handler::sort_members_for_election(
       std::vector<Group_member_info*>* all_members_info,
       std::vector<Group_member_info*>::iterator lowest_version_end) const
{
  Group_member_info* first_member= *(all_members_info->begin());
  Member_version lowest_version= first_member->get_member_version();

  // sort only lower version members as they only will be needed to pick leader
  if (lowest_version >= PRIMARY_ELECTION_MEMBER_WEIGHT_VERSION)
    std::sort(all_members_info->begin(), lowest_version_end,
              Group_member_info::comparator_group_member_weight);
  else
    std::sort(all_members_info->begin(), lowest_version_end,
              Group_member_info::comparator_group_member_uuid);
}

void Plugin_gcs_events_handler::handle_leader_election_if_needed() const
{
  // take action if in single leader mode
  if (!local_member_info->in_primary_mode())
    return;

  bool am_i_leaving= true;
#ifndef DBUG_OFF
  int n=0;
#endif
  Group_member_info* the_primary= NULL;
  std::vector<Group_member_info*>* all_members_info=
    group_member_mgr->get_all_members();

  std::vector<Group_member_info*>::iterator it;
  std::vector<Group_member_info*>::iterator lowest_version_end;

  /* sort members based on member_version and get first iterator position
     where member version differs
   */
  lowest_version_end=
    sort_and_get_lowest_version_member_position(all_members_info);

  /*  Sort lower version members based on member weight if member version
      is greater than equal to PRIMARY_ELECTION_MEMBER_WEIGHT_VERSION or uuid.
   */
  sort_members_for_election(all_members_info, lowest_version_end);

  /*
   1. Iterate over the list of all members and check if there is a primary
      defined already.
   2. Check if I am leaving the group or not.
   */
  for(it= all_members_info->begin(); it != all_members_info->end(); it++)
  {
#ifndef DBUG_OFF
    DBUG_ASSERT(!(n > 1));
#endif

    Group_member_info* member= *it;
    if (the_primary == NULL &&
        member->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY)
    {
      the_primary= member;
#ifndef DBUG_OFF
      n++;
#endif
    }

    /* Found the primary and it is me. Check that I am not offline. */
    if (!member->get_uuid().compare(local_member_info->get_uuid()))
    {
      am_i_leaving= member->get_recovery_status() == Group_member_info::MEMBER_OFFLINE;
    }
  }

  /* If I am not leaving, then run election. Otherwise do nothing. */
  if (!am_i_leaving)
  {
    Sql_service_command_interface *sql_command_interface=
        new Sql_service_command_interface();
    bool skip_set_super_readonly= false;
    if (sql_command_interface == NULL ||
        sql_command_interface->
            establish_session_connection(PSESSION_DEDICATED_THREAD,
                                         GROUPREPL_USER,
                                         get_plugin_pointer()))
    {
      log_message(MY_WARNING_LEVEL,
                  "Unable to open session to (re)set read only mode. Skipping."); /* purecov: inspected */
      /*
       Unable to open session to (re)set read only mode.
       Mark that we should skipping that part code.
       */
      skip_set_super_readonly= true; /* purecov: inspected */
    }


    /*
     There is no primary in the member list. Pick one from
     the list of ONLINE members. The picked one is the first
     viable on in the list that was sorted at the beginning
     of this function.

     The assumption is that std::sort(...) is deterministic
     on all members.

     To pick leaders from only lowest version members loop
     till lowest_version_end.
    */
    if (the_primary == NULL)
    {
      for (it= all_members_info->begin();
           it != lowest_version_end && the_primary == NULL;
           it++)
      {
        Group_member_info* mi= *it;

        DBUG_ASSERT(mi);
        if (mi &&
            mi->get_recovery_status() == Group_member_info::MEMBER_ONLINE)
          the_primary= mi;
      }
    }

    // take actions on the primary
    if (the_primary != NULL)
    {
      std::string primary_uuid= the_primary->get_uuid();
      const bool is_primary_local= !primary_uuid.compare(local_member_info->get_uuid());
      const bool has_primary_changed=
          Group_member_info::MEMBER_ROLE_PRIMARY != the_primary->get_role();

      if (has_primary_changed)
      {
        /*
          A new primary was elected, inform certifier to enable conflict
          detection until the new primary apply all relay logs.
        */
        Single_primary_action_packet *single_primary_action=
            new Single_primary_action_packet(Single_primary_action_packet::NEW_PRIMARY);
        applier_module->add_single_primary_action_packet(single_primary_action);

        // declare this as the new primary
        group_member_mgr->update_member_role(
          primary_uuid,
          Group_member_info::MEMBER_ROLE_PRIMARY,
          m_notification_ctx);

        log_message(MY_INFORMATION_LEVEL, "A new primary with address %s:%u "
                    "was elected, enabling conflict detection until the new "
                    "primary applies all relay logs.",
                    the_primary->get_hostname().c_str(),
                    the_primary->get_port());

        // Check if the session was established, it can (re)set read only mode.
        if (!skip_set_super_readonly)
        {
          if (is_primary_local)
          {
            if (disable_super_read_only_mode(sql_command_interface))
            {
              log_message(MY_WARNING_LEVEL,
                          "Unable to disable super read only flag. "
                          "Try to disable it manually."); /* purecov: inspected */
            }
          }
          else
          {
            if (enable_super_read_only_mode(sql_command_interface))
            {
              log_message(MY_WARNING_LEVEL,
                          "Unable to set super read only flag. "
                          "Try to set it manually."); /* purecov: inspected */
            }
          }
        }
        /* code position limits messaging to primary change */
        if (is_primary_local)
          log_message(MY_INFORMATION_LEVEL,
                      "This server is working as primary member.");
        else
          log_message(MY_INFORMATION_LEVEL,
                      "This server is working as secondary member with primary "
                      "member address %s:%u.",
                      the_primary->get_hostname().c_str(),
                      the_primary->get_port());
      }
    }
    else if (!skip_set_super_readonly)
    {
      /*
       If there is only one server in the group, no need to pollute the error log with
       an entry about no suitable candidate while (quick) recovery is running for the first member.
      */
      if (all_members_info->size() != 1)
      {
        // There are no servers in the group or they are all
        // recoverying WARN to the user
        log_message(MY_WARNING_LEVEL,
                    "Unable to set any member as primary. No suitable candidate."); /* purecov: inspected */
      }

      if(enable_super_read_only_mode(sql_command_interface))
      {
        log_message(MY_WARNING_LEVEL,
                    "Unable to set super read only flag. "
                    "Try to set it manually."); /* purecov: inspected */
      }
    }
    delete sql_command_interface;
  }

  //clean the members
  for (it= all_members_info->begin(); it!= all_members_info->end(); it++)
  {
    delete (*it);
  }
  delete all_members_info;
}

int Plugin_gcs_events_handler::
update_group_info_manager(const Gcs_view& new_view,
                          const Exchanged_data &exchanged_data,
                          bool is_joining,
                          bool is_leaving) const
{
  int error= 0;

  //update the Group Manager with all the received states
  vector<Group_member_info*> to_update;

  if(!is_leaving)
  {
    //Process local state of exchanged data.
    if ((error= process_local_exchanged_data(exchanged_data, is_joining)))
      goto err;

    to_update.insert(to_update.end(),
                     temporary_states->begin(),
                     temporary_states->end());

    //Clean-up members that are leaving
    vector<Gcs_member_identifier> leaving= new_view.get_leaving_members();
    vector<Gcs_member_identifier>::iterator left_it;
    vector<Group_member_info*>::iterator to_update_it;
    for(left_it= leaving.begin(); left_it != leaving.end(); left_it++)
    {
      for(to_update_it= to_update.begin();
          to_update_it != to_update.end();
          to_update_it++)
      {
        if( (*left_it) == (*to_update_it)->get_gcs_member_id() )
        {
          /* purecov: begin inspected */
          delete (*to_update_it);
          to_update.erase(to_update_it);
          break;
          /* purecov: end */
        }
      }
    }
  }
  group_member_mgr->update(&to_update);
  temporary_states->clear();

err:
  DBUG_ASSERT(temporary_states->size() == 0);
  return error;
}

void Plugin_gcs_events_handler::handle_joining_members(const Gcs_view& new_view,
                                                       bool is_joining,
                                                       bool is_leaving)
                                                       const
{
  //nothing to do here
  size_t number_of_members= new_view.get_members().size();
  if (number_of_members == 0 || is_leaving)
  {
    return;
  }
  size_t number_of_joining_members= new_view.get_joined_members().size();
  size_t number_of_leaving_members= new_view.get_leaving_members().size();

  /*
   If we are joining, 3 scenarios exist:
   1) We are incompatible with the group so we leave
   2) We are alone so we declare ourselves online
   3) We are in a group and recovery must happen
  */
  if (is_joining)
  {
    int error= 0;
    if ((error= check_group_compatibility(number_of_members)))
    {
      view_change_notifier->cancel_view_modification(error);
      return;
    }
    view_change_notifier->end_view_modification();

    /**
     On the joining list there can be 2 types of members: online/recovering
     members coming from old views where this member was not present and new
     joining members that still have their status as offline.

     As so, for offline members, their state is changed to member_in_recovery
     after member compatibility with group is checked.
    */
    update_member_status(new_view.get_joined_members(),
                         Group_member_info::MEMBER_IN_RECOVERY,
                         Group_member_info::MEMBER_OFFLINE,
                         Group_member_info::MEMBER_END);
    /**
      Set the read mode if not set during start (auto-start)
    */
    if (enable_server_read_mode(PSESSION_DEDICATED_THREAD))
    {
      log_message(MY_ERROR_LEVEL,
                  "Error when activating super_read_only mode on start. "
                  "The member will now exit the group.");

      /*
        The notification will be triggered in the top level handle function
        that calls this one. In this case, the on_view_changed handle.
      */
      group_member_mgr->update_member_status(
        local_member_info->get_uuid(),
        Group_member_info::MEMBER_ERROR,
        m_notification_ctx);
      this->leave_group_on_error();
      return;
    }

    /**
      On the joining member log an error when group contains more members than
      auto_increment_increment variable.
    */
    ulong auto_increment_increment= get_auto_increment_increment();

    if (!local_member_info->in_primary_mode() &&
        new_view.get_members().size() > auto_increment_increment)
    {
      log_message(MY_ERROR_LEVEL,
                  "Group contains %lu members which is greater than"
                  " group_replication_auto_increment_increment value of %lu."
                  " This can lead to an higher rate of transactional aborts.",
                  new_view.get_members().size(), auto_increment_increment);
    }

    /*
     During the view change, a suspension packet is sent to the applier module
     so all posterior transactions inbound are not applied, but queued, until
     the member finishes recovery.
    */
    applier_module->add_suspension_packet();

    /*
     Marking the view in the joiner since the incoming event from the donor
     is discarded in the Recovery process.
     */

    std::string view_id= new_view.get_view_id().get_representation();
    View_change_packet * view_change_packet= new View_change_packet(view_id);
    applier_module->add_view_change_packet(view_change_packet);

    /*
     Launch the recovery thread so we can receive missing data and the
     certification information needed to apply the transactions queued after
     this view change.

     Recovery receives a view id, as a means to identify logically on joiners
     and donors alike where this view change happened in the data. With that
     info we can then ask for the donor to give the member all the data until
     this point in the data, and the certification information for all the data
     that comes next.

     When alone, the server will go through Recovery to wait for the consumption
     of his applier relay log that may contain transactions from previous
     executions.
    */
    recovery_module->start_recovery(new_view.get_group_id().get_group_id(),
                                    new_view.get_view_id()
                                                      .get_representation());
  }
  /*
    The condition
      number_of_joining_members == 0 && number_of_leaving_members == 0
    is needed due to the following scenario:
    We have a group with 2 members, one does crash (M2), and the group
    blocks with M1 ONLINE and M2 UNREACHABLE.
    Then M2 rejoins and the group unblocks.
    When M2 rejoins the group, from M2 perspective it is joining
    the group, that is, it does receive a view (V3) on which it is
    marked as a joining member.
    But from M1 perspective, M2 may never left, so the view delivered
    (V3) has the same members as V2, that is, M1 and M2, without joining
    members, thence we need to consider that condition and log that view.
  */
  else if (number_of_joining_members > 0 ||
           (number_of_joining_members == 0 && number_of_leaving_members == 0))
  {
    /**
     On the joining list there can be 2 types of members: online/recovering
     members coming from old views where this member was not present and new
     joining members that still have their status as offline.

     As so, for offline members, their state is changed to member_in_recovery.
    */
    update_member_status(new_view.get_joined_members(),
                         Group_member_info::MEMBER_IN_RECOVERY,
                         Group_member_info::MEMBER_OFFLINE,
                         Group_member_info::MEMBER_END);
    /**
     If not a joining member, all members should record on their own binlogs a
     marking event that identifies the frontier between the data the joining
     member was to receive and the data it should queue.
     The joining member can then wait for this event to know it was all the
     needed data.

     This packet will also pass in the certification process at this exact
     frontier giving us the opportunity to gather the necessary certification
     information to certify the transactions that will come after this view
     change. If selected as a donor, this info will also be sent to the joiner.

     Associated to this process, we collect and intersect the executed GTID sets
     of all ONLINE members so we can cut the certification info to gather and
     transmit to the minimum.
    */

    std::string view_id= new_view.get_view_id().get_representation();
    View_change_packet * view_change_packet= new View_change_packet(view_id);
    collect_members_executed_sets(view_change_packet);
    applier_module->add_view_change_packet(view_change_packet);
  }
}

void
Plugin_gcs_events_handler::handle_leaving_members(const Gcs_view& new_view,
                                                  bool is_joining,
                                                  bool is_leaving)
                                                  const
{
  Group_member_info::Group_member_status member_status=
      local_member_info->get_recovery_status();

  bool members_left= (new_view.get_leaving_members().size() > 0);

  //if the member is joining or not in recovery, no need to update the process
  if (!is_joining && member_status == Group_member_info::MEMBER_IN_RECOVERY)
  {
    /*
     This method has 2 purposes:
     If a donor leaves, recovery needs to switch donor
     If this member leaves, recovery needs to shutdown.
    */
    recovery_module->update_recovery_process(members_left, is_leaving);
  }

  if (members_left)
  {
    update_member_status(new_view.get_leaving_members(),
                         Group_member_info::MEMBER_OFFLINE,
                         Group_member_info::MEMBER_END,
                         Group_member_info::MEMBER_ERROR);
  }

  if (is_leaving)
  {
    view_change_notifier->end_view_modification();
  }
}

bool
Plugin_gcs_events_handler::
is_member_on_vector(const vector<Gcs_member_identifier>& members,
                    const Gcs_member_identifier& member_id)
                    const
{
  vector<Gcs_member_identifier>::const_iterator it;

  it= std::find(members.begin(), members.end(), member_id);

  return it != members.end();
}

int
Plugin_gcs_events_handler::
process_local_exchanged_data(const Exchanged_data &exchanged_data,
                             bool is_joining)
                             const
{
  uint local_uuid_found= 0;

  /*
  For now, we are only carrying Group Member Info on Exchangeable data
  Since we are receiving the state from all Group members, one shall
  store it in a set to ensure that we don't have repetitions.

  All collected data will be given to Group Member Manager at view install
  time.
  */
  for (Exchanged_data::const_iterator exchanged_data_it= exchanged_data.begin();
       exchanged_data_it != exchanged_data.end();
       exchanged_data_it++)
  {
    const uchar* data= exchanged_data_it->second->get_payload();
    size_t length= exchanged_data_it->second->get_payload_length();
    Gcs_member_identifier* member_id= exchanged_data_it->first;
    if (data == NULL)
    {
      /* purecov: begin inspected */
      Group_member_info * member_info= group_member_mgr->get_group_member_info_by_member_id(*member_id);
      if (member_info != NULL)
      {
        log_message(MY_ERROR_LEVEL, "Member with address '%s:%u' didn't provide any data"
                                    " during the last group change. Group"
                                    " information can be outdated and lead to"
                                    " errors on recovery",
                                    member_info->get_hostname().c_str(), member_info->get_port());
      }
      continue;
      /* purecov: end */
    }

    //Process data provided by member.
    vector<Group_member_info*>* member_infos=
        group_member_mgr->decode(data, length);

    //This construct is here in order to deallocate memory of duplicates
    vector<Group_member_info*>::iterator member_infos_it;
    for(member_infos_it= member_infos->begin();
        member_infos_it != member_infos->end();
        member_infos_it++)
    {
      if (local_member_info->get_uuid() == (*member_infos_it)->get_uuid())
      {
        local_uuid_found++;
      }

      /*
        Accept only the information the member has about himself
        Information received about other members is probably outdated
      */
      if (local_uuid_found < 2 &&
          (*member_infos_it)->get_gcs_member_id() == *member_id)
      {
        this->temporary_states->insert((*member_infos_it));
      }
      else
      {
        delete (*member_infos_it); /* purecov: inspected */
      }
    }

    member_infos->clear();
    delete member_infos;

    if (local_uuid_found > 1)
    {
      if (is_joining)
      {
        log_message(MY_ERROR_LEVEL,
                    "There is already a member with server_uuid %s. "
                    "The member will now exit the group.",
                    local_member_info->get_uuid().c_str());
      }

      // Clean up temporary states.
      std::set<Group_member_info*,Group_member_info_pointer_comparator>::iterator
          temporary_states_it;
      for (temporary_states_it= temporary_states->begin();
           temporary_states_it != temporary_states->end();
           temporary_states_it++)
      {
        delete (*temporary_states_it);
      }
      temporary_states->clear();

      return 1;
    }
  }

  return 0;
}

Gcs_message_data*
Plugin_gcs_events_handler::get_exchangeable_data() const
{
  std::string server_executed_gtids;
  std::string applier_retrieved_gtids;
  Replication_thread_api applier_channel("group_replication_applier");

  Sql_service_command_interface *sql_command_interface=
      new Sql_service_command_interface();

  if (sql_command_interface->
          establish_session_connection(PSESSION_DEDICATED_THREAD,
                                       GROUPREPL_USER,
                                       get_plugin_pointer())
     )
  {
    log_message(MY_WARNING_LEVEL,
                "Error when extracting information for group change. "
                "Operations and checks made to group joiners may be incomplete"); /* purecov: inspected */
    goto sending; /* purecov: inspected */
  }

  if (sql_command_interface->get_server_gtid_executed(server_executed_gtids))
  {
    log_message(MY_WARNING_LEVEL,
                "Error when extracting this member GTID executed set. "
                "Operations and checks made to group joiners may be incomplete"); /* purecov: inspected */
    goto sending; /* purecov: inspected */
  }
  if (applier_channel.get_retrieved_gtid_set(applier_retrieved_gtids))
  {
    log_message(MY_WARNING_LEVEL,
                "Error when extracting this member retrieved set for its applier. "
                "Operations and checks made to group joiners may be incomplete"); /* purecov: inspected */
  }

  group_member_mgr->update_gtid_sets(local_member_info->get_uuid(),
                                     server_executed_gtids,
                                     applier_retrieved_gtids);
sending:

  delete sql_command_interface;

  std::vector<uchar> data;

  Group_member_info* local_member_copy= new Group_member_info(*local_member_info);
  Group_member_info_manager_message *group_info_message=
    new Group_member_info_manager_message(local_member_copy);
  group_info_message->encode(&data);
  delete group_info_message;

  Gcs_message_data* msg_data= new Gcs_message_data(0, data.size());
  msg_data->append_to_payload(&data.front(), data.size());

  return msg_data;
}

void
Plugin_gcs_events_handler::
update_member_status(const vector<Gcs_member_identifier>& members,
                     Group_member_info::Group_member_status status,
                     Group_member_info::Group_member_status old_status_equal_to,
                     Group_member_info::Group_member_status old_status_different_from)
                     const
{
  for (vector<Gcs_member_identifier>::const_iterator it= members.begin();
       it != members.end();
       ++it)
  {
    Gcs_member_identifier member = *it;
    Group_member_info* member_info=
        group_member_mgr->get_group_member_info_by_member_id(member);

    if (member_info == NULL)
    {
      //Trying to update a non-existing member
      continue;
    }

    // if  (the old_status_equal_to is not defined or
    //      the previous status is equal to old_status_equal_to)
    //    and
    //     (the old_status_different_from is not defined or
    //      the previous status is different from old_status_different_from)
    if ((old_status_equal_to == Group_member_info::MEMBER_END ||
        member_info->get_recovery_status() == old_status_equal_to) &&
       (old_status_different_from == Group_member_info::MEMBER_END ||
        member_info->get_recovery_status() != old_status_different_from))
    {
      /*
        The notification will be handled on the top level handle
        function that calls this one down the stack.
      */
      group_member_mgr->update_member_status(
        member_info->get_uuid(),
        status,
        m_notification_ctx);
    }
  }
}

/**
  Here we check:
  1) If the number of members was exceeded
  2) If member version is compatible with the group
  3) If the gtid_assignment_block_size is equal to the group
  4) If the hash algorithm used is equal to the group
  5) If the member has more known transactions than the group
  6) If the member has the same configuration flags that the group has
*/
int
Plugin_gcs_events_handler::check_group_compatibility(size_t number_of_members) const
{
  /*
    Check if group size did reach the maximum number of members.
  */
#ifndef DBUG_OFF
  if (set_number_of_members_on_view_changed_to_10)
    number_of_members= 10;
#endif
  if (number_of_members > 9)
  {
    log_message(MY_ERROR_LEVEL,
                "The START GROUP_REPLICATION command failed since the group "
                "already has 9 members");
    return GROUP_REPLICATION_MAX_GROUP_SIZE;
  }

  /*
    Check if the member is compatible with the group.
    It can be incompatible because its major version is lower or a rule says it.
    If incompatible notify whoever is waiting for the view with an error, so
    the plugin exits the group.
  */
  *joiner_compatibility_status= COMPATIBLE;
  int group_data_compatibility= 0;
  if (number_of_members > 1)
  {
    *joiner_compatibility_status= check_version_compatibility_with_group();
    group_data_compatibility= compare_member_transaction_sets();
  }

  if (*joiner_compatibility_status == INCOMPATIBLE)
  {
    log_message(MY_ERROR_LEVEL,
                "Member version is incompatible with the group");
    return GROUP_REPLICATION_CONFIGURATION_ERROR;
  }

  /*
    All group members must have the same gtid_assignment_block_size
    and transaction-write-set-extraction value, if joiner has a
    different value it is not allowed to join.
  */
  if (number_of_members > 1 &&
      compare_member_option_compatibility())
  {
    return GROUP_REPLICATION_CONFIGURATION_ERROR;
  }

  /*
    Check that the joiner doesn't has more GTIDs than the rest of the group.
    All the executed and received transactions in the group are collected and
    merged into a GTID set and all joiner transactions must be contained in it.
  */
  if (group_data_compatibility)
  {
    if (group_data_compatibility > 0)
    {
      log_message(MY_ERROR_LEVEL,
                  "The member contains transactions not present in the group. "
                  "The member will now exit the group.");
      return GROUP_REPLICATION_CONFIGURATION_ERROR;
    }
    else //error
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "It was not possible to assess if the member has more "
                  "transactions than the group. "
                  "The member will now exit the group.");
      return GROUP_REPLICATION_CONFIGURATION_ERROR;
      /* purecov: end */
    }
  }

  return 0;
}

Compatibility_type
Plugin_gcs_events_handler::check_version_compatibility_with_group() const
{
  bool override_lower_incompatibility= false;
  Compatibility_type compatibility_type= INCOMPATIBLE;
  bool read_compatible= false;

  std::vector<Group_member_info*> *all_members= group_member_mgr->get_all_members();
  std::vector<Group_member_info*>::iterator all_members_it;
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++)
  {
    Member_version member_version= (*all_members_it)->get_member_version();
    compatibility_type=
      compatibility_manager->check_local_incompatibility(member_version);

    if (compatibility_type == READ_COMPATIBLE)
    {
      read_compatible= true;
    }

    if (compatibility_type == INCOMPATIBLE)
    {
      break;
    }

    if (compatibility_type == INCOMPATIBLE_LOWER_VERSION)
    {
      if (get_allow_local_lower_version_join())
      {
        /*
          Despite between these two members the compatibility type
          is INCOMPATIBLE_LOWER_VERSION, when compared with others
          group members this server may be INCOMPATIBLE, so we need
          to test with all group members.
        */
        override_lower_incompatibility= true;
        compatibility_type= COMPATIBLE;
      }
      else
      {
        compatibility_type= INCOMPATIBLE;
        break;
      }
    }
  }

  if (compatibility_type != INCOMPATIBLE && override_lower_incompatibility)
  {
    log_message(MY_INFORMATION_LEVEL,
                "Member version is lower than some group member, but since "
                "option 'group_replication_allow_local_lower_version_join' "
                "is enabled, member will be allowed to join");
  }

  if (read_compatible && compatibility_type != INCOMPATIBLE)
  {
    compatibility_type= READ_COMPATIBLE;
  }

  //clean the members
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++)
  {
    delete (*all_members_it);
  }
  delete all_members;

  return compatibility_type;
}

int Plugin_gcs_events_handler::compare_member_transaction_sets() const
{
  int result= 0;

  Sid_map local_sid_map(NULL);
  Sid_map group_sid_map(NULL);
  Gtid_set local_member_set(&local_sid_map, NULL);
  Gtid_set group_set(&group_sid_map, NULL);

  std::vector<Group_member_info*> *all_members= group_member_mgr->get_all_members();
  std::vector<Group_member_info*>::iterator all_members_it;
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++) {

    std::string member_exec_set_str= (*all_members_it)->get_gtid_executed();
    std::string applier_ret_set_str= (*all_members_it)->get_gtid_retrieved();
    if ((*all_members_it)->get_gcs_member_id() ==
            local_member_info->get_gcs_member_id())
    {
      if (local_member_set.
              add_gtid_text(member_exec_set_str.c_str()) != RETURN_STATUS_OK ||
          local_member_set.
              add_gtid_text(applier_ret_set_str.c_str()) != RETURN_STATUS_OK)
      {
        /* purecov: begin inspected */
        log_message(MY_ERROR_LEVEL,
                    "Error processing local GTID sets when comparing this member"
                    " transactions against the group");
        result= -1;
        goto cleaning;
        /* purecov: end */
      }
    }
    else
    {
      if (group_set.
              add_gtid_text(member_exec_set_str.c_str()) != RETURN_STATUS_OK ||
          group_set.
              add_gtid_text(applier_ret_set_str.c_str()) != RETURN_STATUS_OK)
      {
        /* purecov: begin inspected */
        log_message(MY_ERROR_LEVEL,
                    "Error processing group GTID sets when comparing this member"
                    " transactions with the group");
        result= -1;
        goto cleaning;
        /* purecov: end */
      }
    }

  }

  /*
    Here we only error out if the joiner set is bigger, i.e, if they are equal
    no error is returned.
    One could argue that if a joiner has the same transaction set as the group
    then something is wrong as the group also has transaction associated to
    previous view changes.
    To reject this cases cause however false negatives when members leave and
    quickly rejoin the group or when groups are started by add several nodes at
    once.
  */
  if (!local_member_set.is_subset(&group_set))
  {
    char *local_gtid_set_buf;
    local_member_set.to_string(&local_gtid_set_buf);
    char *group_gtid_set_buf;
    group_set.to_string(&group_gtid_set_buf);
    log_message(MY_ERROR_LEVEL,
                "This member has more executed transactions than those present"
                " in the group. Local transactions: %s > Group transactions: %s",
                local_gtid_set_buf, group_gtid_set_buf);
    my_free(local_gtid_set_buf);
    my_free(group_gtid_set_buf);
    result= 1;
  }

cleaning:

  //clean the members
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++)
  {
    delete (*all_members_it);
  }
  delete all_members;

  return result;
}

void Plugin_gcs_events_handler::
collect_members_executed_sets(View_change_packet *view_packet) const
{
  std::vector<Group_member_info*> *all_members= group_member_mgr->get_all_members();
  std::vector<Group_member_info*>::iterator all_members_it;
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++)
  {

    // Joining/Recovering members don't have valid GTID executed information
    if ((*all_members_it)->get_recovery_status() ==
            Group_member_info::MEMBER_IN_RECOVERY)
    {
      continue;
    }

    std::string exec_set_str= (*all_members_it)->get_gtid_executed();
    view_packet->group_executed_set.push_back(exec_set_str);
  }

  //clean the members
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++)
  {
    delete (*all_members_it);
  }
  delete all_members;
}

int
Plugin_gcs_events_handler::compare_member_option_compatibility() const
{
  int result= 0;

  std::vector<Group_member_info*> *all_members= group_member_mgr->get_all_members();
  std::vector<Group_member_info*>::iterator all_members_it;
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++)
  {
    if (local_member_info->get_gtid_assignment_block_size() !=
        (*all_members_it)->get_gtid_assignment_block_size())
    {
      result= 1;
      log_message(MY_ERROR_LEVEL,
                  "The member is configured with a "
                  "group_replication_gtid_assignment_block_size option "
                  "value '%llu' different from the group '%llu'. "
                  "The member will now exit the group.",
                  local_member_info->get_gtid_assignment_block_size(),
                  (*all_members_it)->get_gtid_assignment_block_size());
      goto cleaning;
    }

    if (local_member_info->get_write_set_extraction_algorithm() !=
       (*all_members_it)->get_write_set_extraction_algorithm())
    {
      result= 1;
      log_message(MY_ERROR_LEVEL,
                  "The member is configured with a "
                  "transaction-write-set-extraction option "
                  "value '%s' different from the group '%s'. "
                  "The member will now exit the group.",
                  get_write_set_algorithm_string(
                      local_member_info->get_write_set_extraction_algorithm()),
                  get_write_set_algorithm_string(
                      (*all_members_it)->get_write_set_extraction_algorithm()));
      goto cleaning;
    }

    if (local_member_info->get_configuration_flags() !=
        (*all_members_it)->get_configuration_flags())
    {
      const uint32 member_configuration_flags = (*all_members_it)->get_configuration_flags();
      const uint32 local_configuration_flags = local_member_info->get_configuration_flags();

      result= 1;
      log_message(MY_ERROR_LEVEL,
                  "The member configuration is not compatible with "
                  "the group configuration. Variables such as "
                  "single_primary_mode or enforce_update_everywhere_checks "
                  "must have the same value on every server in the group. "
                  "(member configuration option: [%s], group configuration "
                  "option: [%s]).",
                  Group_member_info::get_configuration_flags_string(local_configuration_flags).c_str(),
                  Group_member_info::get_configuration_flags_string(member_configuration_flags).c_str());
      goto cleaning;
    }
  }

cleaning:
  for (all_members_it= all_members->begin();
       all_members_it!= all_members->end();
       all_members_it++)
    delete (*all_members_it);
  delete all_members;

  return result;
}

void
Plugin_gcs_events_handler::leave_group_on_error() const
{
  Gcs_operations::enum_leave_state state= gcs_module->leave();
  char **error_message= NULL;

  int error= channel_stop_all(CHANNEL_APPLIER_THREAD|CHANNEL_RECEIVER_THREAD,
                              stop_wait_timeout, error_message);
  if (error)
  {
    if (error_message != NULL && *error_message != NULL)
    {
      log_message(MY_ERROR_LEVEL,
                  "Error stopping all replication channels while server was"
                  " leaving the group. %s", *error_message);
      my_free(error_message);
    }
    else
    {
      log_message(MY_ERROR_LEVEL,
                  "Error stopping all replication channels while server was"
                  " leaving the group. Got error: %d. Please check the error"
                  " log for more details.", error);
    }
  }

  std::stringstream ss;
  plugin_log_level log_severity= MY_WARNING_LEVEL;
  switch (state)
  {
    case Gcs_operations::ERROR_WHEN_LEAVING:
      /* purecov: begin inspected */
      ss << "Unable to confirm whether the server has left the group or not. "
            "Check performance_schema.replication_group_members to check group membership information.";
      log_severity= MY_ERROR_LEVEL;
      break;
      /* purecov: end */
    case Gcs_operations::ALREADY_LEAVING:
      /* purecov: begin inspected */
      ss << "Skipping leave operation: concurrent attempt to leave the group is on-going.";
      break;
      /* purecov: end */
    case Gcs_operations::ALREADY_LEFT:
      /* purecov: begin inspected */
      ss << "Skipping leave operation: member already left the group.";
      break;
      /* purecov: end */
    case Gcs_operations::NOW_LEAVING:
      return;
  }
  log_message(log_severity, ss.str().c_str()); /* purecov: inspected */
}
