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

#ifndef GROUP_ACTION_COORDINATOR_INCLUDED
#define GROUP_ACTION_COORDINATOR_INCLUDED

#include "plugin/group_replication/include/group_actions/group_action.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/include/plugin_utils.h"

class Group_action_information {
 public:
  Group_action_information();
  Group_action_information(bool is_local, Group_action *current_proposed_action,
                           Group_action_diagnostics *execution_message_area);
  ~Group_action_information();

  /* Was it proposed locally*/
  bool is_local;
  /* The proposed action */
  Group_action *executing_action;
  /* The diagnostics area associated to the action */
  Group_action_diagnostics *execution_message_area;
  /* What is the action return value */
  Group_action::enum_action_execution_result action_result;
};

/**
  @Group_action_coordinator
  The coordinator class where group actions are submitted
*/
class Group_action_coordinator : public Group_event_observer {
 public:
  /** Enum for the flags used in the return field of the end message */
  enum enum_end_action_message_return_flags {
    END_ACTION_MESSAGE_UNKNOWN_FLAG = 0,  // This type should not be used
    END_ACTION_MESSAGE_WARNING_FLAG = 1,  // The message has a warning
    END_ACTION_MESSAGE_FLAG_END = 2,      // The enum end
  };

  /** The group coordinator constructor  */
  Group_action_coordinator();

  /** The group coordinator destructor  */
  ~Group_action_coordinator();

  /**
   Register the coordinator observers
   @note Calling this method in constructors/destructors can lead to errors
   as virtual methods are called upon an "incomplete" object.
  */
  void register_coordinator_observers();

  /**
   Unregister the coordinator observers
   @note Calling this method in constructors/destructors can lead to errors
   as virtual methods are called upon an "incomplete" object.
  */
  void unregister_coordinator_observers();

  /** Submit an action for execution in the coordinator
    @param action         The action instance to execute in the group
    @param execution_info The result information for this action execution
    @return !=0 if something wrong happened in the action
  */
  int coordinate_action_execution(Group_action *action,
                                  Group_action_diagnostics *execution_info);

  /**
    Asks the coordinator to stop any ongoing action
    @param coordinator_stop is the coordinator terminating
    @param wait shall it wait for the process to terminate
  */
  int stop_coordinator_process(bool coordinator_stop, bool wait = true);

  /**
    Resets flags as the coordinator can live trough multiple stops and starts
  */
  void reset_coordinator_process();

  /**
   * Handle incoming  action message (start or stop)
   * @param message The action message pointer
   * @param message_origin the message origin
   * @return true if something wrong happen, false otherwise
   */
  bool handle_action_message(Group_action_message *message,
                             const std::string &message_origin);

  /**
    Returns if there is a group action running

    @return true if an action is being executed
  */
  bool is_group_action_running();

  /**
    The main thread process for the action execution process
    @return
  */
  int execute_group_action_handler();

 private:
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
    Handle incoming start action message
    @param message The action message pointer
    @param message_origin the message origin
    @return true if something wrong happen, false otherwise
  */
  bool handle_action_start_message(Group_action_message *msg,
                                   const std::string &message_origin);

  /**
    Handle incoming end action message
    @param message The action message pointer
    @param message_origin the message origin
    @return true if something wrong happen, false otherwise
  */
  bool handle_action_stop_message(Group_action_message *msg,
                                  const std::string &message_origin);

  /**
    This method checks if there is a member in recovery in the group
    @param all_members_info the list of info objects for all members
    @return true if yes, false if no member is in recovery
  */
  bool member_in_recovery(std::vector<Group_member_info *> *all_members_info);

  /**
    This method checks if there is a member from a version that does not allow
    actions
    @param all_members_info the list of info objects for all members
    @return true if yes, false if all members are valid
  */
  bool member_from_invalid_version(
      std::vector<Group_member_info *> *all_members_info);

  /**
    Set an error message and awake the coordinator
    @param execution_info The information for the executing action
    @param error_message the message to send to the user if local
    @param is_local_executor are we aborting something proposed on this member
    @param is_action_running if the action was already running
  */
  void awake_coordinator_on_error(Group_action_information *execution_info,
                                  const char *error_message,
                                  bool is_local_executor,
                                  bool is_action_running = false);

  /**
    Awake the coordinator and report the error
    @param execution_info The information for the executing action
    @param is_local_executor are we aborting something proposed on this member
    @param is_action_running if the action was already running
  */
  void awake_coordinator_on_error(Group_action_information *execution_info,
                                  bool is_local_executor,
                                  bool is_action_running = false);

  /**
    Declare this action as terminated to other members
    @param message_type for the sent message
  */
  int signal_action_terminated();

  //
  /** Handle the termination of current action */
  void terminate_action();

  /**
   Declares the action as not running, wait or not for its finish
   @param wait wait for the execution thread to finish or not
  */
  void signal_and_wait_action_termination(bool wait);

  /**
   This method returns if the current thread was killed
   @return true if yes, false otherwise
  */
  bool thread_killed();

  /**
    Internal method that contains the logic for leaving and killing transactions
  */
  void kill_transactions_and_leave();

  /** The list of members known for the current action */
  std::list<std::string> known_members_addresses;
  /** The number of received stop messages or dead members */
  int number_of_known_members;
  /** The number of received stop messages or dead members */
  int number_of_terminated_members;

  /** The lock too coordinate start and stop requests */
  mysql_mutex_t coordinator_process_lock;

  /** The condition to wake up on error or termination */
  mysql_cond_t coordinator_process_condition;

  /** Is this member the sender and controller of this action*/
  bool is_sender;

  /** The flag to avoid concurrent action start requests */
  bool action_proposed;

  /** The flag to avoid concurrent action running */
  std::atomic<bool> action_running;

  /** The currently proposed action owner*/
  Group_action_information *proposed_action;
  /** The currently proposed action owner*/
  Group_action_information *current_executing_action;

  /** Is the local submitted action terminating */
  bool local_action_terminating;

  /** Was the local action killed */
  bool local_action_killed;

  /** Is there an error on the message after receiving the action*/
  bool action_execution_error;

  /** The flag to avoid action starts post stop */
  bool coordinator_terminating;

  /** Is this member leaving */
  bool member_leaving_group;

  /** There were remote warnings reported */
  bool remote_warnings_reported;

  /** The handler where actions can report progress through stages */
  Plugin_stage_monitor_handler monitoring_stage_handler;

  /** launch_group_action_handler_thread */
  int launch_group_action_handler_thread();

  /** The action handler thread status */
  thread_state action_handler_thd_state;

  /** If the group action execution method is being executed */
  bool is_group_action_being_executed;

  // run conditions and locks
  /** The thread handle for the group action process */
  my_thread_handle action_execution_pthd;
  /** The group action process thread lock */
  mysql_mutex_t group_thread_run_lock;
  /** The group action process thread condition */
  mysql_cond_t group_thread_run_cond;

  /** The group action process thread end lock */
  mysql_mutex_t group_thread_end_lock;
  /** The group action process thread end condition */
  mysql_cond_t group_thread_end_cond;
};

#endif /* GROUP_ACTION_COORDINATOR_INCLUDED */
