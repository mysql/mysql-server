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

#include "plugin/group_replication/include/plugin_handlers/primary_election_validation_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_validation_message.h"

int send_validation_message(Group_validation_message *message) {
  enum_gcs_error msg_error = gcs_module->send_message(*message);
  if (msg_error != GCS_OK) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                 "group validation operation"); /* purecov: inspected */
    return 1;                                   /* purecov: inspected */
  }
  return 0;
}

Primary_election_validation_handler::Primary_election_validation_handler()
    : validation_process_aborted(false), number_of_responses(0) {
  mysql_mutex_init(key_GR_LOCK_primary_election_validation_notification,
                   &notification_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_primary_election_validation_notification,
                  &notification_cond);
}

Primary_election_validation_handler::~Primary_election_validation_handler() {
  mysql_mutex_destroy(&notification_lock);
  mysql_cond_destroy(&notification_cond);
}

bool Primary_election_validation_handler::initialize_validation_structures() {
  DBUG_ASSERT(group_member_mgr);
  validation_process_aborted = false;
  number_of_responses = 0;
  group_members_info.clear();
  if (group_member_mgr != NULL) {
    std::vector<Group_member_info *> *all_members_info =
        group_member_mgr->get_all_members();
    for (Group_member_info *member : *all_members_info) {
      bool is_primary =
          member->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY &&
          local_member_info->in_primary_mode();
      Election_member_info *election_info = new Election_member_info(
          member->get_uuid(), member->get_member_version(), is_primary);
      group_members_info.insert(
          std::pair<const std::string, Election_member_info *>(
              member->get_gcs_member_id().get_member_id(), election_info));
      delete member;
    }
    delete all_members_info;
  } else {
    return true; /* purecov: inspected */
  }
  group_events_observation_manager->register_group_event_observer(this);
  return false;
}

void Primary_election_validation_handler::terminates_validation_structures() {
  group_events_observation_manager->unregister_group_event_observer(this);

  for (const std::pair<const std::string, Election_member_info *> &member_info :
       group_members_info) {
    delete member_info.second;
  }
}

Primary_election_validation_handler::enum_primary_validation_result
Primary_election_validation_handler::validate_primary_uuid(std::string &uuid) {
  if (local_member_info && local_member_info->in_primary_mode()) {
    // Check the uuid is not the current primary already
    std::string current_primary;
    group_member_mgr->get_primary_member_uuid(current_primary);
    if (current_primary == uuid) return CURRENT_PRIMARY;
  }

  // Check the uuid is present in the group
  for (const std::pair<const std::string, Election_member_info *> &member_info :
       group_members_info) {
    if (member_info.second->get_uuid() == uuid) return VALID_PRIMARY;
  }
  return INVALID_PRIMARY; /* purecov: inspected */
}

Primary_election_validation_handler::enum_primary_validation_result
Primary_election_validation_handler::validate_primary_version(
    std::string &uuid, std::string &error_msg) {
  uint32 primary_major_version = 0;
  uint32 lowest_major_version = 9999;

  /*
    Check if any of the members is below the needed version
    Also get the group lowest version and the version of the primary
  */
  for (const std::pair<const std::string, Election_member_info *> &member_info :
       group_members_info) {
    if (member_info.second->get_member_version().get_version() <
        PRIMARY_ELECTION_LEGACY_ALGORITHM_VERSION) {
      error_msg.assign(
          "The group contains a member of version that does not"
          " support appointed elections.");
      return INVALID_PRIMARY;
    }

    if (member_info.second->get_uuid() == uuid) {
      primary_major_version =
          member_info.second->get_member_version().get_major_version();
    }
    if (member_info.second->get_member_version().get_major_version() <
        lowest_major_version) {
      lowest_major_version =
          member_info.second->get_member_version().get_major_version();
    }
  }

  if (!uuid.empty()) {
    if (lowest_major_version < primary_major_version) {
      error_msg.assign(
          "The appointed primary member has a major version that is"
          " greater than the one of some of the members"
          " in the group.");  /* purecov: inspected */
      return INVALID_PRIMARY; /* purecov: inspected */
    }
  }

  return VALID_PRIMARY;
}

bool Primary_election_validation_handler::prepare_election() {
  mysql_mutex_lock(&notification_lock);
  bool is_slave_channel_running = is_any_slave_channel_running(
      CHANNEL_RECEIVER_THREAD | CHANNEL_APPLIER_THREAD);
  Group_validation_message *group_validation_message =
      new Group_validation_message(is_slave_channel_running,
                                   local_member_info->get_member_weight());
  if (send_validation_message(group_validation_message)) {
    /* purecov: begin inspected */
    mysql_mutex_unlock(&notification_lock);
    delete group_validation_message;
    return true;
    /* purecov: end */
  }
  delete group_validation_message;

  while ((group_members_info.size() > number_of_responses) &&
         !validation_process_aborted) {
    DBUG_PRINT(
        "sleep",
        ("Waiting for the primary election validation info to be gathered."));
    mysql_cond_wait(&notification_cond, &notification_lock);
  }

  mysql_mutex_unlock(&notification_lock);

  return false;
}

Primary_election_validation_handler::enum_primary_validation_result
Primary_election_validation_handler::validate_election(std::string &uuid,
                                                       std::string &valid_uuid,
                                                       std::string &error_msg) {
  if (validation_process_aborted) {
    return VALID_PRIMARY;
  }

  if (local_member_info && local_member_info->in_primary_mode()) {
    for (const std::pair<const std::string, Election_member_info *>
             &member_info : group_members_info) {
      if (member_info.second->is_primary() &&
          !member_info.second->member_left() &&
          member_info.second->has_channels()) {
        error_msg.assign(
            "There is a slave channel running in the group's"
            " current primary member.");
        return Primary_election_validation_handler::INVALID_PRIMARY;
      }
    }
    return VALID_PRIMARY;
  } else {
    enum_primary_validation_result result =
        validate_group_slave_channels(valid_uuid);
    if (Primary_election_validation_handler::GROUP_SOLO_PRIMARY == result) {
      if (!uuid.empty()) {
        if (uuid == valid_uuid) {
          // Check that the solo primary has the lowest version in the group
          enum_primary_validation_result result =
              validate_primary_version(valid_uuid, error_msg);
          if (Primary_election_validation_handler::INVALID_PRIMARY == result)
            error_msg.assign(
                "There is a member of a major version that"
                " has running slave channels"); /* purecov: inspected */
          return result;                        /* purecov: inspected */
        } else {
          error_msg.assign(
              "The requested primary is not valid as a slave channel"
              " is running on member " +
              valid_uuid);
          return INVALID_PRIMARY;
        }
      } else {
        return GROUP_SOLO_PRIMARY;
      }
    } else {
      if (Primary_election_validation_handler::INVALID_PRIMARY == result)
        error_msg.assign(
            "There is more than a member in the group with"
            " running slave channels so no primary can be"
            " elected.");
      return result;
    }
  }
}

Primary_election_validation_handler::enum_primary_validation_result
Primary_election_validation_handler::validate_group_slave_channels(
    std::string &valid_uuid) {
  int number_of_member_with_slave_channels = 0;

  for (const std::pair<const std::string, Election_member_info *> &member_info :
       group_members_info) {
    if (!member_info.second->member_left() &&
        member_info.second->has_channels()) {
      number_of_member_with_slave_channels++;
      valid_uuid.assign(member_info.second->get_uuid());
    }
  }

  // This means the process was aborted, no point in returning an error
  if (validation_process_aborted) return VALID_PRIMARY;
  if (number_of_member_with_slave_channels > 1) return INVALID_PRIMARY;
  if (number_of_member_with_slave_channels == 1) return GROUP_SOLO_PRIMARY;

  return VALID_PRIMARY;
}

void Primary_election_validation_handler::abort_validation_process() {
  mysql_mutex_lock(&notification_lock);
  validation_process_aborted = true;
  mysql_cond_broadcast(&notification_cond);
  mysql_mutex_unlock(&notification_lock);
}

// The listeners for group events

int Primary_election_validation_handler::after_view_change(
    const std::vector<Gcs_member_identifier> &,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &, bool is_leaving, bool *,
    enum_primary_election_mode *, std::string &) {
  if (is_leaving) {
    abort_validation_process();
  }

  for (const Gcs_member_identifier &member_identifier : leaving) {
    std::map<const std::string, Election_member_info *>::iterator map_it;
    map_it = group_members_info.find(member_identifier.get_member_id());
    /*
      In theory we start with a group of 2 members but a new one tries to join
      and leaves, so we can see someone leaving that was not in the group when
      we started.
    */
    if (map_it != group_members_info.end()) {
      map_it->second->set_has_running_channels(false);
      map_it->second->set_member_left(true);
      // Don't count a member that sent its information and then left
      if (!map_it->second->is_information_set()) {
        number_of_responses++;
      }
      map_it->second->set_information_set(true);
    }
  }

  mysql_mutex_lock(&notification_lock);
  if (group_members_info.size() == number_of_responses)
    mysql_cond_broadcast(&notification_cond);
  mysql_mutex_unlock(&notification_lock);

  return 0;
}

int Primary_election_validation_handler::after_primary_election(
    std::string, bool, enum_primary_election_mode, int) {
  return 0; /* purecov: inspected */
}

int Primary_election_validation_handler::before_message_handling(
    const Plugin_gcs_message &message, const std::string &message_origin,
    bool *skip_message) {
  *skip_message = false;
  Plugin_gcs_message::enum_cargo_type message_type = message.get_cargo_type();

  if (message_type == Plugin_gcs_message::CT_GROUP_VALIDATION_MESSAGE) {
    const Group_validation_message group_validation_message =
        (const Group_validation_message &)message;

    std::map<const std::string, Election_member_info *>::iterator map_it;
    map_it = group_members_info.find(message_origin);

    DBUG_ASSERT(map_it != group_members_info.end());
    if (map_it != group_members_info.end()) {
      map_it->second->set_has_running_channels(
          group_validation_message.has_slave_channels());
      map_it->second->set_information_set(true);
      // Only update remote values
      if (message_origin !=
          local_member_info->get_gcs_member_id().get_member_id()) {
        group_member_mgr->update_member_weight(
            map_it->second->get_uuid(),
            group_validation_message.get_member_weight());
      }
      number_of_responses++;
    }
  }

  mysql_mutex_lock(&notification_lock);
  if (group_members_info.size() == number_of_responses)
    mysql_cond_broadcast(&notification_cond);
  mysql_mutex_unlock(&notification_lock);

  return 0;
}
