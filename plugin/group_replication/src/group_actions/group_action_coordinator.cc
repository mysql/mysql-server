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

#include "plugin/group_replication/include/group_actions/group_action_coordinator.h"
#include "plugin/group_replication/include/group_actions/multi_primary_migration_action.h"
#include "plugin/group_replication/include/group_actions/primary_election_action.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"

Group_action_information::Group_action_information(
    bool is_local_arg, Group_action *action,
    Group_action_diagnostics *message_area)
    : is_local(is_local_arg),
      executing_action(action),
      execution_message_area(message_area),
      action_result(Group_action::GROUP_ACTION_RESULT_END) {}

Group_action_information::Group_action_information()
    : is_local(false),
      executing_action(nullptr),
      execution_message_area(new Group_action_diagnostics()),
      action_result(Group_action::GROUP_ACTION_RESULT_END) {}

Group_action_information::~Group_action_information() {}

/**
 The 'action' / 'action information' object life cycle:

  The action comes from the coordinate_action_execution.
  It is set as current_proposed_action and only when the start message is
  received we decide if it is accepted or not.

  If accepted, the current_executing_action is set to the proposed action.

  Some notes on current_executing_action.
  1) It is set on the handle_action_start_message.
     There is no lock, we just ensure it is set before the action_running is set
     to true.
  2) It is deleted under the coordinator_process_lock on terminate_action or
     awake_coordinator_on_error
  3) All accesses are under the execution of the said action on the thread, or
     on the coordination or stop methods, were we use the
     coordinator_process_lock

 ------------------

 Action_running the flag:

 Since it is accessed in multiple places and contexts we use an atomic value

 The base of the flag is that in GCS invoked methods it is
   set to true on handle_action_start_message (Gcs logical moment)
   set to false on terminate_action (Gcs logical moment)

 On critical action errors it can be set to false on awake_coordinator_on_error.
 This usually means the member will leave the group or the coordinator is
 stopping, meaning new actions wont be accepted or processed since the member
 left.

 There is a wait based on this variable in the execute_group_action_handler but
 for that reason we have the group_thread_end_lock.

 current_executing_action checks the variable in a optimistic to ensure a fail
 fast code path.
 It is also used to check if we should stop an action when the query is killed.
 For this we first set local_action_killed to ensure that start will fail when
 receives the action, and then we try to stop the action if it is already
 running.

 The stop_coordinator_process works on the same approach, it checks if the an
 action is running the but coordinator_terminating flag is set first, so if we
 miss the action start, it will fail due to the flag.
*/

Group_action_coordinator::Group_action_coordinator()
    : is_sender(false),
      action_proposed(false),
      action_running(false),
      proposed_action(nullptr),
      current_executing_action(nullptr),
      local_action_terminating(false),
      local_action_killed(false),
      action_execution_error(false),
      coordinator_terminating(false),
      member_leaving_group(false),
      remote_warnings_reported(false),
      action_handler_thd_state(),
      is_group_action_being_executed(false) {
  mysql_mutex_init(key_GR_LOCK_group_action_coordinator_process,
                   &coordinator_process_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_group_action_coordinator_process,
                  &coordinator_process_condition);
  mysql_mutex_init(key_GR_LOCK_group_action_coordinator_thread,
                   &group_thread_run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_group_action_coordinator_thread,
                  &group_thread_run_cond);
  mysql_mutex_init(key_GR_LOCK_group_action_coordinator_thread_end,
                   &group_thread_end_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_group_action_coordinator_thread_end,
                  &group_thread_end_cond);
}

Group_action_coordinator::~Group_action_coordinator() {
  mysql_mutex_destroy(&coordinator_process_lock);
  mysql_cond_destroy(&coordinator_process_condition);
  mysql_mutex_destroy(&group_thread_run_lock);
  mysql_cond_destroy(&group_thread_run_cond);
  mysql_mutex_destroy(&group_thread_end_lock);
  mysql_cond_destroy(&group_thread_end_cond);
}

void Group_action_coordinator::register_coordinator_observers() {
  group_events_observation_manager->register_group_event_observer(this);
}

void Group_action_coordinator::unregister_coordinator_observers() {
  group_events_observation_manager->unregister_group_event_observer(this);
}

static void *launch_handler_thread(void *arg) {
  Group_action_coordinator *handler = (Group_action_coordinator *)arg;
  handler->execute_group_action_handler();
  return 0;
}

bool Group_action_coordinator::is_group_action_running() {
  return action_running.load();
}

int send_message(Group_action_message *message) {
  enum_gcs_error msg_error = gcs_module->send_message(*message);
  if (msg_error != GCS_OK) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                 "coordination on group configuration"
                 " operation."); /* purecov: inspected */
    return 1;                    /* purecov: inspected */
  }
  return 0;
}

void Group_action_coordinator::reset_coordinator_process() {
  coordinator_terminating = false;
  action_running = false;
  local_action_killed = false;
  action_execution_error = false;
  action_proposed = false;
  member_leaving_group = false;
  remote_warnings_reported = false;
}

int Group_action_coordinator::stop_coordinator_process(bool coordinator_stop,
                                                       bool wait) {
  mysql_mutex_lock(&coordinator_process_lock);
  coordinator_terminating = coordinator_stop;

  if (action_running) {
    current_executing_action->executing_action->stop_action_execution(false);
  }
  mysql_mutex_unlock(&coordinator_process_lock);

  /**
    Either the action will end and will see the coordinator stop flag,
    or we wake it up in here
  */
  mysql_mutex_lock(&group_thread_end_lock);
  // Signal in case the thread is waiting for other members to finish
  mysql_cond_broadcast(&group_thread_end_cond);
  mysql_mutex_unlock(&group_thread_end_lock);

  if (wait) {
    mysql_mutex_lock(&group_thread_run_lock);
    // Signal in case the thread is waiting for other members to finish
    mysql_cond_broadcast(&group_thread_end_cond);
    while (action_handler_thd_state.is_thread_alive()) {
      DBUG_PRINT("sleep",
                 ("Waiting for the group action execution thread to end"));
      mysql_cond_wait(&group_thread_run_cond, &group_thread_run_lock);
    }
    mysql_mutex_unlock(&group_thread_run_lock);
  }

  return 0;
}

int Group_action_coordinator::coordinate_action_execution(
    Group_action *action, Group_action_diagnostics *execution_info) {
  mysql_mutex_lock(&coordinator_process_lock);

  int error = 0;
  Group_action_message *start_message = nullptr;
  Group_action_information *action_info = nullptr;

  if (action_proposed) {
    execution_info->set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "There is already a configuration action being proposed"
        " locally. Wait for it to finish.");
    error = 1;
    goto end;
  }

  if (action_running) {
    execution_info->set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "There is already a configuration action being executed."
        " Wait for it to finish.");
    error = 1;
    goto end;
  }

  if (coordinator_terminating) {
    execution_info->set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "The group coordination process is terminating.");
    error = 1;
    goto end;
  }

  if (primary_election_handler->is_an_election_running()) {
    execution_info->set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "A primary election is occurring in the group. Wait for it to end.");
    error = 1;
    goto end;
  }

  action_proposed = true;

  /* Concurrency notes on this flag:
   * Only one action can be submitted locally at one time.
   * We set it false here as new action was submitted so no other exists locally
   * This flag only goes to true when something locally submitted fails
   */
  action_execution_error = false;

  local_action_killed = false;

  action_info = new Group_action_information(true, action, execution_info);
  proposed_action = action_info;

  action->get_action_message(&start_message);
  start_message->set_group_action_message_phase(
      Group_action_message::ACTION_START_PHASE);

  if (send_message(start_message)) {
    /* purecov: begin inspected */
    execution_info->set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "There was a problem sending the configuration action"
        " proposal to the group. Check the plugin status.");
    action_proposed = false;
    delete start_message;
    error = 2;
    goto end;
    /* purecov: end */
  }

  delete start_message;

  struct timespec abstime;
  set_timespec(&abstime, 1);

  while (!local_action_terminating && !action_execution_error &&
         !thread_killed()) {
    mysql_cond_timedwait(&coordinator_process_condition,
                         &coordinator_process_lock, &abstime);
  }

  if (thread_killed()) {
    local_action_killed = true;
    // If it is not the local one running the method wont do nothing
    if (action_running) {
      action->stop_action_execution(true);
    }
    while (!local_action_terminating && !action_execution_error) {
      mysql_cond_wait(&coordinator_process_condition,
                      &coordinator_process_lock);
    }

    if (Group_action::GROUP_ACTION_RESULT_KILLED !=
            action_info->action_result &&
        Group_action::GROUP_ACTION_RESULT_ERROR != action_info->action_result &&
        !action_execution_error) {
      execution_info->append_execution_message(
          " Despite being killed, the operation was still completed.");
    }
  }

  if (action_execution_error &&
      Group_action::GROUP_ACTION_RESULT_ABORTED == action_info->action_result &&
      member_leaving_group) {
    // Had a little more context to the message.
    std::string exec_message = execution_info->get_execution_message();
    execution_info->set_execution_message(
        execution_info->get_execution_message_level(),
        "Member has left the group. ");
    execution_info->append_execution_message(exec_message);
  }

  if (!action_execution_error && !local_action_killed &&
      remote_warnings_reported) {
    if (execution_info->has_warning())
      execution_info->append_warning_message(
          " There were warnings detected also on other members, check their "
          "logs.");
    else
      execution_info->append_warning_message(
          " There were warnings detected on other members, check their logs.");
  }

  action_proposed = false;
  local_action_terminating = false;

end:
  // Don't unset a proposed action for another thread.
  if (error > 1) proposed_action = nullptr;

  delete action_info;

  if (!error) error = action_execution_error;

  mysql_mutex_unlock(&coordinator_process_lock);
  return error;
}

bool Group_action_coordinator::thread_killed() {
  return current_thd != NULL && current_thd->is_killed();
}

bool Group_action_coordinator::handle_action_message(
    Group_action_message *message, const std::string &message_origin) {
  // If we are not online just ignore it
  Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if (member_status != Group_member_info::MEMBER_ONLINE) return 0;

  if (coordinator_terminating) {
    return 0; /* purecov: inspected */
  }

  Group_action_message::enum_action_message_phase message_phase =
      message->get_group_action_message_phase();

  switch (message_phase) {
    case Group_action_message::ACTION_START_PHASE:
      handle_action_start_message(message, message_origin);
      break;
    case Group_action_message::ACTION_END_PHASE:
      handle_action_stop_message(message, message_origin);
      break;
    case Group_action_message::ACTION_ABORT_PHASE:
      break; /* purecov: inspected */
    default:
      break; /* purecov: inspected */
  }

  return 0;
}

bool Group_action_coordinator::handle_action_start_message(
    Group_action_message *message, const std::string &message_origin) {
  std::vector<Group_member_info *> *all_members_info = nullptr;
  int error = 0;
  bool is_message_sender = !message_origin.compare(
      local_member_info->get_gcs_member_id().get_member_id());

  Group_action_information *action_info = nullptr;
  if (!is_message_sender) {
    action_info = new Group_action_information();
  } else {
    action_info = proposed_action;
  }

  if (action_running) {
    // This message holds no meaning for non invoker members so suppress it
    if (is_message_sender) {
      awake_coordinator_on_error(action_info,
                                 "There is already a configuration action being"
                                 " executed. Wait for it to finish.",
                                 is_message_sender, false);
    } else {
      awake_coordinator_on_error(action_info, is_message_sender, false);
    }
    error = 1;
    goto end;
  }

  all_members_info = group_member_mgr->get_all_members();

  if (member_in_recovery(all_members_info)) {
    awake_coordinator_on_error(action_info,
                               "A member is joining the group, "
                               "wait for it to be ONLINE.",
                               is_message_sender, false);
    error = 1;
    goto end;
  }

  if (member_from_invalid_version(all_members_info)) {
    awake_coordinator_on_error(
        action_info,
        "The group has a member with a version that does not support group "
        "coordinated operations.",
        is_message_sender, false);
    error = 1;
    goto end;
  }

  if (primary_election_handler->is_an_election_running()) {
    awake_coordinator_on_error(
        action_info,
        "A primary election is occurring in the group. Wait for it to end.",
        is_message_sender, false);
    error = 1;
    goto end;
  }

  remote_warnings_reported = false;
  known_members_addresses.clear();
  number_of_known_members = 0;
  number_of_terminated_members = 0;
  for (Group_member_info *member : *all_members_info) {
    number_of_known_members++;
    known_members_addresses.push_back(
        member->get_gcs_member_id().get_member_id());
  }

  is_sender = is_message_sender;

  if (!is_sender) {
    Group_action_message::enum_action_message_type message_type =
        message->get_group_action_message_type();
    if (message_type == Group_action_message::ACTION_MULTI_PRIMARY_MESSAGE)
      action_info->executing_action = new Multi_primary_migration_action();
    else if (message_type ==
             Group_action_message::ACTION_PRIMARY_ELECTION_MESSAGE)
      action_info->executing_action = new Primary_election_action();
  }

  current_executing_action = action_info;
  // Only the sender can invalidate the proposed action
  if (is_message_sender) proposed_action = nullptr;
  action_running = true;

  /**
   Double check for termination /killing request  in case the code sees
   action_running as being false
  */
  if (coordinator_terminating) {
    bool is_message_sender = !message_origin.compare(
        local_member_info->get_gcs_member_id().get_member_id());

    awake_coordinator_on_error(action_info,
                               "The group coordination process is terminating.",
                               is_message_sender, true);
    error = 1;
    goto end;
  }

  if (is_sender && local_action_killed) {
    awake_coordinator_on_error(action_info,
                               "This group configuration operation was killed.",
                               is_message_sender, true);
    error = 1;
    goto end;
  }

  if (action_info->executing_action->process_action_message(*message,
                                                            message_origin)) {
    /* purecov: begin inspected */
    std::string error_code("Error processing configuration start message: ");
    error_code.append(action_info->executing_action->get_execution_info()
                          ->get_execution_message());
    action_running = false;
    awake_coordinator_on_error(action_info, error_code.c_str(),
                               is_message_sender, true);
    error = 1;
    goto end;
    /* purecov: end */
  }

  launch_group_action_handler_thread();

end:

  if (all_members_info) {
    for (Group_member_info *member : *all_members_info) {
      delete member;
    }
    delete all_members_info;
  }

  return error;
}

bool Group_action_coordinator::handle_action_stop_message(
    Group_action_message *message, const std::string &origin) {
  known_members_addresses.remove(origin);
  number_of_terminated_members++;

  // Check if remote members also reported some errors.
  bool is_message_sender =
      !origin.compare(local_member_info->get_gcs_member_id().get_member_id());
  if (!is_message_sender && !remote_warnings_reported) {
    remote_warnings_reported =
        message->get_return_value() == END_ACTION_MESSAGE_WARNING_FLAG;
  }

  // if the action is running, don't report progress on another stage
  if (!is_group_action_being_executed)
    monitoring_stage_handler.set_completed_work(number_of_terminated_members);

  if (known_members_addresses.empty()) {
    terminate_action();
  }

  return false;
}

bool Group_action_coordinator::member_in_recovery(
    std::vector<Group_member_info *> *all_members_info) {
  bool member_in_recovery = false;
  for (Group_member_info *member : *all_members_info) {
    if (member->get_recovery_status() ==
        Group_member_info::MEMBER_IN_RECOVERY) {
      member_in_recovery = true;
      break;
    }
  }
  return member_in_recovery;
}

bool Group_action_coordinator::member_from_invalid_version(
    std::vector<Group_member_info *> *all_members_info) {
  bool member_from_invalid_version = false;
  for (Group_member_info *member : *all_members_info) {
    if (member->get_member_version().get_version() <
        PRIMARY_ELECTION_LEGACY_ALGORITHM_VERSION) {
      member_from_invalid_version = true;
      break;
    }
  }
  return member_from_invalid_version;
}

void Group_action_coordinator::terminate_action() {
  mysql_mutex_lock(&coordinator_process_lock);

  signal_and_wait_action_termination(true);

  DBUG_ASSERT(current_executing_action);

  // Log what was the result of the action
  LogPluginErr(
      INFORMATION_LEVEL, ER_GRP_RPL_CONFIGURATION_ACTION_LOCAL_TERMINATION,
      current_executing_action->executing_action->get_action_name(),
      current_executing_action->execution_message_area->get_execution_message()
          .c_str());

  // If there is no one listening just delete it here
  if (!current_executing_action->is_local) {
    delete current_executing_action->executing_action;
    delete current_executing_action->execution_message_area;
    delete current_executing_action;
  }

  if (is_sender) {
    if (current_executing_action->action_result ==
        Group_action::GROUP_ACTION_RESULT_ABORTED) {
      action_execution_error = true;
    }
    local_action_terminating = true;
    mysql_cond_broadcast(&coordinator_process_condition);
  }

  mysql_mutex_unlock(&coordinator_process_lock);
}

void Group_action_coordinator::signal_and_wait_action_termination(bool wait) {
  mysql_mutex_lock(&group_thread_run_lock);
  mysql_mutex_lock(&group_thread_end_lock);
  action_running = false;
  // Signal in case the thread is waiting for other members to finish
  mysql_cond_broadcast(&group_thread_end_cond);
  mysql_mutex_unlock(&group_thread_end_lock);
  if (wait) {
    while (action_handler_thd_state.is_thread_alive()) {
      DBUG_PRINT("sleep",
                 ("Waiting for the group action execution thread to end"));
      mysql_cond_wait(&group_thread_run_cond, &group_thread_run_lock);
    }
  }
  mysql_mutex_unlock(&group_thread_run_lock);
}

void Group_action_coordinator::awake_coordinator_on_error(
    Group_action_information *execution_info, const char *error_msg,
    bool is_local_executor, bool is_action_running) {
  execution_info->execution_message_area->set_execution_message(
      Group_action_diagnostics::GROUP_ACTION_LOG_ERROR, error_msg);
  awake_coordinator_on_error(execution_info, is_local_executor,
                             is_action_running);
}

void Group_action_coordinator::awake_coordinator_on_error(
    Group_action_information *execution_info, bool is_local_executor,
    bool is_action_running) {
  mysql_mutex_lock(&coordinator_process_lock);

  if (!is_local_executor) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CONFIGURATION_ACTION_ERROR,
                 execution_info->execution_message_area->get_execution_message()
                     .c_str());
    // Delete actions and their info if there is no query to awake.
    delete execution_info->executing_action;
    delete execution_info->execution_message_area;
    delete execution_info;
  }

  if (is_local_executor) {
    action_execution_error = true;
  }

  if (is_action_running) {
    mysql_mutex_lock(&group_thread_end_lock);
    action_running = false;
    mysql_cond_broadcast(&group_thread_end_cond);
    mysql_mutex_unlock(&group_thread_end_lock);

    if (is_local_executor) {
      local_action_terminating = true;
    }
  }

  if (is_local_executor) {
    mysql_cond_broadcast(&coordinator_process_condition);
  }

  mysql_mutex_unlock(&coordinator_process_lock);
}

int Group_action_coordinator::signal_action_terminated() {
  monitoring_stage_handler.set_stage(current_executing_action->executing_action
                                         ->get_action_stage_termination_key(),
                                     __FILE__, __LINE__,
                                     number_of_known_members,
                                     number_of_terminated_members);

  DBUG_EXECUTE_IF("group_replication_block_group_action_stop", {
    const char act[] = "now wait_for signal.action_stop_continue";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  Group_action_message *end_message = NULL;
  current_executing_action->executing_action->get_action_message(&end_message);
  end_message->set_group_action_message_phase(
      Group_action_message::ACTION_END_PHASE);
  if (current_executing_action->execution_message_area->has_warning()) {
    end_message->set_return_value(END_ACTION_MESSAGE_WARNING_FLAG);
  }
  if (send_message(end_message)) {
    /* purecov: begin inspected */
    current_executing_action->execution_message_area->set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "There was a problem sending the configuration operation"
        " end message to the group. Check the plugin status.");
    delete end_message;
    return 1;
    /* purecov: end */
  }
  delete end_message;
  return 0;
}

int Group_action_coordinator::launch_group_action_handler_thread() {
  DBUG_ENTER("Group_action_coordinator::launch_group_action_handler_thread()");

  mysql_mutex_lock(&group_thread_run_lock);

  if (action_handler_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&group_thread_run_lock); /* purecov: inspected */
    DBUG_RETURN(0);                             /* purecov: inspected */
  }

  if (mysql_thread_create(key_GR_THD_group_action_coordinator,
                          &action_execution_pthd, get_connection_attrib(),
                          launch_handler_thread, (void *)this)) {
    mysql_mutex_unlock(&group_thread_run_lock); /* purecov: inspected */
    DBUG_RETURN(1);                             /* purecov: inspected */
  }
  action_handler_thd_state.set_created();

  while (action_handler_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep",
               ("Waiting for the group action execution thread to start"));
    mysql_cond_wait(&group_thread_run_cond, &group_thread_run_lock);
  }
  mysql_mutex_unlock(&group_thread_run_lock);

  DBUG_RETURN(0);
}

int Group_action_coordinator::execute_group_action_handler() {
  DBUG_ENTER("Group_action_coordinator::execute_group_action_handler()");
  int error = 0;

  THD *thd = NULL;
  thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();
  global_thd_manager_add_thd(thd);

  mysql_mutex_lock(&group_thread_run_lock);
  action_handler_thd_state.set_running();
  mysql_cond_broadcast(&group_thread_run_cond);
  mysql_mutex_unlock(&group_thread_run_lock);

  DBUG_EXECUTE_IF("group_replication_block_group_action", {
    const char act[] = "now wait_for signal.action_continue";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  current_executing_action->action_result =
      Group_action::GROUP_ACTION_RESULT_RESTART;

  monitoring_stage_handler.initialize_stage_monitor();
  is_group_action_being_executed = true;

  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_CONFIGURATION_ACTION_START,
               current_executing_action->executing_action->get_action_name());
  while (Group_action::GROUP_ACTION_RESULT_RESTART ==
         current_executing_action->action_result) {
    current_executing_action->action_result =
        current_executing_action->executing_action->execute_action(
            is_sender, &monitoring_stage_handler);
  }
  is_group_action_being_executed = false;
  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_CONFIGURATION_ACTION_END,
               current_executing_action->executing_action->get_action_name());

  current_executing_action->execution_message_area->set_execution_info(
      current_executing_action->executing_action->get_execution_info());
  error = current_executing_action->action_result;

  switch (current_executing_action->action_result) {
    case Group_action::GROUP_ACTION_RESULT_TERMINATED:
      signal_action_terminated();
      break;
    case Group_action::GROUP_ACTION_RESULT_KILLED:
    case Group_action::GROUP_ACTION_RESULT_ERROR:
      kill_transactions_and_leave();
      awake_coordinator_on_error(current_executing_action, is_sender, true);
      break;
    case Group_action::GROUP_ACTION_RESULT_ABORTED:
      if (!coordinator_terminating) {
        signal_action_terminated();
      } else {
        awake_coordinator_on_error(current_executing_action, is_sender, true);
      }
      break;
    default:
      awake_coordinator_on_error(current_executing_action, is_sender,
                                 true); /* purecov: inspected */
      DBUG_ASSERT(0);                   /* purecov: inspected */
      break;                            /* purecov: inspected */
  }

  mysql_mutex_lock(&group_thread_end_lock);
  while (action_running && !coordinator_terminating) {
    DBUG_PRINT("sleep",
               ("Waiting for the group action execution process to terminate"));
    mysql_cond_wait(&group_thread_end_cond, &group_thread_end_lock);
  }
  mysql_mutex_unlock(&group_thread_end_lock);

  /**
   Did the action terminate locally but not remotely?
   Just add a warning and return.
  */
  if (coordinator_terminating) {
    if (action_running) {
      if (current_executing_action->execution_message_area
              ->get_execution_message_level() <
          Group_action_diagnostics::GROUP_ACTION_LOG_WARNING) {
        current_executing_action->execution_message_area
            ->set_execution_message_level(
                Group_action_diagnostics::GROUP_ACTION_LOG_WARNING);
      }
      current_executing_action->execution_message_area
          ->append_execution_message(
              " This information is local, the operation coordinator is "
              "terminating and no status is known about other members "
              "termination.");
      current_executing_action->execution_message_area->append_warning_message(
          " The operation coordinator is terminating and no status is known "
          "about the termination of this operation on other members.");
      awake_coordinator_on_error(current_executing_action, is_sender, true);
    }
  }

  monitoring_stage_handler.terminate_stage_monitor();

  thd->release_resources();
  global_thd_manager_remove_thd(thd);

  mysql_mutex_lock(&group_thread_run_lock);
  action_handler_thd_state.set_terminated();
  mysql_cond_broadcast(&group_thread_run_cond);
  mysql_mutex_unlock(&group_thread_run_lock);

  Gcs_interface_factory::cleanup_thread_communication_resources(
      Gcs_operations::get_gcs_engine());

  my_thread_end();
  delete thd;

  DBUG_RETURN(error);
}

int Group_action_coordinator::after_view_change(
    const std::vector<Gcs_member_identifier> &,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &, bool is_leaving,
    bool *skip_election, enum_primary_election_mode *, std::string &) {
  *skip_election = false;

  if (is_leaving) {
    member_leaving_group = true;
    stop_coordinator_process(true, false);
    return 0;
  }

  if (!is_group_action_running()) return 0;

  for (Gcs_member_identifier leaving_member : leaving) {
    bool found =
        (std::find(
             known_members_addresses.begin(), known_members_addresses.end(),
             leaving_member.get_member_id()) != known_members_addresses.end());
    if (found) {
      number_of_terminated_members++;
      known_members_addresses.remove(leaving_member.get_member_id());
    }
  }

  // if the action is running, don't report progress on another stage
  if (!is_group_action_being_executed)
    monitoring_stage_handler.set_completed_work(number_of_terminated_members);

  if (known_members_addresses.empty()) {
    terminate_action();
  }

  return 0;
}

int Group_action_coordinator::after_primary_election(std::string, bool,
                                                     enum_primary_election_mode,
                                                     int) {
  return 0;
}

int Group_action_coordinator::before_message_handling(
    const Plugin_gcs_message &, const std::string &, bool *) {
  return 0;
}

void Group_action_coordinator::kill_transactions_and_leave() {
  DBUG_ENTER("Group_action_coordinator::kill_transactions_and_leave");

  Notification_context ctx;

  LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CONFIGURATION_ACTION_KILLED_ERROR);
  if (exit_state_action_var != EXIT_STATE_ACTION_ABORT_SERVER) {
    current_executing_action->execution_message_area->append_execution_message(
        " The member will now leave the group.");
  }

  /*
    Suspend the applier for the uncommon case of a network restore happening
    when this termination process is ongoing.
    Don't care if an error is returned because the applier failed.
  */
  applier_module->add_suspension_packet();

  /* Notify member status update. */
  group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                         Group_member_info::MEMBER_ERROR, ctx);

  /*
    unblock threads waiting for the member to become ONLINE
  */
  terminate_wait_on_start_process();

  /* Single state update. Notify right away. */
  notify_and_reset_ctx(ctx);

  bool set_read_mode = false;
  if (view_change_notifier != NULL &&
      !view_change_notifier->is_view_modification_ongoing()) {
    view_change_notifier->start_view_modification();
  }
  Gcs_operations::enum_leave_state state = gcs_module->leave();

  longlong errcode = 0;
  longlong log_severity = WARNING_LEVEL;
  switch (state) {
    case Gcs_operations::ERROR_WHEN_LEAVING:
      errcode = ER_GRP_RPL_FAILED_TO_CONFIRM_IF_SERVER_LEFT_GRP; /* purecov:
                                                                    inspected */
      log_severity = ERROR_LEVEL; /* purecov: inspected */
      set_read_mode = true;       /* purecov: inspected */
      break;                      /* purecov: inspected */
    case Gcs_operations::ALREADY_LEAVING:
      errcode = ER_GRP_RPL_SERVER_IS_ALREADY_LEAVING; /* purecov: inspected */
      break;                                          /* purecov: inspected */
    case Gcs_operations::ALREADY_LEFT:
      errcode = ER_GRP_RPL_SERVER_ALREADY_LEFT; /* purecov: inspected */
      break;                                    /* purecov: inspected */
    case Gcs_operations::NOW_LEAVING:
      set_read_mode = true;
      errcode = ER_GRP_RPL_SERVER_SET_TO_READ_ONLY_DUE_TO_ERRORS;
      log_severity = ERROR_LEVEL;
      break;
  }
  LogPluginErr(log_severity, errcode);

  /*
    If true it means:
    1) The plugin is stopping and waiting on some transactions to finish.
       No harm in unblocking them first cutting the stop command time
    2) There was an error in the applier and the plugin will leave the group.
       No problem, both processes will try to kill the transactions and set the
       read mode to true.
  */
  bool already_locked = shared_plugin_stop_lock->try_grab_write_lock();

  // kill pending transactions
  blocked_transaction_handler->unblock_waiting_transactions();

  if (!already_locked) shared_plugin_stop_lock->release_write_lock();

  if (set_read_mode) enable_server_read_mode(PSESSION_INIT_THREAD);

  if (view_change_notifier != NULL) {
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_WAITING_FOR_VIEW_UPDATE);
    if (view_change_notifier->wait_for_view_modification()) {
      LogPluginErr(
          WARNING_LEVEL,
          ER_GRP_RPL_TIMEOUT_RECEIVING_VIEW_CHANGE_ON_SHUTDOWN); /* purecov:
                                                                    inspected */
    }
  }

  if (exit_state_action_var == EXIT_STATE_ACTION_ABORT_SERVER) {
    std::string error_message(
        "Fatal error during a Group Replication configuration change. ");
    error_message.append(current_executing_action->execution_message_area
                             ->get_execution_message());
    abort_plugin_process(error_message.c_str());
  }

  DBUG_VOID_RETURN;
}
