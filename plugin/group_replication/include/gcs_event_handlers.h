/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_EVENT_HANDLERS_INCLUDE
#define GCS_EVENT_HANDLERS_INCLUDE

#include <set>
#include <vector>

#include "plugin/group_replication/include/applier.h"
#include "plugin/group_replication/include/compatibility_module.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/gcs_view_modification_notifier.h"
#include "plugin/group_replication/include/plugin_constants.h"
#include "plugin/group_replication/include/plugin_handlers/read_mode_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/plugin_messages/recovery_message.h"
#include "plugin/group_replication/include/plugin_messages/single_primary_message.h"
#include "plugin/group_replication/include/recovery.h"
#include "plugin/group_replication/include/services/notification/notification.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_event_listener.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_event_listener.h"

/**
  Group_member_info_pointer_comparator to guarantee uniqueness
 */
struct Group_member_info_pointer_comparator {
  bool operator()(Group_member_info *one, Group_member_info *other) const {
    return one->has_lower_uuid(other);
  }
};

/*
  @class Plugin_gcs_events_handler

  Implementation of all GCS event handlers to the plugin
 */
class Plugin_gcs_events_handler : public Gcs_communication_event_listener,
                                  public Gcs_control_event_listener {
 public:
  /**
    Plugin_gcs_events_handler constructor

    It receives, via the constructor, all the necessary dependencies to work.
  */
  Plugin_gcs_events_handler(Applier_module_interface *applier_module,
                            Recovery_module *recovery_module,
                            Plugin_gcs_view_modification_notifier *vc_notifier,
                            Compatibility_module *compatibility_manager,
                            ulong components_stop_timeout);
  virtual ~Plugin_gcs_events_handler();

  /*
   Implementation of all callback methods
   */
  void on_message_received(const Gcs_message &message) const;
  void on_view_changed(const Gcs_view &new_view,
                       const Exchanged_data &exchanged_data) const;
  Gcs_message_data *get_exchangeable_data() const;
  void on_suspicions(
      const std::vector<Gcs_member_identifier> &members,
      const std::vector<Gcs_member_identifier> &unreachable) const;

  /**
    Sets the component stop timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout(ulong timeout) { stop_wait_timeout = timeout; }

 private:
  /*
   Individual handling methods for all possible message types
   received via on_message_received(...)
   */
  void handle_transactional_message(const Gcs_message &message) const;
  void handle_certifier_message(const Gcs_message &message) const;
  void handle_recovery_message(Plugin_gcs_message *message) const;
  void handle_stats_message(const Gcs_message &message) const;
  void handle_single_primary_message(
      Plugin_gcs_message *processed_message) const;
  void handle_group_action_message(const Gcs_message &message) const;
  /**
    This method passes the message to message listeners
    @param plugin_message A pointer to the processed plugin gcs message
    @param message_origin A id of the message originating member
    @return true if the message should be skipped, false otherwise
  */
  bool pre_process_message(Plugin_gcs_message *plugin_message,
                           const std::string &message_origin) const;
  /*
   Methods to act upon members after a on_view_change(...) is called
   */
  int update_group_info_manager(const Gcs_view &new_view,
                                const Exchanged_data &exchanged_data,
                                bool is_joining, bool is_leaving) const;
  void handle_joining_members(const Gcs_view &new_view, bool is_joining,
                              bool is_leaving) const;
  void handle_leaving_members(const Gcs_view &new_view, bool is_joining,
                              bool is_leaving) const;

  /**
    This method updates the status of the members in the list according to the
    given parameters.

    @param members               the vector with members to change the status to
    @param status                the status to change to.
    @param old_equal_to          change if the old status is equal to
    @param old_different_from    change if the old status if different from

    @note When not using the old_equal_to and old_different_from parameters, you
    can pass the Group_member_info::MEMBER_END value.
  */
  void update_member_status(
      const std::vector<Gcs_member_identifier> &members,
      Group_member_info::Group_member_status status,
      Group_member_info::Group_member_status old_equal_to,
      Group_member_info::Group_member_status old_different_from) const;

  /**
    This method handles the election of a new primary node when the plugin runs
    in single primary mode.

    @param enum_primary_election_mode election mode
    @param suggested_primary what should be the next primary to elect

    @note This function unsets the super read only mode on primary node
          and sets it on secondary nodes
  */
  void handle_leader_election_if_needed(
      enum_primary_election_mode election_mode,
      std::string &suggested_primary) const;

  int process_local_exchanged_data(const Exchanged_data &exchanged_data,
                                   bool is_joining) const;

  /**
    Verifies if a certain Vector of Member Ids contains a given member id.

    @param members   the vector with members to verify
    @param member_id the member to check if it contained.

    @return true if member_id occurs in members.
   */
  bool is_member_on_vector(const std::vector<Gcs_member_identifier> &members,
                           const Gcs_member_identifier &member_id) const;

  /**
    Checks the compatibility of the member with the group.
    It checks:
      1) If the number of members was exceeded
      2) If member version is compatible with the group
      3) If the gtid_assignment_block_size is equal to the group
      4) If the hash algorithm used is equal to the group
      5) If the member has more known transactions than the group

    @param number_of_members  the number of members in the new view

    @retval 0      compatible
    @retval >0     not compatible with the group
  */
  int check_group_compatibility(size_t number_of_members) const;

  /**
    When the member is joining, cycle through all members on group and see if it
    is compatible with them.

    @return the compatibility with the group
      @retval INCOMPATIBLE      //Versions not compatible
      @retval COMPATIBLE        //Versions compatible
      @retval READ_COMPATIBLE   //Member can read but not write
   */
  st_compatibility_types check_version_compatibility_with_group() const;

  /**
   Method that compares the group's aggregated GTID set against the joiner
   GTID set. These sets contain executed and received GTIDs present
   in the relay log files belonging to each member plugin applier channel.

   @return if the joiner has more GTIDs then the group.
     @retval 0     Joiner has less GTIDs than the group
     @retval >0    Joiner has more GTIDS than the group
     @retval <0    Error when processing GTID information
 */
  int compare_member_transaction_sets() const;

  /**
    This method takes all the group executed sets and adds those belonging to
    non recovering member to the view change packet

    @param[in]  view_packet         the view change packet
  */
  void collect_members_executed_sets(View_change_packet *view_packet) const;

  /**
    Method that compares the member options with
    the value of the same option on all other members.
    It compares:
      1) GTID assignment block size
      2) Write set hash algorithm

    @return
      @retval 0     Joiner has the same value as all other members
      @retval !=0   Otherwise
  */
  int compare_member_option_compatibility() const;

  /**
    Check if a member is not entering a group where an action is running

    @return
      @retval false     no group action is running
      @retval true   a group action is running
  */
  bool is_group_running_a_configuration_change() const;

  /**
    This method submits a request to leave the group
  */
  void leave_group_on_error() const;

  /**
    This method checks if member was expelled from the group due
    to network failures.

    @param[in]  view        the view delivered by the GCS

    @return
        @retval true   the member was expelled
        @retval false  otherwise
  */
  bool was_member_expelled_from_group(const Gcs_view &view) const;

  /**
    Logs member joining message to error logs from view.

    @param[in]  new_view        the view delivered by the GCS
  */
  void log_members_joining_message(const Gcs_view &new_view) const;

  /**
    Logs member leaving message to error logs from view.

    @param[in]  new_view        the view delivered by the GCS
  */
  void log_members_leaving_message(const Gcs_view &new_view) const;

  /**
    This function return all members present in vector of Gcs_member_identifier
    in HOST:PORT format separated by comma.
    Function also return PRIMARY member if any in HOST:PORT format.

    @param[in]    members      joining or leaving members for this view
    @param[out]   all_hosts    host and port of all members from view
    @param[out]   primary_host primary member hosts and port of all members from
    view
  */
  void get_hosts_from_view(const std::vector<Gcs_member_identifier> &members,
                           std::string &all_hosts,
                           std::string &primary_host) const;

  Applier_module_interface *applier_module;
  Recovery_module *recovery_module;

  /*
    Holds, until view can be installed, all Member information received from
    other members
  */
  std::set<Group_member_info *, Group_member_info_pointer_comparator>
      *temporary_states;

  Plugin_gcs_view_modification_notifier *view_change_notifier;

  Compatibility_module *compatibility_manager;

  /**The status of this member when it joins*/
  st_compatibility_types *joiner_compatibility_status;

  /* Component stop timeout on shutdown */
  ulong stop_wait_timeout;

#ifndef DBUG_OFF
  bool set_number_of_members_on_view_changed_to_10;
#endif

  /** The notification context for the GCS delivery thread. */
  mutable Notification_context m_notification_ctx;
};

#endif /* GCS_EVENT_HANDLERS_INCLUDE */
