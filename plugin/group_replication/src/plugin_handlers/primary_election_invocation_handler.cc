/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/plugin_handlers/primary_election_invocation_handler.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_utils.h"

Primary_election_handler::Primary_election_handler()
    : election_process_running(false) {
  mysql_mutex_init(key_GR_LOCK_primary_election_running_flag, &flag_lock,
                   MY_MUTEX_INIT_FAST);
}

Primary_election_handler::~Primary_election_handler() {
  mysql_mutex_destroy(&flag_lock);
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
  Group_member_info *primary_member_info = nullptr;
  std::vector<Group_member_info *> *all_members_info =
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
        group_events_observation_manager->after_primary_election("", false,
                                                                 mode);
        goto end;
      }
      /* purecov: end */
    }
  }

  if (!appointed_uuid) {
    pick_primary_member(primary_uuid, all_members_info);
  }

  primary_member_info = group_member_mgr->get_group_member_info(primary_uuid);

  if (primary_member_info == NULL) {
    if (all_members_info->size() != 1) {
      // There are no servers in the group or they are all recovering WARN the
      // user
      LogPluginErr(WARNING_LEVEL,
                   ER_GRP_RPL_NO_SUITABLE_PRIMARY_MEM); /* purecov: inspected */
    }
    group_events_observation_manager->after_primary_election(
        "", false, mode, PRIMARY_ELECTION_NO_CANDIDATES_ERROR);
    if (enable_server_read_mode(PSESSION_DEDICATED_THREAD)) {
      LogPluginErr(WARNING_LEVEL,
                   ER_GRP_RPL_ENABLE_READ_ONLY_FAILED); /* purecov: inspected */
    }
    goto end;
  }

  in_primary_mode = local_member_info->in_primary_mode();
  has_primary_changed = Group_member_info::MEMBER_ROLE_PRIMARY !=
                            primary_member_info->get_role() ||
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

      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_NEW_PRIMARY_ELECTED,
                   primary_member_info->get_hostname().c_str(),
                   primary_member_info->get_port(), message.c_str());
      internal_primary_election(primary_uuid, mode);
    } else {
      // retain the old message
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_NEW_PRIMARY_ELECTED,
                   primary_member_info->get_hostname().c_str(),
                   primary_member_info->get_port(),
                   "Enabling conflict detection until the new primary applies "
                   "all relay logs.");
      legacy_primary_election(primary_uuid);
    }
  } else {
    group_events_observation_manager->after_primary_election("", false, mode);
  }

end:
  // clean the members
  std::vector<Group_member_info *>::iterator it;
  for (it = all_members_info->begin(); it != all_members_info->end(); it++) {
    delete (*it);
  }
  delete all_members_info;
  delete primary_member_info;
  return 0;
}

int Primary_election_handler::internal_primary_election(
    std::string &primary_to_elect, enum_primary_election_mode mode) {
  if (secondary_election_handler.is_election_process_running()) {
    secondary_election_handler.terminate_election_process();
  }

  DBUG_ASSERT(!primary_election_handler.is_election_process_running() ||
              primary_election_handler.is_election_process_terminating());

  /** Wait for an old process to end*/
  if (primary_election_handler.is_election_process_terminating())
    primary_election_handler
        .wait_on_election_process_termination(); /* purecov: inspected */

  std::vector<Group_member_info *> *members_info =
      group_member_mgr->get_all_members();

  /* Declare at this point that all members are in primary mode for switch
   * cases*/
  group_member_mgr->update_primary_member_flag(true);

  if (!local_member_info->get_uuid().compare(primary_to_elect)) {
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
  Group_member_info *primary_member_info =
      group_member_mgr->get_group_member_info(primary_uuid);

  /*
    A new primary was elected, inform certifier to enable conflict
    detection until the new primary apply all relay logs.
  */
  Single_primary_action_packet *single_primary_action =
      new Single_primary_action_packet(
          Single_primary_action_packet::NEW_PRIMARY);
  applier_module->add_single_primary_action_packet(single_primary_action);

  if (is_primary_local) {
    if (disable_server_read_mode(PSESSION_DEDICATED_THREAD)) {
      LogPluginErr(
          WARNING_LEVEL,
          ER_GRP_RPL_DISABLE_READ_ONLY_FAILED); /* purecov: inspected */
    }
  } else {
    if (enable_server_read_mode(PSESSION_DEDICATED_THREAD)) {
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
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SRV_SECONDARY_MEM,
                 primary_member_info->get_hostname().c_str(),
                 primary_member_info->get_port());
  }

  group_events_observation_manager->after_primary_election(primary_uuid, true,
                                                           DEAD_OLD_PRIMARY);
  delete primary_member_info;

  return 0;
}

bool Primary_election_handler::pick_primary_member(
    std::string &primary_uuid,
    std::vector<Group_member_info *> *all_members_info) {
  DBUG_ENTER("Primary_election_handler::pick_primary_member");

  bool am_i_leaving = true;
#ifndef DBUG_OFF
  int n = 0;
#endif
  Group_member_info *the_primary = NULL;

  std::vector<Group_member_info *>::iterator it;
  std::vector<Group_member_info *>::iterator lowest_version_end;

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
#ifndef DBUG_OFF
    DBUG_ASSERT(n <= 1);
#endif

    Group_member_info *member = *it;
    if (local_member_info->in_primary_mode() && the_primary == NULL &&
        member->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY) {
      the_primary = member;
#ifndef DBUG_OFF
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
    if (the_primary == NULL) {
      for (it = all_members_info->begin();
           it != lowest_version_end && the_primary == NULL; it++) {
        Group_member_info *member_info = *it;

        DBUG_ASSERT(member_info);
        if (member_info && member_info->get_recovery_status() ==
                               Group_member_info::MEMBER_ONLINE)
          the_primary = member_info;
      }
    }
  }

  if (the_primary == NULL) DBUG_RETURN(1);

  primary_uuid.assign(the_primary->get_uuid());
  DBUG_RETURN(0);
}

std::vector<Group_member_info *>::iterator
sort_and_get_lowest_version_member_position(
    std::vector<Group_member_info *> *all_members_info) {
  std::vector<Group_member_info *>::iterator it;

  // sort in ascending order of lower member version
  std::sort(all_members_info->begin(), all_members_info->end(),
            Group_member_info::comparator_group_member_version);

  /* if vector contains only single version then leader should be picked from
     all members
   */
  std::vector<Group_member_info *>::iterator lowest_version_end =
      all_members_info->end();

  /* first member will have lowest version as members are already
     sorted above using member_version.
   */
  it = all_members_info->begin();
  Group_member_info *first_member = *it;
  uint32 lowest_major_version =
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
  for (it = all_members_info->begin() + 1; it != all_members_info->end();
       it++) {
    if (lowest_major_version !=
        (*it)->get_member_version().get_major_version()) {
      lowest_version_end = it;
      break;
    }
  }

  return lowest_version_end;
}

void sort_members_for_election(
    std::vector<Group_member_info *> *all_members_info,
    std::vector<Group_member_info *>::iterator lowest_version_end) {
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
