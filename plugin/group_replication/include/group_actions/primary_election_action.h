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

#ifndef PRIMARY_ELECTION_INCLUDED
#define PRIMARY_ELECTION_INCLUDED

#include "plugin/group_replication/include/group_actions/group_action.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_validation_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"

/**
  @Primary_election_action
  The group action class to do migration to primary mode or elect a primary
*/
class Primary_election_action : public Group_action, Group_event_observer {
 public:
  /** Enum for type of primary election executed */
  enum enum_action_execution_mode {
    PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH = 0,  // Change to a new primary
    PRIMARY_ELECTION_ACTION_MODE_SWITCH = 1,  // Change to single primary mode
    PRIMARY_ELECTION_ACTION_END = 2           // Enum end
  };

  /** Enum for the phases on the primary action */
  enum enum_primary_election_phase {
    PRIMARY_NO_PHASE = 0,            // No phase yet
    PRIMARY_VALIDATION_PHASE = 1,    //  Check if primary is valid
    PRIMARY_SAFETY_CHECK_PHASE = 2,  //  Make the change safe
    PRIMARY_ELECTION_PHASE = 3,      //  Invoke primary election
    PRIMARY_ELECTED_PHASE = 4        //  Primary was elected/group in read mode
  };

  /**
    Create a new primary election action
  */
  Primary_election_action();

  /**
    Create a new primary election action with a given uuid
    @param primary_uuid the primary uuid to elect, can be empty
    @param thread_id the local thread id that is invoking this action
  */
  Primary_election_action(std::string primary_uuid, my_thread_id thread_id);

  ~Primary_election_action();

  /*
    Get the message with parameters to this action
    @param message  [out] the message to start the action
  */
  virtual void get_action_message(Group_action_message **message);

  /*
    Get the message with parameters to this action
    @param message the message to start the action
    @param message_origin the invoker address
  */
  virtual int process_action_message(Group_action_message &message,
                                     const std::string &message_origin);

  /**
    Execute the action
    @param invoking_member is the member that invoked it
    @param stage_handler the stage handler to report progress

    @returns the execution result
  */
  virtual Group_action::enum_action_execution_result execute_action(
      bool invoking_member, Plugin_stage_monitor_handler *stage_handler);

  /*
    Terminate the executing configuration operation
    @param killed are we killing the action.

    @return true if a problem was found when stopping the action.
  */
  virtual bool stop_action_execution(bool killed);

  /**
    Returns the name of the action for debug messages and such
    @return the action name
  */
  virtual const char *get_action_name();

  /**
    Gets the info about execution, be it success or failure
    @return the execution diagnostics object that was the message and its level
  */
  virtual Group_action_diagnostics *get_execution_info();

  /**
    For this action, what is the PSI key for the last stage when the action is
    terminating.
    @return The stage key for this class
  */
  virtual PSI_stage_key get_action_stage_termination_key();

 private:
  /**
    Persist the value of the variables changed in the action
    @return true if a problem was found, false otherwise
  */
  bool persist_variable_values();

  /**
   Log the result of the execution
   @param error Did an error occurred
   @param aborted was the action aborted?
   @param mode_changed was the mode changed to single primary?
  */
  void log_result_execution(bool error, bool aborted, bool mode_changed);

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

  /** Is this an primary change or mode change*/
  enum_action_execution_mode action_execution_mode;

  /**
    Changes the phase where the action is currently
    @param phase
  */
  void change_action_phase(enum_primary_election_phase phase);
  /** The current phase */
  enum_primary_election_phase current_action_phase;
  /** Lock for the phase change */
  mysql_mutex_t phase_lock;
  /** Is this action aborted */
  bool single_election_action_aborted;
  /** Was there an error in the election */
  bool error_on_primary_election;
  /** Was this action order to terminate by a kill signal*/
  bool action_killed;

  /** The primary to elect*/
  std::string appointed_primary_uuid;
  /** The id of the primary to elect*/
  std::string appointed_primary_gcs_id;
  /** The id of the invoking primary*/
  std::string invoking_member_gcs_id;
  /** The uuid of the original master at action invocation */
  std::string old_primary_uuid;
  /** If this member is old primary*/
  bool is_primary;
  /** The thread that invoked this action - if applicable, 0 otherwise */
  my_thread_id invoking_thread_id;

  /** Is the primary election invoked*/
  bool is_primary_election_invoked;
  /** Is the primary elected*/
  bool is_primary_elected;
  /** Did the primary change*/
  bool primary_changed;
  /** Is the transaction back log consumed*/
  bool is_transaction_queue_applied;

  /**The lock for notifications*/
  mysql_mutex_t notification_lock;
  /**The condition for notifications*/
  mysql_cond_t notification_cond;

  /** The handler for primary election validations */
  Primary_election_validation_handler validation_handler;

  /**Place to store result messages*/
  Group_action_diagnostics execution_message_area;
};

#endif /* PRIMARY_ELECTION_INCLUDED */
