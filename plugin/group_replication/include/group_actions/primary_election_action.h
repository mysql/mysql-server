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

#ifndef PRIMARY_ELECTION_INCLUDED
#define PRIMARY_ELECTION_INCLUDED

#include "plugin/group_replication/include/group_actions/group_action.h"
#include "plugin/group_replication/include/group_actions/group_actions_transaction_controller.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_validation_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"

/**
  @class Primary_election_action
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

  /** Enum for the phases on the primary action */
  enum enum_primary_election_status {
    PRIMARY_ELECTION_INIT = 0,          // Initiated
    PRIMARY_ELECTION_END_ELECTION = 1,  // End the election
    PRIMARY_ELECTION_END_ERROR = 2,     // Error in election
  };

  /**
    Create a new primary election action
  */
  Primary_election_action();

  /**
    Create a new primary election action with a given uuid
    @param primary_uuid the primary uuid to elect, can be empty
    @param thread_id the local thread id that is invoking this action
    @param transaction_wait_timeout The number of seconds to wait before setting
    the THD::KILL_CONNECTION flag for the transactions that did not reach commit
    stage.
  */
  Primary_election_action(std::string primary_uuid, my_thread_id thread_id,
                          int32 transaction_wait_timeout = -1);

  ~Primary_election_action() override;

  /*
    Get the message with parameters to this action
    @param message  [out] the message to start the action
  */
  void get_action_message(Group_action_message **message) override;

  /*
    Get the message with parameters to this action
    @param message the message to start the action
    @param message_origin the invoker address
  */
  int process_action_message(Group_action_message &message,
                             const std::string &message_origin) override;

  /**
    Execute the action
    @param invoking_member is the member that invoked it
    @param stage_handler the stage handler to report progress

    @returns the execution result
  */
  Group_action::enum_action_execution_result execute_action(
      bool invoking_member, Plugin_stage_monitor_handler *stage_handler,
      Notification_context *) override;

  /*
    Terminate the executing configuration operation
    @param killed are we killing the action.

    @return true if a problem was found when stopping the action.
  */
  bool stop_action_execution(bool killed) override;

  /**
    Gets the info about execution, be it success or failure
    @return the execution diagnostics object that was the message and its level
  */
  Group_action_diagnostics *get_execution_info() override;

  /**
    For this action, what is the PSI key for the last stage when the action is
    terminating.
    @return The stage key for this class
  */
  PSI_stage_key get_action_stage_termination_key() override;

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
   @param error_message details of error
  */
  void log_result_execution(bool error, bool aborted, bool mode_changed,
                            std::string &error_message);

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

  /**
   Stop the transaction_monitor_thread if running.

   @return status
   @retval true failed to stop the thread
   @retval false thread stopped successfully.
  */
  bool stop_transaction_monitor_thread();

  /** Is this an primary change or mode change*/
  enum_action_execution_mode action_execution_mode;

  /**
    Changes the phase where the action is currently
    @param phase the new election primary execution phase
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
  /** primary election status*/
  enum_primary_election_status m_execution_status{PRIMARY_ELECTION_INIT};
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

  /**
    The number of seconds to wait before setting the THD::KILL_CONNECTION flag
    for the transactions that did not reach commit stage. Client connection is
    dropped.
  */
  int32 m_transaction_wait_timeout = {-1};
  /**
    Used to monitor transactions, this stops the new transactions and sets the
    THD::KILL_CONNECTION flag for the transactions that did not reach commit
    stage post timeout expire. Client connection is dropped.
  */
  Transaction_monitor_thread *transaction_monitor_thread{nullptr};
};

#endif /* PRIMARY_ELECTION_INCLUDED */
