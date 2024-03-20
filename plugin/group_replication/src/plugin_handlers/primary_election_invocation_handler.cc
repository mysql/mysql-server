/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/primary_election_invocation_handler.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/member_actions_handler.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_utils.h"
#include "plugin/group_replication/include/services/system_variable/get_system_variable.h"

Primary_election_handler::Primary_election_handler(
    ulong components_stop_timeout)
    : election_process_running(false) {
  mysql_mutex_init(key_GR_LOCK_primary_election_running_flag, &flag_lock,
                   MY_MUTEX_INIT_FAST);
  primary_election_handler.set_stop_wait_timeout(components_stop_timeout);
  secondary_election_handler.set_stop_wait_timeout(components_stop_timeout);
}

Primary_election_handler::~Primary_election_handler() {
  mysql_mutex_destroy(&flag_lock);
}

void Primary_election_handler::set_stop_wait_timeout(ulong timeout) {
  primary_election_handler.set_stop_wait_timeout(timeout);
  secondary_election_handler.set_stop_wait_timeout(timeout);
}

bool Primary_election_handler::is_an_election_running() {
  mysql_mutex_lock(&flag_lock);
  bool running_flag = election_process_running;
  mysql_mutex_unlock(&flag_lock);
  return running_flag;
}

void Primary_election_handler::set_election_running(bool election_running) {
  mysql_mutex_lock(&flag_lock);
  election_process_running = election_running;
  mysql_mutex_unlock(&flag_lock);
}

int Primary_election_handler::request_group_primary_election(
    std::string primary_uuid, enum_primary_election_mode mode) {
  Single_primary_message single_primary_message(primary_uuid, mode);
  if (send_message(&single_primary_message)) return 1;
  return 0;
}

int Primary_election_handler::handle_primary_election_message(
    Single_primary_message *message, Notification_context *notification_ctx) {
  return execute_primary_election(message->get_primary_uuid(),
                                  message->get_election_mode(),
                                  notification_ctx);
}

int Primary_election_handler::terminate_election_process() {
  int error = 0;
  if (secondary_election_handler.is_election_process_running()) {
    error = secondary_election_handler
                .terminate_election_process(); /* purecov: inspected */
  }
  if (primary_election_handler.is_election_process_running()) {
    error += primary_election_handler
                 .terminate_election_process(); /* purecov: inspected */
  }
  return error;
}

int Primary_election_handler::execute_primary_election(
    std::string &primary_uuid, enum_primary_election_mode mode,
    Notification_context *notification_ctx) {
  if (Group_member_info::MEMBER_OFFLINE ==
      local_member_info->get_recovery_status()) {
    return 0;
  }

  bool has_primary_changed;
  bool in_primary_mode;
  Group_member_info primary_member_info;
  bool primary_member_info_not_found = true;
  Group_member_info_list *all_members_info =
      group_member_mgr->get_all_members();

  bool appointed_uuid = !primary_uuid.empty();
  if (appointed_uuid) {
    if (!group_member_mgr->is_member_info_present(primary_uuid)) {
      /* purecov: begin inspected */
      // If the old primary died we cannot skip it
      if (mode == DEAD_OLD_PRIMARY) {
        appointed_uuid = false;
      } else {
        // If the requested primary is not there, ignore the request.
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_APPOINTED_PRIMARY_NOT_PRESENT);
        group_events_observation_manager->after_primary_election(
            "",
            enum_primary_election_primary_change_status::
                PRIMARY_DID_NOT_CHANGE_NO_CANDIDATE,
            mode);
        goto end;
      }
      /* purecov: end */
    }
  }

  if (!appointed_uuid) {
    pick_primary_member(primary_uuid, all_members_info);
  }

  primary_member_info_not_found = group_member_mgr->get_group_member_info(
      primary_uuid, primary_member_info);

  if (primary_member_info_not_found) {
    if (all_members_info->size() != 1) {
      // There are no servers in the group or they are all recovering WARN the
      // user
      LogPluginErr(WARNING_LEVEL,
                   ER_GRP_RPL_NO_SUITABLE_PRIMARY_MEM); /* purecov: inspected */
    }
    group_events_observation_manager->after_primary_election(
        "",
        enum_primary_election_primary_change_status::
            PRIMARY_DID_NOT_CHANGE_NO_CANDIDATE,
        mode, PRIMARY_ELECTION_NO_CANDIDATES_ERROR);
    if (enable_server_read_mode("(GR) primary election failed")) {
      LogPluginErr(WARNING_LEVEL,
                   ER_GRP_RPL_ENABLE_READ_ONLY_FAILED); /* purecov: inspected */
    }
    goto end;
  }

  in_primary_mode = local_member_info->in_primary_mode();
  has_primary_changed = Group_member_info::MEMBER_ROLE_PRIMARY !=
                            primary_member_info.get_role() ||
                        !in_primary_mode;
  if (has_primary_changed) {
    /*
      We change roles when elections are just starting.
      What if we are changing to SP and the process aborts?
      Again, that would either mean the members is stopping or moving to error.
      On ERROR, member states might appear as secondaries while SP is stated as
      being OFF.
    */
    // declare this as the new primary
    group_member_mgr->update_group_primary_roles(primary_uuid,
                                                 *notification_ctx);

    bool legacy_election = false;
    for (Group_member_info *member : *all_members_info) {
      if (member->get_member_version().get_version() <
          PRIMARY_ELECTION_LEGACY_ALGORITHM_VERSION) {
        legacy_election = true;
      }
    }

    set_election_running(true);
    if (!primary_uuid.compare(local_member_info->get_uuid())) {
      print_gtid_info_in_log();
    }
    if (!legacy_election) {
      std::string message;
      if (DEAD_OLD_PRIMARY == mode)
        message.assign(
            "The new primary will execute all previous group transactions "
            "before allowing writes.");
      if (UNSAFE_OLD_PRIMARY == mode)
        message.assign(
            "The new primary will execute all previous group transactions "
            "before allowing writes. Enabling conflict detection until the new "
            "primary applies all relay logs.");
      if (SAFE_OLD_PRIMARY == mode)
        message.assign(
            "Enabling conflict detection until the new primary applies all "
            "relay logs.");

      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_NEW_PRIMARY_ELECTED,
                   primary_member_info.get_hostname().c_str(),
                   primary_member_info.get_port(), message.c_str());
      internal_primary_election(primary_uuid, mode);
    } else {
      // retain the old message
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_NEW_PRIMARY_ELECTED,
                   primary_member_info.get_hostname().c_str(),
                   primary_member_info.get_port(),
                   "Enabling conflict detection until the new primary applies "
                   "all relay logs.");
      legacy_primary_election(primary_uuid);
    }
  } else {
    group_events_observation_manager->after_primary_election(
        "", enum_primary_election_primary_change_status::PRIMARY_DID_NOT_CHANGE,
        mode);
  }

end:
  // clean the members
  Group_member_info_list_iterator it;
  for (it = all_members_info->begin(); it != all_members_info->end(); it++) {
    delete (*it);
  }
  delete all_members_info;
  return 0;
}

void Primary_election_handler::print_gtid_info_in_log() {
  Replication_thread_api applier_channel("group_replication_applier");
  std::string applier_retrieved_gtids;
  std::string server_executed_gtids;
  Get_system_variable *get_system_variable = new Get_system_variable();

  if (get_system_variable->get_global_gtid_executed(server_executed_gtids)) {
    /* purecov: begin inspected */
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_GTID_EXECUTED_EXTRACT_ERROR);
    goto err;
    /* purecov: inspected */
  }
  if (applier_channel.get_retrieved_gtid_set(applier_retrieved_gtids)) {
    /* purecov: begin inspected */
    LogPluginErr(WARNING_LEVEL,
                 ER_GRP_RPL_GTID_SET_EXTRACT_ERROR); /* purecov: inspected */
    goto err;
    /* purecov: end */
  }
  LogPluginErr(INFORMATION_LEVEL, ER_GR_ELECTED_PRIMARY_GTID_INFORMATION,
               "gtid_executed", server_executed_gtids.c_str());
  LogPluginErr(INFORMATION_LEVEL, ER_GR_ELECTED_PRIMARY_GTID_INFORMATION,
               "applier channel received_transaction_set",
               applier_retrieved_gtids.c_str());
err:
  delete get_system_variable;
}

int Primary_election_handler::internal_primary_election(
    std::string &primary_to_elect, enum_primary_election_mode mode) {
  if (secondary_election_handler.is_election_process_running()) {
    secondary_election_handler.terminate_election_process();
  }

  assert(!primary_election_handler.is_election_process_running() ||
         primary_election_handler.is_election_process_terminating());

  /** Wait for an old process to end*/
  if (primary_election_handler.is_election_process_terminating())
    primary_election_handler
        .wait_on_election_process_termination(); /* purecov: inspected */

  Group_member_info_list *members_info = group_member_mgr->get_all_members();

  /* Declare at this point that all members are in primary mode for switch
   * cases*/
  group_member_mgr->update_primary_member_flag(true);

  if (!local_member_info->get_uuid().compare(primary_to_elect)) {
    notify_election_running();
    primary_election_handler.launch_primary_election_process(
        mode, primary_to_elect, members_info);
  } else {
    secondary_election_handler.launch_secondary_election_process(
        mode, primary_to_elect, members_info);
  }

  for (Group_member_info *member : *members_info) {
    delete member;
  }
  delete members_info;

  return 0;
}

int Primary_election_handler::legacy_primary_election(
    std::string &primary_uuid) {
  const bool is_primary_local =
      !primary_uuid.compare(local_member_info->get_uuid());
  Group_member_info primary_member_info;
  const bool primary_member_info_not_found =
      group_member_mgr->get_group_member_info(primary_uuid,
                                              primary_member_info);

  /*
    A new primary was elected, inform certifier to enable conflict
    detection until the new primary apply all relay logs.
  */
  Single_primary_action_packet *single_primary_action =
      new Single_primary_action_packet(
          Single_primary_action_packet::NEW_PRIMARY);
  applier_module->add_single_primary_action_packet(single_primary_action);

  if (is_primary_local) {
    member_actions_handler->trigger_actions(
        Member_actions::AFTER_PRIMARY_ELECTION);
  } else {
    if (enable_server_read_mode("(GR) new primary elected")) {
      LogPluginErr(WARNING_LEVEL,
                   ER_GRP_RPL_ENABLE_READ_ONLY_FAILED); /* purecov: inspected */
    }
  }

  /* code position limits messaging to primary change */
  if (is_primary_local) {
    /*
     Due the the cleanup on the applier code, we launch the new primary
     process thread here to ensure the message about the relay log queue being
     consumed is sent to other members.
    */
    internal_primary_election(primary_uuid, LEGACY_ELECTION_PRIMARY);
  } else {
    set_election_running(false);
    if (primary_member_info_not_found) {
      LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEMBER_INFO_DOES_NOT_EXIST,
                   "as the primary by the member uuid", primary_uuid.c_str(),
                   "a primary election. The group will heal itself on the next "
                   "primary election that will be triggered automatically");
    } else {
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SRV_SECONDARY_MEM,
                   primary_member_info.get_hostname().c_str(),
                   primary_member_info.get_port());
    }
  }

  group_events_observation_manager->after_primary_election(
      primary_uuid,
      enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE,
      DEAD_OLD_PRIMARY);

  return 0;
}

bool Primary_election_handler::pick_primary_member(
    std::string &primary_uuid, Group_member_info_list *all_members_info) {
  DBUG_TRACE;

  bool am_i_leaving = true;
#ifndef NDEBUG
  int n = 0;
#endif
  Group_member_info *the_primary = nullptr;

  Group_member_info_list_iterator it;
  Group_member_info_list_iterator lowest_version_end;

  /* sort members based on member_version and get first iterator position
     where member version differs
   */
  lowest_version_end =
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
  for (it = all_members_info->begin(); it != all_members_info->end(); it++) {
#ifndef NDEBUG
    assert(n <= 1);
#endif

    Group_member_info *member = *it;
    if (local_member_info->in_primary_mode() && the_primary == nullptr &&
        member->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY) {
      the_primary = member;
#ifndef NDEBUG
      n++;
#endif
    }

    /* Found the primary and it is me. Check that I am not offline. */
    if (!member->get_uuid().compare(local_member_info->get_uuid())) {
      am_i_leaving =
          member->get_recovery_status() == Group_member_info::MEMBER_OFFLINE;
    }
  }

  /* If I am not leaving, then pick a member. Otherwise do nothing. */
  if (!am_i_leaving) {
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
    if (the_primary == nullptr) {
      for (it = all_members_info->begin();
           it != lowest_version_end && the_primary == nullptr; it++) {
        Group_member_info *member_info = *it;

        assert(member_info);
        if (member_info && member_info->get_recovery_status() ==
                               Group_member_info::MEMBER_ONLINE)
          the_primary = member_info;
      }
    }
  }

  if (the_primary == nullptr) return true;

  primary_uuid.assign(the_primary->get_uuid());
  return false;
}

Group_member_info_list_iterator sort_and_get_lowest_version_member_position(
    Group_member_info_list *all_members_info) {
  Group_member_info_list_iterator it;

  // sort in ascending order of lower member version
  std::sort(all_members_info->begin(), all_members_info->end(),
            Group_member_info::comparator_group_member_version);

  /* if vector contains only single version then leader should be picked from
     all members
   */
  Group_member_info_list_iterator lowest_version_end = all_members_info->end();

  /* first member will have lowest version as members are already
     sorted above using member_version.
   */
  it = all_members_info->begin();
  Group_member_info *first_member = *it;

  /* to avoid read compatibility issue leader should be picked only from lowest
     version members so save position where member version differs.

     set lowest_version_end when major version changes

     eg: for a list: 8.0.17, 8.0.18, 8.0.19
         the members to be considered for election will be:
            8.0.17
  */

  for (it = all_members_info->begin() + 1; it != all_members_info->end();
       it++) {
    if (first_member->get_member_version() != (*it)->get_member_version()) {
      lowest_version_end = it;
      break;
    }
  }

  return lowest_version_end;
}

void sort_members_for_election(
    Group_member_info_list *all_members_info,
    Group_member_info_list_iterator lowest_version_end) {
  Group_member_info *first_member = *(all_members_info->begin());
  Member_version lowest_version = first_member->get_member_version();

  // sort only lower version members as they only will be needed to pick leader
  if (lowest_version >= PRIMARY_ELECTION_MEMBER_WEIGHT_VERSION)
    std::sort(all_members_info->begin(), lowest_version_end,
              Group_member_info::comparator_group_member_weight);
  else
    std::sort(all_members_info->begin(), lowest_version_end,
              Group_member_info::comparator_group_member_uuid);
}

void Primary_election_handler::notify_election_running() {
  transaction_consistency_manager->enable_primary_election_checks();
}

void Primary_election_handler::notify_election_end() {
  transaction_consistency_manager->disable_primary_election_checks();
}
