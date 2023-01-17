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

#ifndef GROUP_ACTION_INCLUDED
#define GROUP_ACTION_INCLUDED

#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/services/notification/notification.h"

/**
  @class Group_action_diagnostics
  The parent class for group wide operations
*/
class Group_action_diagnostics {
 public:
  enum enum_action_result_level {
    GROUP_ACTION_LOG_INFO = 0,     // Info message on termination
    GROUP_ACTION_LOG_WARNING = 1,  // Warning on termination
    GROUP_ACTION_LOG_ERROR = 2,    // Error message on error situations
    GROUP_ACTION_LOG_END = 3       // Enum end
  };

  /**
   Constructor
  */
  Group_action_diagnostics();

  /**
   Set the message for this operation
   @param diagnostics another diagnostics object
  */
  void set_execution_info(Group_action_diagnostics *diagnostics);

  /**
   Set the result level for the message for this operation
   @param level the level of the return message
  */
  void set_execution_message_level(enum_action_result_level level);

  /**
   Set the message for this operation
   @param level if it is an information, warning or error message
   @param message the message to the user
  */
  void set_execution_message(enum_action_result_level level,
                             std::string &message);

  /**
   Set the message for this operation
   @param level if it is an information, warning or error message
   @param message the message to the user
  */
  void set_execution_message(enum_action_result_level level,
                             const char *message);

  /**
   Appends the given message to the execution message for this operation
   @param message the message to append
*/
  void append_execution_message(const char *message);

  /**
   Appends the given message to the execution message for this operation
   @param message the message to append
  */
  void append_execution_message(std::string &message);

  /**
   Set the warning message for this operation
   @param warning_msg the message to the user
  */
  void set_warning_message(const char *warning_msg);

  /**
   Append to the warning message for this operation
   @param warning_msg the message to the user
  */
  void append_warning_message(const char *warning_msg);

  /**
    @return the message to be shown to the user
  */
  std::string &get_execution_message();

  /**
   @return the warning to be shown to the user
  */
  std::string &get_warning_message();

  /**
   @return the message level be shown to the user
  */
  enum_action_result_level get_execution_message_level();

  /**
    return The test has logged warnings?
  */
  bool has_warning();

  /**
   Removes the old log messages and level information
  */
  void clear_info();

 private:
  /** Simply log, are there warnings, or should we report an error*/
  enum_action_result_level message_level;
  /** The log execution message: success or failure*/
  std::string log_message;
  /** The warning message*/
  std::string warning_message;
};

/**
  @class Group_action
  The parent class for group wide operations
*/
class Group_action {
 public:
  /** Enum for the end results of a action execution */
  enum enum_action_execution_result {
    GROUP_ACTION_RESULT_TERMINATED = 0,  // Terminated with success
    GROUP_ACTION_RESULT_ERROR = 1,       // Error on execution
    GROUP_ACTION_RESULT_RESTART = 2,  // Due to error the action shall restart
    GROUP_ACTION_RESULT_ABORTED = 3,  // Was aborted due to some internal check
    GROUP_ACTION_RESULT_KILLED = 4,   // Action was killed
    GROUP_ACTION_RESULT_END = 5       // Enum end
  };

  virtual ~Group_action() = 0;

  /**
    Get the message with parameters to this action
    @param[out] message the message to start the action
  */
  virtual void get_action_message(Group_action_message **message) = 0;

  /*
    Get the message with parameters to this action
    @param message the message to start the action
    @param message_origin the invoker address
  */
  virtual int process_action_message(Group_action_message &message,
                                     const std::string &message_origin) = 0;

  /**
    Execute the action
    @param invoking_member is the member that invoked it
    @param stage_handler the stage handler to report progress

    @returns the execution result
  */
  virtual enum_action_execution_result execute_action(
      bool invoking_member, Plugin_stage_monitor_handler *stage_handler,
      Notification_context *) = 0;

  /*
    Terminate the executing configuration operation
    @param killed are we killing the action.

    @return true if a problem was found when stopping the action.
  */
  virtual bool stop_action_execution(bool killed) = 0;

  /**
    Gets the info about execution, be it success or failure
    @return the execution diagnostics object that was the message and its level
  */
  virtual Group_action_diagnostics *get_execution_info() = 0;

  /**
    For this action, what is the PSI key for the last stage when the action is
    terminating.

    @note if not implemented this method will return  an invalid key that will
    make the PFS stage mechanism to never start any stage.

    @return The stage key, -1 if the method is not implemented by the class
  */
  virtual PSI_stage_key get_action_stage_termination_key();
};

#endif /* GROUP_ACTION_INCLUDED */
