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

#ifndef PRIMARY_ELECTION_SECONDARY_PROCESS_INCLUDED
#define PRIMARY_ELECTION_SECONDARY_PROCESS_INCLUDED

#include <list>
#include <string>

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_include.h"
#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/include/plugin_utils.h"

/**
  @class Primary_election_secondary_process
  Class that contains the primary election process logic for secondary members
*/
class Primary_election_secondary_process : public Group_event_observer {
 public:
  /**
    Class constructor for secondary election process
  */
  Primary_election_secondary_process();

  /**
    Class destructor for secondary election process
  */
  ~Primary_election_secondary_process();

  /**
    Launch the local process on the secondary members for primary election

    @param election_mode the context on which election is occurring
    @param primary_to_elect the uuid of the primary to elect
    @param group_members_info the member info about group members

    @returns 0 in case of success, or 1 otherwise
  */
  int launch_secondary_election_process(
      enum_primary_election_mode election_mode, std::string &primary_to_elect,
      std::vector<Group_member_info *> *group_members_info);

  /**
    Is the election process running?
    @returns  election_process_running
  */
  bool is_election_process_running();

  /**
    Terminate the election process on shutdown
  */
  int terminate_election_process(bool wait = true);

  /*
    Internal thread execution method with the election process
  */
  int secondary_election_process_handler();

 private:
  enum enum_election_read_mode_status {
    SECONDARY_ELECTION_READ_MODE_NOT_SET = 0,    // Read only mode not set
    SECONDARY_ELECTION_READ_MODE_BEING_SET = 1,  // Read only mode being set
    SECONDARY_ELECTION_READ_MODE_IS_SET = 2,     // Read only mode is set
  };

  // The listeners for group events

  virtual int after_view_change(
      const std::vector<Gcs_member_identifier> &joining,
      const std::vector<Gcs_member_identifier> &leaving,
      const std::vector<Gcs_member_identifier> &group, bool is_leaving,
      bool *skip_election, enum_primary_election_mode *election_mode,
      std::string &suggested_primary);
  virtual int after_primary_election(std::string primary_uuid,
                                     bool primary_changed,
                                     enum_primary_election_mode election_mode,
                                     int error);
  virtual int before_message_handling(const Plugin_gcs_message &message,
                                      const std::string &message_origin,
                                      bool *skip_message);

  /**
    Enables the read mode in the server
    @return false in case of success, or true otherwise
  */
  bool enable_read_mode_on_server();

  /**
    Kills the current read mode query that might be stuck
    @return false in case of success, or true otherwise
  */
  bool kill_read_mode_query();

  /**
     Signal that the read mode is ready on this member
     @returns false in case of success, or true otherwise
  */
  bool signal_read_mode_ready();

  /** The election thread status */
  thread_state election_process_thd_state;
  /** Is the process aborted */
  bool election_process_aborted;

  /** Waiting for old primary transaction execution */
  bool waiting_on_old_primary_transactions;
  /* Is the primary ready? */
  bool primary_ready;
  /** Are all group members in read mode */
  bool group_in_read_mode;
  /** Process is waiting on read mode - stage related var*/
  bool is_waiting_on_read_mode_group;
  /** The session id that is set the read mode*/
  unsigned long read_mode_session_id;
  /** What is the status on the read only mode enabling query */
  enum_election_read_mode_status is_read_mode_set;

  /** The election invocation context */
  enum_primary_election_mode election_mode;

  /** The primary to be elected */
  std::string primary_uuid;

  /** The number of known members when the election started */
  ulong number_of_know_members;
  /** The members known for the current action */
  std::list<std::string> known_members_addresses;

  /** The stage handler for progress reporting */
  Plugin_stage_monitor_handler *stage_handler;

  // Run thread locks and conditions

  /** The thread run lock*/
  mysql_mutex_t election_lock;
  /** The thread run condition*/
  mysql_cond_t election_cond;
  /** The thread handle*/
  my_thread_handle primary_election_pthd;
};

#endif /* PRIMARY_ELECTION_SECONDARY_PROCESS_INCLUDED */
