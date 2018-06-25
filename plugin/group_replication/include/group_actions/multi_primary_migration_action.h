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

#ifndef MULTI_PRIMARY_MIGRATION_INCLUDED
#define MULTI_PRIMARY_MIGRATION_INCLUDED

#include "plugin/group_replication/include/group_actions/group_action.h"
#include "plugin/group_replication/include/pipeline_interfaces.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"

/**
  @Multi_primary_migration_action
  The group action class to do migration to multi primary mode
*/
class Multi_primary_migration_action : public Group_action,
                                       Group_event_observer {
 public:
  /**
    Create a new primary election action
  */
  Multi_primary_migration_action();

  /**
    Create a new primary election action

    @param invoking_thread_id the local thread id that is invoking this action
  */
  Multi_primary_migration_action(my_thread_id invoking_thread_id);

  ~Multi_primary_migration_action();

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
   @param aborted was the action aborted?
   @param mode_changed was the mode changed to multi primary?
  */
  void log_result_execution(bool aborted, bool mode_changed);

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

  /** The thread that invoked this action - if applicable, 0 otherwise */
  my_thread_id invoking_thread_id;

  /** Is the process aborted */
  bool multi_primary_switch_aborted;
  /** Was this action order to terminate by a kill signal*/
  bool action_killed;

  /** The current primary*/
  std::string primary_uuid;
  /** The id of the primary*/
  std::string primary_gcs_id;
  /** If the member is primary*/
  bool is_primary;
  /** Is the primary transaction back log consumed*/
  bool is_primary_transaction_queue_applied;

  /** Continuation object to wait on the applier queue consumption*/
  std::shared_ptr<Continuation> applier_checkpoint_condition;

  /**The lock for notifications*/
  mysql_mutex_t notification_lock;
  /**The condition for notifications*/
  mysql_cond_t notification_cond;

  /**Place to store result messages*/
  Group_action_diagnostics execution_message_area;
};

#endif /* MULTI_PRIMARY_MIGRATION_INCLUDED */
