/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef PRIMARY_ELECTION_VALIDATION_HANDLER_INCLUDED
#define PRIMARY_ELECTION_VALIDATION_HANDLER_INCLUDED

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_utils.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/include/plugin_utils.h"

class Primary_election_validation_handler : public Group_event_observer {
 public:
  /** Enum for the end results of validation */
  enum enum_primary_validation_result {
    VALID_PRIMARY = 0,      // Primary / Group is valid
    INVALID_PRIMARY = 1,    // Primary is invalid
    CURRENT_PRIMARY = 2,    // Primary is the current one
    GROUP_SOLO_PRIMARY = 3  // Only a member can become primary
  };

  /** Constructor */
  Primary_election_validation_handler();

  /** Destructor */
  ~Primary_election_validation_handler() override;

  /**
    Initialize the group member structures and registers an observer

    @note this method should be called in a GCS logical moment

    @return true if some error happened, false otherwise
  */
  bool initialize_validation_structures();

  /**
     Cleans the membership info, and pending notifications.
     Deregister the observer for group events.
  */
  void terminates_validation_structures();

  /**
   Shares information among members about weights and channels in all members
  */
  bool prepare_election();

  /**
    Validate group for election
    @param[in] uuid   member to validate
    @param[out] valid_uuid the only member valid for election
    @param[out] error_msg the error message outputted by validation

    @returns if the primary is valid or what is the solo valid primary
      @retval VALID_PRIMARY if valid
      @retval INVALID_PRIMARY if not valid
      @retval CURRENT_PRIMARY if it is already the primary
      @retval GROUP_SOLO_PRIMARY only one member is valid
  */
  enum_primary_validation_result validate_election(std::string &uuid,
                                                   std::string &valid_uuid,
                                                   std::string &error_msg);

  /**
    Check that the UUID is valid and present in the group
    @param uuid   member to validate

    @retval INVALID_PRIMARY if not present in the group anymore
    @retval CURRENT_PRIMARY if the uuid provided is already the primary
    @retval VALID_PRIMARY   if the uuid is valid in the group
  */
  enum_primary_validation_result validate_primary_uuid(std::string &uuid);

  /**
    Check that the group members have valid versions
    1. All support appointed elections
    2. The appointed member is from the lowest version present in the group

    @param[in] uuid   member to validate
    @param[out] error_msg the error message outputted by validation
  */
  enum_primary_validation_result validate_primary_version(
      std::string &uuid, std::string &error_msg);

  /**
    Interrupt the validation process in case of an abort
  */
  void abort_validation_process();

 private:
  /** Check which members have slave channels */
  enum_primary_validation_result validate_group_slave_channels(
      std::string &uuid);

  // The listeners for group events

  int after_view_change(const std::vector<Gcs_member_identifier> &joining,
                        const std::vector<Gcs_member_identifier> &leaving,
                        const std::vector<Gcs_member_identifier> &group,
                        bool is_leaving, bool *skip_election,
                        enum_primary_election_mode *election_mode,
                        std::string &suggested_primary) override;
  int after_primary_election(
      std::string primary_uuid,
      enum_primary_election_primary_change_status primary_change_status,
      enum_primary_election_mode election_mode, int error) override;
  int before_message_handling(const Plugin_gcs_message &message,
                              const std::string &message_origin,
                              bool *skip_message) override;

  /** Was the validation process aborted */
  bool validation_process_aborted;

  /** The number of members that sent info or left */
  uint number_of_responses;

  /** The information about the group members <address,member>*/
  std::map<const std::string, Election_member_info *> group_members_info;

  /** The lock for notifications*/
  mysql_mutex_t notification_lock;
  /** The condition for notifications*/
  mysql_cond_t notification_cond;
};

#endif /** PRIMARY_ELECTION_VALIDATION_HANDLER_INCLUDED */
