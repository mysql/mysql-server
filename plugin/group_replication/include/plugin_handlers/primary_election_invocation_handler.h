/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef PRIMARY_ELECTION_INVOCATION_HANDLER_INCLUDED
#define PRIMARY_ELECTION_INVOCATION_HANDLER_INCLUDED

#include <string>
#include <vector>

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_include.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_primary_process.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_secondary_process.h"
#include "plugin/group_replication/include/plugin_messages/single_primary_message.h"
#include "plugin/group_replication/include/plugin_observers/group_transaction_observation_manager.h"

/**
  @class Primary_election_handler
  The base class to request and execute an election
*/
class Primary_election_handler {
 public:
  /**
    Instantiate a new election handler
    @param[in] components_stop_timeout  the timeout when waiting on shutdown
  */
  Primary_election_handler(ulong components_stop_timeout);

  /** Class destructor */
  ~Primary_election_handler();

  /**
    Send a message to all members requesting an election

    @param primary_uuid  the primary member to elect
    @param mode          the election mode to use
  */
  int request_group_primary_election(std::string primary_uuid,
                                     enum_primary_election_mode mode);

  /**
    Handle new received primary message of type SINGLE_PRIMARY_PRIMARY_ELECTION
    @param message The received primary message
    @param notification_ctx the notification object to report changes
    @return !=0 in case of error
  */
  int handle_primary_election_message(Single_primary_message *message,
                                      Notification_context *notification_ctx);

  /**
    Execute the primary member selection if needed and the election algorithm
    invocation.

    @param primary_uuid  the primary member to elect
    @param mode          the election mode to use
    @param notification_ctx the notification object to report changes

    @return !=0 in case of error
  */
  int execute_primary_election(std::string &primary_uuid,
                               enum_primary_election_mode mode,
                               Notification_context *notification_ctx);

  /**
    Print server executed GTID and applier retrieved GTID in logs.
  */
  void print_gtid_info_in_log();

  /**
    Is an election process running?
    @return true if yes, false if no
  */
  bool is_an_election_running();

  /**
   Sets if the election process is running or not
   @param election_running is the election running or not
  */
  void set_election_running(bool election_running);

  /**
    End any running election process.
    @return !=0 in case of error
  */
  int terminate_election_process();

  // Consistency transaction manager notifiers
  /**
    Notify transaction consistency manager that election is running
  */
  void notify_election_running();

  /**
    Notify transaction consistency manager that election ended
  */
  void notify_election_end();

  /**
    Sets the component stop timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout(ulong timeout);

 private:
  /**
    Get the member to elect from all group members.
    This method returns the current primary if one exists
    If no primary exists this method returns one of the lowest version present
    in the group according to a weight or uuid criteria.

    @param[out] primary_uuid  the primary member to elect
    @param[in]  all_members_info The members currently in the group

    @return true if a primary is found, false otherwise
  */
  bool pick_primary_member(std::string &primary_uuid,
                           Group_member_info_list *all_members_info);

  /**
  Execute the standard primary election algorithm (that supports primary
  appointments)

    @param primary_uuid  the primary member to elect
    @param mode          the election mode to use
  */
  int internal_primary_election(std::string &primary_uuid,
                                enum_primary_election_mode mode);

  /**
    Execute the legacy (<8.0.12) primary election algorithm

    @param primary_uuid  the primary member to elect
  */
  int legacy_primary_election(std::string &primary_uuid);

  /** The handler to handle the election on the primary member */
  Primary_election_primary_process primary_election_handler;

  /** The handler to handle the election in the secondary members */
  Primary_election_secondary_process secondary_election_handler;

  /** Is an election running? */
  bool election_process_running;

  /** The lock for the running flag*/
  mysql_mutex_t flag_lock;
};

/**
  Sort lower version members based on member weight if member version
  is greater than equal to PRIMARY_ELECTION_MEMBER_WEIGHT_VERSION or uuid.

  @param all_members_info    the vector with members info
  @param lowest_version_end  first iterator position where members version
                             increases.
*/
void sort_members_for_election(
    Group_member_info_list *all_members_info,
    Group_member_info_list_iterator lowest_version_end);

/**
  Sort members based on member_version and get first iterator position
  where member version differs.

  @param all_members_info    the vector with members info

  @return  the first iterator position where members version increase.

  @note from the start of the list to the returned iterator, all members have
        the lowest version in the group.
 */
Group_member_info_list_iterator sort_and_get_lowest_version_member_position(
    Group_member_info_list *all_members_info);

#endif /* PRIMARY_ELECTION_INVOCATION_HANDLER_INCLUDED */
