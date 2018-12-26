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

#include "plugin/group_replication/include/plugin_handlers/primary_election_primary_process.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_utils.h"

static void *launch_handler_thread(void *arg) {
  Primary_election_primary_process *handler =
      (Primary_election_primary_process *)arg;
  handler->primary_election_process_handler();
  return 0;
}

Primary_election_primary_process::Primary_election_primary_process()
    : election_process_thd_state(),
      election_process_aborted(false),
      primary_ready(false),
      group_in_read_mode(false),
      waiting_on_queue_applied_message(false),
      election_process_ending(false),
      number_of_know_members(0) {
  mysql_mutex_init(key_GR_LOCK_primary_election_primary_process_run,
                   &election_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_primary_election_primary_process_run,
                  &election_cond);
}

Primary_election_primary_process::~Primary_election_primary_process() {
  mysql_mutex_destroy(&election_lock);
  mysql_cond_destroy(&election_cond);
}

bool Primary_election_primary_process::is_election_process_running() {
  return election_process_thd_state.is_thread_alive();
}

bool Primary_election_primary_process::is_election_process_terminating() {
  mysql_mutex_lock(&election_lock);
  bool process_ending = election_process_ending;
  mysql_mutex_unlock(&election_lock);
  return process_ending;
}

int Primary_election_primary_process::launch_primary_election_process(
    enum_primary_election_mode mode, std::string &primary_to_elect,
    std::vector<Group_member_info *> *group_members_info) {
  DBUG_ENTER(
      "Primary_election_primary_process::launch_primary_election_process");

  mysql_mutex_lock(&election_lock);

  // Callers should ensure the process is terminated
  DBUG_ASSERT(!election_process_thd_state.is_thread_alive());
  if (election_process_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&election_lock); /* purecov: inspected */
    DBUG_RETURN(2);                     /* purecov: inspected */
  }

  election_mode = mode;
  primary_uuid.assign(primary_to_elect);
  group_in_read_mode = false;
  waiting_on_queue_applied_message = false;
  election_process_ending = false;
  primary_ready = false;
  election_process_aborted = false;
  applier_checkpoint_condition.reset(new Continuation());

  known_members_addresses.clear();
  for (Group_member_info *member : *group_members_info) {
    known_members_addresses.push_back(
        member->get_gcs_member_id().get_member_id());
  }
  number_of_know_members = known_members_addresses.size();

  stage_handler = new Plugin_stage_monitor_handler();
  // If the service acquirement fails, the calls to this class have no effect
  if (stage_handler->initialize_stage_monitor()) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_NO_STAGE_SERVICE); /* purecov: inspected */
  }

  group_events_observation_manager->register_group_event_observer(this);

  if (mysql_thread_create(key_GR_THD_primary_election_primary_process,
                          &primary_election_pthd, get_connection_attrib(),
                          launch_handler_thread, (void *)this)) {
    /* purecov: begin inspected */
    group_events_observation_manager->unregister_group_event_observer(this);
    mysql_mutex_unlock(&election_lock);
    DBUG_RETURN(1);
    /* purecov: end */
  }
  election_process_thd_state.set_created();

  while (election_process_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep",
               ("Waiting for the Primary election process thread to start"));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);

  DBUG_RETURN(0);

  return 0;
}

int Primary_election_primary_process::primary_election_process_handler() {
  DBUG_ENTER(
      "Primary_election_primary_process::primary_election_process_handler");
  int error = 0;
  std::string err_msg;

  THD *thd = NULL;
  thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();
  global_thd_manager_add_thd(thd);

  mysql_mutex_lock(&election_lock);
  election_process_thd_state.set_running();
  mysql_cond_broadcast(&election_cond);
  mysql_mutex_unlock(&election_lock);

  // declared here because of goto label cross initializations
  Single_primary_message single_primary_queue_applied(
      Single_primary_message::SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE);

  Single_primary_message single_primary_message_ready(
      Single_primary_message::SINGLE_PRIMARY_PRIMARY_READY);

  if (election_mode == LEGACY_ELECTION_PRIMARY) {
    goto wait_for_local_transactions;
  }

  stage_handler->set_stage(
      info_GR_STAGE_primary_election_buffered_transactions.m_key, __FILE__,
      __LINE__, 1, 0);
  if (election_mode != SAFE_OLD_PRIMARY) {
    if (applier_module->wait_for_current_events_execution(
            applier_checkpoint_condition, &election_process_aborted, false)) {
      error = 1;
      err_msg.assign("Could not wait for the execution of local transactions.");
      goto end;
    }
  }
  stage_handler->set_completed_work(1);

  if (!election_process_aborted) {
    if (send_message(&single_primary_message_ready)) {
      error = 2; /* purecov: inspected */
      err_msg.assign(
          "Couldn't notify the group about the primary execution of its "
          "backlog."); /* purecov: inspected */
      goto end;        /* purecov: inspected */
    }
  }

  /**
    Wait for own message so we unset the read mode when the certification packet
    was already queued
  */
  mysql_mutex_lock(&election_lock);
  while (!primary_ready && !election_process_aborted) {
    DBUG_PRINT("sleep", ("Waiting for the primary member to be ready message"));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);

  if (!election_process_aborted) {
    if (disable_server_read_mode(PSESSION_USE_THREAD)) {
      LogPluginErr(
          WARNING_LEVEL,
          ER_GRP_RPL_DISABLE_READ_ONLY_FAILED); /* purecov: inspected */
    }
  }
  if (!election_process_aborted && election_mode == DEAD_OLD_PRIMARY) {
    /*
     Why send this message here if we did not wait for this step?
     Since the election is now a complex process, previous invocations might
     have have enabled certification and then died, so we send this message.
    */
    if (send_message(&single_primary_queue_applied)) {
      /* purecov: begin inspected */
      error = 3;
      err_msg.assign(
          "Couldn't instruct the group members to disable certification.");
      goto end;
      /* purecov: end */
    }
    group_events_observation_manager->after_primary_election(primary_uuid, true,
                                                             election_mode);
    goto wait_for_queued_message;
  }

  /* Can only set the stage here as the read mode call removes the stage info */
  mysql_mutex_lock(&election_lock);
  stage_handler->set_stage(
      info_GR_STAGE_primary_election_group_read_only.m_key, __FILE__, __LINE__,
      number_of_know_members,
      number_of_know_members - known_members_addresses.size());
  mysql_mutex_unlock(&election_lock);

  /**
    Note that we also send a message from us so the decision is always in
    a group logical moment, even if only a member exists
  */
  if (!election_process_aborted) {
    signal_read_mode_ready();
  }

  mysql_mutex_lock(&election_lock);
  while (!group_in_read_mode && !election_process_aborted) {
    DBUG_PRINT("sleep", ("Waiting for the group to be in read mode."));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);

wait_for_local_transactions:

  stage_handler->set_stage(
      info_GR_STAGE_primary_election_old_primary_transactions.m_key, __FILE__,
      __LINE__, 1, 0);

  if (applier_module->wait_for_current_events_execution(
          applier_checkpoint_condition, &election_process_aborted, false)) {
    /* purecov: begin inspected */
    error = 4;
    err_msg.assign("Could not wait for the execution of remote transactions.");
    goto end;
    /* purecov: end */
  }

  if (!election_process_aborted) {
    if (send_message(&single_primary_queue_applied)) {
      /* purecov: begin inspected */
      error = 5;
      err_msg.assign(
          "Couldn't instruct the group members to disable certification.");
      goto end;
      /* purecov: end */
    }
  }
  stage_handler->set_completed_work(1);

wait_for_queued_message:

  /**
    Why wait for the receiving of this message? So we can declare the election
    process as stopped in all member at the same logical moment.
    This is important for group configuration actions that can't start in
    parallel.

    Note however that this packet is going to be applied asynchronously and
    disable certification so configuration actions should be aware of that
  */
  mysql_mutex_lock(&election_lock);
  while (!waiting_on_queue_applied_message && !election_process_aborted) {
    DBUG_PRINT("sleep", ("Waiting for own message about queued applied"));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  mysql_mutex_unlock(&election_lock);

end:

  primary_election_handler->set_election_running(false);

  group_events_observation_manager->unregister_group_event_observer(this);

  if (error && !election_process_aborted) {
    group_events_observation_manager->after_primary_election(
        primary_uuid, true, election_mode, PRIMARY_ELECTION_PROCESS_ERROR);
    kill_transactions_and_leave_on_election_error(err_msg);
  }

  if (!election_process_aborted && !error) {
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SRV_PRIMARY_MEM);
  }

  stage_handler->end_stage();
  stage_handler->terminate_stage_monitor();
  delete stage_handler;
  stage_handler = nullptr;

  thd->release_resources();
  global_thd_manager_remove_thd(thd);

  mysql_mutex_lock(&election_lock);
  election_process_thd_state.set_terminated();
  election_process_ending = false;
  mysql_cond_broadcast(&election_cond);
  mysql_mutex_unlock(&election_lock);

  Gcs_interface_factory::cleanup_thread_communication_resources(
      Gcs_operations::get_gcs_engine());

  my_thread_end();
  delete thd;

  DBUG_RETURN(error);
}

int Primary_election_primary_process::after_view_change(
    const std::vector<Gcs_member_identifier> &,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &, bool is_leaving,
    bool *skip_election, enum_primary_election_mode *, std::string &) {
  *skip_election = false;

  if (is_leaving) {
    terminate_election_process(false);
    return 0;
  }

  mysql_mutex_lock(&election_lock);
  for (const Gcs_member_identifier &member_identifier : leaving) {
    known_members_addresses.remove(member_identifier.get_member_id());
  }
  stage_handler->set_completed_work(number_of_know_members -
                                    known_members_addresses.size());

  if (known_members_addresses.empty() && !group_in_read_mode) {
    group_in_read_mode = true;
    mysql_cond_broadcast(&election_cond);
    group_events_observation_manager->after_primary_election(primary_uuid, true,
                                                             election_mode);
  }
  mysql_mutex_unlock(&election_lock);

  return 0;
}

int Primary_election_primary_process::after_primary_election(
    std::string, bool, enum_primary_election_mode, int) {
  return 0;
}

int Primary_election_primary_process::before_message_handling(
    const Plugin_gcs_message &message, const std::string &message_origin,
    bool *skip_message) {
  *skip_message = false;
  Plugin_gcs_message::enum_cargo_type message_type = message.get_cargo_type();

  if (message_type == Plugin_gcs_message::CT_SINGLE_PRIMARY_MESSAGE) {
    const Single_primary_message single_primary_message =
        (const Single_primary_message &)message;
    Single_primary_message::Single_primary_message_type
        single_primary_msg_type =
            single_primary_message.get_single_primary_message_type();

    if (single_primary_msg_type ==
        Single_primary_message::SINGLE_PRIMARY_PRIMARY_READY) {
      mysql_mutex_lock(&election_lock);
      primary_ready = true;
      if (election_mode != DEAD_OLD_PRIMARY) {
        applier_module->queue_certification_enabling_packet();
      }
      mysql_cond_broadcast(&election_cond);
      mysql_mutex_unlock(&election_lock);
    }

    if (single_primary_msg_type ==
        Single_primary_message::SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE) {
      mysql_mutex_lock(&election_lock);
      election_process_ending = true;
      waiting_on_queue_applied_message = true;
      mysql_cond_broadcast(&election_cond);
      mysql_mutex_unlock(&election_lock);
    }

    if (single_primary_msg_type ==
        Single_primary_message::SINGLE_PRIMARY_READ_MODE_SET) {
      mysql_mutex_lock(&election_lock);
      known_members_addresses.remove(message_origin);
      stage_handler->set_completed_work(number_of_know_members -
                                        known_members_addresses.size());
      if (known_members_addresses.empty()) {
        group_in_read_mode = true;
        mysql_cond_broadcast(&election_cond);
        group_events_observation_manager->after_primary_election(
            primary_uuid, true, election_mode);
      }
      mysql_mutex_unlock(&election_lock);
    }
  }

  return 0;
}

bool Primary_election_primary_process::signal_read_mode_ready() {
  Single_primary_message single_primary_message(
      Single_primary_message::SINGLE_PRIMARY_READ_MODE_SET);
  return send_message(&single_primary_message);
}

int Primary_election_primary_process::terminate_election_process(bool wait) {
  mysql_mutex_lock(&election_lock);

  if (election_process_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&election_lock); /* purecov: inspected */
    return 0;                           /* purecov: inspected */
  }
  election_process_aborted = true;
  if (applier_checkpoint_condition) applier_checkpoint_condition->signal();

  // Awake up possible stuck conditions
  mysql_cond_broadcast(&election_cond);

  if (wait) {
    while (election_process_thd_state.is_thread_alive()) {
      DBUG_PRINT("sleep",
                 ("Waiting for the Primary election process thread to finish"));
      mysql_cond_wait(&election_cond, &election_lock);
    }

    DBUG_ASSERT(election_process_thd_state.is_thread_dead());
  }

  mysql_mutex_unlock(&election_lock);

  return 0;
}

void Primary_election_primary_process::wait_on_election_process_termination() {
  mysql_mutex_lock(&election_lock);

  if (election_process_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&election_lock);
    return;
  }

  while (election_process_thd_state.is_thread_alive()) {
    DBUG_PRINT("sleep",
               ("Waiting for the Primary election process thread to finish"));
    mysql_cond_wait(&election_cond, &election_lock);
  }
  DBUG_ASSERT(election_process_thd_state.is_thread_dead());

  mysql_mutex_unlock(&election_lock);

  return;
}
